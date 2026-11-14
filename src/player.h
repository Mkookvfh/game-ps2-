#ifndef PLAYER_H
#define PLAYER_H

#include <math.h>

#define ANIM_IDLE        0
#define ANIM_WALK        1
#define ANIM_RUN         2
#define ANIM_CROUCH      3
#define ANIM_CLIMB       4
#define ANIM_LOOK_AROUND 5

typedef struct{
    float x,y,z;
    float rot;
    int anim_state;
    int is_crouch;
    int is_climbing;
}Player;

extern Player player;

void Player_Init();
void Player_Update(float delta);
void Player_SetAnim(int anim);

#endif
