#ifndef __ANGLE_LOCK_DIAG_H_
#define __ANGLE_LOCK_DIAG_H_

#include <stdint.h>

#define ANGLE_LOCK_DIAG_INITIAL_WAIT_MS  1000U
#define ANGLE_LOCK_DIAG_HOLD_MS           800U
#define ANGLE_LOCK_DIAG_VOLTAGE          0.20f
#define ANGLE_LOCK_DIAG_HALF_PI          1.57079633f

typedef struct
{
    uint8_t active;
    uint8_t step;
    float target_angle;
    float voltage_q;
} angle_lock_diag_output_t;

static float angle_lock_diag_measured_elec(float mechanical_angle,
                                           float pole_pairs,
                                           float direction,
                                           float offset)
{
    float electrical_angle = mechanical_angle * pole_pairs * direction + offset;

    while (electrical_angle >= 6.28318530f)
    {
        electrical_angle -= 6.28318530f;
    }
    while (electrical_angle < 0.0f)
    {
        electrical_angle += 6.28318530f;
    }

    return electrical_angle;
}

static angle_lock_diag_output_t angle_lock_diag_get_output(uint32_t elapsed_ms)
{
    angle_lock_diag_output_t output;
    uint32_t active_elapsed;

    output.active       = 0U;
    output.step         = 0U;
    output.target_angle = 0.0f;
    output.voltage_q    = 0.0f;

    if (elapsed_ms < ANGLE_LOCK_DIAG_INITIAL_WAIT_MS)
    {
        return output;
    }

    active_elapsed = elapsed_ms - ANGLE_LOCK_DIAG_INITIAL_WAIT_MS;
    output.step = (uint8_t)(active_elapsed / ANGLE_LOCK_DIAG_HOLD_MS + 1U);

    if (output.step <= 5U)
    {
        output.active       = 1U;
        output.target_angle = (float)(output.step - 1U) * ANGLE_LOCK_DIAG_HALF_PI;
        output.voltage_q    = ANGLE_LOCK_DIAG_VOLTAGE;
    }
    else
    {
        output.step = 6U;
    }

    return output;
}

#endif
