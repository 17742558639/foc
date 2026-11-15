#include <assert.h>
#include <math.h>
#include <stdint.h>

#define TEST_STARTUP_TICKS   20000U
/* 旧版：#define TEST_STARTUP_IQ 0.30f */
#define TEST_STARTUP_IQ      0.35f
/* 旧版：#define TEST_RELEASE_SPEED 0.30f */
#define TEST_RELEASE_RATIO   0.80f
#define TEST_CONFIRM_TICKS   200U
#define TEST_SPEED_KP        0.08f
#define TEST_SPEED_KI        0.50f
#define TEST_TS              0.00005f
#define TEST_SPEED_I_LIMIT   0.30f
#define TEST_SPEED_IQ_LIMIT  0.30f

typedef struct
{
    uint32_t ticks;
    uint8_t active;
    uint8_t complete;
    uint8_t timeout;
    uint32_t confirm_ticks;
    float speed_integral;
} startup_state_t;

static float clampf(float value, float low, float high)
{
    if (value > high) { return high; }
    if (value < low)  { return low; }
    return value;
}

/*
 * === 旧版：只按固定时间退出启动（已弃用）===
 * 问题：电机尚未转动时，300ms后仍会降到低电流速度环。
 * === 新版：达到最低速度才退出，超时则停止输出 ===
 */
#if 0
/*
 * === 旧版：100ms确认后直接切入纯P速度环（已弃用）===
 * 问题：启动过冲较大，切环时iq会突然反向。
 * === 新版：10ms确认，并使用无冲击PI切换 ===
 */
#if 0
static float startup_step(startup_state_t *state,
                          float target_speed,
                          float measured_speed)
{
    if (target_speed == 0.0f)
    {
        state->ticks = 0U;
        state->active = 0U;
        state->complete = 0U;
        return 0.0f;
    }

    if (!state->complete && state->ticks < TEST_STARTUP_TICKS)
    {
        state->active = 1U;
        state->ticks++;
        return target_speed > 0.0f ? TEST_STARTUP_IQ : -TEST_STARTUP_IQ;
    }

    state->active = 0U;
    state->complete = 1U;
    return clampf(TEST_SPEED_KP * (target_speed - measured_speed),
                  -TEST_SPEED_IQ_LIMIT,
                  TEST_SPEED_IQ_LIMIT);
}
#endif

static float startup_step(startup_state_t *state,
                          float target_speed,
                          float measured_speed)
{
    float error_speed;
    float proportional;
    float integral_candidate;
    float iq_candidate;
    uint8_t reached_speed;
    uint8_t just_completed = 0U;

    if (target_speed == 0.0f)
    {
        state->ticks = 0U;
        state->active = 0U;
        state->complete = 0U;
        state->timeout = 0U;
        state->confirm_ticks = 0U;
        state->speed_integral = 0.0f;
        return 0.0f;
    }

    if (state->timeout)
    {
        return 0.0f;
    }

    error_speed = target_speed - measured_speed;
    reached_speed = ((target_speed > 0.0f &&
                      measured_speed >= target_speed * TEST_RELEASE_RATIO) ||
                     (target_speed < 0.0f &&
                      measured_speed <= target_speed * TEST_RELEASE_RATIO));

    if (!state->complete)
    {
        if (reached_speed)
        {
            if (state->confirm_ticks < TEST_CONFIRM_TICKS)
            {
                state->confirm_ticks++;
            }

            if (state->confirm_ticks >= TEST_CONFIRM_TICKS)
            {
                float handoff_iq = target_speed > 0.0f ?
                                   TEST_SPEED_IQ_LIMIT : -TEST_SPEED_IQ_LIMIT;
                state->active = 0U;
                state->complete = 1U;
                state->speed_integral = clampf(handoff_iq -
                                                TEST_SPEED_KP * error_speed,
                                                -TEST_SPEED_I_LIMIT,
                                                TEST_SPEED_I_LIMIT);
                just_completed = 1U;
            }
        }
        else
        {
            state->confirm_ticks = 0U;
        }
    }

    if (!state->complete)
    {
        if (state->ticks >= TEST_STARTUP_TICKS)
        {
            state->active = 0U;
            state->timeout = 1U;
            return 0.0f;
        }

        state->active = 1U;
        state->ticks++;
        return target_speed > 0.0f ? TEST_STARTUP_IQ : -TEST_STARTUP_IQ;
    }

    proportional = TEST_SPEED_KP * error_speed;

    if (!just_completed)
    {
        integral_candidate = clampf(state->speed_integral +
                                    TEST_SPEED_KI * error_speed * TEST_TS,
                                    -TEST_SPEED_I_LIMIT,
                                    TEST_SPEED_I_LIMIT);
        iq_candidate = proportional + integral_candidate;

        if (!((iq_candidate > TEST_SPEED_IQ_LIMIT && error_speed > 0.0f) ||
              (iq_candidate < -TEST_SPEED_IQ_LIMIT && error_speed < 0.0f)))
        {
            state->speed_integral = integral_candidate;
        }
    }

    return clampf(proportional + state->speed_integral,
                  -TEST_SPEED_IQ_LIMIT,
                  TEST_SPEED_IQ_LIMIT);
}
#endif

static float startup_step(startup_state_t *state,
                          float target_speed,
                          float measured_speed)
{
    uint8_t reached_speed;

    if (target_speed == 0.0f)
    {
        state->ticks = 0U;
        state->active = 0U;
        state->complete = 0U;
        state->timeout = 0U;
        state->confirm_ticks = 0U;
        return 0.0f;
    }

    if (state->timeout)
    {
        return 0.0f;
    }

    reached_speed = ((target_speed > 0.0f &&
                      measured_speed >= target_speed * TEST_RELEASE_RATIO) ||
                     (target_speed < 0.0f &&
                      measured_speed <= target_speed * TEST_RELEASE_RATIO));

    if (!state->complete)
    {
        if (reached_speed)
        {
            if (state->confirm_ticks < TEST_CONFIRM_TICKS)
            {
                state->confirm_ticks++;
            }

            if (state->confirm_ticks >= TEST_CONFIRM_TICKS)
            {
                state->active = 0U;
                state->complete = 1U;
            }
        }
        else
        {
            state->confirm_ticks = 0U;
        }
    }

    if (!state->complete)
    {
        if (state->ticks >= TEST_STARTUP_TICKS)
        {
            state->active = 0U;
            state->timeout = 1U;
            return 0.0f;
        }

        state->active = 1U;
        state->ticks++;
        return target_speed > 0.0f ? TEST_STARTUP_IQ : -TEST_STARTUP_IQ;
    }

    return clampf(TEST_SPEED_KP * (target_speed - measured_speed),
                  -TEST_SPEED_IQ_LIMIT,
                  TEST_SPEED_IQ_LIMIT);
}

static float startup_step_pi(startup_state_t *state,
                             float target_speed,
                             float measured_speed)
{
    float error_speed;
    float proportional;
    float integral_candidate;
    float iq_candidate;
    uint8_t reached_speed;
    uint8_t just_completed = 0U;

    if (target_speed == 0.0f)
    {
        state->ticks = 0U;
        state->active = 0U;
        state->complete = 0U;
        state->timeout = 0U;
        state->confirm_ticks = 0U;
        state->speed_integral = 0.0f;
        return 0.0f;
    }

    if (state->timeout)
    {
        return 0.0f;
    }

    error_speed = target_speed - measured_speed;
    reached_speed = ((target_speed > 0.0f &&
                      measured_speed >= target_speed * TEST_RELEASE_RATIO) ||
                     (target_speed < 0.0f &&
                      measured_speed <= target_speed * TEST_RELEASE_RATIO));

    if (!state->complete)
    {
        if (reached_speed)
        {
            if (state->confirm_ticks < TEST_CONFIRM_TICKS)
            {
                state->confirm_ticks++;
            }

            if (state->confirm_ticks >= TEST_CONFIRM_TICKS)
            {
                float handoff_iq = target_speed > 0.0f ?
                                   TEST_SPEED_IQ_LIMIT : -TEST_SPEED_IQ_LIMIT;
                state->active = 0U;
                state->complete = 1U;
                state->speed_integral = clampf(handoff_iq -
                                                TEST_SPEED_KP * error_speed,
                                                -TEST_SPEED_I_LIMIT,
                                                TEST_SPEED_I_LIMIT);
                just_completed = 1U;
            }
        }
        else
        {
            state->confirm_ticks = 0U;
        }
    }

    if (!state->complete)
    {
        if (state->ticks >= TEST_STARTUP_TICKS)
        {
            state->active = 0U;
            state->timeout = 1U;
            return 0.0f;
        }

        state->active = 1U;
        state->ticks++;
        return target_speed > 0.0f ? TEST_STARTUP_IQ : -TEST_STARTUP_IQ;
    }

    proportional = TEST_SPEED_KP * error_speed;

    if (!just_completed)
    {
        integral_candidate = clampf(state->speed_integral +
                                    TEST_SPEED_KI * error_speed * TEST_TS,
                                    -TEST_SPEED_I_LIMIT,
                                    TEST_SPEED_I_LIMIT);
        iq_candidate = proportional + integral_candidate;

        if (!((iq_candidate > TEST_SPEED_IQ_LIMIT && error_speed > 0.0f) ||
              (iq_candidate < -TEST_SPEED_IQ_LIMIT && error_speed < 0.0f)))
        {
            state->speed_integral = integral_candidate;
        }
    }

    return clampf(proportional + state->speed_integral,
                  -TEST_SPEED_IQ_LIMIT,
                  TEST_SPEED_IQ_LIMIT);
}

static int near_equal(float a, float b)
{
    return fabsf(a - b) < 0.00001f;
}

/*
 * === 旧版测试：固定300ms启动（已弃用）===
 * === 新版测试：速度确认与超时保护 ===
 */
#if 0
int main(void)
{
    startup_state_t positive = {0U, 0U, 0U};
    startup_state_t negative = {0U, 0U, 0U};
    uint32_t i;

    assert(near_equal(startup_step(&positive, 0.0f, 0.0f), 0.0f));
    assert(positive.active == 0U);
    assert(positive.complete == 0U);

    for (i = 0U; i < TEST_STARTUP_TICKS; ++i)
    {
        assert(near_equal(startup_step(&positive, 3.0f, 0.0f), 0.25f));
        assert(positive.active == 1U);
    }

    assert(near_equal(startup_step(&positive, 3.0f, 0.0f), 0.24f));
    assert(positive.active == 0U);
    assert(positive.complete == 1U);

    assert(near_equal(startup_step(&negative, -3.0f, 0.0f), -0.25f));
    assert(negative.active == 1U);

    positive.complete = 1U;
    assert(near_equal(startup_step(&positive, 10.0f, 0.0f), 0.25f));
    assert(near_equal(startup_step(&positive, -10.0f, 0.0f), -0.25f));

    return 0;
}
#endif

int main(void)
{
    startup_state_t positive = {0U, 0U, 0U, 0U, 0U, 0.0f};
    startup_state_t negative = {0U, 0U, 0U, 0U, 0U, 0.0f};
    startup_state_t timeout  = {0U, 0U, 0U, 0U, 0U, 0.0f};
    startup_state_t target_3 = {0U, 0U, 0U, 0U, 0U, 0.0f};
    uint32_t i;

    assert(near_equal(startup_step_pi(&positive, 0.0f, 0.0f), 0.0f));
    assert(near_equal(startup_step_pi(&positive, 1.0f, 0.0f), 0.35f));
    assert(positive.active == 1U);

    /* 单次速度尖峰不得退出启动阶段。 */
    assert(near_equal(startup_step_pi(&positive, 1.0f, 2.0f), 0.35f));
    assert(positive.complete == 0U);
    assert(near_equal(startup_step_pi(&positive, 1.0f, 0.0f), 0.35f));
    assert(positive.confirm_ticks == 0U);

    for (i = 0U; i < TEST_CONFIRM_TICKS - 1U; ++i)
    {
        assert(near_equal(startup_step_pi(&positive, 1.0f, 0.80f), 0.35f));
        assert(positive.complete == 0U);
    }
    assert(near_equal(startup_step_pi(&positive, 1.0f, 0.80f), 0.30f));
    assert(positive.complete == 1U);
    assert(positive.active == 0U);
    assert(startup_step_pi(&positive, 1.0f, 2.0f) > 0.0f);

    assert(near_equal(startup_step_pi(&negative, -1.0f, 0.0f), -0.35f));
    for (i = 0U; i < TEST_CONFIRM_TICKS - 1U; ++i)
    {
        assert(near_equal(startup_step_pi(&negative, -1.0f, -0.80f), -0.35f));
    }
    assert(near_equal(startup_step_pi(&negative, -1.0f, -0.80f), -0.30f));
    assert(negative.complete == 1U);

    for (i = 0U; i < TEST_STARTUP_TICKS; ++i)
    {
        assert(near_equal(startup_step_pi(&timeout, 1.0f, 0.0f), 0.35f));
    }
    assert(near_equal(startup_step_pi(&timeout, 1.0f, 0.0f), 0.0f));
    assert(timeout.timeout == 1U);
    assert(timeout.complete == 0U);

    assert(near_equal(startup_step_pi(&timeout, 0.0f, 0.0f), 0.0f));
    assert(timeout.timeout == 0U);

    assert(near_equal(startup_step_pi(&target_3, 3.0f, 0.0f), 0.35f));
    /* 3rad/s目标下，手推到0.3rad/s和2.39rad/s都不得切环。 */
    assert(near_equal(startup_step_pi(&target_3, 3.0f, 0.30f), 0.35f));
    assert(near_equal(startup_step_pi(&target_3, 3.0f, 2.39f), 0.35f));
    assert(target_3.confirm_ticks == 0U);

    for (i = 0U; i < TEST_CONFIRM_TICKS - 1U; ++i)
    {
        assert(near_equal(startup_step_pi(&target_3, 3.0f, 2.40f), 0.35f));
    }
    assert(near_equal(startup_step_pi(&target_3, 3.0f, 2.40f), 0.30f));
    assert(target_3.complete == 1U);
    assert(startup_step_pi(&target_3, 3.0f, 4.0f) >= 0.0f);

    return 0;
}
