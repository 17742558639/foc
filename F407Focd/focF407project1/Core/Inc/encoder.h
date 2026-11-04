/**
 ******************************************************************************
 * @file    encoder.h
 * @brief   MT6701 磁编码器（SPI/SSI 模式）驱动
 *
 *  接线：
 *      MT6701 VCC  -> 3.3V
 *      MT6701 GND  -> GND
 *      MT6701 Z/CS -> PA4   （普通 GPIO，软件片选）
 *      MT6701 B/SCLK -> PA5 （SPI1_SCK）
 *      MT6701 A/MISO -> PA6 （SPI1_MISO）
 *      MOSI(PA7) 不接
 *
 *  MT6701 的 SSI 是单向只读协议：主机发 16 个时钟，从机在 MISO 上吐出
 *  16 位数据，高 14 位是绝对角度，低 2 位是状态位。
 *  所以只需要 CS / SCK / MISO 三根信号线。
 ******************************************************************************
 */

#ifndef __ENCODER_H
#define __ENCODER_H

#include <stdint.h>
#include "main.h"

/* ============================================================
 *  一、可调参数
 * ============================================================ */

/* MT6701 是 14 位，每机械圈 16384 个计数 */
#define ENC_COUNTS_PER_REV   8192.0f

/* 电机极对数，必须和 foc.h 里的 FOC_POLE_PAIRS 一致 */
#define ENC_POLE_PAIRS       10

/* 编码器计数方向。
 * 正常情况下电机磁场正转时读数应该增大；
 * 如果位置环"往反方向推"（误差收不回去、Uq 一直饱和），改成 -1.0f。
 * ★ 改了这个值必须重新标定 ENC_ELEC_OFFSET（回到 ENC_TEST_MODE=2 读 ch4） */
#define ENC_DIR              (1.0f)

/* 片选引脚：CubeMX 里 PA4 已经配成 GPIO_Output，标签叫 cs_MT6701 */
#define ENC_CS_PORT          cs_MT6701_GPIO_Port
#define ENC_CS_PIN           cs_MT6701_Pin

/* 原始 16 位里，角度占据的位。
 * 高 14 位是角度 → 右移 2 位；若读出来不对，改成 0 试试（取低 14 位） */
#define ENC_ANGLE_SHIFT      2

/* 速度计算窗口：每 100 拍（20kHz 下 = 5ms）算一次速度。
 * 用窗口而不是逐拍差分，是因为 16384 分辨率下逐拍差分量化噪声太大 */
#define ENC_SPEED_WINDOW     40

/* 速度低通系数：0.2 表示新值占 20%，越小越平滑但越滞后 */
#define ENC_SPEED_LPF        0.5f

#define ENC_TWO_PI           6.2831853f
#define ENC_PI               3.14159265f
#define ENC_HALF_PI          1.57079633f

/* 电角度零位偏移。极对数 21 确认后用 ENC_TEST_MODE=2 静态标定：
 * 转子被吸到 mech=6.1727，ch4 给出 5.460992。
 * 自洽校验：6.1727×21 mod 2π = 3.964，+5.461 = 9.425 mod 2π = π，
 * 正好等于预定位矢量方向（FOC_ALIGN_THETA π/2 + 90°）✅
 * 配 FOC_TORQUE_SIGN = +1 */
#define ENC_ELEC_OFFSET      0.6627f

/* ============================================================
 *  二、观测量（给 VOFA+ 看）
 * ============================================================ */

typedef struct {
    uint16_t raw;            /* SPI 读到的原始 16 位         */
    float    angle_mech;       /* 机械角，0 ~ 2π               */
    float    angle;       /* 电角度，0 ~ 2π               */
    float    speed;    /* 机械角速度，rad/s            */
    float    zero_offset;    /* 机械零位偏移                 */
    uint32_t error_count;    /* 读数异常计数（全 0 或全 1）  */
} encoder_debug_t;

extern volatile encoder_debug_t g_encoder_dbg;

/* ============================================================
 *  三、对外函数
 * ============================================================ */

/* 初始化：把 CS 拉高，清空内部状态 */
void encoder_init(void);

/* 读一次 SPI，返回 16 位原始数据（调试用） */
uint16_t encoder_read_raw(void);

/* 读一次机械角，单位弧度 0 ~ 2π（会发起一次 SPI 传输） */
float encoder_read_mech(void);

/* 读一次电角度，单位弧度 0 ~ 2π（已经乘过极对数） */
float encoder_read_elec(void);

/* 在 FOC 中断里每拍调一次：刷新 g_encoder_dbg，并累计速度 */
void encoder_update(void);

/* 取最近一次算出的机械角速度，rad/s */
float encoder_read_speed(void);

/* 把当前位置设为机械零点（调用后 mech_rad 归零） */
void encoder_set_zero(void);

#endif /* __ENCODER_H */
