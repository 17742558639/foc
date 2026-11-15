#include <assert.h>
#include <math.h>
#include <stdint.h>

/* 旧版：#define TEST_TARGET_IQ 0.35f */
#define TEST_TARGET_IQ      0.40f
#define TEST_TIMEOUT_TICKS  20000U

typedef struct
{
    uint32_t ticks;
    uint8_t finished;
} timed_current_state_t;

static float timed_current_step(timed_current_state_t *state)
{
    if (state->finished)
    {
        return 0.0f;
    }

    if (state->ticks >= TEST_TIMEOUT_TICKS)
    {
        state->finished = 1U;
        return 0.0f;
    }

    state->ticks++;
    return TEST_TARGET_IQ;
}

static int near_equal(float a, float b)
{
    return fabsf(a - b) < 0.00001f;
}

int main(void)
{
    timed_current_state_t state = {0U, 0U};
    uint32_t i;

    for (i = 0U; i < TEST_TIMEOUT_TICKS; ++i)
    {
        assert(near_equal(timed_current_step(&state), 0.40f));
    }

    assert(near_equal(timed_current_step(&state), 0.0f));
    assert(state.finished == 1U);
    assert(near_equal(timed_current_step(&state), 0.0f));

    return 0;
}
