#include <assert.h>
#include <math.h>

#define TEST_CURRENT_KP       0.12f
#define TEST_CURRENT_KI       12.0f
#define TEST_TS               0.00005f
#define TEST_I_LIMIT          0.40f
#define TEST_VOLTAGE_LIMIT    0.60f

static float clampf(float value, float low, float high)
{
    if (value > high) { return high; }
    if (value < low)  { return low; }
    return value;
}

int main(void)
{
    float integral = 0.0f;
    float error = 0.35f;
    float voltage_q;
    unsigned int i;

    for (i = 0U; i < 2000U; ++i)
    {
        integral += TEST_CURRENT_KI * error * TEST_TS;
        integral = clampf(integral, -TEST_I_LIMIT, TEST_I_LIMIT);
    }

    voltage_q = TEST_CURRENT_KP * error + integral;
    voltage_q = clampf(voltage_q, -TEST_VOLTAGE_LIMIT, TEST_VOLTAGE_LIMIT);

    assert(fabsf(integral - 0.40f) < 0.00001f);
    assert(voltage_q > 0.30f);
    assert(voltage_q <= TEST_VOLTAGE_LIMIT);

    return 0;
}
