
#ifndef MUEANG_GLB_LOADER_H
#define MUEANG_GLB_LOADER_H

#include <android/asset_manager.h>
#include <GLES2/gl2.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Renderer3D Renderer3D;

/* Initialize the GLB model loader using Android assets. */
int GLBLoader_Load(
    Renderer3D *renderer,
    AAssetManager *asset_manager,
    const char *asset_path
);

/* Draw the loaded main character model. */
int GLBLoader_Draw(
    Renderer3D *renderer,
    float x,
    float y,
    float z,
    float yaw,
    float bob
);

/* Release OpenGL resources while EGL context is current. */
void GLBLoader_Release(Renderer3D *renderer);

#ifdef __cplusplus
}
#endif

#endif
