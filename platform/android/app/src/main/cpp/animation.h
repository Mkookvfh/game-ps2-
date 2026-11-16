#ifndef MUEANG_ANIMATION_H
#define MUEANG_ANIMATION_H
#include "game.h"
typedef enum {
    MUEANG_ANIM_IDLE=0,
    MUEANG_ANIM_WALK,
    MUEANG_ANIM_JUMP,
    MUEANG_ANIM_ROLL,
    MUEANG_ANIM_SLIDE,
    MUEANG_ANIM_WALL_RUN,
    MUEANG_ANIM_GRAPPLE,
    MUEANG_ANIM_VAULT,
    MUEANG_ANIM_ATTACK,
    MUEANG_ANIM_CROUCH
} MueangAnimState;
MueangAnimState Animation_SelectState(const MueangPlayer *player,int moving);
#endif
