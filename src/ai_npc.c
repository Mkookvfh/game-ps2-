#include "ai_npc.h"
#include <math.h>

void NPC_Update(NPCGuard *g, float px, float pz, float delta){
    (void)delta;
    if(g->alive==0) return;
    float dist= sqrtf((g->x‑px)*(g->x‑px)+(g->z‑pz)*(g->z‑pz));
    if(dist<4.0f){
        g->alert_level=1;
    }else if(dist<7.0f){
        g->alert_level=0;
    }
}
