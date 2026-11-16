#ifndef MUEANG_GLB_LOADER_H
#define MUEANG_GLB_LOADER_H
#include <android/asset_manager.h>
#include <GLES2/gl2.h>
#ifdef __cplusplus
extern "C" {
#endif
struct Renderer3D;
int GLBLoader_Load(struct Renderer3D *renderer,AAssetManager *asset_manager,const char *asset_path);
int GLBLoader_Draw(struct Renderer3D *renderer,const float *view_projection,float x,float y,float z,float yaw,float bob,int animation_state,float animation_phase,int crouching);
void GLBLoader_Release(struct Renderer3D *renderer);
#ifdef __cplusplus
}
#endif
#endif
