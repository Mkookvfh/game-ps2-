#include <tamtypes.h>
#include <gsKit.h>
#include <dmaKit.h>

static GSGLOBAL *gsGlobal;

int main(void)
{
    unsigned int frame = 0;
    unsigned int colorIndex = 0;

    /* ตั้งค่าระบบ DMA สำหรับ GS */
    dmaKit_init(
        D_CTRL_RELE_OFF,
        D_CTRL_MFD_OFF,
        D_CTRL_STS_UNSPEC,
        D_CTRL_STD_OFF,
        D_CTRL_RCYC_8,
        1 << DMA_CHANNEL_GIF
    );

    dmaKit_chan_init(DMA_CHANNEL_GIF);

    /* เริ่มต้น GSKit */
    gsGlobal = gsKit_init_global();

    gsGlobal->Mode = GS_MODE_NTSC;
    gsGlobal->Width = 640;
    gsGlobal->Height = 448;
    gsGlobal->PSM = GS_PSM_CT16;
    gsGlobal->PSMZ = GS_PSMZ_16;
    gsGlobal->ZBuffering = GS_SETTING_ON;

    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal, GS_PERSISTENT);

    /* สลับสีทุก 120 เฟรม */
    while (1)
    {
        colorIndex = (frame / 120) % 4;

        switch (colorIndex)
        {
            case 0:
                gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(255, 0, 0, 0x80, 0));
                break;

            case 1:
                gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0, 255, 0, 0x80, 0));
                break;

            case 2:
                gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0, 0, 255, 0x80, 0));
                break;

            default:
                gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(255, 255, 255, 0x80, 0));
                break;
        }

        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);
        gsKit_queue_reset(gsGlobal->Per_Queue);

        frame++;
    }

    return 0;
}