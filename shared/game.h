
#ifndef MUEANG_SHARED_GAME_H
#define MUEANG_SHARED_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

#define MUEANG_NPC_COUNT 8
#define MUEANG_LOCATION_COUNT 6

typedef struct {
    const char *name;
    float x;
    float z;
    float radius;
} MueangLocation;

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
    float health;

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
    float last_seen_x;
    float last_seen_z;
    float lost_sight_timer;
    float search_timer;

    int type;
    int state;
    int alive;
    int body_hidden;
    int hit_points;
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
    int sight_active;
    int carried_body;
    int rival_rank;
    float rival_return_timer;
    unsigned int discovered_locations;
    int discovered_count;

    /* Synchronization Point */
    int sync_unlocked;
    int sync_active;
    int sync_prompt;
    float sync_timer;

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

/* Restore health by up to 40 points. Returns 1 if health changed. */
int MueangGame_Heal(MueangGame *game);

void MueangGame_ToggleSight(MueangGame *game);
/* Picks up a nearby body; a second interaction hides it near hay or trees,
 * otherwise drops it. Returns 1 when an interaction succeeds. */
int MueangGame_InteractBody(MueangGame *game);
const MueangLocation *MueangGame_GetLocation(int index);

#ifdef __cplusplus
}
#endif

#endif
