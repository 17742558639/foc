#include <assert.h>
#include "encoder_jump_filter.h"

#define DEG_TO_RAD  0.0174532925f

int main(void)
{
    assert(encoder_delta_is_plausible(0.0f));
    assert(encoder_delta_is_plausible(2.0f * DEG_TO_RAD));
    assert(encoder_delta_is_plausible(-2.0f * DEG_TO_RAD));
    assert(encoder_delta_is_plausible(11.0f * DEG_TO_RAD));
    assert(encoder_delta_is_plausible(-11.0f * DEG_TO_RAD));
    assert(encoder_delta_is_plausible(20.0f * DEG_TO_RAD));
    assert(!encoder_delta_is_plausible(43.0f * DEG_TO_RAD));
    assert(!encoder_delta_is_plausible(-43.0f * DEG_TO_RAD));
    assert(!encoder_delta_is_plausible(136.0f * DEG_TO_RAD));
    assert(!encoder_delta_is_plausible(-180.0f * DEG_TO_RAD));

    return 0;
}
