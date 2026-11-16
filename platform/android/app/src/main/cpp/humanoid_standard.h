#ifndef MUEANG_HUMANOID_STANDARD_H
#define MUEANG_HUMANOID_STANDARD_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Hybrid Mixamo + UE5 Humanoid compatibility layer.
 * Target IDs intentionally match the existing 21-bone Mueang skeleton.
 * This is a name/rig adapter; it does not yet import skin weights or clips.
 */
#define MUEANG_HUMANOID_BONE_COUNT 21
#define MUEANG_HUMANOID_UNKNOWN (-1)

typedef enum {
    MUEANG_HUMANOID_ROOT = 0,
    MUEANG_HUMANOID_HIPS,
    MUEANG_HUMANOID_SPINE,
    MUEANG_HUMANOID_CHEST,
    MUEANG_HUMANOID_NECK,
    MUEANG_HUMANOID_HEAD,
    MUEANG_HUMANOID_LEFT_UPPER_ARM,
    MUEANG_HUMANOID_LEFT_FOREARM,
    MUEANG_HUMANOID_LEFT_HAND,
    MUEANG_HUMANOID_RIGHT_UPPER_ARM,
    MUEANG_HUMANOID_RIGHT_FOREARM,
    MUEANG_HUMANOID_RIGHT_HAND,
    MUEANG_HUMANOID_LEFT_THIGH,
    MUEANG_HUMANOID_LEFT_CALF,
    MUEANG_HUMANOID_LEFT_FOOT,
    MUEANG_HUMANOID_RIGHT_THIGH,
    MUEANG_HUMANOID_RIGHT_CALF,
    MUEANG_HUMANOID_RIGHT_FOOT,
    MUEANG_HUMANOID_CAPE_ROOT,
    MUEANG_HUMANOID_CAPE_MID,
    MUEANG_HUMANOID_CAPE_TIP
} MueangHumanoidBone;

typedef enum {
    MUEANG_RIG_UNKNOWN = 0,
    MUEANG_RIG_MIXAMO = 1,
    MUEANG_RIG_UE5_MANNY = 2,
    MUEANG_RIG_UE5_QUINN = 3,
    MUEANG_RIG_MUEANG_LEGACY = 4
} MueangHumanoidRig;

typedef enum {
    MUEANG_BONE_ROLE_DEFORM = 0,
    MUEANG_BONE_ROLE_IK_HELPER = 1,
    MUEANG_BONE_ROLE_FINGER = 2,
    MUEANG_BONE_ROLE_UNKNOWN = 3
} MueangHumanoidBoneRole;

/* Accepts Mixamo, UE5 Manny/Quinn and legacy Mueang bone names. */
int HumanoidStandard_MapBoneName(const char *source_name);
const char *HumanoidStandard_BoneName(int target_bone);
MueangHumanoidBoneRole HumanoidStandard_ClassifyBoneName(const char *source_name);
MueangHumanoidRig HumanoidStandard_DetectRig(const char *const *bone_names, int bone_count);

/* Maps a source skeleton's ordered names to target IDs. Unknown bones become -1. */
int HumanoidStandard_BuildMap(const char *const *bone_names, int bone_count,
                              int *out_target_ids, int out_capacity);

/* Returns the number of distinct mapped deform bones in a map. */
int HumanoidStandard_CountMapped(const int *target_ids, int count);

#ifdef __cplusplus
}
#endif
#endif
