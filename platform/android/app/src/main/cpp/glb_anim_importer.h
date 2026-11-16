#ifndef MUEANG_GLB_ANIM_IMPORTER_H
#define MUEANG_GLB_ANIM_IMPORTER_H

#include <android/asset_manager.h>
#include "humanoid_clip.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MUEANG_GLB_ANIM_MAX_TRACKS MUEANG_CLIP_MAX_TRACKS

typedef struct {
    MueangHumanoidClip clip;
    MueangClipTrack tracks[MUEANG_GLB_ANIM_MAX_TRACKS];
    float *owned_times[MUEANG_GLB_ANIM_MAX_TRACKS];
    float *owned_values[MUEANG_GLB_ANIM_MAX_TRACKS];
    int imported_channels;
    int ignored_channels;
    int detected_rig;
} MueangGLBAnimation;

/* Loads one glTF 2.0 animation from a binary GLB stored in Android assets.
 * Supports float SCALAR time inputs, float VEC3/VEC4 outputs, STEP/LINEAR.
 * Unknown node names/channels and unsupported cubic/sparse data are skipped.
 */
int GLBAnimImporter_Load(AAssetManager *assets, const char *asset_path,
                         int animation_index, int loop,
                         MueangGLBAnimation *out_animation);
void GLBAnimImporter_Release(MueangGLBAnimation *animation);

#ifdef __cplusplus
}
#endif
#endif
