#include "game.h"
#include <math.h>

#define ISLAND_TOP 20.10f
#define ISLAND_MIN_X (-6.70f)
#define ISLAND_MAX_X ( 6.70f)
#define ISLAND_MIN_Z 33.45f
#define ISLAND_MAX_Z 44.45f
#define STAIR_X_HALF 2.15f
#define STAIR_START_Z 10.0f
#define STAIR_STEP_Z 0.84f
#define STAIR_COUNT 28

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float terrain_height(float x, float z)
{
    return sinf(x * 0.075f) * 1.5f
         + cosf(z * 0.09f) * 1.2f
         + sinf((x + z) * 0.045f);
}

static float stair_rise(void)
{
    return (ISLAND_TOP - terrain_height(0.0f, STAIR_START_Z))
         / (float)STAIR_COUNT;
}

float MueangGame_GroundHeight(float x, float z)
{
    float ground = terrain_height(x, z);

    if (fabsf(x) <= STAIR_X_HALF &&
        z >= STAIR_START_Z &&
        z < STAIR_START_Z + STAIR_STEP_Z * (float)STAIR_COUNT) {
        int step = (int)floorf((z - STAIR_START_Z) / STAIR_STEP_Z);
        if (step < 0) step = 0;
        if (step >= STAIR_COUNT) step = STAIR_COUNT - 1;
        float stair_top = terrain_height(0.0f, STAIR_START_Z)
                        + (float)(step + 1) * stair_rise();
        if (stair_top > ground) ground = stair_top;
    }

    if (x >= ISLAND_MIN_X && x <= ISLAND_MAX_X &&
        z >= ISLAND_MIN_Z && z <= ISLAND_MAX_Z &&
        ISLAND_TOP > ground) {
        ground = ISLAND_TOP;
    }
    return ground;
}

void MueangGame_Init(MueangGame *game)
{
    if (!game) return;
    game->player.x = 0.0f;
    game->player.z = 0.0f;
    game->player.yaw = 0.0f;
    game->player.move_speed = 3.0f;
    game->player.jump_height = 0.0f;
    game->player.vertical_velocity = 0.0f;
    game->player.surface_y = MueangGame_GroundHeight(0.0f, 0.0f);
    game->player.grounded = 1;
    game->player.jump_was_down = 0;
    game->camera_yaw = 0.0f;
    game->camera_pitch = 0.18f;
    game->camera_distance = 5.0f;
}

void MueangGame_Update(
    MueangGame *game,
    float move_x, float move_y,
    float look_x, float look_y,
    int jump_pressed, int sprint_pressed,
    float dt
)
{
    if (!game) return;
    dt = clampf(dt, 0.0f, 0.05f);

    float length = sqrtf(move_x * move_x + move_y * move_y);
    if (length > 1.0f) {
        move_x /= length;
        move_y /= length;
        length = 1.0f;
    }

    game->camera_yaw += look_x * 2.2f * dt;
    game->camera_pitch = clampf(game->camera_pitch + look_y * 1.5f * dt,
                                -0.35f, 0.75f);

    float speed = game->player.move_speed * (sprint_pressed ? 1.75f : 1.0f);
    float yaw = game->camera_yaw;
    game->player.x += (cosf(yaw) * move_x + sinf(yaw) * move_y) * speed * dt;
    game->player.z += (-sinf(yaw) * move_x + cosf(yaw) * move_y) * speed * dt;

    if (length > 0.05f)
        game->player.yaw = atan2f(move_x, move_y) + yaw;

    float new_floor = MueangGame_GroundHeight(game->player.x, game->player.z);

    if (jump_pressed && !game->player.jump_was_down && game->player.grounded) {
        game->player.vertical_velocity = 7.2f;
        game->player.grounded = 0;
    }
    game->player.jump_was_down = jump_pressed;

    if (game->player.grounded) {
        /* Step upward with the staircase; walking off a ledge starts a real fall. */
        if (new_floor < game->player.surface_y - 0.25f) {
            game->player.jump_height = game->player.surface_y - new_floor;
            game->player.vertical_velocity = 0.0f;
            game->player.grounded = 0;
        } else {
            game->player.surface_y = new_floor;
            game->player.jump_height = 0.0f;
        }
    }

    if (!game->player.grounded) {
        game->player.vertical_velocity -= 18.0f * dt;
        game->player.jump_height += game->player.vertical_velocity * dt;

        if (game->player.jump_height <= 0.0f) {
            game->player.jump_height = 0.0f;
            game->player.vertical_velocity = 0.0f;
            game->player.grounded = 1;
            game->player.surface_y = new_floor;
        }
    }

    /* Falling off the high island triggers a respawn before reaching the lower terrain. */
    if (!game->player.grounded &&
        game->player.surface_y > 10.0f &&
        new_floor < 10.0f &&
        game->player.jump_height < 2.0f) {
        game->player.x = 0.0f;
        game->player.z = 0.0f;
        game->player.jump_height = 0.0f;
        game->player.vertical_velocity = 0.0f;
        game->player.surface_y = MueangGame_GroundHeight(0.0f, 0.0f);
        game->player.grounded = 1;
        game->player.yaw = 0.0f;
        game->camera_yaw = 0.0f;
    }
}
