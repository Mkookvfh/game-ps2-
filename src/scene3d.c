
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

/* Parkour test wall */
#define WALL_LEFT   -2.5f
#define WALL_RIGHT   2.5f
#define WALL_FRONT   5.55f
#define WALL_BACK    5.85f
#define WALL_HEIGHT  0.85f
#define PLAYER_BLOCK_Z 5.15f
#define CLIMB_END_Z  6.35f
#define WALL2_LEFT   -2.5f
#define WALL2_RIGHT   2.5f
#define WALL2_FRONT  10.55f
#define WALL2_BACK   10.85f
#define WALL2_HEIGHT  0.85f
#define WALL2_BLOCK_Z 10.15f
#define WALL2_END_Z  11.35f

typedef struct {
    float x, y, z;
} Vec3;

/* Player state */
static float player_x = 0.0f;
static float player_z = 3.0f;
static float player_y = 0.0f;
static float player_yaw = 0.0f;
static float camera_yaw = 0.0f;

static float jump_velocity = 0.0f;
static int is_jumping = 0;

static float walk_time = 0.0f;
static int is_running = 0;
static float body_bob = 0.0f;
static float body_sway = 0.0f;

/* Parkour state */
static int is_climbing = 0;
static float climb_timer = 0.0f;
static float climb_start_z = 0.0f;
static float climb_start_x = 0.0f;
static int is_hanging = 0;
static int is_mantling = 0;

static int triangle_prev = 0;
static int cross_prev = 0;

static float mantle_timer = 0.0f;
static float mantle_start_z = 0.0f;
static float mantle_start_y = 0.0f;
static int active_wall = 0;
static float active_wall_front = WALL_FRONT;
static float active_wall_end_z = CLIMB_END_Z;

/* PS2 controller */
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

/* Projection */
static int project_world(
    Vec3 p,
    float *sx,
    float *sy
) {
    float dx = p.x - player_x;
    float dy = p.y - CAMERA_HEIGHT;
    float dz = p.z - player_z;

    float cs = cosf(camera_yaw);
    float sn = sinf(camera_yaw);

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

/* Triangle */
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

/* Rectangle */
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

/* Parkour wall */
static void draw_parkour_wall_at(
    GSGLOBAL *gs,
    float left, float right,
    float front, float back,
    float height, u64 wall_color
) {
    const u64 wall_side =
        GS_SETREG_RGBAQ(70,58,48,0x80,0);
    const u64 wall_top =
        GS_SETREG_RGBAQ(145,125,95,0x80,0);

    float bottom = -1.5f;
    float top = bottom + height;

    draw_rect(gs,left,bottom,right,top,front,wall_color);
    draw_rect(gs,left,bottom,right,top,back,wall_side);

    Vec3 a={left,bottom,front};
    Vec3 b={left,bottom,back};
    Vec3 c={left,top,back};
    Vec3 d={left,top,front};

    draw_triangle(gs,a,b,c,wall_side);
    draw_triangle(gs,a,c,d,wall_side);

    a.x=right;
    b.x=right;
    c.x=right;
    d.x=right;

    draw_triangle(gs,a,b,c,wall_side);
    draw_triangle(gs,a,c,d,wall_side);

    Vec3 t1={left,top,front};
    Vec3 t2={right,top,front};
    Vec3 t3={right,top,back};
    Vec3 t4={left,top,back};

    draw_triangle(gs,t1,t2,t3,wall_top);
    draw_triangle(gs,t1,t3,t4,wall_top);
}

static void draw_parkour_wall(GSGLOBAL *gs)
{
    draw_parkour_wall_at(
        gs,WALL_LEFT,WALL_RIGHT,
        WALL_FRONT,WALL_BACK,
        WALL_HEIGHT,
        GS_SETREG_RGBAQ(105,85,65,0x80,0)
    );

    draw_parkour_wall_at(
        gs,WALL2_LEFT,WALL2_RIGHT,
        WALL2_FRONT,WALL2_BACK,
        WALL2_HEIGHT,
        GS_SETREG_RGBAQ(105,95,72,0x80,0)
    );
}

static void select_parkour_wall(int index)
{
    active_wall=index;

    if(index==1) {
        active_wall_front=WALL2_FRONT;
        active_wall_end_z=WALL2_END_Z;
    } else {
        active_wall=0;
        active_wall_front=WALL_FRONT;
        active_wall_end_z=CLIMB_END_Z;
    }
}

/* Player rectangle */
static void draw_player_rect(
    GSGLOBAL *gs,
    float left,
    float bottom,
    float right,
    float top,
    float local_z,
    u64 color
) {
    float cs=cosf(player_yaw);
    float sn= sinf(player_yaw);

    float y0=bottom+player_y+body_bob;
    float y1=top+player_y+body_bob;

    left+=body_sway;
    right+=body_sway;

    Vec3 a={
        player_x+left*cs+local_z*sn,
        y0,
        player_z-left*sn+local_z*cs
    };

    Vec3 b={
        player_x+right*cs+local_z*sn,
        y0,
        player_z-right*sn+local_z*cs
    };

    Vec3 c={
        player_x+right*cs+local_z*sn,
        y1,
        player_z-right*sn+local_z*cs
    };

    Vec3 d={
        player_x+left*cs+local_z*sn,
        y1,
        player_z-left*sn+local_z*cs
    };

    draw_triangle(gs,a,b,c,color);
    draw_triangle(gs,a,c,d,color);
}

/* CPU joint animation */
static void draw_player_limb(
    GSGLOBAL *gs,
    float pivot_x,
    float pivot_y,
    float length,
    float width,
    float angle,
    float local_z,
    u64 color
) {
    float c=cosf(angle);
    float sn=sinf(angle);
    float half=width*0.5f;

    float lx[4]={-half,half,half,-half};
    float ly[4]={0.0f,0.0f,-length,-length};

    Vec3 p[4];

    float body_c=cosf(player_yaw);
    float body_s=sinf(player_yaw);

    for(int i=0;i<4;i++) {
        float rx=lx[i]*c-ly[i]*sn+pivot_x;
        float ry=lx[i]*sn+ly[i]*c+pivot_y;

        rx+=body_sway;

        p[i].x=player_x+rx*body_c+local_z*body_s;
        p[i].y=ry+player_y+body_bob;
        p[i].z=player_z-rx*body_s+local_z*body_c;
    }

    draw_triangle(gs,p[0],p[1],p[2],color);
    draw_triangle(gs,p[0],p[2],p[3],color);
}

/* Player */
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

    float leg_swing;
    float arm_swing;

    if(is_hanging) {
    leg_swing = 0.30f;
    arm_swing = 0.0f;
}
else if(is_mantling) {
    leg_swing = 0.45f;
    arm_swing = -0.55f;
}
else if(is_climbing) {
    leg_swing = sinf(climb_timer*PI)*0.45f;
    arm_swing = -0.65f;
}
else {
    leg_swing = is_jumping
        ? 0.18f
        : sinf(walk_time)*
          (is_running ? 0.62f : 0.38f);

    arm_swing = is_jumping
        ? -0.48f
        : -sinf(walk_time)*
          (is_running ? 0.48f : 0.24f);
}

    /* Legs */
    draw_player_limb(gs,
        -0.17f,-0.55f,
        0.78f,0.23f,
        leg_swing,0.15f,trousers);

    draw_player_limb(gs,
         0.17f,-0.55f,
        0.78f,0.23f,
        -leg_swing,0.15f,trousers);

    /* Boots */
    draw_player_limb(gs,
        -0.17f+sinf(leg_swing)*0.78f,
        -1.30f,
        0.25f,0.30f,
        leg_swing,0.16f,boots);

    draw_player_limb(gs,
         0.17f-sinf(leg_swing)*0.78f,
        -1.30f,
        0.25f,0.30f,
        -leg_swing,0.16f,boots);

    /* Arms */
    draw_player_limb(gs,
        -0.39f,0.34f,
        0.78f,0.21f,
        arm_swing,0.17f,coat);

    draw_player_limb(gs,
         0.39f,0.34f,
        0.78f,0.21f,
        -arm_swing,0.17f,coat);

    /* Torso */
    draw_player_rect(gs,
        -0.38f,-0.64f,
         0.38f,0.43f,
        0.18f,coat);

    /* Coat center */
    draw_player_rect(gs,
        -0.10f,-0.61f,
         0.10f,0.35f,
        0.19f,coat_light);

    /* Belt */
    draw_player_rect(gs,
        -0.35f,-0.55f,
         0.35f,-0.43f,
        0.20f,leather);

    /* Neck */
    draw_player_rect(gs,
        -0.12f,0.40f,
         0.12f,0.68f,
        0.20f,skin);

    /* Face */
    draw_player_rect(gs,
        -0.19f,0.65f,
         0.19f,1.12f,
        0.21f,skin);

    /* Hood */
    draw_player_rect(gs,
        -0.23f,1.08f,
         0.23f,1.30f,
        0.22f,hood);
}

/* Controller and movement */
static void update_player(void)
{
    float move_x=0.0f;
    float move_z=0.0f;

    int running=0;
    int jump_pressed=0;
    int triangle_down=0;
int cross_down=0;
int climb_pressed=0;
int drop_pressed=0;

    if(pad_ready &&
       padGetState(0,0)==PAD_STATE_STABLE)
    {
        if(!analog_requested) {
            padSetMainMode(
                0,0,
                PAD_MMODE_DUALSHOCK,
                PAD_MMODE_LOCK
            );
            analog_requested=1;
        }

        if(padRead(0,0,&pad)) {
            /* D-pad */
            if(!(pad.btns&PAD_LEFT))
                move_x-=1.0f;

            if(!(pad.btns&PAD_RIGHT))
                move_x+=1.0f;

            if(!(pad.btns&PAD_UP))
                move_z+=1.0f;

            if(!(pad.btns&PAD_DOWN))
                move_z-=1.0f;

            /* Analog */
            int lx=(int)pad.ljoy_h;
            int ly=(int)pad.ljoy_v;

            if(lx>0 && lx<93)
                move_x=-1.0f;
            else if(lx>163)
                move_x=1.0f;

            if(ly>0 && ly<93)
                move_z=1.0f;
            else if(ly>163)
                move_z=-1.0f;

            /* R1 = Run */
            if(!(pad.btns&PAD_R1))
                running=1;

            /* Cross = Jump */
            if(!(pad.btns & PAD_CROSS))
                cross_down=1;

            /* Triangle = Climb */
            if(!(pad.btns & PAD_TRIANGLE))
                triangle_down=1;
            climb_pressed = triangle_down && !triangle_prev;
            jump_pressed = cross_down && !cross_prev;
            drop_pressed = jump_pressed;

            triangle_prev = triangle_down;
            cross_prev = cross_down;
        }
    }

    /* Climbing animation */
    if(is_climbing) {
        climb_timer+=0.035f;

        if(climb_timer>1.0f)
            climb_timer=1.0f;

        float t=climb_timer;
        float smooth=t*t*(3.0f-2.0f*t);

        player_x=climb_start_x;
        player_z=climb_start_z+
    (active_wall_end_z-climb_start_z)*smooth;

        player_y=sinf(t*PI)*0.85f;

        player_yaw=0.0f;
        body_bob=0.0f;
        body_sway=0.0f;

        if(climb_timer>=1.0f) {
            is_climbing=0;
            player_y=0.0f;
            player_z=active_wall_end_z;
            climb_timer=0.0f;
        }

        return;
    }

    /* Drop from ledge */
if(is_hanging && drop_pressed) {
    is_hanging = 0;
    player_y = 0.0f;
    is_jumping = 1;
    jump_velocity = -0.04f;
    return;
}

/* Start mantle */
if(is_hanging && climb_pressed) {
    is_hanging = 0;
    is_mantling = 1;
    mantle_timer = 0.0f;

    mantle_start_z = player_z;
    mantle_start_y = player_y;

    return;
}

/* Hanging: move along the wall */
if(is_hanging) {
    player_y = -0.30f + sinf(walk_time * 2.0f) * 0.025f;
    player_z = active_wall_front - 0.12f;
    player_yaw = 0.0f;

    if(move_x < -0.1f)
        player_x -= 0.035f;
    else if(move_x > 0.1f)
        player_x += 0.035f;

    if(player_x < WALL_LEFT + 0.35f)
        player_x = WALL_LEFT + 0.35f;
    if(player_x > WALL_RIGHT - 0.35f)
        player_x = WALL_RIGHT - 0.35f;

    body_bob = 0.0f;
    body_sway = 0.0f;

    return;
}

/* Mantle animation */
if(is_mantling) {
    mantle_timer += 0.035f;

    if(mantle_timer > 1.0f)
        mantle_timer = 1.0f;

    float t = mantle_timer;
    float smooth = t*t*(3.0f-2.0f*t);

    player_z = mantle_start_z +
    (active_wall_end_z - mantle_start_z) * smooth;

    player_y = mantle_start_y +
        0.95f * sinf(t * PI);

    if(mantle_timer >= 1.0f) {
        is_mantling = 0;
        player_y = 0.0f;
        player_z = active_wall_end_z;
        mantle_timer = 0.0f;
    }

    return;
}

    /* Start ledge grab near either wall */
int requested_wall = -1;

if(fabsf(player_x) < 2.2f &&
   player_z > 4.65f &&
   player_z < 5.35f)
{
    requested_wall = 0;
}
else if(fabsf(player_x) < 2.2f &&
        player_z > 9.65f &&
        player_z < 10.35f)
{
    requested_wall = 1;
}

if(climb_pressed &&
   !is_jumping &&
   requested_wall >= 0 &&
   cosf(player_yaw) > 0.65f)
{
    select_parkour_wall(requested_wall);

    is_hanging = 1;
    player_z = active_wall_front - 0.12f;
    player_y = -0.30f;
    jump_velocity = 0.0f;

    return;
}

    float speed=running?0.085f:0.045f;

    if(move_x!=0.0f || move_z!=0.0f) {
        float length=sqrtf(
            move_x*move_x+
            move_z*move_z
        );

        if(length>0.0f) {
            move_x/=length;
            move_z/=length;
        }

        float next_x=player_x+move_x*speed;
        float next_z=player_z+move_z*speed;

        /* Wall collision: front side */
        if(fabsf(next_x)<2.2f &&
           player_z<=PLAYER_BLOCK_Z &&
           next_z>PLAYER_BLOCK_Z &&
           player_y<WALL_HEIGHT*0.65f)
        {
            next_z=PLAYER_BLOCK_Z;
        }

        /* Wall collision: back side */
        if(fabsf(next_x)<2.2f &&
           player_z>=6.25f &&
           next_z<6.25f &&
           player_y<WALL_HEIGHT*0.65f)
        {
            next_z=6.25f;
        }

                /* Second wall collision: front side */
        if(fabsf(next_x)<2.2f &&
           player_z<=WALL2_BLOCK_Z &&
           next_z>WALL2_BLOCK_Z &&
           player_y<WALL2_HEIGHT*0.65f)
        {
            next_z=WALL2_BLOCK_Z;
        }

        /* Second wall collision: back side */
        if(fabsf(next_x)<2.2f &&
           player_z>=WALL2_BACK+0.40f &&
           next_z<WALL2_BACK+0.40f &&
           player_y<WALL2_HEIGHT*0.65f)
        {
            next_z=WALL2_BACK+0.40f;
        }

        player_x=next_x;
        player_z=next_z;

        player_yaw=atan2f(move_x,move_z);

        float yaw_diff=atan2f(
            sinf(player_yaw-camera_yaw),
            cosf(player_yaw-camera_yaw)
        );

        camera_yaw+=yaw_diff*0.12f;

        is_running=running;

        walk_time+=is_running?0.32f:0.18f;

        body_bob=sinf(walk_time*2.0f)*
            (is_running?0.045f:0.025f);

        body_sway=sinf(walk_time)*
            (is_running?0.025f:0.012f);
    } else {
        is_running=0;
        body_bob*=0.75f;
        body_sway*=0.75f;

        if(fabsf(body_bob)<0.001f)
            body_bob=0.0f;

        if(fabsf(body_sway)<0.001f)
            body_sway=0.0f;
    }

    /* Jump */
    if(jump_pressed && !is_jumping) {
        is_jumping=1;
        jump_velocity=0.18f;
    }

    if(is_jumping) {
        player_y+=jump_velocity;
        jump_velocity-=0.012f;

        if(player_y<=0.0f) {
            player_y=0.0f;
            jump_velocity=0.0f;
            is_jumping=0;
        }
    }

        /* Automatically catch either wall while airborne */
if(is_jumping &&
   fabsf(player_x) < 2.2f &&
   player_y > 0.12f &&
   cosf(player_yaw) > 0.65f)
{
    int caught_wall = -1;

    if(player_z >= PLAYER_BLOCK_Z - 0.20f &&
       player_z <= PLAYER_BLOCK_Z + 0.12f)
    {
        caught_wall = 0;
    }
    else if(player_z >= WALL2_BLOCK_Z - 0.20f &&
            player_z <= WALL2_BLOCK_Z + 0.12f)
    {
        caught_wall = 1;
    }

    if(caught_wall >= 0)
    {
        select_parkour_wall(caught_wall);

        is_jumping = 0;
        jump_velocity = 0.0f;
        is_hanging = 1;

        player_z = active_wall_front - 0.12f;
        player_y = -0.30f;

        body_bob = 0.0f;
        body_sway = 0.0f;
    }
}

    /* World bounds */
    if(player_x<-8.0f) player_x=-8.0f;
    if(player_x>8.0f) player_x=8.0f;

    if(player_z<-1.0f) player_z=-1.0f;
    if(player_z>16.0f) player_z=16.0f;
}

/* Cube */
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

        visible[i]=project_world(p,&sx[i],&sy[i]);
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

/* Main */
int main(void)
{
    GSGLOBAL *gs=gsKit_init_global();

    if(gs==NULL)
        return 1;

    /* Keep original PS2 controller driver order */
    SifInitRpc(0);

    int sio=SifLoadModule(
        "rom0:XSIO2MAN",0,NULL
    );

    if(sio>=0) {
        int pman=SifLoadModule(
            "rom0:XPADMAN",0,NULL
        );

        if(pman>=0) {
            if(padInit(0)==1) {
                if(padPortOpen(0,0,padBuf)!=0) {
                    pad_ready=1;
                }
            }
        }
    }

    /* Graphics */
    dmaKit_init(
        D_CTRL_RELE_OFF,
        D_CTRL_MFD_OFF,
        D_CTRL_STS_UNSPEC,
        D_CTRL_STD_OFF,
        D_CTRL_RCYC_8,
        1<<DMA_CHANNEL_GIF
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
        draw_parkour_wall(gs);
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
