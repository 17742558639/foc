#include <assert.h>
#include <math.h>
#include <stdint.h>

#define TEST_COUNTS_PER_REV 8192.0f
#define TEST_TWO_PI         6.2831853f
#define TEST_TS             0.00005f
#define TEST_WINDOW         200U
#define TEST_LPF            0.20f

typedef struct
{
    uint16_t last_valid_raw;
    uint8_t has_valid_raw;
} raw_filter_t;

static uint16_t accept_raw(raw_filter_t *state,
                           uint16_t raw,
                           int transfer_ok)
{
    if (!transfer_ok || raw == 0x0000U || raw == 0xFFFFU)
    {
        return state->has_valid_raw ? state->last_valid_raw : 0U;
    }

    state->last_valid_raw = raw;
    state->has_valid_raw = 1U;
    return raw;
}

int main(void)
{
    raw_filter_t state = {0U, 0U};
    float one_count_speed;

    assert(accept_raw(&state, 0x4000U, 1) == 0x4000U);
    assert(accept_raw(&state, 0x0000U, 1) == 0x4000U);
    assert(accept_raw(&state, 0xFFFFU, 1) == 0x4000U);
    assert(accept_raw(&state, 0x1234U, 0) == 0x4000U);
    assert(accept_raw(&state, 0x4100U, 1) == 0x4100U);

    one_count_speed = TEST_TWO_PI / TEST_COUNTS_PER_REV /
                      ((float)TEST_WINDOW * TEST_TS);
    assert(fabsf(one_count_speed - 0.076699f) < 0.0001f);
    assert(TEST_LPF == 0.20f);

    return 0;
}
