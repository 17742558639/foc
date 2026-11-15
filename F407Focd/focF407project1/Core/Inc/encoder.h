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

/*
 * === 旧版：按14位标称值使用16384计数/圈（已弃用）===
 * 问题：实测电机轴转一机械圈，I3从8193变化到约16207，只增加约8014计数，
 *       使用16384会让机械角只变化约半圈。
 * === 新版：按实物标定为8192计数/机械圈 ===
 * 后续机械角、速度和电角度计算都以实测机械圈为准。
 */
/* #define ENC_COUNTS_PER_REV   16384.0f */
#define ENC_COUNTS_PER_REV   8192.0f

/* 电机极对数，必须和 foc.h 里的 FOC_POLE_PAIRS 一致 */
#define ENC_POLE_PAIRS       10

/* 编码器计数方向。
 * 正常情况下电机磁场正转时读数应该增大；
 * 如果位置环"往反方向推"（误差收不回去、Uq 一直饱和），改成 -1.0f。
 * ★ 改了这个值必须重新标定 ENC_ELEC_OFFSET（回到 ENC_TEST_MODE=2 读 ch4） */
/*
 * === 旧版：逆时针为机械角正方向（已弃用）===
 * 问题：实测顺时针转动时计数和机械角递减，不符合本项目“顺时针为正”的约定。
 * === 新版：顺时针为机械角正方向 ===
 */
/* #define ENC_DIR           (1.0f) */
#define ENC_DIR              (-1.0f)

/* 片选引脚：CubeMX 里 PA4 已经配成 GPIO_Output，标签叫 cs_MT6701 */
#define ENC_CS_PORT          cs_MT6701_GPIO_Port
#define ENC_CS_PIN           cs_MT6701_Pin

/* 原始 16 位里，角度占据的位。
 * 高 14 位是角度 → 右移 2 位；若读出来不对，改成 0 试试（取低 14 位） */
#define ENC_ANGLE_SHIFT      2

/*
 * 旧版：#define ENC_SPEED_WINDOW 40，实际只有2ms，对量化抖动过于敏感。
 * 新版：200拍=10ms，单计数对速度的影响降至约0.077rad/s。
 */
#define ENC_SPEED_WINDOW     200

/* 速度低通系数：0.2 表示新值占 20%，越小越平滑但越滞后 */
/* 旧版：#define ENC_SPEED_LPF 0.5f */
#define ENC_SPEED_LPF        0.2f

#define ENC_TWO_PI           6.2831853f
#define ENC_PI               3.14159265f
#define ENC_HALF_PI          1.57079633f

/*
 * === 旧版：旧方向和旧参数下的电角度偏移（已弃用）===
 * 原记录使用21极对和旧方向参数，已经不适用于当前10极对配置。
 * === 新版：当前实物静态标定 ===
 * ENC_DIR=-1、ENC_ELEC_DIR=-1、10极对；模式2实测机械角3.294224rad，
 * 未加偏移电角度4.756876rad，VOFA I4给出偏移4.667905rad。
 * 加偏移后电角度归一化到约π，与FOC_ALIGN_THETA+π/2一致。
 */
/* 电角度方向：编码器机械角 -> 电角度时乘这个系数。
 * 换相序后如果出现"静止时符号对、转动时 iq 整体反号"，就是这个要改成 -1。
 * 它只影响 FOC 用的电角度，不影响测速读数。 */
#define ENC_ELEC_DIR        (-1.0f)

/* #define ENC_ELEC_OFFSET  2.9480f */
/* #define ENC_ELEC_OFFSET  4.6679f */
#define ENC_ELEC_OFFSET     4.7093f

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
    uint32_t jump_reject_count; /* 不可能的单拍大角度突跳计数 */
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
