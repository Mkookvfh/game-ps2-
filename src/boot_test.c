
#include <gsKit.h>
#include <dmaKit.h>
#include <kernel.h>

int main(void)
{
    GSGLOBAL *gsGlobal;

    /* Initialize Graphics Synthesizer */
    gsGlobal = gsKit_init_global();

    if (gsGlobal == NULL)
        return 1;

    /* Initialize GIF DMA */
    dmaKit_init(
        D_CTRL_RELE_OFF,
        D_CTRL_MFD_OFF,
        D_CTRL_STS_UNSPEC,
        D_CTRL_STD_OFF,
        D_CTRL_RCYC_8,
        1 << DMA_CHANNEL_GIF
    );

    dmaKit_chan_init(DMA_CHANNEL_GIF);

    /* Configure display */
    gsGlobal->PSM = GS_PSM_CT24;
    gsGlobal->PSMZ = GS_PSMZ_16;
    gsGlobal->ZBuffering = GS_SETTING_OFF;

    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal, GS_PERSISTENT);

    /* Color test */
    const u64 colors[] = {
        GS_SETREG_RGBAQ(255, 0, 0, 0x80, 0),
        GS_SETREG_RGBAQ(0, 255, 0, 0x80, 0),
        GS_SETREG_RGBAQ(0, 0, 255, 0x80, 0),
        GS_SETREG_RGBAQ(255, 255, 255, 0x80, 0)
    };

    int color = 0;
    int frame = 0;

    while (1)
    {
        gsKit_clear(gsGlobal, colors[color]);

        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);

        frame++;

        if (frame >= 120)
        {
            frame = 0;
            color = (color + 1) % 4;
        }
    }

    return 0;
}
