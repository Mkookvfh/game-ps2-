
#include <gsKit.h>
#include <dmaKit.h>
#include <kernel.h>
#include <libpad.h>
#include <math.h>
#include <sifrpc.h>
#include <loadfile.h>
#include "world.h"

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
static float camera_pitch = 0.12f;
static float camera_distance = 8.0f;
static int camera_manual = 0;

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
static int hanging_on_side = 0;

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

static int select_prev = 0;
static int start_prev = 0;

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


/* Projection with reversed Z depth */
static int project_world(
    Vec3 p,
    float *sx,
    float *sy,
    int *sz
) {
    float dx = p.x - player_x;
    float dy = p.y - CAMERA_HEIGHT;
    float dz = p.z - player_z;

    float cs = cosf(camera_yaw);
    float sn = sinf(camera_yaw);

    float camera_x = dx * cs - dz * sn;
    float camera_z = camera_distance + dx * sn + dz * cs;

    float cp = cosf(camera_pitch);
    float sp = sinf(camera_pitch);

    float cy = dy * cp - camera_z * sp;
    float cz = dy * sp + camera_z * cp;

    if (cz < 0.5f)
        return 0;

    *sx = CENTER_X + camera_x * FOCAL_LENGTH / cz;
    *sy = CENTER_Y - cy * FOCAL_LENGTH / cz;

    if (sz != NULL) {
        int depth = (int)(16777215.0f / cz);

        if (depth > 0xFFFFFF)
            depth = 0xFFFFFF;

        if (depth < 1)
            depth = 1;

        *sz = depth;
    }

    return 1;
}

/* Cinematic Lighting V1 - Golden Sunlight */
static u64 apply_sun_lighting(
    u64 color,
    Vec3 a,
    Vec3 b,
    Vec3 c
) {
    float ux = b.x - a.x;
    float uy = b.y - a.y;
    float uz = b.z - a.z;

    float vx = c.x - a.x;
    float vy = c.y - a.y;
    float vz = c.z - a.z;

    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;

    float len = sqrtf(nx*nx + ny*ny + nz*nz);

    if (len < 0.0001f)
        return color;

    nx /= len;
    ny /= len;
    nz /= len;

    /* Warm sunlight from upper-left */
    float light = fabsf(
        nx * -0.45f +
        ny *  0.82f +
        nz * -0.35f
    );

    float intensity = 0.72f + light * 0.28f;

    int r = (int)(color & 0xFF);
    int g = (int)((color >> 8) & 0xFF);
    int bl = (int)((color >> 16) & 0xFF);
    int alpha = (int)((color >> 24) & 0xFF);

    r = (int)(r * intensity * 1.05f);
    g = (int)(g * intensity * 1.02f);
    bl = (int)(bl * intensity * 0.94f);

    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (bl > 255) bl = 255;

    return GS_SETREG_RGBAQ(r,g,bl,alpha,0);
}

/* Cinematic Atmosphere V1 - Distance Fog */
static u64 apply_distance_fog(
    u64 color,
    int z1,
    int z2,
    int z3
) {
    const WorldEra *era = world_current_era();

    float avg_z = (
        (float)z1 +
        (float)z2 +
        (float)z3
    ) / 3.0f;

    if (avg_z < 1.0f)
        return color;

    float distance = 16777215.0f / avg_z;

    float fog = (distance - 18.0f) / 30.0f;

    if (fog < 0.0f) fog = 0.0f;
    if (fog > 1.0f) fog = 1.0f;

    int r = (int)(color & 0xFF);
    int g = (int)((color >> 8) & 0xFF);
    int b = (int)((color >> 16) & 0xFF);
    int a = (int)((color >> 24) & 0xFF);

    r += (int)((era->sky_r - r) * fog);
    g += (int)((era->sky_g - g) * fog);
    b += (int)((era->sky_b - b) * fog);

    return GS_SETREG_RGBAQ(r, g, b, a, 0);
}

/* Cinematic Sky V1 - Vertical Gradient */
static void draw_cinematic_sky(
    GSGLOBAL *gs,
    const WorldEra *era
) {
    int bands = 12;

    int top_r = era->sky_r * 55 / 100;
    int top_g = era->sky_g * 55 / 100;
    int top_b = era->sky_b * 65 / 100;

    int horizon_r = era->sky_r + 22;
    int horizon_g = era->sky_g + 18;
    int horizon_b = era->sky_b + 10;

    if (horizon_r > 255) horizon_r = 255;
    if (horizon_g > 255) horizon_g = 255;
    if (horizon_b > 255) horizon_b = 255;

    for (int i = 0; i < bands; i++) {
        float t = (float)i / (float)(bands - 1);

        int r = top_r + (int)((horizon_r - top_r) * t);
        int g = top_g + (int)((horizon_g - top_g) * t);
        int b = top_b + (int)((horizon_b - top_b) * t);

        int y1 = i * 448 / bands;
        int y2 = (i + 1) * 448 / bands + 1;

        u64 color = GS_SETREG_RGBAQ(r, g, b, 0x80, 0);

        gsKit_prim_sprite(
            gs,
            0.0f, (float)y1,
            640.0f, (float)y2,
            1,
            color
        );
    }
}

/* Triangle with hardware depth */
static void draw_triangle(
    GSGLOBAL *gs,
    Vec3 a,
    Vec3 b,
    Vec3 c,
    u64 color
) {
    float x1,y1,x2,y2,x3,y3;
    int z1,z2,z3;

    if (!project_world(a,&x1,&y1,&z1)) return;
    if (!project_world(b,&x2,&y2,&z2)) return;
    if (!project_world(c,&x3,&y3,&z3)) return;

/* Golden sunlight */
color = apply_sun_lighting(color,a,b,c);

/* Distance fog */
color = apply_distance_fog(color,z1,z2,z3);

    gsKit_prim_triangle_3d(
        gs,
        x1,y1,z1,
        x2,y2,z2,
        x3,y3,z3,
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
/* Expanded City Ground V1 */
static void draw_ground(GSGLOBAL *gs)
{
    const WorldEra *era = world_current_era();

    const u64 grass1 = GS_SETREG_RGBAQ(
        era->ground_r,
        era->ground_g,
        era->ground_b,
        0x80, 0
    );

    const u64 grass2 = GS_SETREG_RGBAQ(
        era->ground_r + 12,
        era->ground_g + 10,
        era->ground_b + 8,
        0x80, 0
    );

    const u64 grid = GS_SETREG_RGBAQ(
        era->accent_r,
        era->accent_g,
        era->accent_b,
        0x80, 0
    );

    /* Open World Ground - 90 x 90 */
for(int z=17;z>=0;z--) {
    for(int x=0;x<18;x++) {

        float x0=-45.0f+x*5.0f;
        float x1=x0+5.0f;

        float z0=-25.0f+z*5.0f;
            float z1=z0+5.0f;

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
    for(int x=0;x<=18;x++) {

        float wx=-45.0f+x*5.0f;

        Vec3 a={wx,-1.48f,-10.0f};
        Vec3 b={wx,-1.48f,50.0f};

        
float ax,ay,bx,by;
int az,bz;

if(project_world(a,&ax,&ay,&az) &&
   project_world(b,&bx,&by,&bz)) {

    gsKit_prim_line_3d(
        gs,ax,ay,az,bx,by,bz,grid
    );
}

    }

    for(int z=0;z<=18;z++) {

        float wz=-25.0f+z*5.0f;

        Vec3 a={-45.0f,-1.48f,wz};
        Vec3 b={45.0f,-1.48f,wz};

        
float ax,ay,bx,by;
int az,bz;

if(project_world(a,&ax,&ay,&az) &&
   project_world(b,&bx,&by,&bz)) {

    gsKit_prim_line_3d(
        gs,ax,ay,az,bx,by,bz,grid
    );
}

    }
}


/* =========================================
   MUEANG ISARA - CITY MAP V1
   ========================================= */

static void draw_city_building(
    GSGLOBAL *gs,
    float x, float width,
    float front, float depth,
    float height,
    u64 front_color,
    u64 side_color,
    u64 roof_color,
    u64 window_color
) {
    float bottom = -1.5f;
    float top = bottom + height;
    float right = x + width;
    float back = front + depth;

    /* Front wall */
    draw_rect(
        gs, x, bottom, right, top,
        front, front_color
    );

    /* Side wall */
    Vec3 a = {right,bottom,front};
    Vec3 b = {right,bottom,back};
    Vec3 c = {right,top,back};
    Vec3 d = {right,top,front};

    draw_triangle(gs,a,b,c,side_color);
    draw_triangle(gs,a,c,d,side_color);

    /* Roof */
    a.x=x; a.y=top; a.z=front;
    b.x=right; b.y=top; b.z=front;
    c.x=right; c.y=top; c.z=back;
    d.x=x; d.y=top; d.z=back;

    draw_triangle(gs,a,b,c,roof_color);
    draw_triangle(gs,a,c,d,roof_color);

    /* Windows */
    int row,col;

    for(row=0;row<2;row++) {
        for(col=0;col<2;col++) {
            float wx=x+width*(col+1)/3.0f;
            float wy=bottom+height*(row+1)/3.0f;

            draw_rect(
                gs,
                wx-0.12f,wy-0.14f,
                wx+0.12f,wy+0.14f,
                front-0.015f,
                window_color
            );
        }
    }
    /* Building Entrance Door V2 */
    u64 door_color = GS_SETREG_RGBAQ(
        45, 30, 22, 0x80, 0
    );

    float door_width = 0.72f;
    float door_height = 0.78f;
    float door_center = x + width * 0.5f;

    draw_rect(
        gs,
        door_center - door_width * 0.5f,
        bottom,
        door_center + door_width * 0.5f,
        bottom + door_height,
        front - 0.025f,
        door_color
    );
}


/* City Landmark - Pyramid Roof */
static void draw_landmark_roof(
    GSGLOBAL *gs,
    float x,
    float width,
    float front,
    float depth,
    float base_y,
    float peak_y,
    u64 color
) {
    float mid_x = x + width * 0.5f;
    float mid_z = front + depth * 0.5f;

    Vec3 a = {x, base_y, front};
    Vec3 b = {x + width, base_y, front};
    Vec3 c = {x + width, base_y, front + depth};
    Vec3 d = {x, base_y, front + depth};
    Vec3 top = {mid_x, peak_y, mid_z};

    draw_triangle(gs, a, b, top, color);
    draw_triangle(gs, b, c, top, color);
    draw_triangle(gs, c, d, top, color);
    draw_triangle(gs, d, a, top, color);
}


/* =========================================
   CITY DISTRICTS V1 - 32 EXTRA BUILDINGS
   ========================================= */

typedef struct {
    int city;
    float x;
    float width;
    float front;
    float depth;
    float height;
} CityBuildingData;

static const CityBuildingData city_buildings[] = {
    /* PARIS - 8 buildings */
    {0,-15.0f,3.5f,17.0f,2.0f,3.5f},
    {0,-9.5f,3.0f,20.0f,2.0f,4.0f},
    {0,5.0f,3.2f,18.0f,2.0f,3.8f},
    {0,11.0f,3.5f,21.0f,2.0f,4.5f},
    {0,-16.0f,3.8f,29.0f,2.5f,4.2f},
    {0,-9.5f,3.0f,34.0f,2.0f,3.2f},
    {0,5.0f,3.5f,30.0f,2.0f,4.0f},
    {0,11.0f,4.0f,36.0f,2.5f,5.0f},

    /* VENICE - 8 buildings */
    {1,-15.0f,3.5f,17.0f,2.5f,3.8f},
    {1,-9.0f,3.0f,22.0f,2.0f,4.5f},
    {1,5.0f,3.5f,18.0f,2.5f,4.0f},
    {1,11.0f,3.5f,23.0f,2.0f,3.5f},
    {1,-16.0f,3.8f,30.0f,2.5f,4.5f},
    {1,-9.5f,3.0f,35.0f,2.0f,3.5f},
    {1,5.0f,3.5f,31.0f,2.5f,4.2f},
    {1,11.0f,4.0f,37.0f,2.5f,5.0f},

    /* ROME - 8 buildings */
    {2,-15.0f,3.5f,17.0f,2.5f,4.0f},
    {2,-9.0f,3.0f,22.0f,2.0f,3.5f},
    {2,5.0f,3.5f,18.0f,2.5f,4.5f},
    {2,11.0f,3.5f,23.0f,2.0f,4.0f},
    {2,-16.0f,4.0f,30.0f,2.5f,4.5f},
    {2,-9.5f,3.0f,35.0f,2.0f,3.8f},
    {2,5.0f,3.5f,31.0f,2.5f,4.2f},
    {2,11.0f,4.0f,37.0f,2.5f,5.2f},

    /* ENGLAND - 8 buildings */
    {3,-15.0f,3.5f,17.0f,2.0f,3.0f},
    {3,-9.0f,3.0f,22.0f,2.0f,3.5f},
    {3,5.0f,3.5f,18.0f,2.0f,3.2f},
    {3,11.0f,3.5f,23.0f,2.0f,3.8f},
    {3,-16.0f,4.0f,30.0f,2.5f,3.5f},
    {3,-9.5f,3.0f,35.0f,2.0f,3.0f},
    {3,5.0f,3.5f,31.0f,2.5f,3.8f},
    {3,11.0f,4.0f,37.0f,2.5f,4.0f}
};

/* Graphics Upgrade V2 - Building Shadows Enhanced */
static void draw_building_shadow(
    GSGLOBAL *gs,
    float x,
    float width,
    float front,
    float depth
) {
    float y = -1.44f;

    float extend_x = 2.8f;
    float extend_z = 4.5f;

    u64 shadow = GS_SETREG_RGBAQ(
        12, 11, 10, 0x80, 0
    );

    Vec3 a = {
        x + 0.25f, y, front + 0.25f
    };

    Vec3 b = {
        x + width + extend_x, y, front + 0.25f
    };

    Vec3 c = {
        x + width + extend_x, y,
        front + depth + extend_z
    };

    Vec3 d = {
        x + 0.25f, y,
        front + depth + extend_z
    };

    draw_triangle(gs, a, b, c, shadow);
    draw_triangle(gs, a, c, d, shadow);
}

static void draw_city_districts(
    GSGLOBAL *gs,
    int city,
    u64 facade,
    u64 stone,
    u64 roof,
    u64 window
) {
    int count = sizeof(city_buildings) /
                sizeof(city_buildings[0]);

    for(int i=0; i<count; i++) {
        const CityBuildingData *b = &city_buildings[i];

        if(b->city != city)
            continue;

draw_building_shadow(
    gs,
    b->x,
    b->width,
    b->front,
    b->depth
);

        draw_city_building(
            gs,
            b->x,
            b->width,
            b->front,
            b->depth,
            b->height,
            facade,
            stone,
            roof,
            window
        );
    }
}


/* =========================================
   CITY MAP V2 - ROADS AND ALLEYS
   ========================================= */

static void draw_ground_quad(
    GSGLOBAL *gs,
    float left,
    float right,
    float front,
    float back,
    u64 color
) {
    Vec3 a={left,-1.455f,front};
    Vec3 b={right,-1.455f,front};
    Vec3 c={right,-1.455f,back};
    Vec3 d={left,-1.455f,back};

    draw_triangle(gs,a,b,c,color);
    draw_triangle(gs,a,c,d,color);
}

static void draw_city_roads(
    GSGLOBAL *gs,
    int city
) {
    const u64 road =
        GS_SETREG_RGBAQ(75,72,68,0x80,0);

    const u64 alley =
        GS_SETREG_RGBAQ(100,94,82,0x80,0);

    const u64 sidewalk =
        GS_SETREG_RGBAQ(135,125,105,0x80,0);

    const u64 road_line =
        GS_SETREG_RGBAQ(190,175,130,0x80,0);

    /* Venice: canal replaces the central road */
    if(city==1) {
        draw_ground_quad(
            gs,-3.7f,-2.05f,-2.0f,18.0f,
            sidewalk
        );

        draw_ground_quad(
            gs,2.05f,3.7f,-2.0f,18.0f,
            sidewalk
        );
    }
    else {
        /* Main road */
        draw_ground_quad(
            gs,-1.8f,1.8f,-2.0f,18.0f,
            road
        );

        /* Road borders */
        draw_ground_quad(
            gs,-2.05f,-1.8f,-2.0f,18.0f,
            sidewalk
        );

        draw_ground_quad(
            gs,1.8f,2.05f,-2.0f,18.0f,
            sidewalk
        );

        /* Center road marking */
        {
            int z;

            for(z=0;z<9;z++) {
                float front=-1.0f+z*2.0f;

                draw_ground_quad(
                    gs,-0.035f,0.035f,
                    front,front+0.65f,
                    road_line
                );
            }
        }
    }

    /* Left and right alleys */
    draw_ground_quad(
        gs,-3.8f,-2.9f,-2.0f,18.0f,
        alley
    );

    draw_ground_quad(
        gs,2.9f,3.8f,-2.0f,18.0f,
        alley
    );

    /* Cross street */
    draw_ground_quad(
        gs,-8.0f,8.0f,7.5f,8.5f,
        road
    );

    /* Small connecting paths */
    draw_ground_quad(
        gs,-5.0f,-3.8f,7.5f,8.5f,
        sidewalk
    );

    draw_ground_quad(
        gs,3.8f,5.0f,7.5f,8.5f,
        sidewalk
    );
}

static void draw_city_map(GSGLOBAL *gs)
{
    const WorldEra *era=world_current_era();

    const u64 facade=GS_SETREG_RGBAQ(
        era->accent_r,
        era->accent_g,
        era->accent_b,
        0x80,0
    );

    const u64 stone=GS_SETREG_RGBAQ(
        95,85,72,0x80,0
    );

    const u64 roof=GS_SETREG_RGBAQ(
        120,75,48,0x80,0
    );

    
const u64 window=GS_SETREG_RGBAQ(
    25,30,40,0x80,0
);

/* Draw streets and alleys */
draw_city_roads(gs,world_city_index());

/* Draw additional city districts */
draw_city_districts(
    gs,
    world_city_index(),
    facade,
    stone,
    roof,
    window
);

/* City Map V3 - Rooftop Parkour */
draw_city_building(
    gs,-6.2f,2.5f,
    7.0f,2.0f,0.90f,
    facade,stone,roof,window
);

draw_city_building(
    gs,3.7f,2.5f,
    7.0f,2.0f,0.90f,
    facade,stone,roof,window
);


    switch(world_city_index()) {

        /* PARIS */
        case 0:
            draw_city_building(
                gs,-7.0f,2.7f,11.0f,1.6f,3.0f,
                facade,stone,roof,window
            );

            draw_city_building(
                gs,4.1f,2.8f,10.2f,1.8f,3.4f,
                facade,stone,roof,window
            );

            draw_city_building(
                gs,-1.0f,2.0f,15.0f,1.4f,4.2f,
                facade,stone,roof,window
            );
/* Paris Landmark - Grand Tower */
{
    u64 tower_roof = GS_SETREG_RGBAQ(
        75, 48, 38, 0x80, 0
    );

    draw_city_building(
        gs,
        -1.2f, 2.4f,
        24.0f, 2.2f,
        6.0f,
        facade, stone, roof, window
    );

    draw_landmark_roof(
        gs,
        -1.2f, 2.4f,
        24.0f, 2.2f,
        4.5f, 6.5f,
        tower_roof
    );
}
/* Paris - Stone Facade Details */
{
    u64 trim = GS_SETREG_RGBAQ(
        190, 165, 125, 0x80, 0
    );

    /* Horizontal stone bands */
    draw_rect(gs,-1.2f,-0.25f,
        1.2f,-0.15f,23.97f,trim);

    draw_rect(gs,-1.2f,1.65f,
        1.2f,1.75f,23.97f,trim);

    draw_rect(gs,-1.2f,3.55f,
        1.2f,3.65f,23.97f,trim);

    /* Corner stone pillars */
    draw_rect(gs,-1.18f,-1.4f,
        -1.08f,4.4f,23.96f,trim);

    draw_rect(gs,1.08f,-1.4f,
        1.18f,4.4f,23.96f,trim);
}
/* Paris V2 - Grand Stone Arch */
{
    u64 arch_stone = GS_SETREG_RGBAQ(
        175, 155, 125, 0x80, 0
    );

    u64 arch_shadow = GS_SETREG_RGBAQ(
        105, 85, 65, 0x80, 0
    );

    /* Left pillar */
    draw_city_building(
        gs,
        6.5f, 0.65f,
        15.0f, 0.8f,
        3.8f,
        arch_stone, arch_shadow,
        arch_stone, window
    );

    /* Right pillar */
    draw_city_building(
        gs,
        10.0f, 0.65f,
        15.0f, 0.8f,
        3.8f,
        arch_stone, arch_shadow,
        arch_stone, window
    );

    /* Top beam */
    draw_city_building(
        gs,
        6.5f, 4.15f,
        15.0f, 0.8f,
        0.65f,
        arch_stone, arch_shadow,
        arch_stone, window
    );

    /* Decorative top */
    draw_landmark_roof(
        gs,
        6.5f, 4.15f,
        15.0f, 0.8f,
        3.3f, 4.3f,
        arch_shadow
    );
}
            break;

        /* VENICE */
        case 1:
            draw_city_building(
                gs,-7.2f,3.2f,9.4f,2.0f,3.6f,
                facade,stone,roof,window
            );

            draw_city_building(
                gs,4.0f,3.0f,9.8f,2.0f,3.2f,
                facade,stone,roof,window
            );

            /* Canal */
            {
                Vec3 a={-2.0f,-1.43f,7.0f};
                Vec3 b={ 2.0f,-1.43f,7.0f};
                Vec3 c={ 2.0f,-1.43f,18.0f};
                Vec3 d={-2.0f,-1.43f,18.0f};

                u64 water=GS_SETREG_RGBAQ(
                    35,91,116,0x80,0
                );

                draw_triangle(gs,a,b,c,water);
                draw_triangle(gs,a,c,d,water);
            }
/* Venice Landmark - Campanile Tower */
{
    u64 tower_body = GS_SETREG_RGBAQ(
        155, 82, 55, 0x80, 0
    );

    u64 tower_side = GS_SETREG_RGBAQ(
        105, 55, 42, 0x80, 0
    );

    u64 tower_roof = GS_SETREG_RGBAQ(
        65, 105, 75, 0x80, 0
    );

    draw_city_building(
        gs,
        -6.0f, 2.4f,
        25.0f, 2.2f,
        7.0f,
        tower_body,
        tower_side,
        roof,
        window
    );

    draw_landmark_roof(
        gs,
        -6.0f, 2.4f,
        25.0f, 2.2f,
        5.5f, 8.0f,
        tower_roof
    );
}
/* Venice - Canal Side Balconies */
{
    u64 balcony = GS_SETREG_RGBAQ(
        185, 155, 115, 0x80, 0
    );

    u64 rail = GS_SETREG_RGBAQ(
        65, 48, 38, 0x80, 0
    );

    /* Left balcony */
    draw_rect(gs,-7.35f,0.10f,
        -3.85f,0.20f,9.15f,balcony);

    draw_rect(gs,-7.30f,0.55f,
        -3.90f,0.65f,9.12f,rail);

    for(int i=0;i<5;i++) {
        float x=-7.15f+i*0.75f;

        draw_rect(gs,x,0.20f,
            x+0.06f,0.55f,9.12f,rail);
    }

    /* Right balcony */
    draw_rect(gs,3.85f,0.10f,
        7.25f,0.20f,9.55f,balcony);

    draw_rect(gs,3.90f,0.55f,
        7.20f,0.65f,9.52f,rail);

    for(int i=0;i<5;i++) {
        float x=4.05f+i*0.70f;

        draw_rect(gs,x,0.20f,
            x+0.06f,0.55f,9.52f,rail);
    }
}
/* Venice V2 - Stone Canal Bridge */
{
    u64 bridge = GS_SETREG_RGBAQ(
        180, 155, 120, 0x80, 0
    );

    u64 bridge_dark = GS_SETREG_RGBAQ(
        105, 85, 65, 0x80, 0
    );

    /* Bridge deck */
    Vec3 a={-2.3f,-1.05f,12.0f};
    Vec3 b={ 2.3f,-1.05f,12.0f};
    Vec3 c={ 2.3f,-1.05f,13.5f};
    Vec3 d={-2.3f,-1.05f,13.5f};

    draw_triangle(gs,a,b,c,bridge);
    draw_triangle(gs,a,c,d,bridge);

    /* Left ramp */
    Vec3 l1={-2.3f,-1.5f,11.5f};
    Vec3 l2={-2.3f,-1.05f,12.0f};
    Vec3 l3={-2.3f,-1.5f,12.0f};

    draw_triangle(gs,l1,l2,l3,bridge_dark);

    /* Right ramp */
    Vec3 r1={2.3f,-1.5f,11.5f};
    Vec3 r2={2.3f,-1.05f,12.0f};
    Vec3 r3={2.3f,-1.5f,12.0f};

    draw_triangle(gs,r1,r2,r3,bridge_dark);

    /* Bridge railings */
    draw_rect(
        gs,-2.3f,-0.85f,
        2.3f,-0.72f,
        12.0f,bridge_dark
    );

    draw_rect(
        gs,-2.3f,-0.85f,
        2.3f,-0.72f,
        13.5f,bridge_dark
    );
}
            break;

        /* ROME */
        case 2:
            draw_city_building(
                gs,-7.0f,3.0f,11.0f,2.0f,3.2f,
                facade,stone,roof,window
            );

            draw_city_building(
                gs,4.0f,3.0f,11.0f,2.0f,3.2f,
                facade,stone,roof,window
            );

            /* Roman columns */
            {
                int i;

                for(i=0;i<5;i++) {
                    float cx=-2.0f+i*1.0f;

                    draw_city_building(
                        gs,cx,0.28f,
                        12.0f,0.45f,2.7f,
                        stone,stone,stone,window
                    );
                }
            }
/* Rome Landmark - Ancient Roman Temple */
{
    u64 temple_stone = GS_SETREG_RGBAQ(
        190, 174, 140, 0x80, 0
    );

    u64 temple_shadow = GS_SETREG_RGBAQ(
        125, 108, 85, 0x80, 0
    );

    u64 temple_roof = GS_SETREG_RGBAQ(
        145, 105, 65, 0x80, 0
    );

    /* Main temple */
    draw_city_building(
        gs,
        -2.4f, 4.8f,
        26.0f, 3.5f,
        5.0f,
        temple_stone,
        temple_shadow,
        temple_roof,
        window
    );

    /* Front columns */
    for(int i=0; i<6; i++) {
        float cx = -2.0f + i * 0.8f;

        draw_city_building(
            gs,
            cx, 0.30f,
            25.65f, 0.55f,
            4.5f,
            temple_stone,
            temple_shadow,
            temple_stone,
            window
        );
    }

    /* Temple roof */
    draw_landmark_roof(
        gs,
        -2.6f, 5.2f,
        25.7f, 3.8f,
        3.5f, 5.8f,
        temple_roof
    );
}
/* Rome - Classical Column Capitals */
{
    u64 capital = GS_SETREG_RGBAQ(
        220, 205, 170, 0x80, 0
    );

    for(int i=0;i<6;i++) {
        float cx=-2.0f+i*0.8f;

        draw_rect(
            gs,
            cx-0.22f,1.05f,
            cx+0.22f,1.22f,
            25.58f,
            capital
        );
    }

    /* Temple front pediment */
    Vec3 a={-2.6f,3.45f,25.60f};
    Vec3 b={ 2.6f,3.45f,25.60f};
    Vec3 c={ 0.0f,5.30f,25.60f};

    draw_triangle(
        gs,a,b,c,
        GS_SETREG_RGBAQ(165,125,80,0x80,0)
    );
}
/* Rome V2 - Triumphal Arch */
{
    u64 marble = GS_SETREG_RGBAQ(
        190, 175, 145, 0x80, 0
    );

    u64 marble_dark = GS_SETREG_RGBAQ(
        125, 110, 85, 0x80, 0
    );

    /* Left tower */
    draw_city_building(
        gs,
        6.0f, 0.85f,
        18.0f, 1.0f,
        3.6f,
        marble, marble_dark,
        marble, window
    );

    /* Right tower */
    draw_city_building(
        gs,
        10.0f, 0.85f,
        18.0f, 1.0f,
        3.6f,
        marble, marble_dark,
        marble, window
    );

    /* Upper arch beam */
    draw_city_building(
        gs,
        6.0f, 4.85f,
        18.0f, 1.0f,
        0.8f,
        marble, marble_dark,
        marble, window
    );

    /* Top decoration */
    draw_landmark_roof(
        gs,
        6.0f, 4.85f,
        18.0f, 1.0f,
        2.9f, 4.1f,
        marble_dark
    );

    /* Decorative horizontal line */
    draw_rect(
        gs,6.0f,2.75f,
        10.85f,2.90f,
        17.97f,marble_dark
    );
}
            break;

        /* ENGLAND */
        default:
            draw_city_building(
                gs,-7.0f,3.4f,10.5f,2.0f,2.8f,
                facade,stone,roof,window
            );

            draw_city_building(
                gs,3.8f,3.4f,10.5f,2.0f,2.8f,
                facade,stone,roof,window
            );

            draw_city_building(
                gs,-2.0f,4.0f,15.0f,2.0f,3.2f,
                facade,stone,roof,window
            );
/* England Landmark - Medieval Stone Keep */
{
    u64 keep_body = GS_SETREG_RGBAQ(
        125, 118, 102, 0x80, 0
    );

    u64 keep_side = GS_SETREG_RGBAQ(
        82, 78, 70, 0x80, 0
    );

    u64 keep_roof = GS_SETREG_RGBAQ(
        75, 65, 55, 0x80, 0
    );

    /* Main castle tower */
    draw_city_building(
        gs,
        5.0f, 3.4f,
        26.0f, 3.0f,
        6.5f,
        keep_body,
        keep_side,
        keep_roof,
        window
    );

    /* Castle roof */
    draw_landmark_roof(
        gs,
        5.0f, 3.4f,
        26.0f, 3.0f,
        5.0f, 7.0f,
        keep_roof
    );

    /* Side defensive towers */
    draw_city_building(
        gs,
        4.0f, 0.8f,
        25.5f, 2.0f,
        5.0f,
        keep_body,
        keep_side,
        keep_roof,
        window
    );

    draw_city_building(
        gs,
        8.6f, 0.8f,
        25.5f, 2.0f,
        5.0f,
        keep_body,
        keep_side,
        keep_roof,
        window
    );
}
/* England - Medieval Timber Frames */
{
    u64 timber = GS_SETREG_RGBAQ(
        65, 42, 28, 0x80, 0
    );

    /* Left house */
    draw_rect(gs,-6.85f,-1.35f,
        -6.65f,1.15f,10.47f,timber);

    draw_rect(gs,-5.25f,-1.35f,
        -5.05f,1.15f,10.47f,timber);

    draw_rect(gs,-3.75f,-1.35f,
        -3.60f,1.15f,10.47f,timber);

    draw_rect(gs,-7.0f,-0.35f,
        -3.6f,-0.20f,10.46f,timber);

    draw_rect(gs,-7.0f,0.75f,
        -3.6f,0.90f,10.46f,timber);

    /* Right house */
    draw_rect(gs,4.0f,-1.35f,
        4.18f,1.15f,10.47f,timber);

    draw_rect(gs,5.55f,-1.35f,
        5.73f,1.15f,10.47f,timber);

    draw_rect(gs,7.0f,-1.35f,
        7.15f,1.15f,10.47f,timber);

    draw_rect(gs,3.8f,-0.35f,
        7.2f,-0.20f,10.46f,timber);

    draw_rect(gs,3.8f,0.75f,
        7.2f,0.90f,10.46f,timber);
}
/* England V2 - Medieval Castle Gate */
{
    u64 castle = GS_SETREG_RGBAQ(
        135, 128, 110, 0x80, 0
    );

    u64 castle_dark = GS_SETREG_RGBAQ(
        75, 72, 65, 0x80, 0
    );

    u64 gate = GS_SETREG_RGBAQ(
        42, 32, 25, 0x80, 0
    );

    /* Left gate tower */
    draw_city_building(
        gs,
        6.0f, 1.1f,
        18.0f, 1.5f,
        4.5f,
        castle, castle_dark,
        castle_dark, window
    );

    /* Right gate tower */
    draw_city_building(
        gs,
        10.0f, 1.1f,
        18.0f, 1.5f,
        4.5f,
        castle, castle_dark,
        castle_dark, window
    );

    /* Castle wall above gate */
    draw_city_building(
        gs,
        6.0f, 5.1f,
        18.0f, 1.5f,
        0.9f,
        castle, castle_dark,
        castle_dark, window
    );

    /* Dark gate entrance */
    draw_rect(
        gs,
        7.1f,-1.4f,
        10.0f,1.4f,
        17.97f,
        gate
    );

    /* Battlements */
    for(int i=0;i<5;i++) {
        float x=6.0f+i*1.2f;

        draw_rect(
            gs,
            x,3.9f,
            x+0.55f,4.5f,
            17.95f,
            castle_dark
        );
    }
}
            break;
    }
}

/* =========================================
   CITY MAP V3 - ROOFTOP HEIGHT
   ========================================= */

static float city_roof_height(float x, float z)
{
    if(x >= -6.2f && x <= -3.7f &&
       z >= 7.0f && z <= 9.0f)
        return 0.90f;

    if(x >= 3.7f && x <= 6.2f &&
       z >= 7.0f && z <= 9.0f)
        return 0.90f;

    return 0.0f;
}

/* =========================================
   BUILDING COLLISION V1
   ========================================= */

typedef struct {
    int city;
    float x;
    float width;
    float front;
    float depth;
    float height;
} CityCollisionBox;

static const CityCollisionBox city_collision_boxes[] = {
    /* PARIS */
    {0,-7.0f,2.7f,11.0f,1.6f,3.0f},
    {0, 4.1f,2.8f,10.2f,1.8f,3.4f},
    {0,-1.0f,2.0f,15.0f,1.4f,4.2f},
    {0,-1.2f,2.4f,24.0f,2.2f,6.0f},
    {0, 6.5f,0.65f,15.0f,0.8f,3.8f},
    {0,10.0f,0.65f,15.0f,0.8f,3.8f},

    /* VENICE */
    {1,-7.2f,3.2f,9.4f,2.0f,3.6f},
    {1, 4.0f,3.0f,9.8f,2.0f,3.2f},
    {1,-6.0f,2.4f,25.0f,2.2f,7.0f},

    /* ROME */
    {2,-7.0f,3.0f,11.0f,2.0f,3.2f},
    {2, 4.0f,3.0f,11.0f,2.0f,3.2f},
    {2,-2.4f,4.8f,26.0f,3.5f,5.0f},
    {2, 6.0f,0.85f,18.0f,1.0f,3.6f},
    {2,10.0f,0.85f,18.0f,1.0f,3.6f},

    /* ENGLAND */
    {3,-7.0f,3.4f,10.5f,2.0f,2.8f},
    {3, 3.8f,3.4f,10.5f,2.0f,2.8f},
    {3,-2.0f,4.0f,15.0f,2.0f,3.2f},
    {3, 5.0f,3.4f,26.0f,3.0f,6.5f},
    {3, 4.0f,0.8f,25.5f,2.0f,5.0f},
    {3, 8.6f,0.8f,25.5f,2.0f,5.0f}
};

/* Building Doorway Collision V2 */
static int city_doorway_open(
    float player_x,
    float building_x,
    float building_width
) {
    /* Small pillars do not have doors */
    if(building_width < 1.5f)
        return 0;

    float door_center =
        building_x + building_width * 0.5f;

    /* Allow passage through the doorway corridor */
    if(fabsf(player_x - door_center) < 0.23f)
        return 1;

    return 0;
}

static void resolve_city_collision(
    float old_x,
    float old_z,
    float *next_x,
    float *next_z
) {
    const float margin = 0.22f;
    int city = world_city_index();

    /* 32 district buildings */
    for(int i=0; i<32; i++) {
        const CityBuildingData *b = &city_buildings[i];

        if(b->city != city)
            continue;
        if(city_doorway_open(
    *next_x,
    b->x,
    b->width
))
    continue;

        if(player_y >= b->height - 0.15f)
            continue;

        float left = b->x - margin;
        float right = b->x + b->width + margin;
        float front = b->front - margin;
        float back = b->front + b->depth + margin;

        if(old_z > front && old_z < back &&
           *next_x > left && *next_x < right)
            *next_x = old_x;

        if(*next_x > left && *next_x < right &&
           *next_z > front && *next_z < back)
            *next_z = old_z;
    }

    /* Main buildings and landmarks */
    for(int i=0;
        i<(int)(sizeof(city_collision_boxes) /
                sizeof(city_collision_boxes[0]));
        i++) {

        const CityCollisionBox *b =
            &city_collision_boxes[i];

        if(b->city != city)
            continue;
        if(city_doorway_open(
    *next_x,
    b->x,
    b->width
))
    continue;

        if(player_y >= b->height - 0.15f)
            continue;

        float left = b->x - margin;
        float right = b->x + b->width + margin;
        float front = b->front - margin;
        float back = b->front + b->depth + margin;

        if(old_z > front && old_z < back &&
           *next_x > left && *next_x < right)
            *next_x = old_x;

        if(*next_x > left && *next_x < right &&
           *next_z > front && *next_z < back)
            *next_z = old_z;
    }

    /* Rooftop buildings */
    if(player_y < 0.72f) {
        const float left_edges[2] = {-6.2f, 3.7f};
        const float right_edges[2] = {-3.7f, 6.2f};

        for(int i=0; i<2; i++) {
            float left = left_edges[i] - margin;
            float right = right_edges[i] + margin;
            float front = 7.0f - margin;
            float back = 9.0f + margin;

            if(old_z > front && old_z < back &&
               *next_x > left && *next_x < right)
                *next_x = old_x;

            if(*next_x > left && *next_x < right &&
               *next_z > front && *next_z < back)
                *next_z = old_z;
        }
    }
}

/* Camera Collision V1 */
static void update_camera_distance(void)
{
    float target_distance = 8.0f;
    float sn = sinf(camera_yaw);
    float cs = cosf(camera_yaw);

    int city = world_city_index();

    for(float d = 0.5f; d <= 8.0f; d += 0.25f)
    {
        float cx = player_x - sn * d;
        float cz = player_z - cs * d;
        int blocked = 0;

        /* District buildings */
        for(int i = 0; i < 32; i++)
        {
            const CityBuildingData *b = &city_buildings[i];

            if(b->city != city)
                continue;

            float top = -1.5f + b->height;

            if(player_y + CAMERA_HEIGHT > -1.5f &&
               player_y + CAMERA_HEIGHT < top &&
               cx > b->x - 0.15f &&
               cx < b->x + b->width + 0.15f &&
               cz > b->front - 0.15f &&
               cz < b->front + b->depth + 0.15f)
            {
                blocked = 1;
                break;
            }
        }

        /* Main buildings */
        if(!blocked)
        {
            int count = sizeof(city_collision_boxes) /
                        sizeof(city_collision_boxes[0]);

            for(int i = 0; i < count; i++)
            {
                const CityCollisionBox *b =
                    &city_collision_boxes[i];

                if(b->city != city)
                    continue;

                float top = -1.5f + b->height;

                if(player_y + CAMERA_HEIGHT > -1.5f &&
                   player_y + CAMERA_HEIGHT < top &&
                   cx > b->x - 0.15f &&
                   cx < b->x + b->width + 0.15f &&
                   cz > b->front - 0.15f &&
                   cz < b->front + b->depth + 0.15f)
                {
                    blocked = 1;
                    break;
                }
            }
        }

        /* Parkour walls */
        if(!blocked &&
           player_y + CAMERA_HEIGHT < -1.5f + WALL_HEIGHT &&
           cx > WALL_LEFT && cx < WALL_RIGHT &&
           cz > WALL_FRONT && cz < WALL_BACK)
        {
            blocked = 1;
        }

        if(!blocked &&
           player_y + CAMERA_HEIGHT < -1.5f + WALL2_HEIGHT &&
           cx > WALL2_LEFT && cx < WALL2_RIGHT &&
           cz > WALL2_FRONT && cz < WALL2_BACK)
        {
            blocked = 1;
        }

        if(blocked)
        {
            target_distance = d - 0.35f;

            if(target_distance < 2.0f)
                target_distance = 2.0f;

            break;
        }
    }

    camera_distance +=
        (target_distance - camera_distance) * 0.18f;
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
    /* V4 - L-shaped side wall */
    draw_parkour_wall_at(
        gs,
        2.5f, 2.8f,
        WALL_FRONT,
        WALL_FRONT + 1.8f,
        WALL_HEIGHT,
        GS_SETREG_RGBAQ(125,100,75,0x80,0)
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


/* =========================================
   MUEANG ISARA - LOW POLY 3D MODEL V1
   ========================================= */

static void draw_player_part(
    GSGLOBAL *gs,
    float cx, float cy, float cz,
    float hx, float hy, float hz,
    float angle,
    float pivot_x, float pivot_y,
    u64 front_color,
    u64 side_color,
    u64 top_color
) {
    float cs = cosf(player_yaw);
    float sn = sinf(player_yaw);

    float ac = cosf(angle);
    float as = sinf(angle);

    Vec3 v[8];

    const float sx[8] = {
        -1, 1, 1, -1, -1, 1, 1, -1
    };

    const float sy[8] = {
        -1, -1, 1, 1, -1, -1, 1, 1
    };

    const float sz[8] = {
        -1, -1, -1, -1, 1, 1, 1, 1
    };

    for(int i=0; i<8; i++) {
        float lx = cx + sx[i]*hx + body_sway;
        float ly = cy + sy[i]*hy;

        float rx = lx - pivot_x;
        float ry = ly - pivot_y;

        float tx = rx*ac - ry*as + pivot_x;
        float ty = rx*as + ry*ac + pivot_y;

        float local_z = cz + sz[i]*hz;

v[i].x = player_x + tx*cs + local_z*sn;
v[i].y = player_y + ty + body_bob;
v[i].z = player_z - tx*sn + local_z*cs;
    }

    /* Front */
    draw_triangle(gs,v[4],v[5],v[6],front_color);
    draw_triangle(gs,v[4],v[6],v[7],front_color);

    /* Back */
    draw_triangle(gs,v[0],v[2],v[1],side_color);
    draw_triangle(gs,v[0],v[3],v[2],side_color);

    /* Left */
    draw_triangle(gs,v[0],v[4],v[7],side_color);
    draw_triangle(gs,v[0],v[7],v[3],side_color);

    /* Right */
    draw_triangle(gs,v[1],v[2],v[6],side_color);
    draw_triangle(gs,v[1],v[6],v[5],side_color);

    /* Top */
    draw_triangle(gs,v[3],v[7],v[6],top_color);
    draw_triangle(gs,v[3],v[6],v[2],top_color);

    /* Bottom */
    draw_triangle(gs,v[0],v[1],v[5],side_color);
    draw_triangle(gs,v[0],v[5],v[4],side_color);
}


/* =========================================
   PLAYER MODEL V1
   ========================================= */

/* Cinematic Shadows V1 - Player Ground Shadow */
static void draw_player_shadow(GSGLOBAL *gs)
{
    float shadow_y = -1.51f;

    float radius_x = 0.42f;
    float radius_z = 0.24f;

    int segments = 12;

    u64 shadow_color =
        GS_SETREG_RGBAQ(25, 22, 20, 0x80, 0);

    Vec3 center = {
        player_x,
        shadow_y,
        player_z
    };

    for (int i = 0; i < segments; i++) {
        float a1 = (float)i * 6.2831853f / segments;
        float a2 = (float)(i + 1) * 6.2831853f / segments;

        Vec3 p1 = {
            player_x + cosf(a1) * radius_x,
            shadow_y,
            player_z + sinf(a1) * radius_z
        };

        Vec3 p2 = {
            player_x + cosf(a2) * radius_x,
            shadow_y,
            player_z + sinf(a2) * radius_z
        };

        draw_triangle(
            gs,
            center,
            p1,
            p2,
            shadow_color
        );
    }
}

static void draw_player(GSGLOBAL *gs)
{
    const u64 coat =
        GS_SETREG_RGBAQ(30,42,65,0x80,0);

    const u64 coat_side =
        GS_SETREG_RGBAQ(19,27,42,0x80,0);

    const u64 coat_top =
        GS_SETREG_RGBAQ(65,82,110,0x80,0);

    const u64 trousers =
        GS_SETREG_RGBAQ(35,40,52,0x80,0);

    const u64 boots =
        GS_SETREG_RGBAQ(75,42,25,0x80,0);

    const u64 skin =
        GS_SETREG_RGBAQ(190,135,95,0x80,0);

    const u64 hood =
        GS_SETREG_RGBAQ(22,27,38,0x80,0);

/* Character Model V3 - Dynamic Cloak */

const u64 cloak =
    GS_SETREG_RGBAQ(28,31,43,0x80,0);

const u64 cloak_side =
    GS_SETREG_RGBAQ(16,19,28,0x80,0);

const u64 cloak_top =
    GS_SETREG_RGBAQ(52,45,48,0x80,0);

const u64 cloak_edge =
    GS_SETREG_RGBAQ(110,55,38,0x80,0);

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

/* Dynamic cloak movement */

float cloak_swing =
    sinf(walk_time * 0.7f) *
    (is_running ? 0.16f : 0.08f);

if(is_jumping)
    cloak_swing -= 0.12f;

if(is_hanging || is_climbing)
    cloak_swing = 0.04f;

/* Main cloak - wider and longer */

draw_player_part(
    gs,
    /* Main cloak */
0.0f,-0.22f,-0.49f,
0.52f,0.75f,0.055f,
    cloak_swing,
    0.0f,0.45f,
    cloak,cloak_side,cloak_top
);

/* Cloak lower edge */

draw_player_part(
    gs,
    /* Cloak lower edge */
0.0f,-0.91f,-0.52f,
0.52f,0.06f,0.065f,
    cloak_swing,
    0.0f,0.45f,
    cloak_edge,cloak_side,cloak_edge
);

    /* Left leg */
    draw_player_part(
        gs,-0.18f,-0.78f,0.0f,
        0.13f,0.34f,0.15f,
        leg_swing,
        -0.18f,-0.46f,
        trousers,trousers,trousers
    );

    /* Right leg */
    draw_player_part(
        gs,0.18f,-0.78f,0.0f,
        0.13f,0.34f,0.15f,
        -leg_swing,
        0.18f,-0.46f,
        trousers,trousers,trousers
    );

    /* Left boot */
    draw_player_part(
        gs,-0.18f,-1.27f,0.08f,
        0.15f,0.14f,0.23f,
        leg_swing,
        -0.18f,-0.46f,
        boots,boots,boots
    );

    /* Right boot */
    draw_player_part(
        gs,0.18f,-1.27f,0.08f,
        0.15f,0.14f,0.23f,
        -leg_swing,
        0.18f,-0.46f,
        boots,boots,boots
    );

    /* Left arm */
    draw_player_part(
        gs,-0.48f,0.02f,0.0f,
        0.12f,0.34f,0.14f,
        arm_swing,
        -0.40f,0.38f,
        coat,coat_side,coat_top
    );

    /* Right arm */
    draw_player_part(
        gs,0.48f,0.02f,0.0f,
        0.12f,0.34f,0.14f,
        -arm_swing,
        0.40f,0.38f,
        coat,coat_side,coat_top
    );

    /* Main torso */
    draw_player_part(
        gs,0.0f,-0.03f,0.0f,
        0.40f,0.55f,0.22f,
        0.0f,0.0f,0.0f,
        coat,coat_side,coat_top
    );


    /* =================================
       CHARACTER MODEL V2 - EQUIPMENT
       ================================= */

    


    /* Waist */
    draw_player_part(
        gs,0.0f,-0.58f,0.0f,
        0.34f,0.16f,0.23f,
        0.0f,0.0f,0.0f,
        boots,coat_side,coat_top
    );


    /* Character Equipment V2 - Camera Facing */

    const u64 leather =
        GS_SETREG_RGBAQ(105,65,32,0x80,0);

    const u64 leather_dark =
        GS_SETREG_RGBAQ(55,32,20,0x80,0);

    const u64 metal =
        GS_SETREG_RGBAQ(190,170,125,0x80,0);

    /* Waist belt - visible front */
    draw_player_part(
        gs,0.0f,-0.53f,-0.245f,
        0.37f,0.075f,0.035f,
        0.0f,0.0f,0.0f,
        leather,leather_dark,leather
    );

    /* Belt buckle */
    draw_player_part(
        gs,0.0f,-0.53f,-0.285f,
        0.085f,0.065f,0.018f,
        0.0f,0.0f,0.0f,
        metal,leather_dark,metal
    );

    /* Chest strap */
    draw_player_part(
        gs,-0.12f,0.02f,-0.245f,
        0.055f,0.46f,0.035f,
        -0.24f,0.0f,0.0f,
        leather,leather_dark,leather
    );

    /* Left shoulder armor */
    draw_player_part(
        gs,-0.43f,0.34f,-0.15f,
        0.19f,0.14f,0.12f,
        0.0f,0.0f,0.0f,
        metal,leather_dark,metal
    );

    /* Right shoulder armor */
    draw_player_part(
        gs,0.43f,0.34f,-0.15f,
        0.19f,0.14f,0.12f,
        0.0f,0.0f,0.0f,
        metal,leather_dark,metal
    );


    /* Neck */
    draw_player_part(
        gs,0.0f,0.60f,0.0f,
        0.13f,0.15f,0.13f,
        0.0f,0.0f,0.0f,
        skin,skin,skin
    );

    /* Head */
    draw_player_part(
        gs,0.0f,0.88f,0.0f,
        0.21f,0.25f,0.19f,
        0.0f,0.0f,0.0f,
        skin,skin,skin
    );

    /* Hood */
    draw_player_part(
        gs,0.0f,1.15f,-0.025f,
        0.25f,0.18f,0.23f,
        0.0f,0.0f,0.0f,
        hood,coat_side,coat_top
    );
}


/* Controller and movement */
static void update_player(void)
{
    float move_x=0.0f;
    float move_z=0.0f;

    update_camera_distance();

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
        /* SELECT = next city, START = next historical era */
    int select_down = !(pad.btns & PAD_SELECT);
    int start_down = !(pad.btns & PAD_START);

    if(select_down && !select_prev)
        world_next_city();

    if(start_down && !start_prev)
        world_next_era();

    select_prev = select_down;
    start_prev = start_down;

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

            /* Right Analog - Camera */
int rx = (int)pad.rjoy_h;
int ry = (int)pad.rjoy_v;

if(rx < 118) {
    camera_yaw -= 0.035f;
    camera_manual = 1;
}
else if(rx > 138) {
    camera_yaw += 0.035f;
    camera_manual = 1;
}

if(ry < 118) {
    camera_pitch -= 0.015f;
    camera_manual = 1;
}
else if(ry > 138) {
    camera_pitch += 0.015f;
    camera_manual = 1;
}

if(camera_pitch < -0.35f)
    camera_pitch = -0.35f;

if(camera_pitch > 0.65f)
    camera_pitch = 0.65f;

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
    hanging_on_side = 0;
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

/* Hanging: L-shaped corner traversal */
if(is_hanging) {
    player_y = -0.30f + sinf(walk_time * 2.0f) * 0.025f;

    if(!hanging_on_side) {
        /* Main wall */
        player_z = active_wall_front - 0.12f;
        player_yaw = 0.0f;

        if(move_x < -0.1f)
            player_x -= 0.035f;
        else if(move_x > 0.1f)
            player_x += 0.035f;

        if(player_x < WALL_LEFT + 0.35f)
            player_x = WALL_LEFT + 0.35f;

        /* Turn around right corner */
        if(player_x >= WALL_RIGHT - 0.35f &&
           move_x > 0.1f &&
           active_wall == 0) {

            hanging_on_side = 1;
            player_x = 2.92f;
            player_z = WALL_FRONT + 0.42f;
        }
    }
    else {
        /* Side wall */
        player_x = 2.92f;
        player_yaw = -PI * 0.5f;

        if(move_z > 0.1f)
            player_z += 0.035f;
        else if(move_z < -0.1f)
            player_z -= 0.035f;

        if(player_z > WALL_FRONT + 1.45f)
            player_z = WALL_FRONT + 1.45f;

        /* Return around the corner */
        if(player_z <= WALL_FRONT + 0.42f &&
           move_z < -0.1f) {

            hanging_on_side = 0;
            player_x = WALL_RIGHT - 0.35f;
            player_z = WALL_FRONT - 0.12f;
        }
    }

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

    /* Camera-relative movement */
    float input_x = move_x;
    float input_z = move_z;

    float cs = cosf(camera_yaw);
    float sn = sinf(camera_yaw);

    move_x = input_x * cs + input_z * sn;
    move_z = -input_x * sn + input_z * cs;

    /* Normalize movement */
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


        /* City Building Collision V1 */
resolve_city_collision(
    player_x,
    player_z,
    &next_x,
    &next_z 
);

player_x=next_x;
player_z=next_z;

        player_yaw=atan2f(move_x,move_z);

        if(!camera_manual) {
    float yaw_diff=atan2f(
        sinf(player_yaw-camera_yaw),
        cosf(player_yaw-camera_yaw)
    );

    camera_yaw+=yaw_diff*0.12f;
}

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

    /* Jump, rooftop landing and falling */
float target_ground =
    city_roof_height(player_x,player_z);

/* Walk off rooftop */
if(!is_jumping &&
   player_y > target_ground + 0.05f) {
    is_jumping = 1;
    jump_velocity = 0.0f;
}

/* Jump */
if(jump_pressed && !is_jumping) {
    is_jumping = 1;
    jump_velocity = 0.18f;
}

/* Gravity and landing */
if(is_jumping) {
    player_y += jump_velocity;
    jump_velocity -= 0.012f;

    if(player_y <= target_ground &&
       jump_velocity < 0.0f) {
        player_y = target_ground;
        jump_velocity = 0.0f;
        is_jumping = 0;
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

    /* Expanded City World Bounds */
if(player_x < -28.0f)
    player_x = -28.0f;

if(player_x > 28.0f)
    player_x = 28.0f;

if(player_z < -8.0f)
    player_z = -8.0f;

if(player_z > 48.0f)
    player_z = 48.0f;
}

/* Cube */
static void draw_cube(
    GSGLOBAL *gs,
    float angle
) {
    float sx[8],sy[8];
int sz[8];
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
    p,&sx[i],&sy[i],&sz[i]
);
    }

    for(int i=0;i<12;i++) {
        int a=edges[i][0];
        int b=edges[i][1];

        if(visible[a] && visible[b]) {
            gsKit_prim_line_3d(
    gs,
    sx[a],sy[a],sz[a],
    sx[b],sy[b],sz[b],
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
gs->PSMZ=GS_PSMZ_32;
gs->ZBuffering=GS_SETTING_ON;

    gsKit_init_screen(gs);
    gsKit_mode_switch(gs,GS_PERSISTENT);

    float angle=0.0f;

while(1) {
    update_player();

    /* Sky color changes with selected era */
    const WorldEra *era = world_current_era();

    const u64 sky = GS_SETREG_RGBAQ(
        era->sky_r,
        era->sky_g,
        era->sky_b,
        0x80, 0
    );

    gsKit_clear(gs,sky);

    /* Cinematic Sky */
    draw_cinematic_sky(gs,era);

    draw_ground(gs);
draw_player_shadow(gs);
draw_city_map(gs);
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
