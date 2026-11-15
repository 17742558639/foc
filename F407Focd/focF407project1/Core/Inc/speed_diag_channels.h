#ifndef __SPEED_DIAG_CHANNELS_H_
#define __SPEED_DIAG_CHANNELS_H_

#include <stdint.h>

static void speed_diag_map_channels(float channel[7],
                                    float target_speed,
                                    float measured_speed,
                                    float iq_command,
                                    float measured_iq,
                                    float measured_id,
                                    uint32_t jump_reject_count,
                                    float voltage_q)
{
    channel[0] = target_speed;
    channel[1] = measured_speed;
    channel[2] = iq_command;
    channel[3] = measured_iq;
    channel[4] = measured_id;
    channel[5] = (float)jump_reject_count;
    channel[6] = voltage_q;
}

#endif
