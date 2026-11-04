#include "foc.h"
#include "encoder.h"
#include <math.h>

extern TIM_HandleTypeDef htim1;

/* ---------- 内部变量（只在本文件用） ---------- */
static float g_angle = 0.0f;    /* 当前开环累加的电角度，弧度 */
static float g_speed_e      = 0.0f;    /* 当前电角速度，弧度/秒 */
static float g_angle_mech = 0.0f;  /* 编码器实测机械角，给调试看 */
static float g_error_pos     = 0.0f;  /* 最近一次位置误差，给调试看 */
static float g_integral_pos  = 0.0f;  /* 位置环积分项，单位伏 */
static float g_offset_trim = 0.0f; /* 电角度微调，offset 扫描标定用 */
static float g_integral_speed   = 0.0f; /* 速度环积分项，单位伏 */
static uint32_t g_stall   = 0;    /* 速度环"卡住"计数 */

void foc_set_offset_trim(float trim)
{
    g_offset_trim = trim;
}

volatile foc_debug_t g_foc_dbg;

#define TWO_PI   6.28318530f

#define FOC_PI   3.14159265f

#define SQRT3_2  0.86602540f

void foc_init(void)
{
    g_angle        = 0.0f;
    g_speed_e             = 0.0f;
    g_integral_pos   = 0.0f;
    g_integral_speed = 0.0f;
}

float foc_voltage_vf(float speed_e)
{
    float voltage_q;

    if (speed_e < 0.0f)
    {
        speed_e = -speed_e;
    }

    voltage_q = FOC_VF_SLOPE * speed_e + FOC_VOLT_OFFSET;

    if (voltage_q > FOC_VOLTAGE_MAX)
    {
        voltage_q = FOC_VOLTAGE_MAX;
    }

    return voltage_q;
}

void foc_align(void)
{
    float voltage_q;
    float u_alpha, u_beta;
    float u_a, u_b, u_c;
    float u_max, u_min, u_com;
    float duty_a, duty_b, duty_c;

    voltage_q = FOC_ALIGN_VOLTAGE;

    g_angle = FOC_ALIGN_THETA;

    u_alpha = -voltage_q * sinf(FOC_ALIGN_THETA);
    u_beta  =  voltage_q * cosf(FOC_ALIGN_THETA);

    u_a =  u_alpha;
    u_b = -0.5f * u_alpha + SQRT3_2 * u_beta;
    u_c = -0.5f * u_alpha - SQRT3_2 * u_beta;

    u_max = u_a;
    if (u_b > u_max) { u_max = u_b; }
    if (u_c > u_max) { u_max = u_c; }
    u_min = u_a;
    if (u_b < u_min) { u_min = u_b; }
    if (u_c < u_min) { u_min = u_c; }
    u_com = -(u_max + u_min) * 0.5f;

    duty_a = 0.5f + (u_a + u_com) / VDC_BUS;
    duty_b = 0.5f + (u_b + u_com) / VDC_BUS;
    duty_c = 0.5f + (u_c + u_com) / VDC_BUS;

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint32_t)(duty_a * PWM_ARR));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, (uint32_t)(duty_b * PWM_ARR));
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, (uint32_t)(duty_c * PWM_ARR));
}

void foc_stop(void)
{
    uint32_t half = (uint32_t)(0.5f * PWM_ARR);

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, half);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, half);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, half);
}

static float wrap_two_pi(float angle)
{
    while (angle >= TWO_PI) { angle -= TWO_PI; }
    while (angle <  0.0f)   { angle += TWO_PI; }
    return angle;
}

static float wrap_pi(float angle)
{
    while (angle >  FOC_PI) { angle -= TWO_PI; }
    while (angle < -FOC_PI) { angle += TWO_PI; }
    return angle;
}

static void foc_output(float angle, float voltage_q)
{
    float u_alpha, u_beta;
    float u_a, u_b, u_c;
    float u_max, u_min, u_com;
    float duty_a, duty_b, duty_c;
    uint32_t ccr_a, ccr_b, ccr_c;

    u_alpha = -voltage_q * sinf(angle);
    u_beta  =  voltage_q * cosf(angle);

    u_a =  u_alpha;
    u_b = -0.5f * u_alpha + SQRT3_2 * u_beta;
    u_c = -0.5f * u_alpha - SQRT3_2 * u_beta;

    u_max = u_a;
    if (u_b > u_max) { u_max = u_b; }
    if (u_c > u_max) { u_max = u_c; }

    u_min = u_a;
    if (u_b < u_min) { u_min = u_b; }
    if (u_c < u_min) { u_min = u_c; }

    u_com = -(u_max + u_min) * 0.5f;

    u_a = u_a + u_com;
    u_b = u_b + u_com;
    u_c = u_c + u_com;

    duty_a = 0.5f + u_a / VDC_BUS;
    duty_b = 0.5f + u_b / VDC_BUS;
    duty_c = 0.5f + u_c / VDC_BUS;

    if (duty_a < 0.0f) { duty_a = 0.0f; }
    if (duty_a > 1.0f) { duty_a = 1.0f; }
    if (duty_b < 0.0f) { duty_b = 0.0f; }
    if (duty_b > 1.0f) { duty_b = 1.0f; }
    if (duty_c < 0.0f) { duty_c = 0.0f; }
    if (duty_c > 1.0f) { duty_c = 1.0f; }

    g_foc_dbg.angle     = angle;
    g_foc_dbg.voltage_q = voltage_q;
    g_foc_dbg.duty_a    = duty_a;
    g_foc_dbg.duty_b    = duty_b;
    g_foc_dbg.duty_c    = duty_c;

    ccr_a = (uint32_t)(duty_a * PWM_ARR);
    ccr_b = (uint32_t)(duty_b * PWM_ARR);
    ccr_c = (uint32_t)(duty_c * PWM_ARR);

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ccr_a);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, ccr_b);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, ccr_c);
}

void foc_open(float target_speed)
{
    float speed_e;
    float angle;
    float voltage_q;

    speed_e = target_speed * (float)FOC_POLE_PAIRS;
    g_speed_e = speed_e;

    angle = g_angle + speed_e * FOC_TS;
    angle = wrap_two_pi(angle);
    g_angle = angle;

    voltage_q = foc_voltage_vf(speed_e);

    /* 安全限幅：V/f 曲线在高速时会给出几伏甚至接近母线电压，
       但那只是 SVPWM 的物理极限，不是芯片散热能承受的。
       必须和闭环一样受 FOC_UQ_LIMIT 约束，否则改大 TARGET_SPEED 会烧板子。 */
    if (voltage_q >  FOC_UQ_LIMIT) { voltage_q =  FOC_UQ_LIMIT; }
    if (voltage_q < -FOC_UQ_LIMIT) { voltage_q = -FOC_UQ_LIMIT; }

    g_foc_dbg.target  = target_speed;
    g_foc_dbg.speed_e = speed_e;

    foc_output(angle, voltage_q);
}

void foc_static(float target_angle, float voltage_q)
{
    g_foc_dbg.target  = target_angle;
    g_foc_dbg.speed_e = 0.0f;
    foc_output(wrap_two_pi(target_angle), voltage_q);
}

void foc_close_position(float target_angle)
{
    float angle_mech;
    float angle;
    float error_pos;
    float voltage_q;

    encoder_update();
    angle_mech = g_encoder_dbg.angle_mech;
    g_angle_mech = angle_mech;

    angle = wrap_two_pi(angle_mech * (float)FOC_POLE_PAIRS
                         + ENC_ELEC_OFFSET
                         + g_offset_trim);

    error_pos = wrap_pi(target_angle - angle_mech);
    g_error_pos = error_pos;

    /* 积分只在"误差确实存在"时累积，且自己有独立的、更小的限幅。
       为什么要单独限幅：这台电机静摩擦大，小误差推不动，
       积分会一直涨到满力（1.3V），一旦涨满就变成开关控制 → 疯狂震荡。
       把积分限幅到满力的 1/3，它就只能提供"偏置"，主力仍由 P 项出。 */
    if (error_pos >  FOC_POS_I_DEADZONE ||
        error_pos < -FOC_POS_I_DEADZONE)
    {
        g_integral_pos += FOC_POS_KI * error_pos * FOC_TS;
    }

    if (g_integral_pos >  FOC_POS_I_LIMIT) { g_integral_pos =  FOC_POS_I_LIMIT; }
    if (g_integral_pos < -FOC_POS_I_LIMIT) { g_integral_pos = -FOC_POS_I_LIMIT; }

    voltage_q = FOC_POS_KP * error_pos
       + g_integral_pos
       - FOC_POS_KD * g_encoder_dbg.speed;

    voltage_q = FOC_TORQUE_SIGN * voltage_q;

    if (voltage_q >  FOC_UQ_LIMIT) { voltage_q =  FOC_UQ_LIMIT; }
    if (voltage_q < -FOC_UQ_LIMIT) { voltage_q = -FOC_UQ_LIMIT; }

    g_foc_dbg.target     = target_angle;
    g_foc_dbg.speed_e    = g_encoder_dbg.speed * (float)FOC_POLE_PAIRS;
    g_foc_dbg.angle_mech = angle_mech;
    g_foc_dbg.error_pos  = error_pos;

    foc_output(angle, voltage_q);
}

void foc_close_speed(float target_speed)
{
    float angle_mech;
    float speed;
    float angle;
    float error_speed;
    float feed_forward;
    float speed_e;
    float voltage_q;
    float direction;

    encoder_update();
    angle_mech    = g_encoder_dbg.angle_mech;
    speed = g_encoder_dbg.speed;

    angle = wrap_two_pi(angle_mech * (float)FOC_POLE_PAIRS
                         + ENC_ELEC_OFFSET
                         + g_offset_trim);

    error_speed = target_speed - speed;

    speed_e = target_speed * (float)FOC_POLE_PAIRS;
    direction = (speed_e >= 0.0f) ? 1.0f : -1.0f;
    if (speed_e < 0.0f) { speed_e = -speed_e; }

    feed_forward = direction * (FOC_VF_SLOPE * speed_e + FOC_VOLT_OFFSET);

    g_integral_speed += FOC_SPEED_KI * error_speed * FOC_TS;

    if (g_integral_speed >  FOC_UQ_LIMIT) { g_integral_speed =  FOC_UQ_LIMIT; }
    if (g_integral_speed < -FOC_UQ_LIMIT) { g_integral_speed = -FOC_UQ_LIMIT; }

    voltage_q = feed_forward
       + FOC_SPEED_KP * error_speed
       + g_integral_speed;

    if (target_speed > 0.2f || target_speed < -0.2f)
    {
        if (speed < 0.1f && speed > -0.1f)
        {
            g_stall++;
            if (g_stall > FOC_STALL_TICKS)
            {
                voltage_q += direction * FOC_START_BOOST;
            }

            if (g_stall > FOC_BOOST_TICKS)
            {
                g_stall = 0;
            }
        }
        else
        {
            g_stall = 0;
        }
    }
    else
    {
        g_stall = 0;
    }

    voltage_q = FOC_TORQUE_SIGN * voltage_q;

    if (voltage_q >  FOC_UQ_LIMIT) { voltage_q =  FOC_UQ_LIMIT; }
    if (voltage_q < -FOC_UQ_LIMIT) { voltage_q = -FOC_UQ_LIMIT; }

    g_foc_dbg.target     = target_speed;
    g_foc_dbg.speed_e    = speed * (float)FOC_POLE_PAIRS;
    g_foc_dbg.angle_mech = angle_mech;
    g_foc_dbg.error_pos  = error_speed;

    foc_output(angle, voltage_q);
}
