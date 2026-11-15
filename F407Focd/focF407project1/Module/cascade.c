#include "cascade.h"
#include "foc.h"
#include "encoder.h"
#include <math.h>

static float s_v_cmd          = 0.0f;
static float s_v_cmd_lpf      = 0.0f;
static float s_iq_cmd         = 0.0f;
static float s_error_pos      = 0.0f;
static float s_speed_integral = 0.0f;
static float s_boost_iq       = 0.05f;

static uint32_t s_speed_startup_ticks         = 0U;
static uint32_t s_speed_startup_confirm_ticks = 0U;
static uint32_t s_speed_restart_ticks         = 0U;
static uint32_t s_speed_timeout_cooldown      = 0U;

static uint8_t s_speed_startup_complete = 0U;
static uint8_t s_speed_startup_timeout  = 0U;

static float clampf(float value, float low, float high)
{
    if (value > high) { return high; }
    if (value < low)  { return low;  }
    return value;
}

static float wrap_pi_local(float angle)
{
    while (angle >  3.14159265f) { angle -= 6.28318530f; }
    while (angle < -3.14159265f) { angle += 6.28318530f; }
    return angle;
}

static void speed_startup_reset(void)
{
    s_speed_startup_ticks         = 0U;
    s_speed_startup_confirm_ticks = 0U;
    s_speed_restart_ticks         = 0U;
    s_speed_timeout_cooldown      = 0U;
    s_speed_startup_complete      = 0U;
    s_speed_startup_timeout       = 0U;
    s_speed_integral              = 0.0f;
    s_boost_iq                    = SPEED_BOOST_START;
}

void cascade_init(void)
{
    s_v_cmd     = 0.0f;
    s_v_cmd_lpf = 0.0f;
    s_iq_cmd    = 0.0f;
    s_error_pos = 0.0f;

    speed_startup_reset();
}

void cascade_speed(float target_speed)
{
    float speed = g_encoder_dbg.speed;
    float error_speed = target_speed - speed;
    float proportional;
    float integral_candidate;
    float iq_candidate;
    float iq_cmd;
    float iq_ff = SPEED_TEST_FF * target_speed;

    if (target_speed == 0.0f)
    {
        speed_startup_reset();
        iq_cmd = 0.0f;
    }
    else if (s_speed_startup_timeout)
    {
        s_speed_timeout_cooldown++;

        if (s_speed_timeout_cooldown >= SPEED_TIMEOUT_COOLDOWN_TICKS)
        {
            speed_startup_reset();
        }

        iq_cmd = 0.0f;
    }
    else if (!s_speed_startup_complete)
    {
        if (fabsf(speed) >= SPEED_BREAK_SPEED)
        {
            if (s_speed_startup_confirm_ticks < SPEED_BREAK_CONFIRM_TICKS)
            {
                s_speed_startup_confirm_ticks++;
            }
        }
        else
        {
            s_speed_startup_confirm_ticks = 0U;
        }

        if (s_speed_startup_confirm_ticks >= SPEED_BREAK_CONFIRM_TICKS)
        {
            s_speed_startup_complete      = 1U;
            s_speed_startup_confirm_ticks = 0U;
            s_speed_integral              = 0.0f;
            s_boost_iq                    = 0.0f;

            iq_cmd = clampf(iq_ff + SPEED_TEST_KP * error_speed + s_speed_integral,
                            -SPEED_TEST_IQ_LIMIT,
                            SPEED_TEST_IQ_LIMIT);
        }
        else
        {
            s_speed_startup_ticks++;

            if (s_speed_startup_ticks >= SPEED_STARTUP_TIMEOUT_TICKS)
            {
                s_speed_startup_timeout = 1U;
                iq_cmd = 0.0f;
            }
            else
            {
                if ((s_speed_startup_ticks % SPEED_BOOST_INTERVAL) == 0U)
                {
                    s_boost_iq += SPEED_BOOST_STEP;

                    if (s_boost_iq > SPEED_BOOST_MAX)
                    {
                        s_boost_iq = SPEED_BOOST_MAX;
                    }
                }

                iq_cmd = (target_speed > 0.0f) ? s_boost_iq : -s_boost_iq;
            }
        }
    }
    else
    {
        proportional = SPEED_TEST_KP * error_speed;

        integral_candidate = clampf(s_speed_integral +
                                    SPEED_TEST_KI * error_speed * FOC_TS,
                                    -SPEED_TEST_I_LIMIT,
                                    SPEED_TEST_I_LIMIT);
        iq_candidate = iq_ff + proportional + integral_candidate;

        if (!((iq_candidate > SPEED_TEST_IQ_LIMIT && error_speed > 0.0f) ||
              (iq_candidate < -SPEED_TEST_IQ_LIMIT && error_speed < 0.0f)))
        {
            s_speed_integral = integral_candidate;
        }

        iq_cmd = clampf(iq_ff + proportional + s_speed_integral,
                        -SPEED_TEST_IQ_LIMIT,
                        SPEED_TEST_IQ_LIMIT);
    }

    if (s_speed_startup_complete && target_speed != 0.0f)
    {
        if (fabsf(speed) < SPEED_RESTART_SPEED)
        {
            s_speed_restart_ticks++;

            if (s_speed_restart_ticks >= SPEED_RESTART_TICKS)
            {
                speed_startup_reset();
            }
        }
        else
        {
            s_speed_restart_ticks = 0U;
        }
    }

    s_v_cmd  = target_speed;
    s_iq_cmd = iq_cmd;

    foc_close_current(iq_cmd, FOC_ID_TARGET);
}

void cascade_position(float target_angle)
{
    float angle_mech;
    float error_pos;
    float v_cmd;

    angle_mech = g_encoder_dbg.angle_mech;

    error_pos = wrap_pi_local(target_angle - angle_mech);

    s_error_pos = error_pos;

    if (fabsf(error_pos) < CASCADE_POS_DEADZONE)
    {
        s_v_cmd     = 0.0f;
        s_v_cmd_lpf = 0.0f;
        cascade_speed(0.0f);
        return;
    }

    v_cmd = CASCADE_POS_KP * error_pos;

    if (fabsf(v_cmd) < CASCADE_POS_V_MIN)
    {
        v_cmd = (v_cmd >= 0.0f) ? CASCADE_POS_V_MIN : -CASCADE_POS_V_MIN;
    }

    v_cmd = clampf(v_cmd, -CASCADE_V_CMD_LIMIT, CASCADE_V_CMD_LIMIT);

    s_v_cmd_lpf += CASCADE_V_CMD_LPF * (v_cmd - s_v_cmd_lpf);

    s_v_cmd = s_v_cmd_lpf;

    cascade_speed(s_v_cmd);
}

float cascade_get_v_cmd(void)
{
    return s_v_cmd;
}

float cascade_get_iq_cmd(void)
{
    return s_iq_cmd;
}

float cascade_get_error_pos(void)
{
    return s_error_pos;
}
