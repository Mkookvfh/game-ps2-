#include "animation.h"
MueangAnimState Animation_SelectState(const MueangPlayer *p,int moving){
    if(!p) return MUEANG_ANIM_IDLE;
    if(p->attacking || p->air_assassination || p->ledge_takedown) return MUEANG_ANIM_ATTACK;
    if(p->roll_timer>0.0f) return MUEANG_ANIM_ROLL;
    if(p->slide_timer>0.0f) return MUEANG_ANIM_SLIDE;
    if(p->traversal_mode==1) return MUEANG_ANIM_WALL_RUN;
    if(p->traversal_mode==2 || p->traversal_mode==3) return MUEANG_ANIM_GRAPPLE;
    if(p->traversal_mode==4) return MUEANG_ANIM_VAULT;
    if(!p->grounded) return MUEANG_ANIM_JUMP;
    if(p->crouching) return MUEANG_ANIM_CROUCH;
    return moving?MUEANG_ANIM_WALK:MUEANG_ANIM_IDLE;
}
