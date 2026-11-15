
#ifndef MUEANG_SHARED_GAME_H
#define MUEANG_SHARED_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

#define MUEANG_NPC_COUNT 8

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
    float combat_timer;
    float stealth_timer;

    int grounded;
    int jump_was_down;
    int action_was_down;
    int descent_active;
    int leap_of_faith;
    int on_beam;
    int safe_landing;

    int traversal_mode;
    int wall_side;

    int stealth_enabled;
    int crouching;
    int attacking;
    int air_assassination;
    int ledge_takedown;
    int detected;
} MueangPlayer;

typedef struct {
    float x;
    float z;
    float yaw;
    float patrol_origin_x;
    float patrol_origin_z;
    float patrol_timer;
    float awareness;
    float attack_timer;

    int type;       /* 0 = guard, 1 = civilian */
    int state;      /* 0 = patrol, 1 = suspicious, 2 = alerted, 3 = defeated */
    int alive;
} MueangNPC;

typedef struct {
    MueangPlayer player;
    MueangNPC npcs[MUEANG_NPC_COUNT];

    float camera_yaw;
    float camera_pitch;
    float camera_distance;

    float alert_level;
    int enemies_defeated;
    int crowd_blending;
} MueangGame;

void MueangGame_Init(MueangGame *game);

void MueangGame_Update(
    MueangGame *game,
    float move_x,
    float move_y,
    float look_x,
    float look_y,
    int jump_pressed,
    int sprint_pressed,
    int action_pressed,
    float dt
);

float MueangGame_GroundHeight(float x, float z);

#ifdef __cplusplus
}
#endif

#endif
