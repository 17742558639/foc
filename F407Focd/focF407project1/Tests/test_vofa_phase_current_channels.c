#include <assert.h>

static void map_channels(float channel[7],
                         float ia,
                         float ib,
                         float iq,
                         float id,
                         float mech_deg,
                         float elec_deg)
{
    channel[0] = ia;
    channel[1] = ib;
    channel[2] = -ia - ib;
    channel[3] = iq;
    channel[4] = id;
    channel[5] = mech_deg;
    channel[6] = elec_deg;
}

int main(void)
{
    float channel[7];

    map_channels(channel, 0.20f, -0.05f, -0.40f, 0.01f, 80.0f, 180.0f);

    assert(channel[0] == 0.20f);
    assert(channel[1] == -0.05f);
    assert(channel[2] == -0.15f);
    assert(channel[3] == -0.40f);
    assert(channel[4] == 0.01f);
    assert(channel[5] == 80.0f);
    assert(channel[6] == 180.0f);

    return 0;
}
