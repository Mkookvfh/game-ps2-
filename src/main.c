/*
 * Mueang Isara - PS2 Slim prototype
 * GSKit low-poly 3D scene, hooded player, follow camera, pad controls,
 * loading screen and start menu.  Prototype geometry is procedural.
 *
 * Fixes in this version:
 *  - load the IOP pad modules (SIO2MAN / PADMAN) before padInit, so the
 *    program no longer hangs on a black screen
 *  - init the screen first and draw one frame before touching the pad
 *  - pad buttons are active-low, so flip them before use
 *  - START now works even when the player is standing still
 */
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <libpad.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PI 3.14159265358979323846f
#define SCREEN_W 640.0f
#define SCREEN_H 448.0f

static GSGLOBAL *gsGlobal;
static char padBuf[256] __attribute__((aligned(64)));

static struct padButtonStatus padButtons;
static int padReady = 0;

typedef struct { float x, y, z; } Vec3;
typedef struct { unsigned char r, g, b; } Color;

static Vec3 player = {0.0f, 0.0f, 0.0f};
static float playerYaw = 0.0f;
static float cameraYaw = 0.0f;
static int menuState = 0; /* 0 loading, 1 title, 2 game */
static int selectedMenu = 0;
static unsigned int frameNo = 0;

static u64 rgba(Color c) {
    return GS_SETREG_RGBAQ(c.r, c.g, c.b, 0x80, 0);
}

static void rect(float x, float y, float w, float h, Color c) {
    gsKit_prim_sprite(gsGlobal, x, y, x + w, y + h, 1, rgba(c));
}

static void pixel_char(float x, float y, char ch, float scale, Color c) {
    /* Compact 5x7 font for title/menu labels. */
    static const unsigned char glyph[37][7] = {
        {14,17,19,21,25,17,14}, /* 0 */
        {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
        {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2},
        {31,16,16,30,1,1,30}, {6,8,16,30,17,17,14},
        {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
        {14,17,17,15,1,2,12},
        {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
        {14,17,16,16,16,17,14}, {28,18,17,17,17,18,28},
        {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
        {14,4,4,4,4,4,14}, {7,2,2,2,18,18,12},
        {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
        {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
        {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
        {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
        {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
        {0,0,0,0,0,0,0}
    };
    int index = 36;
    if (ch >= '0' && ch <= '9') index = ch - '0';
    else if (ch >= 'A' && ch <= 'Z') index = 10 + ch - 'A';
    else if (ch >= 'a' && ch <= 'z') index = 10 + ch - 'a';
    else if (ch == '-') index = 36;
    for (int row = 0; row < 7; ++row)
        for (int col = 0; col < 5; ++col)
            if (glyph[index][row] & (1 << (4 - col)))
                rect(x + col * scale, y + row * scale, scale, scale, c);
}

static void text_draw(float x, float y, const char *s, float scale, Color c) {
    while (*s) {
        if (*s == ' ') x += 4.0f * scale;
        else { pixel_char(x, y, *s, scale, c); x += 6.0f * scale; }
        ++s;
    }
}

static Vec3 rotate_y(Vec3 p, float a) {
    Vec3 r;
    float cs = cosf(a), sn = sinf(a);
    r.x = p.x * cs - p.z * sn;
    r.y = p.y;
    r.z = p.x * sn + p.z * cs;
    return r;
}

static int project(Vec3 p, float *sx, float *sy, int *z) {
    /* Camera sits behind and above the player, looking toward the horizon. */
    Vec3 rel = {p.x - player.x, p.y - 1.5f, p.z - (player.z - 6.0f)};
    rel = rotate_y(rel, -cameraYaw);
    rel.z += 5.0f;
    if (rel.z < 0.25f) return 0;
    *sx = SCREEN_W * 0.5f + (rel.x * 300.0f / rel.z);
    *sy = SCREEN_H * 0.48f - ((rel.y - 1.0f) * 300.0f / rel.z);
    *z = 0xFFFFFF - (int)(rel.z * 1000.0f);
    if (*z < 1) *z = 1;
    return 1;
}

static void triangle3(Vec3 a, Vec3 b, Vec3 c, Color col) {
    float x1,y1,x2,y2,x3,y3; int z1,z2,z3;
    if (!project(a,&x1,&y1,&z1) || !project(b,&x2,&y2,&z2) || !project(c,&x3,&y3,&z3)) return;
    gsKit_prim_triangle_3d(gsGlobal,x1,y1,z1,x2,y2,z2,x3,y3,z3,rgba(col));
}

static void quad3(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Color col) {
    triangle3(a,b,c,col); triangle3(a,c,d,col);
}

static void cube3(Vec3 center, Vec3 half, Color front, Color side, Color top) {
    Vec3 v[8];
    for (int i=0;i<8;i++) {
        v[i].x = center.x + ((i&1)?half.x:-half.x);
        v[i].y = center.y + ((i&2)?half.y:-half.y);
        v[i].z = center.z + ((i&4)?half.z:-half.z);
    }
    quad3(v[0],v[1],v[3],v[2],side);
    quad3(v[4],v[6],v[7],v[5],front);
    quad3(v[0],v[4],v[5],v[1],front);
    quad3(v[2],v[3],v[7],v[6],side);
    quad3(v[0],v[2],v[6],v[4],top);
    quad3(v[1],v[5],v[7],v[3],side);
}

static void draw_world(void) {
    Color grid = {65,95,75};
    Color black = {12,15,22}, dark = {24,29,39}, hood = {7,9,14};
    Color skin = {150,112,83}, shoe = {9,10,12};
    gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(18,27,42,0x80,0));
    /* Flat ground grid receding into the distance. */
    for (int i=-8;i<=8;i++) {
        Vec3 a={(float)i*2,0,-12}, b={(float)i*2,0,18};
        Vec3 c={-16,0,(float)i*2}, d={16,0,(float)i*2};
        float x1,y1,x2,y2; int z1,z2;
        if(project(a,&x1,&y1,&z1)&&project(b,&x2,&y2,&z2))
            gsKit_prim_line_3d(gsGlobal,x1,y1,z1,x2,y2,z2,rgba(grid));
        if(project(c,&x1,&y1,&z1)&&project(d,&x2,&y2,&z2))
            gsKit_prim_line_3d(gsGlobal,x1,y1,z1,x2,y2,z2,rgba(grid));
    }
    /* Stylized low-poly hooded character. */
    cube3((Vec3){player.x,0.88f,player.z},(Vec3){0.42f,0.58f,0.25f},black,dark,black); /* torso */
    cube3((Vec3){player.x,1.66f,player.z},(Vec3){0.34f,0.34f,0.32f},hood,dark,black); /* hood/head */
    cube3((Vec3){player.x,1.58f,player.z+0.29f},(Vec3){0.17f,0.10f,0.035f},skin,skin,skin); /* face opening */
    cube3((Vec3){player.x-0.22f,0.22f,player.z},(Vec3){0.16f,0.25f,0.18f},dark,black,black);
    cube3((Vec3){player.x+0.22f,0.22f,player.z},(Vec3){0.16f,0.25f,0.18f},dark,black,black);
    cube3((Vec3){player.x-0.22f,-0.02f,player.z+0.03f},(Vec3){0.18f,0.08f,0.28f},shoe,shoe,shoe);
    cube3((Vec3){player.x+0.22f,-0.02f,player.z+0.03f},(Vec3){0.18f,0.08f,0.28f},shoe,shoe,shoe);
    /* simple distant buildings */
    for (int i=-2;i<=2;i++) {
        if(i==0) continue;
        cube3((Vec3){i*4.0f,1.2f,player.z+12.0f},(Vec3){1.2f,1.2f,1.0f},
              (Color){55,62,76},(Color){35,42,56},(Color){78,85,98});
    }
}

static void draw_loading(void) {
    gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(5,8,16,0x80,0));
    text_draw(190,170,"MUEANG ISARA",3,(Color){225,232,242});
    text_draw(245,220,"LOADING",2,(Color){120,190,255});
    rect(150,260,340,8,(Color){40,50,68});
    rect(150,260,(float)((frameNo%90)+1)*3.77f,8,(Color){70,170,235});
}

static void draw_title(void) {
    gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(5,8,16,0x80,0));
    /* Background panels and title composition. */
    rect(0,0,640,448,(Color){8,13,24});
    rect(42,52,556,3,(Color){45,93,130});
    rect(42,392,556,2,(Color){45,93,130});
    text_draw(170,105,"MUEANG ISARA",3,(Color){225,235,248});
    text_draw(216,153,"PS2 OPEN WORLD",1,(Color){105,164,205});
    rect(210,230,220,34,selectedMenu==0?(Color){35,100,150}:(Color){24,35,51});
    rect(210,276,220,34,selectedMenu==1?(Color){35,100,150}:(Color){24,35,51});
    text_draw(275,242,"START",2,(Color){245,248,255});
    text_draw(275,288,"QUIT",2,(Color){190,200,215});
    text_draw(175,350,"PRESS X TO SELECT",1,(Color){125,145,165});
    if (!padReady)
        text_draw(230,370,"NO CONTROLLER",1,(Color){220,90,90});
}

static void input_update(void) {
    static unsigned short old = 0;
    if (!padReady) return;
    int state = padGetState(0,0);
    if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1) return;
    if (!padRead(0,0,&padButtons)) return;

    /* libpad reports 0 for a pressed button, so flip the bits first. */
    unsigned short held = (unsigned short)(0xFFFF ^ padButtons.btns);
    unsigned short pressed = held & (unsigned short)~old;
    old = held;

    if (menuState == 1) {
        if (pressed & PAD_UP) selectedMenu = 0;
        if (pressed & PAD_DOWN) selectedMenu = 1;
        if (pressed & PAD_CROSS) {
            if (selectedMenu == 0) menuState = 2;
        }
        return;
    }
    if (menuState == 2) {
        if (pressed & PAD_START) { menuState = 1; return; }
        float step = (held & PAD_R1) ? 0.18f : 0.09f;
        int moved = 0;
        if (held & PAD_LEFT)  { player.x -= step; playerYaw = -0.25f; moved = 1; }
        if (held & PAD_RIGHT) { player.x += step; playerYaw = 0.25f;  moved = 1; }
        if (held & PAD_UP)    { player.z += step; playerYaw = 0.0f;   moved = 1; }
        if (held & PAD_DOWN)  { player.z -= step; playerYaw = PI;     moved = 1; }
        if (moved) cameraYaw = playerYaw * 0.12f;
    }
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    /* The pad library only works after the IOP controller modules are loaded.
     * Without this, padInit() waits forever and the screen stays black. */
    SifInitRpc(0);
    int sio = SifLoadModule("rom0:SIO2MAN", 0, NULL);
    int pad = SifLoadModule("rom0:PADMAN", 0, NULL);
    padReady = (sio >= 0 && pad >= 0);

    gsGlobal = gsKit_init_global();
    if (gsGlobal == NULL) {
        return 1;
    }

    /* NTSC 640x448 interlaced */
    gsGlobal->Mode = GS_MODE_NTSC;
    gsGlobal->Interlace = GS_INTERLACED;
    gsGlobal->Field = GS_FIELD;
    gsGlobal->Width = 640;
    gsGlobal->Height = 448;
    gsGlobal->PrimAlphaEnable = GS_SETTING_ON;

    dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF,D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF,D_CTRL_RCYC_8,1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsKit_init_screen(gsGlobal);

    /* Persistent setup: turn Z-test off so the flat 2D/3D mix draws in order. */
    gsKit_mode_switch(gsGlobal, GS_PERSISTENT);
    gsKit_set_test(gsGlobal, GS_ZTEST_OFF);
    gsKit_mode_switch(gsGlobal, GS_ONESHOT);

    /* Show one frame before starting the pad, so a pad problem can never
     * hide the screen again. */
    draw_loading();
    gsKit_queue_exec(gsGlobal);
    gsKit_sync_flip(gsGlobal);

    if (padReady) {
        padInit(0);
        padPortOpen(0,0,padBuf);
    }

    menuState = 0;
    for (;;) {
        ++frameNo;
        if (menuState == 0) {
            draw_loading();
            if (frameNo > 100) menuState = 1;
        } else if (menuState == 1) {
            draw_title();
            input_update();
        } else {
            draw_world();
            input_update();
            /* HUD */
            rect(8,8,190,22,(Color){5,10,18});
            text_draw(16,15,"MUEANG ISARA",1,(Color){225,235,245});
            rect(8,414,310,25,(Color){5,10,18});
            text_draw(16,422,"PAD ARROWS MOVE  START MENU",1,(Color){180,195,210});
        }
        gsKit_queue_exec(gsGlobal);
        gsKit_sync_flip(gsGlobal);
    }
    return 0;
}
