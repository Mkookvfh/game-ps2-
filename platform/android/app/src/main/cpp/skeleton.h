#ifndef MUEANG_SKELETON_H
#define MUEANG_SKELETON_H
#include "animation.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MUEANG_SKELETON_BONE_COUNT 21
#define MUEANG_SKELETON_PART_COUNT 27
/* Builds the rigid-bone transform for one modular GLB mesh part. */
void Skeleton_BuildPartMatrix(int part,MueangAnimState state,float phase,int crouching,float out[16]);
const char *Skeleton_BoneName(int bone);
#ifdef __cplusplus
}
#endif
#endif
