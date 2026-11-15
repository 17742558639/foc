#include <assert.h>
#include "angle_lock_diag.h"

static int nearly_equal(float a, float b)
{
    float difference = a - b;
    if (difference < 0.0f)
    {
        difference = -difference;
    }
    return difference < 0.0001f;
}

int main(void)
{
    angle_lock_diag_output_t output;
    float electrical_angle;

    electrical_angle = angle_lock_diag_measured_elec(0.0f, 10.0f, -1.0f,
                                                      4.7093f);
    assert(nearly_equal(electrical_angle, 4.7093f));

    electrical_angle = angle_lock_diag_measured_elec(ANGLE_LOCK_DIAG_HALF_PI
                                                      / 10.0f,
                                                      10.0f, -1.0f, 4.7093f);
    assert(nearly_equal(electrical_angle,
                        4.7093f - ANGLE_LOCK_DIAG_HALF_PI));

    electrical_angle = angle_lock_diag_measured_elec(2.0f
                                                      * ANGLE_LOCK_DIAG_HALF_PI
                                                      / 10.0f,
                                                      10.0f, -1.0f, 4.7093f);
    assert(nearly_equal(electrical_angle,
                        4.7093f - 2.0f * ANGLE_LOCK_DIAG_HALF_PI));

    electrical_angle = angle_lock_diag_measured_elec(4.7093f / 10.0f,
                                                      10.0f, -1.0f, 4.7093f);
    assert(nearly_equal(electrical_angle, 0.0f));

    output = angle_lock_diag_get_output(0U);
    assert(output.active == 0U);
    assert(output.step == 0U);
    assert(output.voltage_q == 0.0f);

    output = angle_lock_diag_get_output(999U);
    assert(output.active == 0U);

    output = angle_lock_diag_get_output(1000U);
    assert(output.active == 1U);
    assert(output.step == 1U);
    assert(nearly_equal(output.target_angle, 0.0f));
    assert(nearly_equal(output.voltage_q, ANGLE_LOCK_DIAG_VOLTAGE));

    output = angle_lock_diag_get_output(1800U);
    assert(output.active == 1U);
    assert(output.step == 2U);
    assert(nearly_equal(output.target_angle, ANGLE_LOCK_DIAG_HALF_PI));

    output = angle_lock_diag_get_output(2600U);
    assert(output.active == 1U);
    assert(output.step == 3U);
    assert(nearly_equal(output.target_angle, 2.0f * ANGLE_LOCK_DIAG_HALF_PI));

    output = angle_lock_diag_get_output(3400U);
    assert(output.active == 1U);
    assert(output.step == 4U);
    assert(nearly_equal(output.target_angle, 3.0f * ANGLE_LOCK_DIAG_HALF_PI));

    output = angle_lock_diag_get_output(4200U);
    assert(output.active == 1U);
    assert(output.step == 5U);
    assert(nearly_equal(output.target_angle, 4.0f * ANGLE_LOCK_DIAG_HALF_PI));

    output = angle_lock_diag_get_output(5000U);
    assert(output.active == 0U);
    assert(output.step == 6U);
    assert(output.voltage_q == 0.0f);

    output = angle_lock_diag_get_output(10000U);
    assert(output.active == 0U);
    assert(output.step == 6U);

    return 0;
}
