#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/input.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "renderer3d.h"
#include "game.h"
#include "resource_system.h"
#include "ui_renderer.h"
#include "progress.h"

#define LOG_TAG "MUEANG_ISARA"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define INVALID_POINTER_ID (-1)
#define JOYSTICK_RADIUS 110.0f
#define LOOK_SENSITIVITY 0.003f

typedef struct {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;

    GLuint program;
    GLint position;
    GLint color;

    int ready;

    MueangGame game;
    Renderer3D renderer;
    UIRenderer ui_renderer;
    MueangResourceSystem resources;

    int inventory_selected;
    int inventory_category;
    int last_craft_recipe;
    int last_craft_result;
    char progress_path[1024];
    float save_timer;

    int left_pointer;
    int right_pointer;
    int jump_pointer;
    int sprint_pointer;
    int action_pointer;
    int harvest_pointer;
    int inventory_pointer;
    int inventory_open;

    float anchor_x;
    float anchor_y;

    float last_look_x;
    float last_look_y;

    float move_x;
    float move_y;

    float look_dx;
    float look_dy;

} GameState;

static void clear_touch_input(GameState *s)
{
    s->left_pointer = INVALID_POINTER_ID;
    s->right_pointer = INVALID_POINTER_ID;
    s->jump_pointer = INVALID_POINTER_ID;
    s->sprint_pointer = INVALID_POINTER_ID;
    s->action_pointer = INVALID_POINTER_ID;
    s->harvest_pointer = INVALID_POINTER_ID;
    s->inventory_pointer = INVALID_POINTER_ID;
    s->move_x = 0.0f;
    s->move_y = 0.0f;
    s->look_dx = 0.0f;
    s->look_dy = 0.0f;
}


/* =========================================================
   MONOTONIC TIMER
   ========================================================= */

static double get_time_seconds(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }

    return (double)ts.tv_sec +
           (double)ts.tv_nsec / 1000000000.0;
}


/* =========================================================
   SHADER
   ========================================================= */


/* =========================================================
   OPENGL ES 2.0 SHADERS
   ========================================================= */

static const char *VS =
    "attribute vec4 aPosition;\n"
    "void main() {\n"
    "    gl_Position = vec4(aPosition.xyz, 1.0);\n"
    "}\n";

static const char *FS =
    "precision mediump float;\n"
    "uniform vec4 uColor;\n"
    "void main() {\n"
    "    gl_FragColor = uColor;\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *src)
{
    GLuint shader = glCreateShader(type);

    if (!shader) return 0;

    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint ok = 0;

    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

    if (!ok) {
        char log[512];

        glGetShaderInfoLog(
            shader,
            sizeof(log),
            NULL,
            log
        );

        LOGE("Shader error: %s", log);

        glDeleteShader(shader);

        return 0;
    }

    return shader;
}

static int create_program(GameState *s)
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER, VS);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, FS);

    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

    s->program = glCreateProgram();

    if (!s->program) {
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }

    glAttachShader(s->program, vs);
    glAttachShader(s->program, fs);

    glLinkProgram(s->program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;

    glGetProgramiv(
        s->program,
        GL_LINK_STATUS,
        &ok
    );

    if (!ok) {
        char log[512];

        glGetProgramInfoLog(
            s->program,
            sizeof(log),
            NULL,
            log
        );

        LOGE("UI program link error: %s", log);

        glDeleteProgram(s->program);
        s->program = 0;

        return 0;
    }

    s->position = glGetAttribLocation(
        s->program,
        "aPosition"
    );

    s->color = glGetUniformLocation(
        s->program,
        "uColor"
    );

    return s->position >= 0 && s->color >= 0;
}

/* =========================================================
   UI DRAWING
   ========================================================= */

static void draw_shape(
    GameState *s,
    const GLfloat *v,
    int count,
    float r,
    float g,
    float b)
{
    glUniform4f(
        s->color,
        r,
        g,
        b,
        1.0f
    );

    glVertexAttribPointer(
        s->position,
        3,
        GL_FLOAT,
        GL_FALSE,
        0,
        v
    );

    glEnableVertexAttribArray(s->position);

    glDrawArrays(
        GL_TRIANGLES,
        0,
        count
    );
}

static void draw_disc(
    GameState *s,
    float cx,
    float cy,
    float radius,
    float r,
    float g,
    float b)
{
    enum { SEGMENTS = 32 };

    GLfloat vertices[SEGMENTS * 9];

    for (int i = 0; i < SEGMENTS; i++) {

        float a0 =
            (float)i * 6.2831853f / SEGMENTS;

        float a1 =
            (float)(i + 1) * 6.2831853f / SEGMENTS;

        int j = i * 9;

        vertices[j] = cx;
        vertices[j + 1] = cy;
        vertices[j + 2] = 0;

        vertices[j + 3] =
            cx + cosf(a0) * radius;

        vertices[j + 4] =
            cy + sinf(a0) * radius;

        vertices[j + 5] = 0;

        vertices[j + 6] =
            cx + cosf(a1) * radius;

        vertices[j + 7] =
            cy + sinf(a1) * radius;

        vertices[j + 8] = 0;
    }

    draw_shape(
        s,
        vertices,
        SEGMENTS * 3,
        r,
        g,
        b
    );
}

static void draw_rect(
    GameState *s,
    float x1,
    float y1,
    float x2,
    float y2,
    float r,
    float g,
    float b)
{
    GLfloat vertices[] = {
        x1, y1, 0,
        x2, y1, 0,
        x2, y2, 0,

        x1, y1, 0,
        x2, y2, 0,
        x1, y2, 0
    };

    draw_shape(
        s,
        vertices,
        6,
        r,
        g,
        b
    );
}

/* =========================================================
   INVENTORY FONT
   ========================================================= */

static unsigned short inventory_glyph(char c)
{
    switch (c) {
        case 'H': return 0x5B6D;
        case 'W': return 0x6B7F;
        case 'S': return 0x3972;
        case 'I': return 0x7497;
        case 'F': return 0x3924;
        case 'R': return 0x5B6A;
        case 'B': return 0x6BAE;
        case 'P': return 0x7BE4;
        case 'T': return 0x7492;
        case 'U': return 0x5B6F;
        case 'E': return 0x7997;
        case 'D': return 0x6B6E;

        case '0': return 0x7B6F;
        case '1': return 0x7497;
        case '2': return 0x73E7;
        case '3': return 0x73CF;
        case '4': return 0x5BC9;
        case '5': return 0x79CF;
        case '6': return 0x79EF;
        case '7': return 0x7249;
        case '8': return 0x7BEF;
        case '9': return 0x7BCF;

        default: return 0;
    }
}

static void draw_inventory_char(
    GameState *s,
    char c,
    float x,
    float y,
    float scale,
    float r,
    float g,
    float b)
{
    unsigned short bits = inventory_glyph(c);

    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 3; ++col) {

            int bit = 14 - (row * 3 + col);

            if (bits & (1u << bit)) {

                float px = x + col * scale;
                float py = y - row * scale;

                draw_rect(
                    s,
                    px,
                    py - scale * 0.8f,
                    px + scale * 0.8f,
                    py,
                    r,
                    g,
                    b
                );
            }
        }
    }
}

static void draw_inventory_number(
    GameState *s,
    int value,
    float x,
    float y)
{
    if (value < 0) value = 0;
    if (value > 999) value = 999;

    if (value >= 100) {
        draw_inventory_char(s, (char)('0' + value / 100), x, y,
                            0.018f, 1.0f, 1.0f, 1.0f);
        x += 0.055f;
    }

    draw_inventory_char(
        s,
        (char)('0' + (value / 10) % 10),
        x,
        y,
        0.018f,
        1.0f,
        1.0f,
        1.0f
    );

    draw_inventory_char(
        s,
        (char)('0' + value % 10),
        x + 0.055f,
        y,
        0.018f,
        1.0f,
        1.0f,
        1.0f
    );
}

static void draw_inventory_text(GameState *s, const char *text, float x, float y)
{
    for (; *text; ++text, x += 0.043f)
        draw_inventory_char(s, *text, x, y, 0.010f, 1.0f, 0.88f, 0.66f);
}

/* =========================================================
   RESOURCE UI
   ========================================================= */

static void render_resource_ui(GameState *s)
{
    int near_resource =
        MueangResource_GetNearestNode(
            &s->resources
        ) >= 0;

    int harvesting =
        s->harvest_pointer != INVALID_POINTER_ID;

    MueangHarvestStatus status =
        MueangResource_GetStatus(&s->resources);

    float progress =
        MueangResource_GetProgress(&s->resources);

    draw_disc(
        s,0.78f,0.72f,0.18f,
        0.07f,0.07f,0.07f
    );

    draw_disc(
        s,0.78f,0.72f,0.15f,
        near_resource ? 0.10f : 0.25f,
        near_resource ? 0.75f : 0.25f,
        near_resource ? 0.25f : 0.25f
    );

    draw_disc(
        s,0.765f,0.735f,0.035f,
        0.85f,1.0f,0.85f
    );

    draw_disc(
        s,0.795f,0.705f,0.025f,
        0.85f,1.0f,0.85f
    );

    if (harvesting) {

        draw_disc(
            s,0.78f,0.72f,0.17f,
            0.95f,0.85f,0.15f
        );

        draw_disc(
            s,0.78f,0.72f,0.13f,
            0.12f,0.55f,0.18f
        );
    }

    if (near_resource ||
        status == MUEANG_HARVESTING ||
        status == MUEANG_HARVEST_INVENTORY_FULL) {

        draw_rect(
            s,
            0.55f,0.48f,
            0.98f,0.55f,
            0.08f,0.08f,0.08f
        );

        float fill_width = 0.43f * progress;

        if (fill_width > 0.0f) {

            draw_rect(
                s,
                0.55f,
                0.48f,
                0.55f + fill_width,
                0.55f,
                status == MUEANG_HARVEST_INVENTORY_FULL
                    ? 1.0f : 0.20f,
                status == MUEANG_HARVEST_INVENTORY_FULL
                    ? 0.15f : 0.90f,
                0.20f
            );
        }
    }
}

/* =========================================================
   INVENTORY PANEL
   ========================================================= */

/* =========================================================
   INVENTORY CATEGORY FILTER
   Resource IDs are defined in resource_system.h:
   HERB=0, WOOD=1, STONE=2, IRON_ORE=3, FIBER=4.
   ========================================================= */

static int inventory_category_item(int category, int slot)
{
    static const int items[4][5] = {
        {0, 1, 2, 3, 4},       /* All */
        {0, 4, -1, -1, -1},    /* Nature */
        {1, -1, -1, -1, -1},   /* Construction */
        {2, 3, -1, -1, -1}     /* Minerals */
    };

    if (category < 0 || category >= 4 ||
        slot < 0 || slot >= 5) {
        return -1;
    }

    return items[category][slot];
}

static int inventory_first_item(int category)
{
    for (int slot = 0; slot < 5; ++slot) {
        int item = inventory_category_item(category, slot);
        if (item >= 0) return item;
    }

    return 0;
}

static void render_inventory_ui(GameState *s)
{
    /* Inventory button */
    draw_disc(
        s,-0.78f,0.78f,0.105f,
        0.025f,0.025f,0.020f
    );

    draw_disc(
        s,-0.78f,0.78f,0.087f,
        0.42f,0.29f,0.12f
    );

    draw_inventory_char(
        s,'I',
        -0.805f,0.815f,
        0.018f,
        1.0f,0.86f,0.58f
    );

    if (!s->inventory_open) return;
    /* Main panel */
    draw_rect(
        s,-0.94f,-0.80f,
        0.94f,0.82f,
        0.35f,0.24f,0.12f
    );

    draw_rect(
        s,-0.925f,-0.785f,
        0.925f,0.805f,
        0.035f,0.045f,0.043f
    );

    /* Header */
    draw_rect(
        s,-0.905f,0.675f,
        0.905f,0.765f,
        0.12f,0.085f,0.045f
    );

    draw_rect(
        s,-0.905f,0.665f,
        0.905f,0.672f,
        0.76f,0.53f,0.23f
    );

    draw_disc(
        s,-0.855f,0.72f,0.035f,
        0.72f,0.52f,0.22f
    );

    draw_inventory_char(
        s,'I',
        -0.868f,0.737f,
        0.012f,
        0.07f,0.06f,0.04f
    );

    draw_inventory_char(
        s,'R',
        -0.785f,0.744f,
        0.022f,
        0.96f,0.82f,0.55f
    );

    /* Close button */
    draw_disc(
        s,0.855f,0.72f,0.045f,
        0.62f,0.40f,0.16f
    );

    draw_rect(
        s,0.832f,0.715f,
        0.878f,0.725f,
        0.035f,0.025f,0.015f
    );

    draw_rect(
        s,0.850f,0.697f,
        0.860f,0.743f,
        0.035f,0.025f,0.015f
    );

    /* Category rail */
    const float tab_y[4] = {
        0.50f,0.33f,0.16f,-0.01f
    };

    const char tab_label[4] = {
        'I','H','W','S'
    };

    for (int i=0;i<4;i++) {

        draw_rect(
            s,
            -0.875f,tab_y[i]-0.060f,
            -0.675f,tab_y[i]+0.060f,
            i==s->inventory_category?0.42f:0.105f,
            i==s->inventory_category?0.27f:0.075f,
            i==s->inventory_category?0.11f:0.045f
        );

        draw_rect(
            s,
            -0.870f,tab_y[i]-0.055f,
            -0.680f,tab_y[i]-0.048f,
            0.78f,0.55f,0.24f
        );

        draw_inventory_char(
            s,tab_label[i],
            -0.805f,tab_y[i]+0.020f,
            0.018f,
            0.96f,0.80f,0.52f
        );
    }

    /* Item slots */
    static const float sx[5] = {
        -0.47f,-0.18f,0.11f,-0.47f,-0.18f
    };

    static const float sy[5] = {
        0.37f,0.37f,0.37f,0.08f,0.08f
    };

    static const float rgb[5][3] = {
        {0.22f,0.62f,0.22f},
        {0.48f,0.27f,0.11f},
        {0.52f,0.54f,0.55f},
        {0.65f,0.39f,0.18f},
        {0.68f,0.52f,0.28f}
    };

    const char glyph[5] = {
        'H','W','S','I','F'
    };

    for (int i=0;i<5;i++) {

        int item = inventory_category_item(
            s->inventory_category, i
        );
        if (item < 0) continue;

        float x=sx[i];
        float y=sy[i];

        int selected =
            (s->inventory_selected==item);

        draw_rect(
            s,
            x-0.105f,y-0.112f,
            x+0.105f,y+0.112f,
            selected?0.84f:0.30f,
            selected?0.64f:0.19f,
            selected?0.30f:0.095f
        );

        draw_rect(
            s,
            x-0.096f,y-0.103f,
            x+0.096f,y+0.103f,
            0.045f,0.040f,0.030f
        );

        draw_disc(
            s,
            x,y+0.020f,
            0.050f,
            rgb[item][0],
            rgb[item][1],
            rgb[item][2]
        );

        draw_inventory_char(
            s,
            glyph[item],
            x-0.012f,y+0.036f,
            0.012f,
            0.98f,0.88f,0.68f
        );

        int count =
            MueangResource_GetCount(
                &s->resources,
                (MueangResourceItem)item
            );

        draw_inventory_number(
            s,count,
            x-0.065f,y-0.064f
        );

        draw_rect(
            s,
            x-0.078f,y-0.091f,
            x+0.078f,y-0.084f,
            0.16f,0.13f,0.08f
        );

        float fill =
            0.156f*((float)count/MueangResource_GetCapacity(&s->resources));

        if (fill>0.0f) {

            draw_rect(
                s,
                x-0.078f,y-0.091f,
                x-0.078f+fill,y-0.084f,
                0.76f,0.49f,0.16f
            );
        }
    }

    /* Selected resource details */
    int selected=s->inventory_selected;

    if (selected<0 || selected>=5)
        selected=0;

    int count =
        MueangResource_GetCount(
            &s->resources,
            (MueangResourceItem)selected
        );

    draw_rect(
        s,0.32f,-0.57f,
        0.86f,0.58f,
        0.12f,0.085f,0.050f
    );

    draw_rect(
        s,0.332f,-0.555f,
        0.848f,0.565f,
        0.035f,0.042f,0.038f
    );

    draw_rect(
        s,0.35f,0.42f,
        0.83f,0.427f,
        0.74f,0.49f,0.19f
    );

    draw_disc(
        s,0.47f,0.24f,
        0.080f,
        rgb[selected][0],
        rgb[selected][1],
        rgb[selected][2]
    );

    draw_inventory_char(
        s,glyph[selected],
        0.455f,0.265f,
        0.020f,
        1.0f,0.88f,0.66f
    );

    draw_inventory_number(
        s,count,
        0.59f,0.245f
    );

    draw_rect(
        s,0.36f,0.04f,
        0.80f,0.047f,
        0.64f,0.43f,0.18f
    );

    draw_rect(
        s,0.36f,-0.08f,
        0.80f,-0.055f,
        0.12f,0.095f,0.065f
    );

    float capacity =
        0.44f*((float)count/MueangResource_GetCapacity(&s->resources));

    if (capacity>0.0f) {

        draw_rect(
            s,
            0.36f,-0.08f,
            0.36f+capacity,-0.055f,
            0.72f,0.48f,0.17f
        );
    }

    /* P: potion, T: tool, B: backpack; letters show ingredient costs. */
    static const char *labels[MUEANG_RECIPE_COUNT] = {
        "P H2 F1", "T W3 S2 I1", "B W2 F3"
    };
    static const float tops[MUEANG_RECIPE_COUNT] = {-0.15f, -0.28f, -0.41f};
    for (int i = 0; i < MUEANG_RECIPE_COUNT; ++i) {
        int result = s->last_craft_recipe == i ? s->last_craft_result : 0;
        draw_rect(s, 0.35f, tops[i]-0.10f, 0.82f, tops[i],
                  result < 0 ? 0.45f : 0.22f,
                  result > 0 ? 0.50f : 0.15f, 0.08f);
        draw_inventory_text(s, labels[i], 0.37f, tops[i]-0.025f);
    }
    draw_inventory_text(s, "P", -0.48f, -0.25f);
    draw_inventory_number(s, s->resources.potions, -0.42f, -0.25f);
    draw_inventory_text(s, "T", -0.20f, -0.25f);
    draw_inventory_number(s, s->resources.tool_level, -0.14f, -0.25f);
    draw_inventory_text(s, "B", 0.06f, -0.25f);
    draw_inventory_number(s, s->resources.backpack_level, 0.12f, -0.25f);
    draw_rect(s,-0.48f,-0.45f,0.18f,-0.34f,0.22f,0.15f,0.08f);
    draw_inventory_text(s,"USE P",-0.43f,-0.36f);
    draw_inventory_text(s,"H",-0.10f,-0.36f);
    draw_inventory_number(s,(int)s->game.player.health,-0.04f,-0.36f);
}

/* =========================================================
   MAIN SCENE
   ========================================================= */

static void render_scene(
    GameState *s,
    int w,
    int h)
{
    Renderer3D_Draw(
        &s->renderer,
        &s->game,
        &s->resources,
        w,
        h
    );

    /* Full-screen procedural waterfall background */
    if (s->inventory_open) {

        UIRenderer_Draw(
            &s->ui_renderer,
            w,
            h,
            (float)get_time_seconds()
        );

        glDisable(GL_DEPTH_TEST);

        glUseProgram(s->program);

        render_inventory_ui(s);

        glDisableVertexAttribArray(s->position);

        return;
    }

    glDisable(GL_DEPTH_TEST);

    glUseProgram(s->program);

    draw_rect(s,-0.20f,0.88f,0.20f,0.93f,0.10f,0.08f,0.08f);
    draw_rect(s,-0.20f,0.88f,
              -0.20f + 0.40f * (s->game.player.health / 100.0f),
              0.93f,0.70f,0.12f,0.10f);

    draw_disc(s,-0.45f,0.75f,0.10f,
              s->game.sight_active ? 0.2f : 0.12f,0.30f,0.55f);
    draw_inventory_char(s,'S',-0.465f,0.78f,0.015f,1,1,1);
    draw_disc(s,-0.14f,0.75f,0.10f,0.38f,0.24f,0.12f);
    draw_inventory_char(s,'B',-0.155f,0.78f,0.015f,1,1,1);
    draw_inventory_text(s,"D",0.25f,0.79f);
    draw_inventory_number(s,s->game.discovered_count,0.31f,0.79f);

    /* Movement joystick */
    draw_disc(
        s,-0.72f,-0.72f,0.16f,
        0.12f,0.12f,0.12f
    );

    draw_disc(
        s,
        -0.72f+s->move_x*0.10f,
        -0.72f+s->move_y*0.10f,
        0.065f,
        0.85f,0.85f,0.90f
    );

    /* Camera touch indicator */
    if (s->right_pointer != INVALID_POINTER_ID) {

        draw_disc(
            s,0.72f,-0.72f,0.13f,
            0.20f,0.20f,0.20f
        );

        draw_disc(
            s,0.72f,-0.72f,0.06f,
            0.85f,0.85f,0.90f
        );
    }

    /* Jump button */
    draw_disc(
        s,0.78f,-0.38f,0.18f,
        0.08f,0.08f,0.08f
    );

    draw_disc(
        s,0.78f,-0.38f,0.15f,
        s->jump_pointer != INVALID_POINTER_ID
            ? 1.0f : 0.25f,
        0.85f,
        0.18f
    );

    draw_disc(
        s,0.78f,-0.38f,0.055f,
        1.0f,1.0f,1.0f
    );

    /* Sprint button */
    draw_disc(
        s,0.53f,-0.72f,0.16f,
        0.08f,0.08f,0.08f
    );

    draw_disc(
        s,0.53f,-0.72f,0.135f,
        s->sprint_pointer != INVALID_POINTER_ID
            ? 1.0f : 0.95f,
        0.55f,
        0.08f
    );

    draw_disc(
        s,0.53f,-0.72f,0.05f,
        1.0f,1.0f,1.0f
    );

    /* Action button */
    draw_disc(
        s,0.53f,-0.38f,0.18f,
        0.08f,0.08f,0.08f
    );

    draw_disc(
        s,0.53f,-0.38f,0.15f,
        s->action_pointer != INVALID_POINTER_ID
            ? 0.10f : 0.05f,
        s->action_pointer != INVALID_POINTER_ID
            ? 0.90f : 0.55f,
        1.0f
    );

    draw_disc(
        s,0.53f,-0.38f,0.055f,
        1.0f,1.0f,1.0f
    );

    render_resource_ui(s);
    render_inventory_ui(s);

    glDisableVertexAttribArray(s->position);
}

/* =========================================================
   JOYSTICK
   ========================================================= */

static void update_joystick(
    GameState *s,
    float x,
    float y)
{
    float dx=x-s->anchor_x;
    float dy=y-s->anchor_y;

    float distance=sqrtf(dx*dx+dy*dy);

    if (distance>JOYSTICK_RADIUS) {

        dx*=JOYSTICK_RADIUS/distance;
        dy*=JOYSTICK_RADIUS/distance;
    }

    s->move_x=dx/JOYSTICK_RADIUS;
    s->move_y=-dy/JOYSTICK_RADIUS;
}

/* =========================================================
   TOUCH INPUT
   ========================================================= */

static int find_pointer(
    const AInputEvent *event,
    int pointer_id)
{
    int count=AMotionEvent_getPointerCount(event);

    for (int i=0;i<count;i++) {

        if (AMotionEvent_getPointerId(event,i)
            ==pointer_id) {
            return i;
        }
    }

    return -1;
}

static int hit_button(
    float x,
    float y,
    int w,
    int h,
    float cx,
    float cy,
    float rx,
    float ry)
{
    float px=(cx+1.0f)*0.5f;
    float py=(1.0f-cy)*0.5f;

    float dx=(x/(float)w-px)/rx;
    float dy=(y/(float)h-py)/ry;

    return dx*dx+dy*dy<=1.0f;
}

static int pick_button(
    float x,
    float y,
    int w,
    int h)
{
    static const float centers[7][2]={
        {0.78f,-0.38f},
        {0.53f,-0.72f},
        {0.53f,-0.38f},
        {0.78f,0.72f},
        {-0.78f,0.78f},
        {-0.45f,0.75f},
        {-0.14f,0.75f}
    };

    int best=-1;
    float best_distance=999999.0f;

    for(int i=0;i<7;i++){

        if(!hit_button(
            x,y,w,h,
            centers[i][0],
            centers[i][1],
            0.105f,
            0.115f)){
            continue;
        }

        float px=(centers[i][0]+1.0f)*0.5f;
        float py=(1.0f-centers[i][1])*0.5f;

        float dx=(x/(float)w-px)/0.105f;
        float dy=(y/(float)h-py)/0.115f;

        float distance=dx*dx+dy*dy;

        if(distance<best_distance){
            best_distance=distance;
            best=i;
        }
    }

    return best;
}

/* =========================================================
   INPUT EVENT HANDLER
   ========================================================= */

static int handle_input(
    struct android_app *app,
    AInputEvent *event)
{
    GameState *s=(GameState *)app->userData;

    if(AInputEvent_getType(event)
        !=AINPUT_EVENT_TYPE_MOTION){
        return 0;
    }

    if(!app->window)return 1;

    int action=AMotionEvent_getAction(event);

    int masked=
        action&AMOTION_EVENT_ACTION_MASK;

    int index=
        (action&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
        >>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

    int width=ANativeWindow_getWidth(app->window);
    int height=ANativeWindow_getHeight(app->window);

    if(width<=0||height<=0)return 1;
    /* Finger down */
    if(masked==AMOTION_EVENT_ACTION_DOWN ||
       masked==AMOTION_EVENT_ACTION_POINTER_DOWN){

        int id=AMotionEvent_getPointerId(event,index);
        float x=AMotionEvent_getX(event,index);
        float y=AMotionEvent_getY(event,index);

        /* Inventory modal touch handling */
        if(s->inventory_open){

            float nx=(x/(float)width)*2.0f-1.0f;
            float ny=1.0f-(y/(float)height)*2.0f;

            if (nx >= -0.48f && nx <= 0.18f && ny >= -0.45f && ny <= -0.34f) {
                if (s->resources.potions > 0 && MueangGame_Heal(&s->game))
                    --s->resources.potions;
                return 1;
            }

            if (nx >= 0.35f && nx <= 0.82f) {
                static const float tops[MUEANG_RECIPE_COUNT] = {-0.15f, -0.28f, -0.41f};
                for (int recipe = 0; recipe < MUEANG_RECIPE_COUNT; ++recipe) {
                    if (ny >= tops[recipe]-0.10f && ny <= tops[recipe]) {
                        s->last_craft_recipe = recipe;
                        s->last_craft_result = MueangResource_Craft(
                            &s->resources, (MueangRecipe)recipe) ? 1 : -1;
                        return 1;
                    }
                }
            }

            /* Close button */
            if(nx>=0.79f && ny>=0.64f){
                s->inventory_open=0;
                return 1;
            }

            /* Category rail touch targets */
            static const float category_y[4] = {
                0.50f, 0.33f, 0.16f, -0.01f
            };

            if(nx>=-0.91f && nx<=-0.65f){
                for(int category=0; category<4; ++category){
                    if(ny>=category_y[category]-0.075f &&
                       ny<=category_y[category]+0.075f){

                        s->inventory_category=category;
                        s->inventory_selected=
                            inventory_first_item(category);

                        return 1;
                    }
                }
            }

            /* Item slot touch targets; match render positions */
            static const float slot_x[5] = {
                -0.47f,-0.18f,0.11f,-0.47f,-0.18f
            };
            static const float slot_y[5] = {
                0.37f,0.37f,0.37f,0.08f,0.08f
            };

            for(int slot=0; slot<5; ++slot){
                int item=inventory_category_item(
                    s->inventory_category, slot
                );

                if(item<0) continue;

                if(nx>=slot_x[slot]-0.115f &&
                   nx<=slot_x[slot]+0.115f &&
                   ny>=slot_y[slot]-0.12f &&
                   ny<=slot_y[slot]+0.12f){

                    s->inventory_selected=item;
                    return 1;
                }
            }

            return 1;
        }

        int pressed_button=
            pick_button(x,y,width,height);

        if (pressed_button == 5) {
            MueangGame_ToggleSight(&s->game);
            return 1;
        }
        if (pressed_button == 6) {
            MueangGame_InteractBody(&s->game);
            return 1;
        }

        /* Inventory button before joystick */
        if(pressed_button==4 &&
           s->inventory_pointer==INVALID_POINTER_ID){

            s->inventory_pointer=id;

        } else if(
            x<width*0.48f &&
            s->left_pointer==INVALID_POINTER_ID){

            s->left_pointer=id;

            s->anchor_x=x;
            s->anchor_y=y;

            s->move_x=0;
            s->move_y=0;

        } else {

            int button=pressed_button;

            if(button==0 &&
               s->jump_pointer==INVALID_POINTER_ID){

                s->jump_pointer=id;

            } else if(
                button==1 &&
                s->sprint_pointer==INVALID_POINTER_ID){

                s->sprint_pointer=id;

            } else if(
                button==2 &&
                s->action_pointer==INVALID_POINTER_ID){

                s->action_pointer=id;

            } else if(
                button==3 &&
                s->harvest_pointer==INVALID_POINTER_ID){

                s->harvest_pointer=id;

            } else if(
                x>=width*0.48f &&
                s->right_pointer==INVALID_POINTER_ID){

                s->right_pointer=id;

                s->last_look_x=x;
                s->last_look_y=y;
            }
        }

        return 1;
    }

    /* Finger movement */
    if(masked==AMOTION_EVENT_ACTION_MOVE){

        if(s->left_pointer!=INVALID_POINTER_ID){

            int i=find_pointer(event,s->left_pointer);

            if(i>=0){

                update_joystick(
                    s,
                    AMotionEvent_getX(event,i),
                    AMotionEvent_getY(event,i)
                );
            }
        }

        if(s->right_pointer!=INVALID_POINTER_ID){

            int i=find_pointer(event,s->right_pointer);

            if(i>=0){

                float x=AMotionEvent_getX(event,i);
                float y=AMotionEvent_getY(event,i);

                s->look_dx+=x-s->last_look_x;
                s->look_dy+=y-s->last_look_y;

                s->last_look_x=x;
                s->last_look_y=y;
            }
        }

        return 1;
    }

    /* Finger up */
    if(masked==AMOTION_EVENT_ACTION_UP ||
       masked==AMOTION_EVENT_ACTION_POINTER_UP){

        int id=AMotionEvent_getPointerId(event,index);

        if(id==s->left_pointer){

            s->left_pointer=INVALID_POINTER_ID;

            s->move_x=0;
            s->move_y=0;
        }

        if(id==s->right_pointer)
            s->right_pointer=INVALID_POINTER_ID;

        if(id==s->jump_pointer)
            s->jump_pointer=INVALID_POINTER_ID;

        if(id==s->sprint_pointer)
            s->sprint_pointer=INVALID_POINTER_ID;

        if(id==s->action_pointer)
            s->action_pointer=INVALID_POINTER_ID;

        if(id==s->harvest_pointer)
            s->harvest_pointer=INVALID_POINTER_ID;

        if(id==s->inventory_pointer){

            s->inventory_open=!s->inventory_open;
            clear_touch_input(s);
        }

        return 1;
    }

    /* Cancel all touches */
    if(masked==AMOTION_EVENT_ACTION_CANCEL){

        clear_touch_input(s);

        return 1;
    }

    return 1;
}

/* =========================================================
   EGL
   ========================================================= */

static void destroy_egl(GameState *s)
{
    if(s->display!=EGL_NO_DISPLAY){

        if(s->context!=EGL_NO_CONTEXT &&
           eglGetCurrentContext()==s->context){

            Renderer3D_Release(&s->renderer);

            UIRenderer_Release(&s->ui_renderer);

            if(s->program){

                glDeleteProgram(s->program);

                s->program=0;
            }
        }

        eglMakeCurrent(
            s->display,
            EGL_NO_SURFACE,
            EGL_NO_SURFACE,
            EGL_NO_CONTEXT
        );

        if(s->context!=EGL_NO_CONTEXT)
            eglDestroyContext(s->display,s->context);

        if(s->surface!=EGL_NO_SURFACE)
            eglDestroySurface(s->display,s->surface);

        eglTerminate(s->display);
    }

    s->display=EGL_NO_DISPLAY;
    s->surface=EGL_NO_SURFACE;
    s->context=EGL_NO_CONTEXT;

    s->ready=0;
}

static int init_egl(
    struct android_app *app,
    GameState *s)
{
    EGLint format;
    EGLint num;

    EGLConfig config;

    const EGLint attrs[]={
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_ES2_BIT,

        EGL_SURFACE_TYPE,
        EGL_WINDOW_BIT,

        EGL_RED_SIZE,8,
        EGL_GREEN_SIZE,8,
        EGL_BLUE_SIZE,8,
        EGL_ALPHA_SIZE,8,

        EGL_DEPTH_SIZE,16,

        EGL_NONE
    };

    s->display=eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if(s->display==EGL_NO_DISPLAY)
        return 0;

    if(!eglInitialize(s->display,NULL,NULL)){

        s->display=EGL_NO_DISPLAY;

        return 0;
    }

    if(!eglChooseConfig(
        s->display,
        attrs,
        &config,
        1,
        &num) || num==0){

        destroy_egl(s);

        return 0;
    }

    eglGetConfigAttrib(
        s->display,
        config,
        EGL_NATIVE_VISUAL_ID,
        &format
    );

    ANativeWindow_setBuffersGeometry(
        app->window,
        0,
        0,
        format
    );

    s->surface=eglCreateWindowSurface(
        s->display,
        config,
        app->window,
        NULL
    );

    const EGLint ctx_attrs[]={
        EGL_CONTEXT_CLIENT_VERSION,
        2,
        EGL_NONE
    };

    s->context=eglCreateContext(
        s->display,
        config,
        EGL_NO_CONTEXT,
        ctx_attrs
    );

    if(s->surface==EGL_NO_SURFACE ||
       s->context==EGL_NO_CONTEXT){

        destroy_egl(s);

        return 0;
    }

    if(!eglMakeCurrent(
        s->display,
        s->surface,
        s->surface,
        s->context)){

        destroy_egl(s);

        return 0;
    }

    if(!create_program(s) ||
       !Renderer3D_Init(
           &s->renderer,
           app->activity->assetManager) ||
       !UIRenderer_Init(&s->ui_renderer)){

        destroy_egl(s);

        return 0;
    }

    s->ready=1;

    return 1;
}


/* =========================================================
   ANDROID COMMAND HANDLER
   ========================================================= */

static void handle_cmd(
    struct android_app *app,
    int32_t cmd)
{
    GameState *s = (GameState *)app->userData;

    if (!s) {
        return;
    }

    switch (cmd) {

        case APP_CMD_INIT_WINDOW:
            if (app->window && !s->ready) {
                if (!init_egl(app, s)) {
                    LOGE("Failed to initialize EGL");
                }
            }
            break;

        case APP_CMD_TERM_WINDOW:
            clear_touch_input(s);
            if (s->ready) {
                destroy_egl(s);
            }
            break;

        case APP_CMD_LOST_FOCUS:
            clear_touch_input(s);
            break;

        case APP_CMD_PAUSE:
            if (s->progress_path[0])
                MueangProgress_Save(s->progress_path, &s->game, &s->resources);
            break;

        default:
            break;
    }
}

/* =========================================================
   ANDROID COMMAND
   ========================================================= */

void android_main(struct android_app *app)
{
    app_dummy();

    GameState state={0};

    state.display=EGL_NO_DISPLAY;
    state.surface=EGL_NO_SURFACE;
    state.context=EGL_NO_CONTEXT;

    clear_touch_input(&state);

    state.inventory_open=0;
    state.inventory_selected=0;
    state.inventory_category=0;
    state.last_craft_recipe=-1;

    MueangGame_Init(&state.game);

    MueangResource_Init(&state.resources);

    if (app->activity->internalDataPath) {
        int written = snprintf(state.progress_path, sizeof(state.progress_path),
                               "%s/progress-v1.txt", app->activity->internalDataPath);
        if (written < 0 || (size_t)written >= sizeof(state.progress_path))
            state.progress_path[0] = '\0';
        else
            MueangProgress_Load(state.progress_path, &state.game, &state.resources);
    }

    app->userData=&state;

    app->onAppCmd=handle_cmd;
    app->onInputEvent=handle_input;

    double previous=get_time_seconds();

    while(!app->destroyRequested){

        int events;

        struct android_poll_source *source;

        while(ALooper_pollOnce(
            state.ready?0:-1,
            NULL,
            &events,
            (void **)&source)>=0){

            if(source)
                source->process(app,source);

            if(app->destroyRequested)
                break;
        }

        if(app->destroyRequested)
            break;

        if(state.ready && app->window){

            double now=get_time_seconds();

            float dt=(float)(now-previous);

            previous=now;

            if(dt<0.001f)
                dt=0.001f;

            if(dt>0.05f)
                dt=0.05f;

            float look_x=
                state.look_dx*
                LOOK_SENSITIVITY/
                (2.2f*dt);

            float look_y=
                state.look_dy*
                LOOK_SENSITIVITY/
                (1.5f*dt);

            state.look_dx=0;
            state.look_dy=0;

            /* Pause simulation while the inventory is open. */
            if (!state.inventory_open) {
                MueangGame_Update(
                &state.game,

                state.move_x,
                state.move_y,

                look_x,
                look_y,

                state.jump_pointer!=INVALID_POINTER_ID,

                state.sprint_pointer!=INVALID_POINTER_ID,

                state.action_pointer!=INVALID_POINTER_ID,

                dt
            );

            /* Resource harvesting update */
                MueangResource_Update(
                &state.resources,

                state.game.player.x,
                state.game.player.z,

                state.game.player.grounded &&
                state.harvest_pointer!=INVALID_POINTER_ID,

                dt
                );
            } else {
                state.game.player.jump_was_down = 0;
                state.game.player.action_was_down = 0;
            }

            state.save_timer += dt;
            if (state.save_timer >= 10.0f && state.progress_path[0]) {
                MueangProgress_Save(state.progress_path, &state.game, &state.resources);
                state.save_timer = 0.0f;
            }

            int w=ANativeWindow_getWidth(app->window);
            int h=ANativeWindow_getHeight(app->window);

            render_scene(&state,w,h);

            eglSwapBuffers(
                state.display,
                state.surface
            );

        } else {

            previous=get_time_seconds();
        }
    }

    if (state.progress_path[0])
        MueangProgress_Save(state.progress_path, &state.game, &state.resources);
    destroy_egl(&state);
}
