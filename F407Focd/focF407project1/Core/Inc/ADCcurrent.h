#ifndef __ADCCURRENT_H_
#define __ADCCURRENT_H_

#include "main.h"

#define CURRENT_V_PER_AMP  0.5f

#define CURRENT_AMP_PRE_CONST 0.001612f

#define CURRENT_SENSE_SIGN     (-1.0f)

#define CURRENT_CAL_SAMPLES    512

void current_calibrate(void);

void current_read(float *current_a, float *current_b, float *current_c);

uint16_t current_get_raw(uint8_t index);
uint16_t current_get_zero(uint8_t index);

#endif
