#include <assert.h>
#include <math.h>
#include <stdint.h>

#define TEST_IQ_START    0.40f
#define TEST_IQ_DRIVE    0.35f
#define TEST_START_SPEED 0.30f
#define TEST_MIN_DRIVE_TICKS 2000U
#define TEST_SPEED_LOW   2.0f
#define TEST_SPEED_HIGH  4.0f

typedef struct
{
    uint8_t driving;
    uint32_t drive_ticks;
} hysteresis_state_t;

static float hysteresis_step(hysteresis_state_t *state,
                             float target_speed,
                             float measured_speed)
{
    float direction = target_speed >= 0.0f ? 1.0f : -1.0f;
    float speed_along_target = measured_speed * direction;

    if (target_speed == 0.0f)
    {
        state->driving = 0U;
        state->drive_ticks = 0U;
        return 0.0f;
    }

    if (speed_along_target <= TEST_SPEED_LOW)
    {
        if (!state->driving)
        {
            state->drive_ticks = 0U;
        }
        state->driving = 1U;
    }
    else if (speed_along_target >= TEST_SPEED_HIGH &&
             state->drive_ticks >= TEST_MIN_DRIVE_TICKS)
    {
        state->driving = 0U;
    }

    if (!state->driving)
    {
        return 0.0f;
    }

    if (state->drive_ticks < TEST_MIN_DRIVE_TICKS)
    {
        state->drive_ticks++;
        return direction * TEST_IQ_START;
    }

    if (speed_along_target < TEST_START_SPEED)
    {
        return direction * TEST_IQ_START;
    }

    return direction * TEST_IQ_DRIVE;
}

int main(void)
{
    hysteresis_state_t positive = {0U, 0U};
    hysteresis_state_t negative = {0U, 0U};
    uint32_t i;

    assert(hysteresis_step(&positive, 3.0f, 0.0f) == 0.40f);
    assert(hysteresis_step(&positive, 3.0f, 10.0f) == 0.40f);
    assert(positive.driving == 1U);
    for (i = 2U; i < TEST_MIN_DRIVE_TICKS; ++i)
    {
        assert(hysteresis_step(&positive, 3.0f, 10.0f) == 0.40f);
    }
    assert(hysteresis_step(&positive, 3.0f, 4.0f) == 0.0f);

    assert(hysteresis_step(&positive, 3.0f, 0.0f) == 0.40f);
    for (i = 1U; i < TEST_MIN_DRIVE_TICKS; ++i)
    {
        (void)hysteresis_step(&positive, 3.0f, 0.30f);
    }
    assert(hysteresis_step(&positive, 3.0f, 0.30f) == 0.35f);
    assert(hysteresis_step(&positive, 3.0f, 3.0f) == 0.35f);
    assert(hysteresis_step(&positive, 3.0f, 4.0f) == 0.0f);
    assert(hysteresis_step(&positive, 3.0f, 3.0f) == 0.0f);
    assert(hysteresis_step(&positive, 3.0f, 2.0f) == 0.40f);
    assert(hysteresis_step(&positive, 3.0f, 5.0f) >= 0.0f);

    assert(hysteresis_step(&negative, -3.0f, 0.0f) == -0.40f);
    for (i = 1U; i < TEST_MIN_DRIVE_TICKS; ++i)
    {
        (void)hysteresis_step(&negative, -3.0f, -0.30f);
    }
    assert(hysteresis_step(&negative, -3.0f, -0.30f) == -0.35f);
    assert(hysteresis_step(&negative, -3.0f, -4.0f) == 0.0f);
    assert(hysteresis_step(&negative, -3.0f, -2.0f) == -0.40f);

    assert(hysteresis_step(&positive, 0.0f, 1.0f) == 0.0f);
    assert(positive.driving == 0U);

    return 0;
}
