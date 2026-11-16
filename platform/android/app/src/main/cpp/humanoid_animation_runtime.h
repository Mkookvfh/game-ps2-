
#ifndef MUEANG_HUMANOID_ANIMATION_RUNTIME_H
#define MUEANG_HUMANOID_ANIMATION_RUNTIME_H

#include <android/asset_manager.h>
#include "glb_anim_importer.h"
#include "humanoid_clip.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    MueangGLBAnimation imported;
    MueangHumanoidPose sampled_pose;
    MueangHumanoidPose output_pose;

    int initialized;
    int loaded;
    int playing;
    int last_sample_count;

    float time_seconds;
    float blend;
} MueangHumanoidAnimationRuntime;

void HumanoidRuntime_Init(
    MueangHumanoidAnimationRuntime *runtime
);

int HumanoidRuntime_LoadGLB(
    MueangHumanoidAnimationRuntime *runtime,
    AAssetManager *assets,
    const char *asset_path,
    int animation_index,
    int loop
);

void HumanoidRuntime_Play(
    MueangHumanoidAnimationRuntime *runtime
);

void HumanoidRuntime_Pause(
    MueangHumanoidAnimationRuntime *runtime
);

void HumanoidRuntime_Seek(
    MueangHumanoidAnimationRuntime *runtime,
    float time_seconds
);

void HumanoidRuntime_SetBlend(
    MueangHumanoidAnimationRuntime *runtime,
    float blend
);

int HumanoidRuntime_Update(
    MueangHumanoidAnimationRuntime *runtime,
    float delta_seconds,
    const MueangHumanoidPose *fallback_pose,
    MueangHumanoidPose *out_pose
);

void HumanoidRuntime_Release(
    MueangHumanoidAnimationRuntime *runtime
);

#ifdef __cplusplus
}
#endif

#endif
