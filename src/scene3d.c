
#include <gsKit.h>
#include <dmaKit.h>
#include <kernel.h>
#include <math.h>

#define CENTER_X       320.0f
#define CENTER_Y       224.0f
#define FOCAL_LENGTH   250.0f

#define CAMERA_HEIGHT  3.0f
#define CAMERA_Z       -8.0f
#define CAMERA_PITCH   0.35f

#define PI 3.14159265f

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    float x, y;
    int visible;
} ScreenPoint;

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
    float *screen_x,
    float *screen_y
) {
    float dx = p.x;
    float dy = p.y - CAMERA_HEIGHT;
    float dz = p.z - CAMERA_Z;

    float cp = cosf(CAMERA_PITCH);
    float sp = sinf(CAMERA_PITCH);

    float camera_y = dy * cp - dz * sp;
    float camera_z = dy * sp + dz * cp;

    if (camera_z < 0.5f)
        return 0;

    *screen_x = CENTER_X +
        dx * FOCAL_LENGTH / camera_z;

    *screen_y = CENTER_Y -
        camera_y * FOCAL_LENGTH / camera_z;

    return 1;
}

/* Draw one filled triangle */
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

/* Draw a flat rectangle */
static void draw_quad(
    GSGLOBAL *gs,
    float x1,float y1,
    float x2,float y2,
    float x3,float y3,
    float x4,float y4,
    float z,
    u64 color
) {
    Vec3 a={x1,y1,z};
    Vec3 b={x2,y2,z};
    Vec3 c={x3,y3,z};
    Vec3 d={x4,y4,z};

    draw_triangle(gs,a,b,c,color);
    draw_triangle(gs,a,c,d,color);
}

/* Ground */
static void draw_ground(GSGLOBAL *gs)
{
    const float start_x=-10.0f;
    const float start_z=-2.0f;
    const float cell=2.0f;

    const int columns=10;
    const int rows=10;

    const u64 grass_a =
        GS_SETREG_RGBAQ(55,75,43,0x80,0);

    const u64 grass_b =
        GS_SETREG_RGBAQ(68,83,48,0x80,0);

    const u64 grid_color =
        GS_SETREG_RGBAQ(105,115,75,0x80,0);

    for(int z=rows-1;z>=0;z--) {
        for(int x=0;x<columns;x++) {

            float x0=start_x+x*cell;
            float x1=x0+cell;
            float z0=start_z+z*cell;
            float z1=z0+cell;

            Vec3 a={x0,-1.5f,z0};
            Vec3 b={x1,-1.5f,z0};
            Vec3 c={x1,-1.5f,z1};
            Vec3 d={x0,-1.5f,z1};

            u64 color =
                ((x+z)&1)?grass_a:grass_b;

            draw_triangle(gs,a,b,c,color);
            draw_triangle(gs,a,c,d,color);
        }
    }

    for(int x=0;x<=columns;x++) {
        float wx=start_x+x*cell;

        Vec3 a={wx,-1.48f,start_z};
        Vec3 b={wx,-1.48f,start_z+rows*cell};

        float ax,ay,bx,by;

        if(project_world(a,&ax,&ay) &&
           project_world(b,&bx,&by)) {
            gsKit_prim_line_3d(
                gs,ax,ay,0,bx,by,0,grid_color
            );
        }
    }

    for(int z=0;z<=rows;z++) {
        float wz=start_z+z*cell;

        Vec3 a={start_x,-1.48f,wz};
        Vec3 b={
            start_x+columns*cell,
            -1.48f,
            wz
        };

        float ax,ay,bx,by;

        if(project_world(a,&ax,&ay) &&
           project_world(b,&bx,&by)) {
            gsKit_prim_line_3d(
                gs,ax,ay,0,bx,by,0,grid_color
            );
        }
    }
}

/* Player character: low-poly medieval adventurer */
static void draw_player(GSGLOBAL *gs)
{
    /* Colors */
    const u64 trousers =
        GS_SETREG_RGBAQ(35,40,52,0x80,0);

    const u64 boots =
        GS_SETREG_RGBAQ(75,42,25,0x80,0);

    const u64 coat =
        GS_SETREG_RGBAQ(30,42,65,0x80,0);

    const u64 coat_light =
        GS_SETREG_RGBAQ(55,72,100,0x80,0);

    const u64 leather =
        GS_SETREG_RGBAQ(125,78,35,0x80,0);

    const u64 skin =
        GS_SETREG_RGBAQ(190,135,95,0x80,0);

    const u64 hood =
        GS_SETREG_RGBAQ(22,27,38,0x80,0);

    const u64 hood_light =
        GS_SETREG_RGBAQ(45,50,62,0x80,0);

    /* Legs */
    draw_quad(gs,
        -0.29f,-0.58f,
        -0.06f,-0.58f,
        -0.08f,-1.35f,
        -0.31f,-1.35f,
        3.15f,trousers);

    draw_quad(gs,
         0.06f,-0.58f,
         0.29f,-0.58f,
         0.31f,-1.35f,
         0.08f,-1.35f,
        3.15f,trousers);

    /* Boots */
    draw_quad(gs,
        -0.34f,-1.22f,
        -0.07f,-1.22f,
        -0.04f,-1.48f,
        -0.37f,-1.48f,
        3.05f,boots);

    draw_quad(gs,
         0.07f,-1.22f,
         0.34f,-1.22f,
         0.37f,-1.48f,
         0.04f,-1.48f,
        3.05f,boots);

    /* Arms */
    draw_quad(gs,
        -0.34f,0.36f,
        -0.50f,0.29f,
        -0.69f,-0.30f,
        -0.50f,-0.39f,
        2.98f,coat);

    draw_quad(gs,
         0.34f,0.36f,
         0.50f,0.29f,
         0.69f,-0.30f,
         0.50f,-0.39f,
        2.98f,coat);

    /* Hands */
    draw_quad(gs,
        -0.69f,-0.30f,
        -0.50f,-0.39f,
        -0.46f,-0.52f,
        -0.63f,-0.52f,
        2.90f,skin);

    draw_quad(gs,
         0.50f,-0.39f,
         0.69f,-0.30f,
         0.63f,-0.52f,
         0.46f,-0.52f,
        2.90f,skin);

    /* Main coat / torso */
    draw_quad(gs,
        -0.38f,0.43f,
         0.38f,0.43f,
         0.34f,-0.64f,
        -0.34f,-0.64f,
        2.95f,coat);

    /* Coat center panel */
    draw_quad(gs,
        -0.10f,0.35f,
         0.10f,0.35f,
         0.10f,-0.61f,
        -0.10f,-0.61f,
        2.90f,coat_light);

    /* Belt */
    draw_quad(gs,
        -0.35f,-0.43f,
         0.35f,-0.43f,
         0.34f,-0.55f,
        -0.34f,-0.55f,
        2.85f,leather);

    /* Shoulders */
    draw_quad(gs,
        -0.43f,0.39f,
         0.43f,0.39f,
         0.36f,0.22f,
        -0.36f,0.22f,
        2.88f,hood_light);

    /* Hood behind the head */
    Vec3 h1={-0.30f,0.55f,2.82f};
    Vec3 h2={ 0.30f,0.55f,2.82f};
    Vec3 h3={ 0.25f,1.25f,2.82f};
    Vec3 h4={-0.25f,1.25f,2.82f};

    draw_triangle(gs,h1,h2,h3,hood);
    draw_triangle(gs,h1,h3,h4,hood);

    /* Face */
    draw_quad(gs,
        -0.19f,0.65f,
         0.19f,0.65f,
         0.18f,1.12f,
        -0.18f,1.12f,
        2.78f,skin);

    /* Hair / hood top */
    draw_quad(gs,
        -0.23f,1.08f,
         0.23f,1.08f,
         0.20f,1.30f,
        -0.20f,1.30f,
        2.75f,hood);

    /* Hood side details */
    draw_triangle(gs,
        (Vec3){-0.23f,1.08f,2.74f},
        (Vec3){-0.18f,0.65f,2.74f},
        (Vec3){-0.29f,0.78f,2.74f},
        hood_light);

    draw_triangle(gs,
        (Vec3){0.23f,1.08f,2.74f},
        (Vec3){0.29f,0.78f,2.74f},
        (Vec3){0.18f,0.65f,2.74f},
        hood_light);

    /* Small chest emblem */
    draw_quad(gs,
        -0.07f,0.20f,
         0.07f,0.20f,
         0.07f,0.05f,
        -0.07f,0.05f,
        2.84f,leather);
}

/* Rotating cube moved to the right side */
static void draw_cube(
    GSGLOBAL *gs,
    float angle
) {
    ScreenPoint points[8];

    const u64 color =
        GS_SETREG_RGBAQ(80,220,255,0x80,0);

    for(int i=0;i<8;i++) {
        float x=cube[i].x;
        float y=cube[i].y;
        float z=cube[i].z;

        float ca=cosf(angle);
        float sa=sinf(angle);

        float rx=x*ca+z*sa;
        float rz=-x*sa+z*ca;

        float cb=cosf(angle*0.7f);
        float sb=sinf(angle*0.7f);

        float ry=y*cb-rz*sb;
        rz=y*sb+rz*cb;

        Vec3 world={
            rx+3.0f,
            ry+0.2f,
            rz+3.0f
        };

        points[i].visible=project_world(
            world,
            &points[i].x,
            &points[i].y
        );
    }

    for(int i=0;i<12;i++) {
        int a=edges[i][0];
        int b=edges[i][1];

        if(points[a].visible &&
           points[b].visible) {
            gsKit_prim_line_3d(
                gs,
                points[a].x,points[a].y,0,
                points[b].x,points[b].y,0,
                color
            );
        }
    }
}

int main(void)
{
    GSGLOBAL *gsGlobal=gsKit_init_global();

    if(gsGlobal==NULL)
        return 1;

    dmaKit_init(
        D_CTRL_RELE_OFF,
        D_CTRL_MFD_OFF,
        D_CTRL_STS_UNSPEC,
        D_CTRL_STD_OFF,
        D_CTRL_RCYC_8,
        1 << DMA_CHANNEL_GIF
    );

    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsGlobal->PSM=GS_PSM_CT24;
    gsGlobal->PSMZ=GS_PSMZ_16;
    gsGlobal->ZBuffering=GS_SETTING_OFF;

    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal,GS_PERSISTENT);

    const u64 sky =
        GS_SETREG_RGBAQ(25,40,65,0x80,0);

    float angle=0.0f;

    while(1) {
        gsKit_clear(gsGlobal,sky);

        draw_ground(gsGlobal);
        draw_player(gsGlobal);
        draw_cube(gsGlobal,angle);

        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);

        angle+=0.025f;

        if(angle>=2.0f*PI)
            angle-=2.0f*PI;
    }

    return 0;
}
