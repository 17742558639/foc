#include <assert.h>
#include <math.h>
#include <stdint.h>

#define TEST_IQ_LIMIT          0.35f
#define TEST_STARTUP_IQ        0.35f
#define TEST_STALL_SPEED       0.30f
#define TEST_STALL_TICKS       2000U
#define TEST_KP                0.08f

typedef struct
{
    uint32_t stall_ticks;
    uint8_t startup_complete;
} speed_state_t;

static float clampf(float value, float low, float high)
{
    if (value > high) { return high; }
    if (value < low)  { return low; }
    return value;
}

static float speed_step(speed_state_t *state,
                        float target_speed,
                        float measured_speed)
{
    float iq_cmd;

    if (!state->startup_complete)
    {
        return target_speed >= 0.0f ? TEST_STARTUP_IQ : -TEST_STARTUP_IQ;
    }

    if (target_speed > 0.0f)
    {
        iq_cmd = clampf(TEST_KP * (target_speed - measured_speed),
                        0.0f,
                        TEST_IQ_LIMIT);
    }
    else
    {
        iq_cmd = clampf(TEST_KP * (target_speed - measured_speed),
                        -TEST_IQ_LIMIT,
                        0.0f);
    }

    if (fabsf(measured_speed) < TEST_STALL_SPEED)
    {
        if (state->stall_ticks < TEST_STALL_TICKS)
        {
            state->stall_ticks++;
        }
    }
    else
    {
        state->stall_ticks = 0U;
    }

    if (state->stall_ticks >= TEST_STALL_TICKS)
    {
        state->startup_complete = 0U;
        state->stall_ticks = 0U;
        iq_cmd = target_speed >= 0.0f ? TEST_STARTUP_IQ : -TEST_STARTUP_IQ;
    }

    return iq_cmd;
}

int main(void)
{
    speed_state_t positive = {0U, 1U};
    speed_state_t negative = {0U, 1U};
    uint32_t i;

    /* 正转目标超速时只滑行，不输出反向制动电流。 */
    assert(speed_step(&positive, 3.0f, 5.5f) == 0.0f);
    assert(speed_step(&positive, 3.0f, 2.0f) > 0.0f);

    /* 负转目标同样不允许反向制动。 */
    assert(speed_step(&negative, -3.0f, -5.5f) == 0.0f);
    assert(speed_step(&negative, -3.0f, -2.0f) < 0.0f);

    for (i = 0U; i < TEST_STALL_TICKS - 1U; ++i)
    {
        (void)speed_step(&positive, 3.0f, 0.0f);
        assert(positive.startup_complete == 1U);
    }

    assert(speed_step(&positive, 3.0f, 0.0f) == TEST_STARTUP_IQ);
    assert(positive.startup_complete == 0U);

    return 0;
}
