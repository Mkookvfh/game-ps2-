#include <tamtypes.h>
#include <gsKit.h>
#include <dmaKit.h>

int main(void)
{
    GSGLOBAL *gs;
    u64 red = GS_SETREG_RGBAQ(255, 0, 0, 0, 0);

    gs = gsKit_init_global();

    dmaKit_init(
        D_CTRL_RELE_OFF,
        D_CTRL_MFD_OFF,
        D_CTRL_STS_UNSPEC,
        D_CTRL_STD_OFF,
        D_CTRL_RCYC_8,
        1 << DMA_CHANNEL_GIF
    );

    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsKit_init_screen(gs);
    gsKit_mode_switch(gs, GS_PERSISTENT);

    gsKit_clear(gs, red);
    gsKit_set_test(gs, GS_ZTEST_OFF);

    gsKit_mode_switch(gs, GS_ONESHOT);

    while (1)
    {
        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);
    }

    return 0;
}