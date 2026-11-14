#ifndef AI_NPC_H
#define AI_NPC_H

typedef struct{
    float x,y,z;
    float rot;
    int alive;
    int alert_level;
}NPCGuard;

void NPC_Update(NPCGuard *g, float px, float pz, float delta);

#endif
