#include <assert.h>
#include <math.h>

/* #define TEST_SPEED_KP   0.10f */
/* #define TEST_SPEED_KP   0.14f */
#define TEST_SPEED_KP   0.08f
/* #define TEST_IQ_LIMIT   0.30f */
/* #define TEST_IQ_LIMIT   0.40f */
#define TEST_IQ_LIMIT   0.25f

static float speed_loop_iq(float target_speed, float measured_speed)
{
    float iq = TEST_SPEED_KP * (target_speed - measured_speed);

    if (iq > TEST_IQ_LIMIT)  { iq = TEST_IQ_LIMIT; }
    if (iq < -TEST_IQ_LIMIT) { iq = -TEST_IQ_LIMIT; }

    return iq;
}

static int near_equal(float a, float b)
{
    return fabsf(a - b) < 0.00001f;
}

int main(void)
{
    assert(near_equal(speed_loop_iq(3.0f, 3.0f), 0.0f));
    /*
     * 旧版Kp=0.10时的期望值：
     * assert(near_equal(speed_loop_iq(3.0f, 2.0f), 0.1f));
     * assert(near_equal(speed_loop_iq(3.0f, 4.0f), -0.1f));
     * assert(near_equal(speed_loop_iq(3.0f, 0.0f), 0.3f));
     * assert(near_equal(speed_loop_iq(-3.0f, 0.0f), -0.3f));
     */
    /*
     * 旧版Kp=0.14、限幅0.40A时的期望值：
     * assert(near_equal(speed_loop_iq(3.0f, 2.0f), 0.14f));
     * assert(near_equal(speed_loop_iq(3.0f, 4.0f), -0.14f));
     * assert(near_equal(speed_loop_iq(3.0f, 0.0f), 0.4f));
     */
    assert(near_equal(speed_loop_iq(3.0f, 2.0f), 0.08f));
    assert(near_equal(speed_loop_iq(3.0f, 4.0f), -0.08f));
    assert(near_equal(speed_loop_iq(3.0f, 0.0f), 0.24f));
    assert(near_equal(speed_loop_iq(-3.0f, 0.0f), -0.24f));
    assert(near_equal(speed_loop_iq(10.0f, 0.0f), 0.25f));
    assert(near_equal(speed_loop_iq(-10.0f, 0.0f), -0.25f));
    return 0;
}
