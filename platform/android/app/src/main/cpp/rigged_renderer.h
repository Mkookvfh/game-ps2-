#ifndef MUEANG_RIGGED_RENDERER_H
#define MUEANG_RIGGED_RENDERER_H
#include <android/asset_manager.h>

int RiggedRenderer_Init(AAssetManager *assets, const char *path);
int RiggedRenderer_Draw(const float *view_projection, float x, float y, float z,
                        float yaw, int animation_state);
void RiggedRenderer_Release(void);
#endif
