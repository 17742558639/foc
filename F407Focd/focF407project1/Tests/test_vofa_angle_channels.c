#include <assert.h>
#include <math.h>

#define TEST_RAD_TO_DEG 57.2957795f

static float angle_channel_deg(float angle_rad)
{
    return angle_rad * TEST_RAD_TO_DEG;
}

int main(void)
{
    assert(fabsf(angle_channel_deg(0.0f) - 0.0f) < 0.0001f);
    assert(fabsf(angle_channel_deg(3.14159265f) - 180.0f) < 0.001f);
    assert(fabsf(angle_channel_deg(6.28318530f) - 360.0f) < 0.001f);
    return 0;
}
