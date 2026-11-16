#ifndef MUEANG_HUMANOID_CLIP_H
#define MUEANG_HUMANOID_CLIP_H

#include "humanoid_standard.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MUEANG_CLIP_MAX_TRACKS 192

typedef enum {
    MUEANG_CLIP_TRANSLATION = 0,
    MUEANG_CLIP_ROTATION = 1,
    MUEANG_CLIP_SCALE = 2
} MueangClipPath;

typedef enum {
    MUEANG_CLIP_STEP = 0,
    MUEANG_CLIP_LINEAR = 1
} MueangClipInterpolation;

typedef struct {
    int target_bone;                 /* MueangHumanoidBone ID */
    MueangClipPath path;
    MueangClipInterpolation interpolation;
    int key_count;
    const float *times;              /* key_count seconds, strictly increasing */
    const float *values;             /* VEC3 or VEC4, tightly packed */
} MueangClipTrack;

typedef struct {
    const MueangClipTrack *tracks;   /* caller owns track and key arrays */
    int track_count;
    float duration;
    int loop;
} MueangHumanoidClip;

typedef struct {
    float translation[MUEANG_HUMANOID_BONE_COUNT][3];
    float rotation[MUEANG_HUMANOID_BONE_COUNT][4]; /* XYZW */
    float scale[MUEANG_HUMANOID_BONE_COUNT][3];
} MueangHumanoidPose;

/* Initialize a pose to identity TRS. */
void HumanoidClip_PoseIdentity(MueangHumanoidPose *pose);
/* Sample normalized tracks at time_seconds. Returns number of tracks sampled. */
int HumanoidClip_Sample(const MueangHumanoidClip *clip, float time_seconds,
                        MueangHumanoidPose *out_pose);
/* Blend two local TRS poses. alpha=0 selects a, alpha=1 selects b. */
void HumanoidClip_BlendPose(const MueangHumanoidPose *a,
                            const MueangHumanoidPose *b,
                            float alpha,
                            MueangHumanoidPose *out_pose);
/* Convert local TRS into column-major global matrices using the standard 21-bone hierarchy. */
void HumanoidClip_BuildGlobalMatrices(const MueangHumanoidPose *pose,
                                      float out_matrices[MUEANG_HUMANOID_BONE_COUNT][16]);

#ifdef __cplusplus
}
#endif
#endif
