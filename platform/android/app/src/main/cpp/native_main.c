
#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/input.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include "renderer3d.h"
#include "game.h"

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
    int left_pointer, right_pointer, jump_pointer, sprint_pointer, action_pointer;
    float anchor_x, anchor_y;
    float last_look_x, last_look_y;
    float move_x, move_y;
    float look_dx, look_dy;
} GameState;

static const char *VS = "attribute vec3 aPosition;void main(){gl_Position=vec4(aPosition,1.0);}";
static const char *FS = "precision mediump float;uniform vec4 uColor;void main(){gl_FragColor=uColor;}";

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
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
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
    glGetProgramiv(s->program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(s->program, sizeof(log), NULL, log);
        LOGE("UI program link error: %s", log);
        glDeleteProgram(s->program);
        s->program = 0;
        return 0;
    }
    s->position = glGetAttribLocation(s->program, "aPosition");
    s->color = glGetUniformLocation(s->program, "uColor");
    return s->position >= 0 && s->color >= 0;
}

static void draw_shape(GameState *s, const GLfloat *v, int count,
                       float r, float g, float b)
{
    glUniform4f(s->color, r, g, b, 1.0f);
    glVertexAttribPointer(s->position, 3, GL_FLOAT, GL_FALSE, 0, v);
    glEnableVertexAttribArray(s->position);
    glDrawArrays(GL_TRIANGLES, 0, count);
}

static void draw_disc(GameState *s, float cx, float cy, float radius,
                      float r, float g, float b)
{
    enum { SEGMENTS = 32 };
    GLfloat vertices[SEGMENTS * 9];

    for (int i = 0; i < SEGMENTS; i++) {
        float a0 = (float)i * 6.2831853f / SEGMENTS;
        float a1 = (float)(i + 1) * 6.2831853f / SEGMENTS;
        int j = i * 9;

        vertices[j] = cx;
        vertices[j + 1] = cy;
        vertices[j + 2] = 0;

        vertices[j + 3] = cx + cosf(a0) * radius;
        vertices[j + 4] = cy + sinf(a0) * radius;
        vertices[j + 5] = 0;

        vertices[j + 6] = cx + cosf(a1) * radius;
        vertices[j + 7] = cy + sinf(a1) * radius;
        vertices[j + 8] = 0;
    }

    draw_shape(s, vertices, SEGMENTS * 3, r, g, b);
}

static void render_scene(GameState *s, int w, int h)
{
    Renderer3D_Draw(&s->renderer, &s->game, w, h);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(s->program);

    /* Movement joystick */
    draw_disc(s, -0.72f, -0.72f, 0.16f, 0.12f, 0.12f, 0.12f);
    draw_disc(s, -0.72f + s->move_x * 0.10f,
              -0.72f + s->move_y * 0.10f,
              0.065f, 0.85f, 0.85f, 0.90f);

    /* Camera touch indicator */
    if (s->right_pointer != INVALID_POINTER_ID) {
        draw_disc(s, 0.72f, -0.72f, 0.13f, 0.20f, 0.20f, 0.20f);
        draw_disc(s, 0.72f, -0.72f, 0.06f, 0.85f, 0.85f, 0.90f);
    }

    /* Jump button */
    draw_disc(s, 0.78f, -0.38f, 0.18f, 0.08f, 0.08f, 0.08f);
    draw_disc(s, 0.78f, -0.38f, 0.15f,
              s->jump_pointer != INVALID_POINTER_ID ? 1.0f : 0.25f,
              0.85f, 0.18f);
    draw_disc(s, 0.78f, -0.38f, 0.055f, 1.0f, 1.0f, 1.0f);

    /* Sprint button */
    draw_disc(s, 0.53f, -0.72f, 0.16f, 0.08f, 0.08f, 0.08f);
    draw_disc(s, 0.53f, -0.72f, 0.135f,
              s->sprint_pointer != INVALID_POINTER_ID ? 1.0f : 0.95f,
              0.55f, 0.08f);
    draw_disc(s, 0.53f, -0.72f, 0.05f, 1.0f, 1.0f, 1.0f);

    /* Action button */
    draw_disc(s, 0.53f, -0.38f, 0.18f, 0.08f, 0.08f, 0.08f);
    draw_disc(s, 0.53f, -0.38f, 0.15f,
              s->action_pointer != INVALID_POINTER_ID ? 0.10f : 0.05f,
              s->action_pointer != INVALID_POINTER_ID ? 0.90f : 0.55f,
              1.0f);
    draw_disc(s, 0.53f, -0.38f, 0.055f, 1.0f, 1.0f, 1.0f);

    glDisableVertexAttribArray(s->position);
}

static void update_joystick(GameState *s, float x, float y)
{
    float dx = x - s->anchor_x;
    float dy = y - s->anchor_y;
    float distance = sqrtf(dx * dx + dy * dy);

    if (distance > JOYSTICK_RADIUS) {
        dx *= JOYSTICK_RADIUS / distance;
        dy *= JOYSTICK_RADIUS / distance;
    }

    s->move_x = dx / JOYSTICK_RADIUS;
    s->move_y = -dy / JOYSTICK_RADIUS;
}

static int find_pointer(const AInputEvent *event, int pointer_id)
{
    int count = AMotionEvent_getPointerCount(event);
    for (int i = 0; i < count; i++) {
        if (AMotionEvent_getPointerId(event, i) == pointer_id) return i;
    }
    return -1;
}

static int hit_button(float x, float y, int w, int h,
                      float cx, float cy, float rx, float ry)
{
    float px = (cx + 1.0f) * 0.5f;
    float py = (1.0f - cy) * 0.5f;
    float dx = (x / (float)w - px) / rx;
    float dy = (y / (float)h - py) / ry;
    return dx * dx + dy * dy <= 1.0f;
}

static int pick_button(float x, float y, int w, int h)
{
    static const float centers[3][2] = {
        {0.78f, -0.38f},
        {0.53f, -0.72f},
        {0.53f, -0.38f}
    };

    int best = -1;
    float best_distance = 999999.0f;

    for (int i = 0; i < 3; i++) {
        if (!hit_button(x, y, w, h,
                        centers[i][0], centers[i][1],
                        0.105f, 0.115f)) {
            continue;
        }

        float px = (centers[i][0] + 1.0f) * 0.5f;
        float py = (1.0f - centers[i][1]) * 0.5f;
        float dx = (x / (float)w - px) / 0.105f;
        float dy = (y / (float)h - py) / 0.115f;
        float distance = dx * dx + dy * dy;

        if (distance < best_distance) {
            best_distance = distance;
            best = i;
        }
    }

    return best;
}

static int handle_input(struct android_app *app, AInputEvent *event)
{
    GameState *s = (GameState *)app->userData;

    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) return 0;
    if (!app->window) return 1;

    int action = AMotionEvent_getAction(event);
    int masked = action & AMOTION_EVENT_ACTION_MASK;
    int index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    int width = ANativeWindow_getWidth(app->window);
    int height = ANativeWindow_getHeight(app->window);

    if (width <= 0 || height <= 0) return 1;

    if (masked == AMOTION_EVENT_ACTION_DOWN ||
        masked == AMOTION_EVENT_ACTION_POINTER_DOWN) {

        int id = AMotionEvent_getPointerId(event, index);
        float x = AMotionEvent_getX(event, index);
        float y = AMotionEvent_getY(event, index);

        if (x < width * 0.48f &&
            s->left_pointer == INVALID_POINTER_ID) {

            s->left_pointer = id;
            s->anchor_x = x;
            s->anchor_y = y;
            s->move_x = 0;
            s->move_y = 0;

        } else {
            int button = pick_button(x, y, width, height);

            if (button == 0 &&
                s->jump_pointer == INVALID_POINTER_ID) {
                s->jump_pointer = id;

            } else if (button == 1 &&
                       s->sprint_pointer == INVALID_POINTER_ID) {
                s->sprint_pointer = id;

            } else if (button == 2 &&
                       s->action_pointer == INVALID_POINTER_ID) {
                s->action_pointer = id;

            } else if (x >= width * 0.48f &&
                       s->right_pointer == INVALID_POINTER_ID) {
                s->right_pointer = id;
                s->last_look_x = x;
                s->last_look_y = y;
            }
        }

        return 1;
    }

    if (masked == AMOTION_EVENT_ACTION_MOVE) {
        if (s->left_pointer != INVALID_POINTER_ID) {
            int i = find_pointer(event, s->left_pointer);
            if (i >= 0) {
                update_joystick(s,
                    AMotionEvent_getX(event, i),
                    AMotionEvent_getY(event, i));
            }
        }

        if (s->right_pointer != INVALID_POINTER_ID) {
            int i = find_pointer(event, s->right_pointer);
            if (i >= 0) {
                float x = AMotionEvent_getX(event, i);
                float y = AMotionEvent_getY(event, i);
                s->look_dx += x - s->last_look_x;
                s->look_dy += y - s->last_look_y;
                s->last_look_x = x;
                s->last_look_y = y;
            }
        }

        return 1;
    }

    if (masked == AMOTION_EVENT_ACTION_UP ||
        masked == AMOTION_EVENT_ACTION_POINTER_UP) {

        int id = AMotionEvent_getPointerId(event, index);

        if (id == s->left_pointer) {
            s->left_pointer = INVALID_POINTER_ID;
            s->move_x = 0;
            s->move_y = 0;
        }
        if (id == s->right_pointer) s->right_pointer = INVALID_POINTER_ID;
        if (id == s->jump_pointer) s->jump_pointer = INVALID_POINTER_ID;
        if (id == s->sprint_pointer) s->sprint_pointer = INVALID_POINTER_ID;
        if (id == s->action_pointer) s->action_pointer = INVALID_POINTER_ID;

        return 1;
    }

    if (masked == AMOTION_EVENT_ACTION_CANCEL) {
        s->left_pointer = INVALID_POINTER_ID;
        s->right_pointer = INVALID_POINTER_ID;
        s->jump_pointer = INVALID_POINTER_ID;
        s->sprint_pointer = INVALID_POINTER_ID;
        s->action_pointer = INVALID_POINTER_ID;
        s->move_x = 0;
        s->move_y = 0;
        s->look_dx = 0;
        s->look_dy = 0;
        return 1;
    }

    return 1;
}

static void destroy_egl(GameState *s)
{
    if (s->display != EGL_NO_DISPLAY) {

        /* Release GL resources before destroying the EGL context. */
        if (s->context != EGL_NO_CONTEXT &&
            eglGetCurrentContext() == s->context) {

            Renderer3D_Release(&s->renderer);

            if (s->program) {
                glDeleteProgram(s->program);
                s->program = 0;
            }
        }

        eglMakeCurrent(s->display, EGL_NO_SURFACE,
                       EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (s->context != EGL_NO_CONTEXT)
            eglDestroyContext(s->display, s->context);

        if (s->surface != EGL_NO_SURFACE)
            eglDestroySurface(s->display, s->surface);

        eglTerminate(s->display);
    }

    s->display = EGL_NO_DISPLAY;
    s->surface = EGL_NO_SURFACE;
    s->context = EGL_NO_CONTEXT;
    s->ready = 0;
}

static int init_egl(struct android_app *app, GameState *s)
{
    EGLint format, num;
    EGLConfig config;

    const EGLint attrs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_NONE
    };

    s->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (s->display == EGL_NO_DISPLAY) return 0;

    if (!eglInitialize(s->display, NULL, NULL)) {
        s->display = EGL_NO_DISPLAY;
        return 0;
    }

    if (!eglChooseConfig(s->display, attrs, &config, 1, &num) || num == 0) {
        destroy_egl(s);
        return 0;
    }

    eglGetConfigAttrib(s->display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(app->window, 0, 0, format);

    s->surface = eglCreateWindowSurface(s->display, config, app->window, NULL);

    const EGLint ctx_attrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    s->context = eglCreateContext(s->display, config, EGL_NO_CONTEXT, ctx_attrs);

    if (s->surface == EGL_NO_SURFACE || s->context == EGL_NO_CONTEXT) {
        destroy_egl(s);
        return 0;
    }

    if (!eglMakeCurrent(s->display, s->surface, s->surface, s->context)) {
        destroy_egl(s);
        return 0;
    }

    if (!create_program(s) ||
        !Renderer3D_Init(&s->renderer, app->activity->assetManager)) {
        destroy_egl(s);
        return 0;
    }

    s->ready = 1;
    return 1;
}

static void handle_cmd(struct android_app *app, int32_t cmd)
{
    GameState *s = (GameState *)app->userData;

    if (cmd == APP_CMD_INIT_WINDOW) {
        if (app->window && !s->ready) init_egl(app, s);
    } else if (cmd == APP_CMD_TERM_WINDOW) {
        destroy_egl(s);
    }
}

static double get_time_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

void android_main(struct android_app *app)
{
    app_dummy();

    GameState state = {0};
    state.display = EGL_NO_DISPLAY;
    state.surface = EGL_NO_SURFACE;
    state.context = EGL_NO_CONTEXT;

    state.left_pointer = INVALID_POINTER_ID;
    state.right_pointer = INVALID_POINTER_ID;
    state.jump_pointer = INVALID_POINTER_ID;
    state.sprint_pointer = INVALID_POINTER_ID;
    state.action_pointer = INVALID_POINTER_ID;

    MueangGame_Init(&state.game);

    app->userData = &state;
    app->onAppCmd = handle_cmd;
    app->onInputEvent = handle_input;

    double previous = get_time_seconds();

    while (!app->destroyRequested) {
        int events;
        struct android_poll_source *source;

        while (ALooper_pollOnce(state.ready ? 0 : -1, NULL,
                                &events, (void **)&source) >= 0) {
            if (source) source->process(app, source);
            if (app->destroyRequested) break;
        }

        if (app->destroyRequested) break;

        if (state.ready && app->window) {
            double now = get_time_seconds();
            float dt = (float)(now - previous);
            previous = now;

            if (dt < 0.001f) dt = 0.001f;
            if (dt > 0.05f) dt = 0.05f;

            float look_x = state.look_dx * LOOK_SENSITIVITY / (2.2f * dt);
            float look_y = state.look_dy * LOOK_SENSITIVITY / (1.5f * dt);

            state.look_dx = 0;
            state.look_dy = 0;

            MueangGame_Update(
                &state.game,
                state.move_x,
                state.move_y,
                look_x,
                look_y,
                state.jump_pointer != INVALID_POINTER_ID,
                state.sprint_pointer != INVALID_POINTER_ID,
                state.action_pointer != INVALID_POINTER_ID,
                dt
            );

            int w = ANativeWindow_getWidth(app->window);
            int h = ANativeWindow_getHeight(app->window);

            render_scene(&state, w, h);
            eglSwapBuffers(state.display, state.surface);
        } else {
            previous = get_time_seconds();
        }
    }

    destroy_egl(&state);
}
