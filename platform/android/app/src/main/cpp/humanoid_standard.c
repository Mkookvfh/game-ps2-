#include "humanoid_standard.h"
#include <ctype.h>
#include <string.h>

#define NORMALIZED_NAME_MAX 96

static const char *const g_target_names[MUEANG_HUMANOID_BONE_COUNT] = {
    "Root", "Hips", "Spine", "Chest", "Neck", "Head",
    "L_UpperArm", "L_Forearm", "L_Hand",
    "R_UpperArm", "R_Forearm", "R_Hand",
    "L_Thigh", "L_Calf", "L_Foot",
    "R_Thigh", "R_Calf", "R_Foot",
    "Cape_Root", "Cape_Mid", "Cape_Tip"
};

/* Lowercase and remove namespace/punctuation, e.g. mixamorig:LeftArm -> leftarm. */
static void normalize_name(const char *src, char out[NORMALIZED_NAME_MAX]) {
    int n = 0;
    const char *p;
    if (!src) { out[0] = '\0'; return; }
    p = strrchr(src, ':');
    if (p) src = p + 1;
    p = strrchr(src, '|');
    if (p) src = p + 1;
    while (*src && n < NORMALIZED_NAME_MAX - 1) {
        unsigned char c = (unsigned char)*src++;
        if (isalnum(c)) out[n++] = (char)tolower(c);
    }
    out[n] = '\0';
}

static int one_of(const char *s, const char *a, const char *b,
                  const char *c, const char *d) {
    return (a && strcmp(s, a) == 0) || (b && strcmp(s, b) == 0) ||
           (c && strcmp(s, c) == 0) || (d && strcmp(s, d) == 0);
}

int HumanoidStandard_MapBoneName(const char *source_name) {
    char s[NORMALIZED_NAME_MAX];
    normalize_name(source_name, s);
    if (!s[0]) return MUEANG_HUMANOID_UNKNOWN;

    if (one_of(s,"root","armature","skeleton","")) return MUEANG_HUMANOID_ROOT;
    if (one_of(s,"hips","pelvis","mixamorighips","")) return MUEANG_HUMANOID_HIPS;
    if (one_of(s,"spine","spine1","spine01","")) return MUEANG_HUMANOID_SPINE;
    if (one_of(s,"spine2","spine02","spine3","spine03")) return MUEANG_HUMANOID_CHEST;
    if (one_of(s,"chest","upperchest","spine04","spine05")) return MUEANG_HUMANOID_CHEST;
    if (one_of(s,"neck","neck1","neck01","")) return MUEANG_HUMANOID_NECK;
    if (one_of(s,"head","headtopend","" ,"")) return MUEANG_HUMANOID_HEAD;

    /* Mixamo LeftArm / UE5 upperarm_l / legacy L_UpperArm. */
    if (one_of(s,"leftarm","leftupperarm","upperarml","lupperarm")) return MUEANG_HUMANOID_LEFT_UPPER_ARM;
    if (one_of(s,"leftforearm","lowerarml","lforearm","forearml")) return MUEANG_HUMANOID_LEFT_FOREARM;
    if (one_of(s,"lefthand","handl","lhand","")) return MUEANG_HUMANOID_LEFT_HAND;
    if (one_of(s,"rightarm","rightupperarm","upperarmr","rupperarm")) return MUEANG_HUMANOID_RIGHT_UPPER_ARM;
    if (one_of(s,"rightforearm","lowerarmr","rforearm","forearmr")) return MUEANG_HUMANOID_RIGHT_FOREARM;
    if (one_of(s,"righthand","handr","rhand","")) return MUEANG_HUMANOID_RIGHT_HAND;

    /* Mixamo UpLeg/Leg/Foot and UE5 thigh/calf/foot naming. */
    if (one_of(s,"leftupleg","leftthigh","thighl","lthigh")) return MUEANG_HUMANOID_LEFT_THIGH;
    if (one_of(s,"leftleg","leftcalf","calfl","lcalf")) return MUEANG_HUMANOID_LEFT_CALF;
    if (one_of(s,"leftfoot","footl","lfoot","lefttoebase")) return MUEANG_HUMANOID_LEFT_FOOT;
    if (one_of(s,"rightupleg","rightthigh","thighr","rthigh")) return MUEANG_HUMANOID_RIGHT_THIGH;
    if (one_of(s,"rightleg","rightcalf","calfr","rcalf")) return MUEANG_HUMANOID_RIGHT_CALF;
    if (one_of(s,"rightfoot","footr","rfoot","righttoebase")) return MUEANG_HUMANOID_RIGHT_FOOT;

    /* Optional Mueang cape chain. */
    if (one_of(s,"caperoot","cape01","cloaka","")) return MUEANG_HUMANOID_CAPE_ROOT;
    if (one_of(s,"capemid","cape02","cloakb","")) return MUEANG_HUMANOID_CAPE_MID;
    if (one_of(s,"capetip","cape03","cloakc","")) return MUEANG_HUMANOID_CAPE_TIP;

    /* UE5 clavicles are intentionally folded into upper-arm anchors in v1. */
    if (one_of(s,"claviclel","shoulderl","leftshoulder","")) return MUEANG_HUMANOID_LEFT_UPPER_ARM;
    if (one_of(s,"clavicler","shoulderr","rightshoulder","")) return MUEANG_HUMANOID_RIGHT_UPPER_ARM;

    return MUEANG_HUMANOID_UNKNOWN;
}

const char *HumanoidStandard_BoneName(int target_bone) {
    if (target_bone < 0 || target_bone >= MUEANG_HUMANOID_BONE_COUNT) return "Unknown";
    return g_target_names[target_bone];
}

MueangHumanoidBoneRole HumanoidStandard_ClassifyBoneName(const char *source_name) {
    char s[NORMALIZED_NAME_MAX];
    normalize_name(source_name, s);
    if (!s[0]) return MUEANG_BONE_ROLE_UNKNOWN;

    if (strstr(s,"ik") == s || strstr(s,"ikfoot") != 0 || strstr(s,"ikhand") != 0 ||
        strstr(s,"polevector") != 0 || strstr(s,"weaponroot") != 0 ||
        strstr(s,"vb") == s) return MUEANG_BONE_ROLE_IK_HELPER;

    if (strstr(s,"thumb") || strstr(s,"index") || strstr(s,"middle") ||
        strstr(s,"ring") || strstr(s,"pinky") || strstr(s,"little"))
        return MUEANG_BONE_ROLE_FINGER;

    if (HumanoidStandard_MapBoneName(source_name) >= 0) return MUEANG_BONE_ROLE_DEFORM;
    return MUEANG_BONE_ROLE_UNKNOWN;
}

MueangHumanoidRig HumanoidStandard_DetectRig(const char *const *names, int count) {
    int mixamo = 0, ue5 = 0, legacy = 0;
    if (!names || count <= 0) return MUEANG_RIG_UNKNOWN;
    for (int i = 0; i < count; ++i) {
        char s[NORMALIZED_NAME_MAX];
        normalize_name(names[i], s);
        if (!s[0]) continue;
        if (strstr(s,"mixamorig") == s || one_of(s,"leftupleg","rightupleg","leftforearm","rightforearm")) mixamo++;
        if (strstr(s,"ikfoot") || strstr(s,"ikhand") || strstr(s,"pelvis") ||
            one_of(s,"thighl","thighr","calfl","calfr")) ue5++;
        if (one_of(s,"lupperarm","rupperarm","lforearm","rforearm")) legacy++;
    }
    if (ue5 >= 3) return MUEANG_RIG_UE5_MANNY;
    if (mixamo >= 3) return MUEANG_RIG_MIXAMO;
    if (legacy >= 3) return MUEANG_RIG_MUEANG_LEGACY;
    return MUEANG_RIG_UNKNOWN;
}

int HumanoidStandard_BuildMap(const char *const *names, int count,
                              int *out_ids, int capacity) {
    int mapped = 0;
    if (!names || !out_ids || count <= 0 || capacity < count) return -1;
    for (int i = 0; i < count; ++i) {
        out_ids[i] = HumanoidStandard_MapBoneName(names[i]);
        if (out_ids[i] >= 0) mapped++;
    }
    return mapped;
}

int HumanoidStandard_CountMapped(const int *ids, int count) {
    int seen[MUEANG_HUMANOID_BONE_COUNT] = {0};
    int total = 0;
    if (!ids || count <= 0) return 0;
    for (int i = 0; i < count; ++i) {
        if (ids[i] >= 0 && ids[i] < MUEANG_HUMANOID_BONE_COUNT && !seen[ids[i]]) {
            seen[ids[i]] = 1;
            total++;
        }
    }
    return total;
}
