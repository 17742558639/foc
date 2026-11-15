#include <assert.h>

#define TEST_TARGET_SPEED 3.0f

static float speed_target_init(int ramp_enabled)
{
    return ramp_enabled ? 0.0f : TEST_TARGET_SPEED;
}

int main(void)
{
    assert(speed_target_init(0) == 3.0f);
    assert(speed_target_init(1) == 0.0f);
    return 0;
}
