#ifndef __ENCODER_JUMP_FILTER_H_
#define __ENCODER_JUMP_FILTER_H_

#define ENC_JUMP_REJECT_RAD  0.020f

static int encoder_delta_is_plausible(float delta_angle)
{
    return (delta_angle <= ENC_JUMP_REJECT_RAD)
        && (delta_angle >= -ENC_JUMP_REJECT_RAD);
}

#endif
