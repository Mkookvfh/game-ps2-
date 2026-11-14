#include "combat.h"
#include "player.h"
#include <stdio.h>
#include <math.h>

void Combat_PlayFinisher(int finisher_id){
    switch(finisher_id){
        case FINISHER_BACK_SILENT:
            printf("[เกม] ทำท่าไม้ตายด้านหลังแบบเงียบ\n");
            break;
        case FINISHER_BACK_FAST:
            printf("[เกม] ทำท่าไม้ตายด้านหลังแบบเร่งด่วน\n");
            break;
        case FINISHER_SIDE:
            printf("[เกม] ทำท่าไม้ตายจากด้านข้าง\n");
            break;
        case FINISHER_GRASS_HIDE:
            printf("[เกม] ทำท่าไม้ตายลากศัตรูลงกองหญ้า\n");
            break;
    }
    Combat_PostKillLookAround();
}

void Combat_PostKillLookAround(){
    Player_SetAnim(ANIM_LOOK_AROUND);
    printf("[เกม] ตัวละครหันซ้าย‑ขวาเช็คบริเวณ\n");
}

int Combat_CheckCanSilentKill(float px,float pz,float nx,float nz,float nrot){
    (void)nrot;
    float dist = sqrtf( (px-nx)*(px-nx)+(pz-nz)*(pz-nz) );
    if(dist > 2.0f) return 0;
    return 1;
}
