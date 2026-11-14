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
