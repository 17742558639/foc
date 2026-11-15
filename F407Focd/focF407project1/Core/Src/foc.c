#include "foc.h"
#include "encoder.h"
#include "ADCcurrent.h"
#include <math.h>

extern TIM_HandleTypeDef htim1;

static float g_angle = 0.0f;
static float g_speed_e      = 0.0f;
static float g_angle_mech = 0.0f;
static float g_error_pos     = 0.0f;
static float g_integral_pos  = 0.0f;
static float g_offset_trim = 0.0f;
static float g_integral_speed   = 0.0f;
static uint32_t g_stall   = 0;

static float g_integral_id      = 0.0f;
static float g_integral_iq      = 0.0f;
static float g_current_d_filter = 0.0f;
static float g_current_q_filter = 0.0f;

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

    /* 保留旧版电压环调试状态，当前电流环模式不读取它们。 */
    (void)g_speed_e;
    (void)g_angle_mech;
    (void)g_error_pos;
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

static void foc_output_dq(float angle, float voltage_d, float voltage_q)
{
    float u_alpha, u_beta;
    float u_a, u_b, u_c;
    float u_max, u_min, u_com;
    float duty_a, duty_b, duty_c;
    uint32_t ccr_a, ccr_b, ccr_c;
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);

    u_alpha = voltage_d * cos_a - voltage_q * sin_a;
    u_beta  = voltage_d * sin_a + voltage_q * cos_a;

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

static void foc_output(float angle, float voltage_q)
{
    foc_output_dq(angle, 0.0f, voltage_q);
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

    angle = wrap_two_pi(angle_mech * (float)FOC_POLE_PAIRS * ENC_ELEC_DIR
                         + ENC_ELEC_OFFSET
                         + g_offset_trim);

    error_pos = wrap_pi(target_angle - angle_mech);
    g_error_pos = error_pos;

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

    angle = wrap_two_pi(angle_mech * (float)FOC_POLE_PAIRS * ENC_ELEC_DIR
                         + ENC_ELEC_OFFSET
                         + g_offset_trim);

    error_speed = target_speed - speed;

    speed_e = target_speed * (float)FOC_POLE_PAIRS;
    direction = (speed_e >= 0.0f) ? 1.0f : -1.0f;
    if (speed_e < 0.0f) { speed_e = -speed_e; }

    feed_forward = direction * (FOC_VF_SLOPE * speed_e + FOC_VOLT_OFFSET);

    g_integral_speed += FOC_SPEED_KI * error_speed * FOC_TS;

    if (g_integral_speed >  FOC_SPEED_I_LIMIT) { g_integral_speed =  FOC_SPEED_I_LIMIT; }
    if (g_integral_speed < -FOC_SPEED_I_LIMIT) { g_integral_speed = -FOC_SPEED_I_LIMIT; }

    {
        static float s_last_target_speed = 0.0f;
        float accel = (target_speed - s_last_target_speed) / FOC_TS;
        s_last_target_speed = target_speed;
        feed_forward += FOC_SPEED_ACC_FFF * accel;
    }

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

void foc_close_current(float target_iq, float target_id)
{
    float angle_mech;
    float angle;
    float cos_a, sin_a;
    float current_a, current_b, current_c;
    float current_alpha, current_beta;
    float current_d, current_q;
    float target_iq_control;
    float error_d, error_q;
    float voltage_d, voltage_q;
    float voltage_mag;

    encoder_update();
    angle_mech = g_encoder_dbg.angle_mech;

    angle = wrap_two_pi(angle_mech * (float)FOC_POLE_PAIRS * ENC_ELEC_DIR
                         + ENC_ELEC_OFFSET
                         + g_offset_trim);

    current_read(&current_a, &current_b, &current_c);

    current_alpha = current_a;
    current_beta  = (current_a + 2.0f * current_b) * 0.57735f;

    cos_a = cosf(angle);
    sin_a = sinf(angle);

    current_d =  current_alpha * cos_a + current_beta * sin_a;
    current_q = -current_alpha * sin_a + current_beta * cos_a;

    g_current_d_filter += FOC_CURRENT_LPF * (current_d - g_current_d_filter);
    g_current_q_filter += FOC_CURRENT_LPF * (current_q - g_current_q_filter);

    current_d = g_current_d_filter;
    current_q = g_current_q_filter;

    if (target_iq >  FOC_IQ_LIMIT) { target_iq =  FOC_IQ_LIMIT; }
    if (target_iq < -FOC_IQ_LIMIT) { target_iq = -FOC_IQ_LIMIT; }

    target_iq_control = FOC_TORQUE_SIGN * target_iq;

    error_d = target_id - current_d;
    error_q = target_iq_control - current_q;

    g_integral_id += FOC_CURRENT_KI * error_d * FOC_TS;
    g_integral_iq += FOC_CURRENT_KI * error_q * FOC_TS;

    if (g_integral_id >  FOC_CURRENT_I_LIMIT) { g_integral_id =  FOC_CURRENT_I_LIMIT; }
    if (g_integral_id < -FOC_CURRENT_I_LIMIT) { g_integral_id = -FOC_CURRENT_I_LIMIT; }
    if (g_integral_iq >  FOC_CURRENT_I_LIMIT) { g_integral_iq =  FOC_CURRENT_I_LIMIT; }
    if (g_integral_iq < -FOC_CURRENT_I_LIMIT) { g_integral_iq = -FOC_CURRENT_I_LIMIT; }

    voltage_d = FOC_CURRENT_KP * error_d + g_integral_id;
    voltage_q = FOC_CURRENT_KP * error_q + g_integral_iq;

    voltage_mag = sqrtf(voltage_d * voltage_d + voltage_q * voltage_q);

    if (voltage_mag > FOC_UQ_LIMIT)
    {
        voltage_d = voltage_d * FOC_UQ_LIMIT / voltage_mag;
        voltage_q = voltage_q * FOC_UQ_LIMIT / voltage_mag;
    }

    foc_output_dq(angle, voltage_d, voltage_q);

    g_foc_dbg.target        = target_iq;
    g_foc_dbg.angle_mech    = angle_mech;
    g_foc_dbg.error_pos     = error_q;
    g_foc_dbg.speed_e       = g_encoder_dbg.speed * (float)FOC_POLE_PAIRS;
    g_foc_dbg.current_d     = current_d;
    g_foc_dbg.current_q     = current_q;
    g_foc_dbg.current_q_ref = target_iq_control;
}
