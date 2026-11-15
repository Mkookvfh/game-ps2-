
#include <android_native_app_glue.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <time.h>
#include <math.h>

#include "game.h"

#define LOG_TAG "MUEANG_ISARA"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

typedef struct {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    GLuint program;
    GLint position;
    GLint color;
    int ready;
    MueangGame game;
} GameState;

static const char *VS =
    "attribute vec3 aPosition;"
    "void main(){gl_Position=vec4(aPosition,1.0);}";

static const char *FS =
    "precision mediump float;"
    "uniform vec4 uColor;"
    "void main(){gl_FragColor=uColor;}";

static GLuint compile_shader(GLenum type, const char *src)
{
    GLuint shader = glCreateShader(type);
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
    glAttachShader(s->program, vs);
    glAttachShader(s->program, fs);
    glLinkProgram(s->program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(s->program, GL_LINK_STATUS, &ok);

    if (!ok) {
        glDeleteProgram(s->program);
        s->program = 0;
        return 0;
    }

    s->position = glGetAttribLocation(s->program, "aPosition");
    s->color = glGetUniformLocation(s->program, "uColor");

    return 1;
}

static void draw_shape(GameState *s, const GLfloat *v,
                       int count, float r, float g, float b)
{
    glUniform4f(s->color, r, g, b, 1.0f);
    glVertexAttribPointer(s->position, 3, GL_FLOAT,
                          GL_FALSE, 0, v);
    glEnableVertexAttribArray(s->position);
    glDrawArrays(GL_TRIANGLES, 0, count);
}

static void render_scene(GameState *s, int w, int h)
{
    static const GLfloat ground[] = {
        -1,-0.65f,0, 1,-0.65f,0, -1,-1,0,
        -1,-1,0, 1,-0.65f,0, 1,-1,0
    };

    static const GLfloat building[] = {
        -0.72f,-0.25f,0, -0.25f,-0.25f,0, -0.72f,0.48f,0,
        -0.72f,0.48f,0, -0.25f,-0.25f,0, -0.25f,0.48f,0
    };

    static const GLfloat roof[] = {
        -0.80f,0.48f,0, -0.17f,0.48f,0, -0.48f,0.78f,0
    };

    GLfloat player[] = {
        -0.08f,-0.55f,0, 0.08f,-0.55f,0, -0.08f,-0.12f,0,
        -0.08f,-0.12f,0, 0.08f,-0.55f,0, 0.08f,-0.12f,0
    };

    float px = s->game.player.x * 0.08f;

    for (int i = 0; i < 6; i++)
        player[i * 3] += px;

    glViewport(0, 0, w, h);
    glClearColor(0.35f, 0.62f, 0.82f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(s->program);

    draw_shape(s, ground, 6, 0.25f, 0.48f, 0.22f);
    draw_shape(s, building, 6, 0.65f, 0.48f, 0.30f);
    draw_shape(s, roof, 3, 0.45f, 0.18f, 0.12f);
    draw_shape(s, player, 6, 0.85f, 0.85f, 0.90f);

    glDisableVertexAttribArray(s->position);
}

static void destroy_egl(GameState *s)
{
    if (s->display != EGL_NO_DISPLAY) {
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
        EGL_NONE
    };

    s->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (s->display == EGL_NO_DISPLAY) return 0;

    if (!eglInitialize(s->display, NULL, NULL)) {
        s->display = EGL_NO_DISPLAY;
        return 0;
    }

    if (!eglChooseConfig(s->display, attrs, &config, 1, &num)
        || num == 0) {
        destroy_egl(s);
        return 0;
    }

    eglGetConfigAttrib(s->display, config,
                       EGL_NATIVE_VISUAL_ID, &format);

    ANativeWindow_setBuffersGeometry(app->window, 0, 0, format);

    s->surface = eglCreateWindowSurface(
        s->display, config, app->window, NULL);

    const EGLint ctx_attrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    s->context = eglCreateContext(
        s->display, config, EGL_NO_CONTEXT, ctx_attrs);

    if (s->surface == EGL_NO_SURFACE ||
        s->context == EGL_NO_CONTEXT) {
        destroy_egl(s);
        return 0;
    }

    if (!eglMakeCurrent(s->display, s->surface,
                        s->surface, s->context)) {
        destroy_egl(s);
        return 0;
    }

    if (!create_program(s)) {
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
        if (app->window && !s->ready)
            init_egl(app, s);
    } else if (cmd == APP_CMD_TERM_WINDOW) {
        destroy_egl(s);
    }
}

static double get_time_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

void android_main(struct android_app *app)
{
    app_dummy();

    GameState state = {0};
    state.display = EGL_NO_DISPLAY;
    state.surface = EGL_NO_SURFACE;
    state.context = EGL_NO_CONTEXT;

    MueangGame_Init(&state.game);

    app->userData = &state;
    app->onAppCmd = handle_cmd;

    double previous = get_time_seconds();

    while (!app->destroyRequested) {
        int events;
        struct android_poll_source *source;

        while (ALooper_pollOnce(
            state.ready ? 0 : -1,
            NULL, &events, (void **)&source) >= 0) {
            if (source)
                source->process(app, source);

            if (app->destroyRequested)
                break;
        }

        if (app->destroyRequested)
            break;

        if (state.ready && app->window) {
            double now = get_time_seconds();
            float dt = (float)(now - previous);
            previous = now;

            MueangGame_Update(
                &state.game, 0, 0, 0, 0, dt);

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
