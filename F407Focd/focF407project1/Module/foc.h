#ifndef __FOC_H__
#define __FOC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"



#define FOC_TS          0.00005f    
#define PWM_ARR         4199.0f     
#define VDC_BUS         12.0f      
#define FOC_POLE_PAIRS  7          

#define VQ_MAX          (VDC_BUS * 0.577f)


  
#define FOC_KVF         0.06f       
#define FOC_U_OFFSET    0.8f        



typedef struct { float a, b, c; }      abc_t;       
typedef struct { float alpha, beta; }  alphabeta_t;  
typedef struct { float d, q; }         dq_t;         




void  foc_init(void);


void  foc_align(void);


void  foc_step_openloop(float w_ref_mech);


float foc_speed_generator(float w_e);


void  foc_stop(void);


void  drv_set_pwm(float da, float db, float dc);



alphabeta_t foc_inv_park(dq_t in, float theta);      /* dq  -> alpha,beta */
abc_t       foc_inv_clarke(alphabeta_t in);          /* alpha,beta -> a,b,c */
abc_t       foc_svpwm(float alpha, float beta, float vdc);  


void  foc_set_theta(float theta_e);

#ifdef __cplusplus
}
#endif
#endif
