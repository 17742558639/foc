/**
 ******************************************************************************
 * @file    encoder.c
 * @brief   MT6701 磁编码器（SPI/SSI 模式）驱动
 *
 *  按"从上往下读"的顺序组织：
 *      1. 内部状态
 *      2. 小工具函数（归一化、差值处理）
 *      3. 读原始值（唯一碰 SPI 的地方）
 *      4. 换算成角度
 *      5. 每拍更新（速度、观测量）
 *      6. 零位标定
 ******************************************************************************
 */

#include "encoder.h"
#include "foc.h"
#include <math.h>

/* 这个工程是"扁平结构"，SPI 的句柄直接在 main.c 里定义（没有 spi.c/spi.h），
   所以这里用 extern 引用它 */
extern SPI_HandleTypeDef hspi1;

/* ============================================================
 *  一、内部状态
 * ============================================================ */

volatile encoder_debug_t g_encoder_dbg;

/* 上一次读到的机械角，用来算速度 */
static float s_last_mech_rad = 0.0f;

/* 窗口内累计的角度增量（弧度，已处理过零） */
static float s_delta_accum = 0.0f;

/* 窗口内已经累计了多少拍 */
static uint16_t s_sample_count = 0;

/* 第一次调用时还没有"上一次角度"，不参与差分 */
static uint8_t s_first_run = 1;

/* ============================================================
 *  二、小工具函数
 * ============================================================ */

/* 把角度折回 0 ~ 2π */
static float wrap_two_pi(float angle)
{
    while (angle >= ENC_TWO_PI) { angle -= ENC_TWO_PI; }
    while (angle < 0.0f)        { angle += ENC_TWO_PI; }
    return angle;
}

/* 把角度差折回 -π ~ +π，用来判断是否跨过了 0/2π 边界 */
static float wrap_delta(float delta)
{
    if (delta >  ENC_PI) { delta -= ENC_TWO_PI; }
    if (delta < -ENC_PI) { delta += ENC_TWO_PI; }
    return delta;
}

/* ============================================================
 *  三、读原始值（唯一碰硬件的地方）
 * ============================================================ */

uint16_t encoder_read_raw(void)
{
    uint8_t  tx_buffer[2] = {0xFF, 0xFF};   /* 发什么无所谓，从机不看 MOSI */
    uint8_t  rx_buffer[2] = {0x00, 0x00};
    uint16_t raw_value;

    /* 片选拉低：告诉编码器"开始" */
    HAL_GPIO_WritePin(ENC_CS_PORT, ENC_CS_PIN, GPIO_PIN_RESET);

    /* 打 16 个时钟，从机把 16 位数据吐回来 */
    HAL_SPI_TransmitReceive(&hspi1, tx_buffer, rx_buffer, 2, 10);

    /* 片选拉高：结束这次传输 */
    HAL_GPIO_WritePin(ENC_CS_PORT, ENC_CS_PIN, GPIO_PIN_SET);

    raw_value = ((uint16_t)rx_buffer[0] << 8) | (uint16_t)rx_buffer[1];

    /* 读写异常判断：全 0 或全 1 说明总线没通 */
    if (raw_value == 0x0000 || raw_value == 0xFFFF) {
        g_encoder_dbg.error_count++;
    }

    return raw_value;
}

/* ============================================================
 *  四、换算成角度
 * ============================================================ */

float encoder_read_mech(void)
{
    uint16_t raw_value;
    uint16_t angle_count;
    float    mech_rad;

    raw_value   = encoder_read_raw();
    angle_count = (uint16_t)(raw_value >> ENC_ANGLE_SHIFT);

    /* 计数 → 弧度 */
    mech_rad = (float)angle_count * ENC_TWO_PI / ENC_COUNTS_PER_REV;

    /* 方向：编码器装配方向反了就把读数镜像过来 */
    mech_rad = wrap_two_pi(mech_rad * ENC_DIR);

    /* 减掉零位偏移 */
    mech_rad = wrap_two_pi(mech_rad - g_encoder_dbg.zero_offset);

    g_encoder_dbg.raw = raw_value;

    return mech_rad;
}

float encoder_read_elec(void)
{
    float mech_rad = encoder_read_mech();
    float elec_rad = mech_rad * (float)ENC_POLE_PAIRS;

    g_encoder_dbg.mech_rad = mech_rad;
    g_encoder_dbg.elec_rad = wrap_two_pi(elec_rad);

    return g_encoder_dbg.elec_rad;
}

/* ============================================================
 *  五、每拍更新
 * ============================================================ */

void encoder_update(void)
{
    float mech_rad;
    float delta_rad;

    mech_rad = encoder_read_mech();

    /* 第一拍只记录，不算速度（否则第一拍会出现一个巨大的假速度） */
    if (s_first_run) {
        s_last_mech_rad = mech_rad;
        s_first_run     = 0;
        g_encoder_dbg.mech_rad      = mech_rad;
        g_encoder_dbg.elec_rad      = wrap_two_pi(mech_rad * (float)ENC_POLE_PAIRS);
        g_encoder_dbg.speed_radps   = 0.0f;
        return;
    }

    /* 本拍相对上一拍转过的角度（处理过零） */
    delta_rad = wrap_delta(mech_rad - s_last_mech_rad);
    s_last_mech_rad = mech_rad;

    /* 累积到窗口里 */
    s_delta_accum += delta_rad;
    s_sample_count++;

    /* 窗口满了，算一次速度 */
    if (s_sample_count >= ENC_SPEED_WINDOW) {
        float window_seconds = (float)ENC_SPEED_WINDOW * FOC_TS;
        float instant_speed  = s_delta_accum / window_seconds;

        /* 一阶低通，压掉量化噪声 */
        g_encoder_dbg.speed_radps = g_encoder_dbg.speed_radps * (1.0f - ENC_SPEED_LPF)
                                  + instant_speed * ENC_SPEED_LPF;

        s_delta_accum  = 0.0f;
        s_sample_count = 0;
    }

    g_encoder_dbg.mech_rad = mech_rad;
    g_encoder_dbg.elec_rad = wrap_two_pi(mech_rad * (float)ENC_POLE_PAIRS);
}

float encoder_read_speed(void)
{
    return g_encoder_dbg.speed_radps;
}

/* ============================================================
 *  六、初始化与零位标定
 * ============================================================ */

void encoder_init(void)
{
    /* 片选先拉高，避免上电瞬间被误选中 */
    HAL_GPIO_WritePin(ENC_CS_PORT, ENC_CS_PIN, GPIO_PIN_SET);

    g_encoder_dbg.raw         = 0;
    g_encoder_dbg.mech_rad    = 0.0f;
    g_encoder_dbg.elec_rad    = 0.0f;
    g_encoder_dbg.speed_radps = 0.0f;
    g_encoder_dbg.zero_offset = 0.0f;
    g_encoder_dbg.error_count = 0;

    s_last_mech_rad = 0.0f;
    s_delta_accum   = 0.0f;
    s_sample_count  = 0;
    s_first_run     = 1;
}

void encoder_set_zero(void)
{
    uint16_t raw_value;
    uint16_t angle_count;
    float    raw_rad;

    /* 直接读一次原始角度，不做偏移，作为新的零点 */
    raw_value   = encoder_read_raw();
    angle_count = (uint16_t)(raw_value >> ENC_ANGLE_SHIFT);
    raw_rad     = (float)angle_count * ENC_TWO_PI / ENC_COUNTS_PER_REV;

    g_encoder_dbg.zero_offset = raw_rad;
    s_first_run = 1;   /* 零位变了，速度要重新起算 */
}
