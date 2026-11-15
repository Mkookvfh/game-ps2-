
#ifndef MUEANG_SHARED_GAME_H
#define MUEANG_SHARED_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float x;
    float z;
    float yaw;
    float move_speed;
} MueangPlayer;

typedef struct {
    MueangPlayer player;
    float camera_yaw;
    float camera_pitch;
    float camera_distance;
} MueangGame;

void MueangGame_Init(MueangGame *game);

void MueangGame_Update(
    MueangGame *game,
    float move_x,
    float move_y,
    float look_x,
    float look_y,
    float dt
);

#ifdef __cplusplus
}
#endif

#endif
