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
    float jump_height;
    float vertical_velocity;
    float surface_y;
    float fall_start_y;
    float roll_timer;
    float slide_timer;
    float leap_timer;
    float wallrun_timer;
    float grapple_timer;
    float zipline_timer;
    float pole_vault_timer;
    float traversal_cooldown;
    int grounded;
    int jump_was_down;
    int action_was_down;
    int descent_active;
    int leap_of_faith;
    int on_beam;
    int safe_landing;
    int traversal_mode; /* 0=normal, 1=wall-run, 2=grapple swing, 3=zipline, 4=pole-vault */
    int wall_side;
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
    float move_x, float move_y,
    float look_x, float look_y,
    int jump_pressed, int sprint_pressed, int action_pressed,
    float dt
);
float MueangGame_GroundHeight(float x, float z);

#ifdef __cplusplus
}
#endif
#endif
