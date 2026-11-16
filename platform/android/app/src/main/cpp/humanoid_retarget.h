#ifndef MUEANG_HUMANOID_RETARGET_H
#define MUEANG_HUMANOID_RETARGET_H

#include "humanoid_standard.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MUEANG_RETARGET_MAX_SOURCE_BONES 128

typedef struct {
    int source_count;
    int source_to_target[MUEANG_RETARGET_MAX_SOURCE_BONES];
    int target_to_source[MUEANG_HUMANOID_BONE_COUNT];
    MueangHumanoidRig source_rig;
    float source_height;
    float target_height;
    float blend;
    int mapped_count;
} MueangHumanoidRetargeter;

/* Names are taken from the source rig in the same order as its pose matrices. */
int HumanoidRetarget_Init(MueangHumanoidRetargeter *retargeter,
                          const char *const *source_bone_names,
                          int source_bone_count,
                          float source_height,
                          float target_height);

/* Smoothly changes the contribution of the retargeted pose (0 = legacy, 1 = source). */
void HumanoidRetarget_SetBlend(MueangHumanoidRetargeter *retargeter, float blend);

/*
 * All matrices are column-major 4x4 model-space global joint transforms.
 * source_bind/source_pose use source bone order; target_bind/fallback_pose use
 * Mueang's 21-bone order. Output remains in Mueang target model space.
 * Missing target bones retain fallback_pose. Cape bones also retain fallback.
 */
int HumanoidRetarget_ApplyPose(const MueangHumanoidRetargeter *retargeter,
                               const float *source_bind,
                               const float *source_pose,
                               const float *target_bind,
                               const float *fallback_pose,
                               float *out_pose);

#ifdef __cplusplus
}
#endif
#endif
