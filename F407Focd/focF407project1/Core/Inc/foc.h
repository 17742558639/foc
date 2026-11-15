#ifndef __FOC_H__
#define __FOC_H__

#include "main.h"

#define FOC_TS              0.00005f
#define PWM_ARR             4199.0f
#define VDC_BUS             12.0f

#define FOC_POLE_PAIRS      10
#define FOC_TORQUE_SIGN     (-1.0f)

#define FOC_VOLTAGE_MAX     6.92f

#define FOC_UQ_LIMIT        0.60f

#define FOC_VF_SLOPE        0.0011f

#define FOC_VOLT_OFFSET     0.200f

#define FOC_ALIGN_VOLTAGE   0.35f
#define FOC_ALIGN_THETA     1.5708f

#define FOC_POS_KP          2.0f
#define FOC_POS_KI          0.0f
#define FOC_POS_KD          0.0f

#define FOC_POS_I_LIMIT     0.40f

#define FOC_POS_I_DEADZONE  0.010f

#define FOC_SPEED_KP        0.06f
#define FOC_SPEED_KI        0.0f

#define FOC_SPEED_I_LIMIT   0.60f

#define FOC_SPEED_ACC_FFF   0.000f

#define FOC_START_BOOST     0.0f
#define FOC_STALL_TICKS     4000
#define FOC_BOOST_TICKS     8000

#define FOC_CURRENT_KP      0.50f
#define FOC_CURRENT_KI      12.20f

/* 旧版：#define FOC_CURRENT_I_LIMIT 0.30f，高于0.30A的目标电流难以继续建立。 */
#define FOC_CURRENT_I_LIMIT 0.60f

#define FOC_IQ_LIMIT        3.20f

#define FOC_ID_TARGET       0.0f

#define FOC_CURRENT_LPF     0.50f

void  foc_init(void);
void  foc_align(void);
void  foc_stop(void);
void  foc_static(float target_angle, float voltage_q);
void  foc_set_offset_trim(float trim);
float foc_voltage_vf(float speed_e);

void  foc_open(float target_speed);
void  foc_close_position(float target_angle);
void  foc_close_speed(float target_speed);
void  foc_close_current(float target_iq, float target_id);

typedef struct
{
    float target;
    float speed_e;
    float angle;
    float voltage_q;
    float duty_a;
    float duty_b;
    float duty_c;
    float angle_mech;
    float error_pos;
    float current_d;
    float current_q;
    float current_q_ref;
} foc_debug_t;

extern volatile foc_debug_t g_foc_dbg;

#endif
