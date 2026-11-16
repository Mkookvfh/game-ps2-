#define _POSIX_C_SOURCE 200809L
#include "rigged_renderer.h"
#include "animation.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct AAsset { FILE *file; off_t size; };
AAsset *AAssetManager_open(AAssetManager *manager, const char *path, int mode)
{
    (void)manager; (void)mode;
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return NULL; }
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) { fclose(file); return NULL; }
    AAsset *asset = malloc(sizeof(*asset));
    if (!asset) { fclose(file); return NULL; }
    asset->file = file; asset->size = (off_t)size;
    return asset;
}
off_t AAsset_getLength(AAsset *asset) { return asset->size; }
int AAsset_read(AAsset *asset, void *buffer, size_t count) { return (int)fread(buffer, 1, count, asset->file); }
void AAsset_close(AAsset *asset) { fclose(asset->file); free(asset); }

int main(int argc, char **argv)
{
    assert(argc == 2);
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    assert(display != EGL_NO_DISPLAY && eglInitialize(display, NULL, NULL));
    assert(eglBindAPI(EGL_OPENGL_ES_API));
    const EGLint config_attribs[] = {EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,
        EGL_BLUE_SIZE,8,EGL_NONE};
    EGLConfig config; EGLint found;
    assert(eglChooseConfig(display,config_attribs,&config,1,&found) && found == 1);
    const EGLint surface_attribs[] = {EGL_WIDTH,64,EGL_HEIGHT,64,EGL_NONE};
    const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
    EGLSurface surface = eglCreatePbufferSurface(display,config,surface_attribs);
    EGLContext context = eglCreateContext(display,config,EGL_NO_CONTEXT,context_attribs);
    assert(surface != EGL_NO_SURFACE && context != EGL_NO_CONTEXT);
    assert(eglMakeCurrent(display,surface,surface,context));
    AAssetManager manager = {0};
    assert(!RiggedRenderer_Init(&manager,"/nonexistent-rig.glb"));
    assert(RiggedRenderer_Init(&manager,argv[1]));
    const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    unsigned char first[64*64*4], second[64*64*4];
    glViewport(0,0,64,64);
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    assert(RiggedRenderer_Draw(identity,0,0,0,0,MUEANG_ANIM_WALK));
    glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,first);
    struct timespec wait = {0, 400000000};
    nanosleep(&wait, NULL);
    glClear(GL_COLOR_BUFFER_BIT);
    assert(RiggedRenderer_Draw(identity,0,0,0,0,MUEANG_ANIM_WALK));
    glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,second);
    int lit = 0, changed = 0;
    for (size_t i=0;i<sizeof(first);i+=4) {
        if (first[i] || first[i+1] || first[i+2]) ++lit;
        if (memcmp(first+i,second+i,3)) ++changed;
    }
    assert(lit > 30 && changed > 30 && glGetError() == GL_NO_ERROR);
    RiggedRenderer_Release();
    eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    eglDestroyContext(display,context); eglDestroySurface(display,surface); eglTerminate(display);
    puts("rigged renderer produced different animated frames");
    return 0;
}
