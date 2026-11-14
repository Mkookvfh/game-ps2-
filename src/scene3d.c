
#include <gsKit.h>
#include <dmaKit.h>
#include <kernel.h>
#include <math.h>

#define SCREEN_W 640.0f
#define SCREEN_H 448.0f
#define CENTER_X 320.0f
#define CENTER_Y 224.0f
#define CAMERA_DISTANCE 4.5f
#define FOCAL_LENGTH 220.0f

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    float x, y;
} Vec2;

/* จุดยอดทั้ง 8 ของลูกบาศก์ */
static const Vec3 cube[8] = {
    {-1,-1,-1}, { 1,-1,-1},
    { 1, 1,-1}, {-1, 1,-1},
    {-1,-1, 1}, { 1,-1, 1},
    { 1, 1, 1}, {-1, 1, 1}
};

/* เส้นเชื่อมจุดยอด */
static const int edges[12][2] = {
    {0,1}, {1,2}, {2,3}, {3,0},
    {4,5}, {5,6}, {6,7}, {7,4},
    {0,4}, {1,5}, {2,6}, {3,7}
};

/* หมุนและแปลงพิกัด 3D เป็นพิกัดหน้าจอ */
static int project_vertex(
    Vec3 v,
    float angle,
    float *screen_x,
    float *screen_y
) {
    float cy = cosf(angle);
    float sy = sinf(angle);

    float cx = cosf(angle * 0.7f);
    float sx = sinf(angle * 0.7f);

    /* หมุนรอบแกน Y */
    float x = v.x * cy + v.z * sy;
    float z = -v.x * sy + v.z * cy;

    /* หมุนรอบแกน X */
    float y = v.y * cx - z * sx;
    z = v.y * sx + z * cx;

    /* ระยะจากกล้อง */
    z += CAMERA_DISTANCE;

    if (z <= 0.2f)
        return 0;

    /* Perspective projection */
    *screen_x = CENTER_X + (x * FOCAL_LENGTH / z);
    *screen_y = CENTER_Y - (y * FOCAL_LENGTH / z);

    return 1;
}

int main(void)
{
    GSGLOBAL *gsGlobal;

    gsGlobal = gsKit_init_global();

    if (gsGlobal == NULL)
        return 1;

    /* ตั้งค่าระบบ GS */
    dmaKit_init(
        D_CTRL_RELE_OFF,
        D_CTRL_MFD_OFF,
        D_CTRL_STS_UNSPEC,
        D_CTRL_STD_OFF,
        D_CTRL_RCYC_8,
        1 << DMA_CHANNEL_GIF
    );

    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsGlobal->PSM = GS_PSM_CT24;
    gsGlobal->PSMZ = GS_PSMZ_16;
    gsGlobal->ZBuffering = GS_SETTING_OFF;

    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal, GS_PERSISTENT);

    float angle = 0.0f;

    const u64 background =
        GS_SETREG_RGBAQ(5, 10, 20, 0x80, 0);

    const u64 cube_color =
        GS_SETREG_RGBAQ(80, 220, 255, 0x80, 0);

    while (1)
    {
        Vec2 points[8];
        int visible[8];

        gsKit_clear(gsGlobal, background);

        /* คำนวณตำแหน่งจุดยอด */
        for (int i = 0; i < 8; i++)
        {
            visible[i] = project_vertex(
                cube[i],
                angle,
                &points[i].x,
                &points[i].y
            );
        }

        /* วาดเส้นลูกบาศก์ */
        for (int i = 0; i < 12; i++)
        {
            int a = edges[i][0];
            int b = edges[i][1];

            if (visible[a] && visible[b])
            {
                gsKit_prim_line_3d(
                    gsGlobal,
                    points[a].x,
                    points[a].y,
                    0,
                    points[b].x,
                    points[b].y,
                    0,
                    cube_color
                );
            }
        }

        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);

        angle += 0.025f;

        if (angle > 6.283185f)
            angle -= 6.283185f;
    }

    return 0;
}
