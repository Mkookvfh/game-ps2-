
#include <gsKit.h>
#include <dmaKit.h>
#include <kernel.h>
#include <libpad.h>
#include <math.h>
#include <sifrpc.h>
#include <loadfile.h>

#define CENTER_X 320.0f
#define CENTER_Y 224.0f
#define FOCAL_LENGTH 250.0f
#define CAMERA_HEIGHT 1.8f
#define CAMERA_PITCH 0.12f
#define PI 3.14159265f

typedef struct {
    float x, y, z;
} Vec3;

/* Player state */
static float player_x = 0.0f;
static float player_z = 3.0f;
static float player_yaw = 0.0f;

static float jump_velocity = 0.0f;
static int is_jumping = 0;

static float walk_time = 0.0f;

/* PS2 controller buffer: 64-byte aligned */
static unsigned char padBuf[256]
    __attribute__((aligned(64)));

static struct padButtonStatus pad;
static int pad_ready = 0;
static int analog_requested = 0;

/* Cube */
static const Vec3 cube[8] = {
    {-0.8f,-0.8f,-0.8f},
    { 0.8f,-0.8f,-0.8f},
    { 0.8f, 0.8f,-0.8f},
    {-0.8f, 0.8f,-0.8f},
    {-0.8f,-0.8f, 0.8f},
    { 0.8f,-0.8f, 0.8f},
    { 0.8f, 0.8f, 0.8f},
    {-0.8f, 0.8f, 0.8f}
};

static const int edges[12][2] = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7}
};

/* Camera projection */

static int project_world(
    Vec3 p,
    float *sx,
    float *sy
) {
    float dx = p.x - player_x;
    float dy = p.y - CAMERA_HEIGHT;
    float dz = p.z - player_z;

    float cs = cosf(player_yaw);
    float sn = sinf(player_yaw);

    /* Camera follows behind player's facing direction */
    float camera_x = dx * cs - dz * sn;
    float camera_z = 8.0f + dx * sn + dz * cs;

    float cp = cosf(CAMERA_PITCH);
    float sp = sinf(CAMERA_PITCH);

    float cy = dy * cp - camera_z * sp;
    float cz = dy * sp + camera_z * cp;

    if (cz < 0.5f)
        return 0;

    *sx = CENTER_X + camera_x * FOCAL_LENGTH / cz;
    *sy = CENTER_Y - cy * FOCAL_LENGTH / cz;

    return 1;
}


/* Draw triangle */
static void draw_triangle(
    GSGLOBAL *gs,
    Vec3 a,
    Vec3 b,
    Vec3 c,
    u64 color
) {
    float x1,y1,x2,y2,x3,y3;

    if (!project_world(a,&x1,&y1)) return;
    if (!project_world(b,&x2,&y2)) return;
    if (!project_world(c,&x3,&y3)) return;

    gsKit_prim_triangle_3d(
        gs,
        x1,y1,0,
        x2,y2,0,
        x3,y3,0,
        color
    );
}

/* Draw flat rectangle */
static void draw_rect(
    GSGLOBAL *gs,
    float left,
    float bottom,
    float right,
    float top,
    float z,
    u64 color
) {
    Vec3 a={left,bottom,z};
    Vec3 b={right,bottom,z};
    Vec3 c={right,top,z};
    Vec3 d={left,top,z};

    draw_triangle(gs,a,b,c,color);
    draw_triangle(gs,a,c,d,color);
}

/* Ground */
static void draw_ground(GSGLOBAL *gs)
{
    const u64 grass1 =
        GS_SETREG_RGBAQ(55,75,43,0x80,0);

    const u64 grass2 =
        GS_SETREG_RGBAQ(68,83,48,0x80,0);

    const u64 grid =
        GS_SETREG_RGBAQ(105,115,75,0x80,0);

    for(int z=9;z>=0;z--) {
        for(int x=0;x<10;x++) {

            float x0=-10.0f+x*2.0f;
            float x1=x0+2.0f;

            float z0=-2.0f+z*2.0f;
            float z1=z0+2.0f;

            Vec3 a={x0,-1.5f,z0};
            Vec3 b={x1,-1.5f,z0};
            Vec3 c={x1,-1.5f,z1};
            Vec3 d={x0,-1.5f,z1};

            u64 color =
                ((x+z)&1)?grass1:grass2;

            draw_triangle(gs,a,b,c,color);
            draw_triangle(gs,a,c,d,color);
        }
    }

    /* Grid lines */
    for(int x=0;x<=10;x++) {
        float wx=-10.0f+x*2.0f;

        Vec3 a={wx,-1.48f,-2.0f};
        Vec3 b={wx,-1.48f,18.0f};

        float ax,ay,bx,by;

        if(project_world(a,&ax,&ay) &&
           project_world(b,&bx,&by)) {
            gsKit_prim_line_3d(
                gs,ax,ay,0,bx,by,0,grid
            );
        }
    }

    for(int z=0;z<=10;z++) {
        float wz=-2.0f+z*2.0f;

        Vec3 a={-10.0f,-1.48f,wz};
        Vec3 b={10.0f,-1.48f,wz};

        float ax,ay,bx,by;

        if(project_world(a,&ax,&ay) &&
           project_world(b,&bx,&by)) {
            gsKit_prim_line_3d(
                gs,ax,ay,0,bx,by,0,grid
            );
        }
    }
}

/* Player model */
static void draw_player(GSGLOBAL *gs)
{
    const u64 coat =
        GS_SETREG_RGBAQ(30,42,65,0x80,0);

    const u64 coat_light =
        GS_SETREG_RGBAQ(55,72,100,0x80,0);

    const u64 trousers =
        GS_SETREG_RGBAQ(35,40,52,0x80,0);

    const u64 boots =
        GS_SETREG_RGBAQ(75,42,25,0x80,0);

    const u64 leather =
        GS_SETREG_RGBAQ(125,78,35,0x80,0);

    const u64 skin =
        GS_SETREG_RGBAQ(190,135,95,0x80,0);

    const u64 hood =
        GS_SETREG_RGBAQ(22,27,38,0x80,0);

    float z=player_z+0.15f;
    float y=player_y;

    /* Walking animation */
    float leg_swing =
        is_jumping ? 0.0f : sinf(walk_time)*0.12f;

    /* Legs */
    draw_rect(gs,
        player_x-0.29f,
        -1.35f+y-leg_swing,
        player_x-0.06f,
        -0.58f+y-leg_swing,
        z, trousers);

    draw_rect(gs,
        player_x+0.06f,
        -1.35f+y+leg_swing,
        player_x+0.29f,
        -0.58f+y+leg_swing,
        z, trousers);

    /* Boots */
    draw_rect(gs,
        player_x-0.37f,
        -1.48f+y-leg_swing,
        player_x-0.04f,
        -1.22f+y-leg_swing,
        z+0.01f, boots);

    draw_rect(gs,
        player_x+0.04f,
        -1.48f+y+leg_swing,
        player_x+0.37f,
        -1.22f+y+leg_swing,
        z+0.01f, boots);

    /* Arms */
    float arm_swing =
        is_jumping ? -0.1f : sinf(walk_time)*0.08f;

    draw_rect(gs,
        player_x-0.55f,
        -0.39f+y-arm_swing,
        player_x-0.34f,
        0.39f+y-arm_swing,
        z+0.02f, coat);

    draw_rect(gs,
        player_x+0.34f,
        -0.39f+y+arm_swing,
        player_x+0.55f,
        0.39f+y+arm_swing,
        z+0.02f, coat);

    /* Torso */
    draw_rect(gs,
        player_x-0.38f,
        -0.64f+y,
        player_x+0.38f,
        0.43f+y,
        z+0.03f, coat);

    /* Coat center */
    draw_rect(gs,
        player_x-0.10f,
        -0.61f+y,
        player_x+0.10f,
        0.35f+y,
        z+0.04f, coat_light);

    /* Belt */
    draw_rect(gs,
        player_x-0.35f,
        -0.55f+y,
        player_x+0.35f,
        -0.43f+y,
        z+0.05f, leather);

    /* Neck */
    draw_rect(gs,
        player_x-0.12f,
        0.40f+y,
        player_x+0.12f,
        0.68f+y,
        z+0.05f, skin);

    /* Face */
    draw_rect(gs,
        player_x-0.19f,
        0.65f+y,
        player_x+0.19f,
        1.12f+y,
        z+0.06f, skin);

    /* Hood */
    draw_rect(gs,
        player_x-0.23f,
        1.08f+y,
        player_x+0.23f,
        1.30f+y,
        z+0.07f, hood);
}

/* Controller input */

static void update_player(void)
{
    float move_x = 0.0f;
    float move_z = 0.0f;

    int running = 0;
    int jump_pressed = 0;

    if (pad_ready &&
        padGetState(0, 0) == PAD_STATE_STABLE)
    {
        /* Enable DualShock analog mode once */
        if (!analog_requested) {
            padSetMainMode(
                0, 0,
                PAD_MMODE_DUALSHOCK,
                PAD_MMODE_LOCK
            );
            analog_requested = 1;
        }

        if (padRead(0, 0, &pad)) {

            /* D-pad */
            if (!(pad.btns & PAD_LEFT))
                move_x -= 1.0f;

            if (!(pad.btns & PAD_RIGHT))
                move_x += 1.0f;

            if (!(pad.btns & PAD_UP))
                move_z += 1.0f;

            if (!(pad.btns & PAD_DOWN))
                move_z -= 1.0f;

            /* Analog stick with dead zone */
            int lx = (int)pad.ljoy_h;
            int ly = (int)pad.ljoy_v;

            if (lx > 0 && lx < 93)
                move_x = -1.0f;
            else if (lx > 163)
                move_x = 1.0f;

            if (ly > 0 && ly < 93)
                move_z = 1.0f;
            else if (ly > 163)
                move_z = -1.0f;

            /* R1 = Run */
            if (!(pad.btns & PAD_R1))
                running = 1;

            /* Cross = Jump */
            if (!(pad.btns & PAD_CROSS))
                jump_pressed = 1;
        }
    }

    float speed = running ? 0.085f : 0.045f;

    if (move_x != 0.0f || move_z != 0.0f) {
        float length = sqrtf(
            move_x * move_x +
            move_z * move_z
        );

        if (length > 0.0f) {
            move_x /= length;
            move_z /= length;
        }

        player_x += move_x * speed;
player_z += move_z * speed;

/* Turn player toward movement direction */
player_yaw = atan2f(move_x, move_z);

        walk_time += running ? 0.25f : 0.15f;
    }

    /* Jump */
    if (jump_pressed && !is_jumping) {
        is_jumping = 1;
        jump_velocity = 0.18f;
    }

    if (is_jumping) {
        player_y += jump_velocity;
        jump_velocity -= 0.012f;

        if (player_y <= 0.0f) {
            player_y = 0.0f;
            jump_velocity = 0.0f;
            is_jumping = 0;
        }
    }

    /* Keep player inside test area */
    if (player_x < -8.0f) player_x = -8.0f;
    if (player_x > 8.0f) player_x = 8.0f;

    if (player_z < -1.0f) player_z = -1.0f;
    if (player_z > 16.0f) player_z = 16.0f;
}


/* Rotating test cube */
static void draw_cube(
    GSGLOBAL *gs,
    float angle
) {
    float sx[8],sy[8];
    int visible[8];

    const u64 color =
        GS_SETREG_RGBAQ(80,220,255,0x80,0);

    for(int i=0;i<8;i++) {
        float ca=cosf(angle);
        float sa=sinf(angle);

        float x=cube[i].x;
        float y=cube[i].y;
        float z=cube[i].z;

        float rx=x*ca+z*sa;
        float rz=-x*sa+z*ca;

        Vec3 p={
            player_x+3.0f+rx,
            y+0.2f,
            player_z+3.0f+rz
        };

        visible[i]=project_world(
            p,&sx[i],&sy[i]
        );
    }

    for(int i=0;i<12;i++) {
        int a=edges[i][0];
        int b=edges[i][1];

        if(visible[a] && visible[b]) {
            gsKit_prim_line_3d(
                gs,
                sx[a],sy[a],0,
                sx[b],sy[b],0,
                color
            );
        }
    }
}

int main(void)
{
    GSGLOBAL *gs=gsKit_init_global();

    if(gs==NULL)
        return 1;

    /* Initialize controller RPC */
/* Initialize controller drivers */
SifInitRpc(0);

int sio = SifLoadModule("rom0:XSIO2MAN", 0, NULL);

if(sio >= 0) {
    int pman = SifLoadModule("rom0:XPADMAN", 0, NULL);

    if(pman >= 0) {
        if(padInit(0) == 1) {
            if(padPortOpen(0, 0, padBuf) != 0) {
                pad_ready = 1;
            }
        }
    }
}

    /* Initialize graphics */
    dmaKit_init(
        D_CTRL_RELE_OFF,
        D_CTRL_MFD_OFF,
        D_CTRL_STS_UNSPEC,
        D_CTRL_STD_OFF,
        D_CTRL_RCYC_8,
        1 << DMA_CHANNEL_GIF
    );

    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gs->PSM=GS_PSM_CT24;
    gs->PSMZ=GS_PSMZ_16;
    gs->ZBuffering=GS_SETTING_OFF;

    gsKit_init_screen(gs);
    gsKit_mode_switch(gs,GS_PERSISTENT);

    const u64 sky =
        GS_SETREG_RGBAQ(25,40,65,0x80,0);

    float angle=0.0f;

    while(1) {

        update_player();

        gsKit_clear(gs,sky);

        draw_ground(gs);
        draw_player(gs);
        draw_cube(gs,angle);

        gsKit_queue_exec(gs);
        gsKit_sync_flip(gs);

        angle+=0.025f;

        if(angle>=2.0f*PI)
            angle-=2.0f*PI;
    }

    return 0;
}
