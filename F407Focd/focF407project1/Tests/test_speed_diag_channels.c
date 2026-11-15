#include <assert.h>
#include "speed_diag_channels.h"

int main(void)
{
    float channel[7];

    speed_diag_map_channels(channel, 3.0f, 2.5f, 0.35f,
                            0.34f, 0.01f, 3U, 0.40f);

    assert(channel[0] == 3.0f);
    assert(channel[1] == 2.5f);
    assert(channel[2] == 0.35f);
    assert(channel[3] == 0.34f);
    assert(channel[4] == 0.01f);
    assert(channel[5] == 3.0f);
    assert(channel[6] == 0.40f);

    return 0;
}
