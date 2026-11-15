#ifndef __PHASE_MAP_DIAG_H_
#define __PHASE_MAP_DIAG_H_

#include <stdint.h>

#define PHASE_MAP_DIAG_CENTER_DUTY       0.500f
#define PHASE_MAP_DIAG_HIGH_DUTY         0.510f
#define PHASE_MAP_DIAG_LOW_DUTY          0.495f
#define PHASE_MAP_DIAG_INITIAL_WAIT_MS   1000U
#define PHASE_MAP_DIAG_PULSE_MS          50U
#define PHASE_MAP_DIAG_GAP_MS            1000U

typedef struct
{
    uint8_t step;
    float duty_a;
    float duty_b;
    float duty_c;
} phase_map_diag_output_t;

static phase_map_diag_output_t phase_map_diag_get_output(uint32_t elapsed_ms)
{
    phase_map_diag_output_t output;
    uint32_t pulse_a_start = PHASE_MAP_DIAG_INITIAL_WAIT_MS;
    uint32_t pulse_b_start = pulse_a_start + PHASE_MAP_DIAG_PULSE_MS
                           + PHASE_MAP_DIAG_GAP_MS;
    uint32_t pulse_c_start = pulse_b_start + PHASE_MAP_DIAG_PULSE_MS
                           + PHASE_MAP_DIAG_GAP_MS;

    output.step   = 0U;
    output.duty_a = PHASE_MAP_DIAG_CENTER_DUTY;
    output.duty_b = PHASE_MAP_DIAG_CENTER_DUTY;
    output.duty_c = PHASE_MAP_DIAG_CENTER_DUTY;

    if ((elapsed_ms >= pulse_a_start)
        && (elapsed_ms < pulse_a_start + PHASE_MAP_DIAG_PULSE_MS))
    {
        output.step   = 1U;
        output.duty_a = PHASE_MAP_DIAG_HIGH_DUTY;
        output.duty_b = PHASE_MAP_DIAG_LOW_DUTY;
        output.duty_c = PHASE_MAP_DIAG_LOW_DUTY;
    }
    else if ((elapsed_ms >= pulse_b_start)
             && (elapsed_ms < pulse_b_start + PHASE_MAP_DIAG_PULSE_MS))
    {
        output.step   = 2U;
        output.duty_a = PHASE_MAP_DIAG_LOW_DUTY;
        output.duty_b = PHASE_MAP_DIAG_HIGH_DUTY;
        output.duty_c = PHASE_MAP_DIAG_LOW_DUTY;
    }
    else if ((elapsed_ms >= pulse_c_start)
             && (elapsed_ms < pulse_c_start + PHASE_MAP_DIAG_PULSE_MS))
    {
        output.step   = 3U;
        output.duty_a = PHASE_MAP_DIAG_LOW_DUTY;
        output.duty_b = PHASE_MAP_DIAG_LOW_DUTY;
        output.duty_c = PHASE_MAP_DIAG_HIGH_DUTY;
    }
    else if (elapsed_ms >= pulse_c_start + PHASE_MAP_DIAG_PULSE_MS)
    {
        output.step = 4U;
    }

    return output;
}

#endif
