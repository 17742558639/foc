#ifndef __FOC_H__
#define __FOC_H__

#include "main.h"

/* ==================== 基础参数 ==================== */
#define FOC_TS              0.00005f    /* 控制周期，秒（20kHz） */
#define PWM_ARR             4199.0f     /* 定时器 ARR */
#define VDC_BUS             12.0f       /* 母线电压，伏 */

/* ==================== 电机参数 ==================== */
#define FOC_POLE_PAIRS      10          /* 极对数，实测确认 */
#define FOC_TORQUE_SIGN     1.0f        /* 转矩方向，与 offset 配套 */

/* ==================== 电压限制 ==================== */
#define FOC_VOLTAGE_MAX     6.92f       /* 相电压上限，伏（Vdc/√3） */
#define FOC_UQ_LIMIT        1.30f       /* 实际用的电压限幅，伏 */

/* ==================== 开环 V/f ==================== */
#define FOC_VF_SLOPE        0.0030f     /* 斜率，伏/(弧度/秒) */
#define FOC_VOLT_OFFSET     0.50f       /* 低速补偿电压，伏 */

/* ==================== 预定位标定 ==================== */
#define FOC_ALIGN_VOLTAGE   0.50f       /* 预定位电压，伏 */
#define FOC_ALIGN_THETA     1.5708f     /* 预定位矢量电角度，弧度 */

/* ==================== 位置环 PID ==================== */
#define FOC_POS_KP          5.0f        /* 比例，伏/弧度 */
#define FOC_POS_KI          0.0f        /* 积分，伏/(弧度·秒) */
#define FOC_POS_KD          0.0f       /* 微分，伏/(弧度/秒) */

/* ==================== 速度环 PI ==================== */
#define FOC_SPEED_KP        0.05f       /* 比例，伏/(弧度/秒) */
#define FOC_SPEED_KI        4.0f        /* 积分，伏/(弧度/秒·秒) */

/* ==================== 起动冲击（破静摩擦） ==================== */
#define FOC_START_BOOST     0.60f       /* 冲击电压，伏 */
#define FOC_STALL_TICKS     4000        /* 判卡住：4000 拍 = 0.2 秒 */
#define FOC_BOOST_TICKS     8000        /* 冲击节奏：8000 拍 = 0.4 秒 */

/* ==================== 对外函数 ==================== */
void  foc_init(void);
void  foc_align(void);
void  foc_stop(void);
void  foc_static(float target_angle, float voltage_q);
void  foc_set_offset_trim(float trim);
float foc_voltage_vf(float speed_e);

void  foc_open(float target_speed);                       /* 开环 */
void  foc_close_position(float target_angle);        /* 位置闭环 */
void  foc_close_speed(float target_speed);        /* 速度闭环 */

/* ==================== 调试观测量 ==================== */
typedef struct
{
    float target;        /* 目标转速 或 目标角度 */
    float speed_e;       /* 电角速度，弧度/秒 */
    float angle;         /* 电角度，弧度 */
    float voltage_q;     /* q 轴电压，伏 */
    float duty_a;        /* A 相占空比 0~1 */
    float duty_b;        /* B 相占空比 0~1 */
    float duty_c;        /* C 相占空比 0~1 */
    float angle_mech;    /* 机械角，弧度 */
    float error_pos;     /* 位置误差 或 速度误差 */
} foc_debug_t;

extern volatile foc_debug_t g_foc_dbg;

#endif
