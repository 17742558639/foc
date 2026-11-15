#include <assert.h>
#include "phase_map_diag.h"

static void assert_neutral(phase_map_diag_output_t output)
{
    assert(output.duty_a == PHASE_MAP_DIAG_CENTER_DUTY);
    assert(output.duty_b == PHASE_MAP_DIAG_CENTER_DUTY);
    assert(output.duty_c == PHASE_MAP_DIAG_CENTER_DUTY);
}

int main(void)
{
    phase_map_diag_output_t output;

    output = phase_map_diag_get_output(0U);
    assert(output.step == 0U);
    assert_neutral(output);

    output = phase_map_diag_get_output(1000U);
    assert(output.step == 1U);
    assert(output.duty_a == PHASE_MAP_DIAG_HIGH_DUTY);
    assert(output.duty_b == PHASE_MAP_DIAG_LOW_DUTY);
    assert(output.duty_c == PHASE_MAP_DIAG_LOW_DUTY);

    output = phase_map_diag_get_output(1050U);
    assert(output.step == 0U);
    assert_neutral(output);

    output = phase_map_diag_get_output(2050U);
    assert(output.step == 2U);
    assert(output.duty_a == PHASE_MAP_DIAG_LOW_DUTY);
    assert(output.duty_b == PHASE_MAP_DIAG_HIGH_DUTY);
    assert(output.duty_c == PHASE_MAP_DIAG_LOW_DUTY);

    output = phase_map_diag_get_output(3100U);
    assert(output.step == 3U);
    assert(output.duty_a == PHASE_MAP_DIAG_LOW_DUTY);
    assert(output.duty_b == PHASE_MAP_DIAG_LOW_DUTY);
    assert(output.duty_c == PHASE_MAP_DIAG_HIGH_DUTY);

    output = phase_map_diag_get_output(3150U);
    assert(output.step == 4U);
    assert_neutral(output);

    output = phase_map_diag_get_output(10000U);
    assert(output.step == 4U);
    assert_neutral(output);

    return 0;
}
