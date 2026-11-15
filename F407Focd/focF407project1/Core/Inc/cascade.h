#ifndef __CASCADE_H__
#define __CASCADE_H__

#include "main.h"

/* ---------- 位置环 ---------- */
#define CASCADE_POS_KP            5.0f         /* 速度指令 / 位置误差，rad/s 每 rad */
#define CASCADE_POS_DEADZONE      0.080f       /* 位置死区，rad */
#define CASCADE_POS_V_MIN         10.0f        /* 最低速度指令，rad/s */
#define CASCADE_V_CMD_LIMIT       10.0f        /* 速度指令限幅，rad/s */
#define CASCADE_V_CMD_LPF         0.02f        /* 速度指令一阶低通系数 */

/* ---------- 速度环 ---------- */
#define SPEED_TEST_FF             0.0097f      /* 摩擦前馈，A 每 rad/s */
#define SPEED_TEST_KP             0.02f        /* 比例，A 每 rad/s */
#define SPEED_TEST_KI             0.067f       /* 积分，A 每 (rad/s)·s */
#define SPEED_TEST_I_LIMIT        0.60f        /* 积分限幅，A */
#define SPEED_TEST_IQ_LIMIT       0.80f        /* 输出限幅，A */

/* ---------- 起动（自适应 boost） ---------- */
#define SPEED_BREAK_SPEED         0.50f        /* 判定"起来了"的转速，rad/s */
#define SPEED_BREAK_CONFIRM_TICKS 200U         /* 需连续满足的拍数 */
#define SPEED_BOOST_START         0.05f        /* 起始电流，A */
#define SPEED_BOOST_STEP          0.01f        /* 每次增量，A */
#define SPEED_BOOST_INTERVAL      10U          /* 每几拍加一次 */
#define SPEED_BOOST_MAX           0.80f        /* 电流上限，A */
#define SPEED_STARTUP_TIMEOUT_TICKS  40000U    /* 起动超时，拍 */
#define SPEED_TIMEOUT_COOLDOWN_TICKS 10000U    /* 超时后冷却，拍 */

/* ---------- 堵转重启 ---------- */
#define SPEED_RESTART_SPEED       0.30f        /* 判"没动"的转速，rad/s */
#define SPEED_RESTART_TICKS       40000U       /* 持续多少拍后重启，拍 */

void  cascade_init(void);

void  cascade_speed(float target_speed);

void  cascade_position(float target_angle);

float cascade_get_v_cmd(void);
float cascade_get_iq_cmd(void);
float cascade_get_error_pos(void);

#endif
