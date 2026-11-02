#include "foc.h"
#include <math.h>


extern TIM_HandleTypeDef htim1;

static float s_theta_e = 0.0f;     
static float s_w_e     = 0.0f;    

#define PI_F      3.14159265f
#define TWO_PI_F  6.28318531f
#define SQRT3_2   0.86602540f        


static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float wrap_2pi(float x)
{
    while (x >= TWO_PI_F) x -= TWO_PI_F;
    while (x <  0.0f)     x += TWO_PI_F;
    return x;
}



void foc_init(void)
{
    s_theta_e = 0.0f;
    s_w_e     = 0.0f;
}

void foc_set_theta(float theta_e)
{
    s_theta_e = wrap_2pi(theta_e);
}

void foc_align(void)
{
   
    dq_t udq;
    alphabeta_t uab;
    abc_t duty;

    udq.d = 0.0f;
    udq.q = FOC_U_OFFSET * 2.0f;

    uab  = foc_inv_park(udq, s_theta_e);
    duty = foc_svpwm(uab.alpha, uab.beta, VDC_BUS);
    drv_set_pwm(duty.a, duty.b, duty.c);
}

void foc_stop(void)
{
    
    drv_set_pwm(0.5f, 0.5f, 0.5f);
}


alphabeta_t foc_inv_park(dq_t in, float theta)
{
    float c = cosf(theta);
    float s = sinf(theta);
    alphabeta_t out;

    out.alpha = in.d * c - in.q * s;
    out.beta  = in.d * s + in.q * c;
    return out;
}


abc_t foc_inv_clarke(alphabeta_t in)
{
    abc_t out;

    out.a =  in.alpha;
    out.b = -0.5f * in.alpha + SQRT3_2 * in.beta;
    out.c = -0.5f * in.alpha - SQRT3_2 * in.beta;
    return out;
}



abc_t foc_svpwm(float alpha, float beta, float vdc)
{
    abc_t u;
    alphabeta_t ab;
    float mx, mn, com;

    ab.alpha = alpha;
    ab.beta  = beta;
    u = foc_inv_clarke(ab);

    mx = u.a;
    if (u.b > mx) mx = u.b;
    if (u.c > mx) mx = u.c;

    mn = u.a;
    if (u.b < mn) mn = u.b;
    if (u.c < mn) mn = u.c;

    com = -(mx + mn) * 0.5f;

    u.a = 0.5f + (u.a + com) / vdc;
    u.b = 0.5f + (u.b + com) / vdc;
    u.c = 0.5f + (u.c + com) / vdc;

    u.a = clampf(u.a, 0.0f, 1.0f);
    u.b = clampf(u.b, 0.0f, 1.0f);
    u.c = clampf(u.c, 0.0f, 1.0f);
    return u;
}


 
void drv_set_pwm(float da, float db, float dc)
{
    uint32_t ra, rb, rc;

    ra = (uint32_t)(clampf(da, 0.0f, 1.0f) * PWM_ARR);
    rb = (uint32_t)(clampf(db, 0.0f, 1.0f) * PWM_ARR);
    rc = (uint32_t)(clampf(dc, 0.0f, 1.0f) * PWM_ARR);

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ra);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, rb);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, rc);
}


float foc_speed_generator(float w_e)
{
    float uq;

    uq = FOC_KVF * fabsf(w_e) + FOC_U_OFFSET;

    if (uq > VQ_MAX)  uq = VQ_MAX;
    if (uq < -VQ_MAX) uq = -VQ_MAX;

    return uq;
}


void foc_step_openloop(float w_ref_mech)
{
    float uq;
    dq_t udq;
    alphabeta_t uab;
    abc_t duty;

   
    s_w_e = w_ref_mech * (float)FOC_POLE_PAIRS;

 
    s_theta_e = wrap_2pi(s_theta_e + s_w_e * FOC_TS);

   
    uq = foc_speed_generator(s_w_e);

   
    udq.d = 0.0f;
    udq.q = uq;
    uab = foc_inv_park(udq, s_theta_e);

    duty = foc_svpwm(uab.alpha, uab.beta, VDC_BUS);

    drv_set_pwm(duty.a, duty.b, duty.c);
}
