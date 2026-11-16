#ifndef MUEANG_CHARACTER3D_H
#define MUEANG_CHARACTER3D_H

#include "renderer3d.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Draws a modular humanoid prototype using the renderer's existing ES 2.0 shader. */
void Character3D_Draw(
    Renderer3D *renderer,
    const MueangGame *game,
    const float *view_projection,
    float player_y
);

#ifdef __cplusplus
}
#endif
#endif
