#include "ADCcurrent.h"

extern ADC_HandleTypeDef hadc3;

static uint16_t s_adc_dma[2] = {0, 0};

static uint16_t s_zero[2] = {2048, 2048};

static uint16_t s_raw[2] = {0, 0};

void current_calibrate(void)
{
    uint32_t sum[2] = {0, 0};
    int i, ch;

    HAL_ADC_Start_DMA(&hadc3, (uint32_t *)s_adc_dma, 2);

    for (i = 0; i < CURRENT_CAL_SAMPLES; i++)
    {
        for (ch = 0; ch < 2; ch++)
        {
            sum[ch] += s_adc_dma[ch];
        }
        HAL_Delay(1);
    }

    for (ch = 0; ch < 2; ch++)
    {
        s_zero[ch] = (uint16_t)(sum[ch] / CURRENT_CAL_SAMPLES);
    }
}

void current_read(float *current_a, float *current_b, float *current_c)
{
    uint16_t raw[2];
    int16_t  diff[2];
    int ch;

    raw[0] = s_adc_dma[0];
    raw[1] = s_adc_dma[1];

    s_raw[0] = raw[0];
    s_raw[1] = raw[1];

    for (ch = 0; ch < 2; ch++)
    {
        diff[ch] = (int16_t)raw[ch] - (int16_t)s_zero[ch];
    }

    *current_a = (float)diff[0] * CURRENT_AMP_PRE_CONST * CURRENT_SENSE_SIGN;
    *current_b = (float)diff[1] * CURRENT_AMP_PRE_CONST * CURRENT_SENSE_SIGN;
    *current_c = -*current_a - *current_b;
}

uint16_t current_get_zero(uint8_t index)
{
    if (index > 1)
    {
        index = 1;
    }
    return s_zero[index];
}

uint16_t current_get_raw(uint8_t index)
{
    if (index > 1)
    {
        index = 1;
    }
    return s_raw[index];
}
