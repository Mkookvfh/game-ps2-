#include "game.h"
#include <math.h>
#include <string.h>

#define ISLAND_TOP 20.10f
#define ISLAND_MIN_X (-6.70f)
#define ISLAND_MAX_X ( 6.70f)
#define ISLAND_MIN_Z 33.45f
#define ISLAND_MAX_Z 44.45f
#define STAIR_X_HALF 2.15f
#define STAIR_START_Z 10.0f
#define STAIR_STEP_Z 0.84f
#define STAIR_COUNT 28
#define BEAM_X 0.0f
#define BEAM_Z_MIN 0.45f
#define BEAM_Z_MAX 8.55f
#define BEAM_HALF_WIDTH 0.28f
#define BEAM_Y 0.66f
#define HAY_X 0.0f
#define HAY_Z 54.0f
#define HAY_RADIUS 2.8f
#define WATER_X 10.0f
#define WATER_Z 51.0f
#define WATER_RADIUS 3.5f

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

static int in_haystack(float x, float z)
{
    float dx = x - HAY_X, dz = z - HAY_Z;
    return dx * dx + dz * dz <= HAY_RADIUS * HAY_RADIUS;
}

static int in_water(float x, float z)
{
    float dx = x - WATER_X, dz = z - WATER_Z;
    return dx * dx + dz * dz <= WATER_RADIUS * WATER_RADIUS;
}

static int near_tree(float x, float z, float radius)
{
    static const float trees[][2] = {
        {-12,-8}, {-5,-11}, {2,-13}, {11,-9}, {15,1},
        {-15,8}, {-8,14}, {9,13}, {16,17}
    };
    for (int i = 0; i < 9; ++i) {
        float dx = x - trees[i][0], dz = z - trees[i][1];
        if (dx * dx + dz * dz <= radius * radius) return 1;
    }
    return 0;
}

/* The ancient house is the first wall-run training surface. */
static int near_house_wall(float x, float z)
{
    return x >= 2.9f && x <= 7.1f && z >= -7.2f && z <= -2.8f;
}

static int near_grapple_anchor(float x, float z)
{
    return near_tree(x, z, 2.0f) ||
           (x >= 3.0f && x <= 7.0f && z >= -7.0f && z <= -3.0f);
}

static int near_island_edge_in_direction(float x, float z, float dx, float dz)
{
    if (x < ISLAND_MIN_X || x > ISLAND_MAX_X ||
        z < ISLAND_MIN_Z || z > ISLAND_MAX_Z) return 0;
    if (dx > 0.25f && x > ISLAND_MAX_X - 2.3f) return 1;
    if (dx < -0.25f && x < ISLAND_MIN_X + 2.3f) return 1;
    if (dz > 0.25f && z > ISLAND_MAX_Z - 2.3f) return 1;
    if (dz < -0.25f && z < ISLAND_MIN_Z + 2.3f) return 1;
    return 0;
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
        ISLAND_TOP > ground) ground = ISLAND_TOP;

    if (fabsf(x - BEAM_X) <= BEAM_HALF_WIDTH &&
        z >= BEAM_Z_MIN && z <= BEAM_Z_MAX) {
        float beam_y = terrain_height(BEAM_X, 4.5f) + BEAM_Y;
        if (beam_y > ground) ground = beam_y;
    }

    if (in_haystack(x, z)) {
        float hay_top = terrain_height(x, z) + 1.375f;
        if (hay_top > ground) ground = hay_top;
    }
    if (in_water(x, z)) {
        float water_top = terrain_height(x, z) + 0.24f;
        if (water_top > ground) ground = water_top;
    }
    return ground;
}

static void respawn_player(MueangGame *game)
{
    MueangPlayer *p = &game->player;
    p->x = 0.0f;
    p->z = 0.0f;
    p->jump_height = 0.0f;
    p->vertical_velocity = 0.0f;
    p->surface_y = MueangGame_GroundHeight(0.0f, 0.0f);
    p->fall_start_y = p->surface_y;
    p->grounded = 1;
    p->descent_active = 0;
    p->leap_of_faith = 0;
    p->roll_timer = 0.0f;
    p->slide_timer = 0.0f;
    p->safe_landing = 0;
    p->on_beam = 0;
    p->traversal_mode = 0;
    p->wallrun_timer = 0.0f;
    p->grapple_timer = 0.0f;
    p->zipline_timer = 0.0f;
    p->pole_vault_timer = 0.0f;
    p->traversal_cooldown = 0.7f;
    p->yaw = 0.0f;
    game->camera_yaw = 0.0f;
}

void MueangGame_Init(MueangGame *game)
{
    if (!game) return;
    memset(game, 0, sizeof(*game));
    game->player.move_speed = 3.0f;
    game->player.surface_y = MueangGame_GroundHeight(0.0f, 0.0f);
    game->player.fall_start_y = game->player.surface_y;
    game->player.grounded = 1;
    game->camera_pitch = 0.18f;
    game->camera_distance = 5.0f;
}

void MueangGame_Update(
    MueangGame *game,
    float move_x, float move_y,
    float look_x, float look_y,
    int jump_pressed, int sprint_pressed, int action_pressed,
    float dt
)
{
    if (!game) return;
    MueangPlayer *p = &game->player;
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

    float yaw = game->camera_yaw;
    float dir_x = cosf(yaw) * move_x + sinf(yaw) * move_y;
    float dir_z = -sinf(yaw) * move_x + cosf(yaw) * move_y;
    if (length > 0.05f) p->yaw = atan2f(move_x, move_y) + yaw;

    int jump_edge = jump_pressed && !p->jump_was_down;
    int action_edge = action_pressed && !p->action_was_down;
    p->jump_was_down = jump_pressed;
    p->action_was_down = action_pressed;

    if (p->roll_timer > 0.0f) p->roll_timer = fmaxf(0.0f, p->roll_timer - dt);
    if (p->slide_timer > 0.0f) p->slide_timer = fmaxf(0.0f, p->slide_timer - dt);
    if (p->leap_timer > 0.0f) p->leap_timer = fmaxf(0.0f, p->leap_timer - dt);
    if (p->wallrun_timer > 0.0f) p->wallrun_timer = fmaxf(0.0f, p->wallrun_timer - dt);
    if (p->grapple_timer > 0.0f) p->grapple_timer = fmaxf(0.0f, p->grapple_timer - dt);
    if (p->zipline_timer > 0.0f) p->zipline_timer = fmaxf(0.0f, p->zipline_timer - dt);
    if (p->pole_vault_timer > 0.0f) p->pole_vault_timer = fmaxf(0.0f, p->pole_vault_timer - dt);
    if (p->traversal_cooldown > 0.0f) p->traversal_cooldown -= dt;

    /* Release or jump out of a wall run. */
    if (p->traversal_mode == 1) {
        if (jump_edge) {
            p->traversal_mode = 0;
            p->wallrun_timer = 0.0f;
            p->vertical_velocity = 7.8f;
            p->jump_height += 0.12f;
            p->x += (float)p->wall_side * 0.55f;
            p->traversal_cooldown = 0.25f;
        } else if (p->wallrun_timer <= 0.0f || length < 0.08f) {
            p->traversal_mode = 0;
        }
    }

    /* Grapple: tap Action while airborne beside a tree or the ancient house. */
    if (action_edge && !p->grounded && p->traversal_mode == 0 &&
        p->traversal_cooldown <= 0.0f &&
        near_grapple_anchor(p->x, p->z)) {
        p->traversal_mode = 2;
        p->grapple_timer = 1.35f;
        p->vertical_velocity = 3.2f;
        p->traversal_cooldown = 0.35f;
    }

    /* Zipline: hold Sprint and tap Action at the far end of the island stairs. */
    if (action_edge && sprint_pressed && p->grounded &&
        p->traversal_mode == 0 &&
        p->x >= -2.5f && p->x <= 2.5f &&
        p->z >= 31.5f && p->z <= 35.2f) {
        p->traversal_mode = 3;
        p->zipline_timer = 2.2f;
        p->grounded = 0;
        p->fall_start_y = p->surface_y;
        p->jump_height = 0.0f;
        p->vertical_velocity = -1.0f;
        p->traversal_cooldown = 0.5f;
    }

    /* Pole vault: Jump + Sprint on the balance beam gives a long forward vault. */
    if (jump_edge && sprint_pressed && p->grounded && p->on_beam &&
        p->traversal_mode == 0) {
        p->traversal_mode = 4;
        p->pole_vault_timer = 0.85f;
        p->vertical_velocity = 9.2f;
        p->fall_start_y = p->surface_y;
        p->grounded = 0;
        p->traversal_cooldown = 0.35f;
    }

    /* Leap of Faith: Jump + Action at an island edge, facing outward. */
    if (action_edge && jump_pressed && p->grounded && p->surface_y > 8.0f &&
        length > 0.08f && p->traversal_mode == 0 &&
        near_island_edge_in_direction(p->x, p->z, dir_x, dir_z)) {
        p->grounded = 0;
        p->fall_start_y = p->surface_y;
        p->jump_height = 0.0f;
        p->vertical_velocity = -1.0f;
        p->leap_of_faith = 1;
        p->leap_timer = 1.80f;
        p->descent_active = 0;
    } else if (action_edge && p->grounded && length > 0.15f &&
               p->traversal_mode == 0) {
        p->slide_timer = 0.72f;
    }

    /* Wall run starts with Sprint + Jump while moving beside a wall/tree. */
    if (jump_edge && sprint_pressed && p->grounded &&
        p->traversal_mode == 0 && p->traversal_cooldown <= 0.0f &&
        length > 0.35f &&
        (near_house_wall(p->x, p->z) || near_tree(p->x, p->z, 1.15f))) {
        p->traversal_mode = 1;
        p->wallrun_timer = 1.15f;
        p->wall_side = (p->x >= 0.0f) ? 1 : -1;
        p->grounded = 0;
        p->fall_start_y = p->surface_y;
        p->jump_height = 0.65f;
        p->vertical_velocity = 0.8f;
        p->traversal_cooldown = 0.45f;
    } else if (jump_edge && p->grounded && !p->leap_of_faith &&
               p->traversal_mode == 0) {
        p->vertical_velocity = 7.2f;
        p->fall_start_y = p->surface_y;
        p->grounded = 0;
        p->descent_active = 0;
    }

    float speed = p->move_speed * (sprint_pressed ? 1.75f : 1.0f);
    if (p->slide_timer > 0.0f) speed *= 1.55f;
    if (p->on_beam) speed *= 0.62f;
    if (p->traversal_mode == 1) speed *= 1.8f;
    if (p->traversal_mode == 2) speed *= 1.45f;
    if (p->traversal_mode == 4) speed *= 2.0f;

    if (p->traversal_mode == 3) {
        /* Fixed downhill zipline route from island stair landing toward the pool. */
        float t = clampf((2.2f - p->zipline_timer) / 2.2f, 0.0f, 1.0f);
        float start_x = p->x;
        float start_z = p->z;
        (void)start_x;
        (void)start_z;
        p->x += 4.55f * dt;
        p->z += 7.2f * dt;
        p->jump_height -= 1.2f * dt;
        p->vertical_velocity = -1.2f;
        if (t > 0.88f || p->zipline_timer <= 0.0f) {
            p->traversal_mode = 0;
            p->zipline_timer = 0.0f;
        }
    } else if (p->traversal_mode == 2) {
        p->x += dir_x * speed * dt;
        p->z += dir_z * speed * dt;
        p->jump_height += (sinf((1.35f - p->grapple_timer) * 2.4f) * 2.6f) * dt;
        p->vertical_velocity = 0.0f;
        if (p->grapple_timer <= 0.0f || jump_edge) {
            p->traversal_mode = 0;
            p->vertical_velocity = 2.8f;
        }
    } else {
        if (p->leap_of_faith && p->leap_timer > 0.0f) {
            p->x += dir_x * 7.0f * dt;
            p->z += dir_z * 7.0f * dt;
        } else {
            p->x += dir_x * speed * dt;
            p->z += dir_z * speed * dt;
        }
    }

    float new_floor = MueangGame_GroundHeight(p->x, p->z);
    int on_beam_now = fabsf(p->x) <= BEAM_HALF_WIDTH &&
                      p->z >= BEAM_Z_MIN && p->z <= BEAM_Z_MAX;
    p->on_beam = on_beam_now;

    if (p->grounded) {
        if (new_floor < p->surface_y - 0.25f) {
            p->fall_start_y = p->surface_y;
            p->vertical_velocity = 0.0f;
            p->grounded = 0;
            p->jump_height = 0.0f;
            p->descent_active = action_pressed;
        } else {
            p->surface_y = new_floor;
            p->jump_height = 0.0f;
        }
    }

    if (!p->grounded) {
        if (p->traversal_mode == 1) {
            p->jump_height += 0.18f * dt;
            p->vertical_velocity = 0.0f;
        } else if (p->traversal_mode == 2 || p->traversal_mode == 3) {
            /* These modes control their own vertical movement above. */
        } else if (p->descent_active && action_pressed) {
            p->vertical_velocity -= 5.0f * dt;
            if (p->vertical_velocity < -3.0f) p->vertical_velocity = -3.0f;
        } else {
            p->descent_active = 0;
            p->vertical_velocity -= 18.0f * dt;
        }

        if (p->traversal_mode != 2 && p->traversal_mode != 3)
            p->jump_height += p->vertical_velocity * dt;

        float world_y = p->surface_y + p->jump_height;
        int still_over_launch_island = p->leap_of_faith &&
            new_floor > p->fall_start_y - 2.0f;

        if (p->traversal_mode == 1) {
            /* Wall-run is held above ground; it can be exited with Jump. */
        } else if (!still_over_launch_island && world_y <= new_floor) {
            float fall_distance = p->fall_start_y - new_floor;
            int hay = in_haystack(p->x, p->z);
            int water = in_water(p->x, p->z);
            int controlled = p->descent_active;
            p->safe_landing = hay || water || controlled;

            p->surface_y = new_floor;
            p->jump_height = 0.0f;
            p->vertical_velocity = 0.0f;
            p->grounded = 1;
            p->descent_active = 0;
            p->leap_of_faith = 0;
            p->leap_timer = 0.0f;
            p->traversal_mode = 0;
            p->grapple_timer = 0.0f;
            p->zipline_timer = 0.0f;
            p->pole_vault_timer = 0.0f;

            if (p->safe_landing) {
                p->roll_timer = controlled ? 0.55f : 0.45f;
            } else if (fall_distance > 8.0f) {
                respawn_player(game);
            } else if (fall_distance > 1.5f) {
                p->roll_timer = 0.55f;
            }
        } else if (world_y < -12.0f) {
            respawn_player(game);
        }
    }

    if (p->traversal_mode == 4) {
        if (p->pole_vault_timer <= 0.0f && !p->grounded)
            p->traversal_mode = 0;
    }
}
