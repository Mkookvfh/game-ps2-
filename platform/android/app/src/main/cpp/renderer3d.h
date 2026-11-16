
#ifndef MUEANG_RENDERER3D_H
#define MUEANG_RENDERER3D_H

#include <GLES2/gl2.h>
#include <android/asset_manager.h>
#include "game.h"
#include "resource_system.h"

typedef struct Renderer3D {
    GLuint program;
    GLint position;
    GLint normal;
    GLint mvp;
    GLint model;
    GLint color;
    GLint camera;
} Renderer3D;

int Renderer3D_Init(
    Renderer3D *renderer,
    AAssetManager *assets
);

void Renderer3D_Draw(
    Renderer3D *renderer,
    const MueangGame *game,
    const MueangResourceSystem *resources,
    int width,
    int height
);

void Renderer3D_Release(Renderer3D *renderer);

#endif
