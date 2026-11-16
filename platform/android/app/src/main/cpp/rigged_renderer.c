#define _POSIX_C_SOURCE 200809L
#include "rigged_renderer.h"
#include "rigged_model.h"
#include "animation.h"
#include <GLES2/gl2.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>

#define MAX_GLB_SIZE (16u * 1024u * 1024u)

static MueangRiggedModel *model;
static MueangRiggedVertex *vertices;
static GLuint program, buffer;
static GLint position, normal, color, mvp, transform;
static int previous_state = -1;
static double state_start;

static const char *vertex_shader =
    "attribute vec3 aPosition; attribute vec3 aNormal; attribute vec3 aColor;"
    "uniform mat4 uMVP; uniform mat4 uModel; varying vec3 vColor;"
    "void main(){gl_Position=uMVP*vec4(aPosition,1.0);"
    "vec3 n=normalize(mat3(uModel)*aNormal);"
    "float light=max(dot(n,normalize(vec3(-0.4,0.85,0.3))),0.0);"
    "vColor=aColor*(0.38+light*0.62);}";
static const char *fragment_shader =
    "precision mediump float; varying vec3 vColor;"
    "void main(){gl_FragColor=vec4(vColor,1.0);}";

static GLuint compile(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    if (!shader) return 0;
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) { glDeleteShader(shader); return 0; }
    return shader;
}

void RiggedRenderer_Release(void)
{
    if (buffer) glDeleteBuffers(1, &buffer);
    if (program) glDeleteProgram(program);
    buffer = program = 0;
    free(vertices); vertices = NULL;
    RiggedModel_Free(model); model = NULL;
    previous_state = -1;
}

int RiggedRenderer_Init(AAssetManager *assets, const char *path)
{
    RiggedRenderer_Release();
    if (!assets || !path) return 0;
    AAsset *asset = AAssetManager_open(assets, path, AASSET_MODE_BUFFER);
    if (!asset) return 0;
    off_t length = AAsset_getLength(asset);
    if (length < 20 || (size_t)length > MAX_GLB_SIZE) { AAsset_close(asset); return 0; }
    void *data = malloc((size_t)length);
    if (!data) { AAsset_close(asset); return 0; }
    int read = AAsset_read(asset, data, (size_t)length);
    AAsset_close(asset);
    if (read == length) model = RiggedModel_Load(data, (size_t)length);
    free(data);
    if (!model) return 0;
    vertices = calloc(RiggedModel_VertexCount(model), sizeof(*vertices));
    if (!vertices) goto fail;
    GLuint vs = compile(GL_VERTEX_SHADER, vertex_shader);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragment_shader);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        goto fail;
    }
    program = glCreateProgram();
    if (!program) { glDeleteShader(vs); glDeleteShader(fs); goto fail; }
    glAttachShader(program, vs); glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint linked = 0; glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) goto fail;
    position = glGetAttribLocation(program, "aPosition");
    normal = glGetAttribLocation(program, "aNormal");
    color = glGetAttribLocation(program, "aColor");
    mvp = glGetUniformLocation(program, "uMVP");
    transform = glGetUniformLocation(program, "uModel");
    if (position < 0 || normal < 0 || color < 0 || mvp < 0 || transform < 0) goto fail;
    glGenBuffers(1, &buffer);
    if (!buffer) goto fail;
    return 1;
fail:
    RiggedRenderer_Release();
    return 0;
}

static double now_seconds(void)
{
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) != 0) return 0;
    return (double)time.tv_sec + (double)time.tv_nsec / 1000000000.0;
}

int RiggedRenderer_Draw(const float *vp, float x, float y, float z,
                        float yaw, int state)
{
    static const char *names[] = {"Idle", "Walk", "Jump", "Roll", "Slide",
                                  "WallRun", "Grapple", "Vault", "Attack", "Crouch"};
    if (!model || !program || !buffer || !vp) return 0;
    double now = now_seconds();
    if (state != previous_state) { previous_state = state; state_start = now; }
    int clip = state >= 0 && state < (int)(sizeof(names)/sizeof(names[0]))
        ? RiggedModel_FindAnimation(model, names[state]) : -1;
    if (clip < 0) clip = RiggedModel_FindAnimation(model, "Idle");
    size_t count = RiggedModel_VertexCount(model);
    if (!RiggedModel_Sample(model, clip, (float)(now - state_start), vertices, count)) return 0;

    float c=cosf(yaw), s=sinf(yaw);
    float world[16]={c,0,-s,0, 0,1,0,0, s,0,c,0, x,y,z,1};
    float combined[16];
    for (int col=0;col<4;++col) for (int row=0;row<4;++row) {
        combined[col*4+row]=0;
        for (int k=0;k<4;++k) combined[col*4+row]+=vp[k*4+row]*world[col*4+k];
    }
    glUseProgram(program);
    glUniformMatrix4fv(mvp,1,GL_FALSE,combined);
    glUniformMatrix4fv(transform,1,GL_FALSE,world);
    glBindBuffer(GL_ARRAY_BUFFER,buffer);
    glBufferData(GL_ARRAY_BUFFER,count*sizeof(*vertices),vertices,GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray((GLuint)position);
    glEnableVertexAttribArray((GLuint)normal);
    glEnableVertexAttribArray((GLuint)color);
    glVertexAttribPointer((GLuint)position,3,GL_FLOAT,GL_FALSE,sizeof(*vertices),(const void *)offsetof(MueangRiggedVertex,position));
    glVertexAttribPointer((GLuint)normal,3,GL_FLOAT,GL_FALSE,sizeof(*vertices),(const void *)offsetof(MueangRiggedVertex,normal));
    glVertexAttribPointer((GLuint)color,3,GL_FLOAT,GL_FALSE,sizeof(*vertices),(const void *)offsetof(MueangRiggedVertex,color));
    glDrawArrays(GL_TRIANGLES,0,(GLsizei)count);
    glDisableVertexAttribArray((GLuint)position);
    glDisableVertexAttribArray((GLuint)normal);
    glDisableVertexAttribArray((GLuint)color);
    glBindBuffer(GL_ARRAY_BUFFER,0);
    return 1;
}
