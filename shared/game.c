
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

#define NPC_DETECTION_RANGE 10.0f
#define NPC_TAKEDOWN_RANGE 2.25f

/* Synchronization Point */
#define SYNC_TOWER_X (-22.0f)
#define SYNC_TOWER_Z (-20.0f)
#define SYNC_RAMP_START_X (-34.0f)
#define SYNC_RAMP_END_X (-22.0f)
#define SYNC_TOWER_HEIGHT 13.0f
#define SYNC_INTERACT_RANGE 2.8f

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

static const MueangLocation locations[MUEANG_LOCATION_COUNT] = {
    {"Hidden Waterfall", -32, 12, 5},
    {"Ancient Lost Temple", 24, 18, 5},
    {"Sky Cliffs", 0, 39, 5},
    {"Emerald River", 20, -20, 5},
    {"Misty Bamboo Forest", -25, 25, 5},
    {"Sacred Flower Valley", 30, 25, 5}
};

const MueangLocation *MueangGame_GetLocation(int index)
{
    return index >= 0 && index < MUEANG_LOCATION_COUNT ? &locations[index] : NULL;
}

static float stair_rise(void)
{
    return (ISLAND_TOP - terrain_height(0.0f, STAIR_START_Z))
         / (float)STAIR_COUNT;
}

static float sync_tower_base(void)
{
    return terrain_height(SYNC_TOWER_X, SYNC_TOWER_Z);
}

static float sync_ramp_height(float x, float z)
{
    if (z < SYNC_TOWER_Z - 1.25f ||
        z > SYNC_TOWER_Z + 1.25f ||
        x < SYNC_RAMP_START_X ||
        x > SYNC_RAMP_END_X)
        return -10000.0f;

    float t = (x - SYNC_RAMP_START_X) /
              (SYNC_RAMP_END_X - SYNC_RAMP_START_X);

    return terrain_height(x, z) + t * SYNC_TOWER_HEIGHT;
}

static int near_sync_tower_top(float x, float z, float y)
{
    float dx = x - SYNC_TOWER_X;
    float dz = z - SYNC_TOWER_Z;
    float d2 = dx * dx + dz * dz;
    float top = sync_tower_base() + SYNC_TOWER_HEIGHT;

    return d2 <= SYNC_INTERACT_RANGE * SYNC_INTERACT_RANGE &&
           y >= top - 1.2f;
}

static int in_haystack(float x, float z)
{
    float dx = x - HAY_X;
    float dz = z - HAY_Z;
    return dx * dx + dz * dz <= HAY_RADIUS * HAY_RADIUS;
}

static int in_water(float x, float z)
{
    float dx = x - WATER_X;
    float dz = z - WATER_Z;
    return dx * dx + dz * dz <= WATER_RADIUS * WATER_RADIUS;
}

static int near_tree(float x, float z, float radius)
{
    static const float trees[][2] = {
        {-12,-8}, {-5,-11}, {2,-13}, {11,-9}, {15,1},
        {-15,8}, {-8,14}, {9,13}, {16,17}
    };

    for (int i = 0; i < 9; ++i) {
        float dx = x - trees[i][0];
        float dz = z - trees[i][1];

        if (dx * dx + dz * dz <= radius * radius)
            return 1;
    }

    return 0;
}

static int near_house_wall(float x, float z)
{
    return x >= 2.9f && x <= 7.1f &&
           z >= -7.2f && z <= -2.8f;
}

static int near_grapple_anchor(float x, float z)
{
    return near_tree(x, z, 2.0f) ||
           (x >= 3.0f && x <= 7.0f &&
            z >= -7.0f && z <= -3.0f);
}

static int near_island_edge_in_direction(
    float x, float z, float dx, float dz)
{
    if (x < ISLAND_MIN_X || x > ISLAND_MAX_X ||
        z < ISLAND_MIN_Z || z > ISLAND_MAX_Z)
        return 0;

    if (dx > 0.25f && x > ISLAND_MAX_X - 2.3f) return 1;
    if (dx < -0.25f && x < ISLAND_MIN_X + 2.3f) return 1;
    if (dz > 0.25f && z > ISLAND_MAX_Z - 2.3f) return 1;
    if (dz < -0.25f && z < ISLAND_MIN_Z + 2.3f) return 1;

    return 0;
}

/* ================= NPC SYSTEM ================= */

static void respawn_player(MueangGame *game);

static float distance_xz(
    float x1, float z1, float x2, float z2)
{
    float dx = x1 - x2;
    float dz = z1 - z2;
    return sqrtf(dx * dx + dz * dz);
}

static void init_npcs(MueangGame *game)
{
    static const float positions[MUEANG_NPC_COUNT][2] = {
        {  7.0f,  2.0f },
        { -8.0f,  4.0f },
        { 12.0f, -8.0f },
        {-13.0f, -4.0f },
        {  3.0f,  8.0f },
        { -4.0f,  6.0f },
        { 10.0f,  5.0f },
        {-10.0f,  9.0f }
    };

    for (int i = 0; i < MUEANG_NPC_COUNT; ++i) {
        MueangNPC *n = &game->npcs[i];

        memset(n, 0, sizeof(*n));

        n->x = positions[i][0];
        n->z = positions[i][1];

        n->patrol_origin_x = n->x;
        n->patrol_origin_z = n->z;

        n->type = (i < 4) ? 0 : 1;
        n->alive = 1;
        n->state = 0;
        n->hit_points = i == 0 ? 2 : 1;
    }
}

static MueangNPC *find_nearest_target(
    MueangGame *game, float range)
{
    MueangNPC *best = NULL;
    float best_distance = range;

    for (int i = 0; i < MUEANG_NPC_COUNT; ++i) {
        MueangNPC *n = &game->npcs[i];

        if (!n->alive)
            continue;

        float d = distance_xz(
            game->player.x, game->player.z,
            n->x, n->z
        );

        if (d < best_distance) {
            best_distance = d;
            best = n;
        }
    }

    return best;
}

static void defeat_npc(MueangGame *game, MueangNPC *npc)
{
    if (!npc || !npc->alive)
        return;

    npc->alive = 0;
    npc->state = 3;
    npc->awareness = 0.0f;

    if (npc->type == 0)
        game->enemies_defeated++;
    if (npc == &game->npcs[0])
        game->rival_return_timer = 45.0f;
}

static void move_npc_towards(
    MueangNPC *n,
    float target_x,
    float target_z,
    float speed,
    float dt)
{
    float dx = target_x - n->x;
    float dz = target_z - n->z;
    float distance = sqrtf(dx * dx + dz * dz);

    if (distance < 0.001f)
        return;

    n->yaw = atan2f(dx, dz);

    float step = speed * dt;

    if (step > distance)
        step = distance;

    n->x += (dx / distance) * step;
    n->z += (dz / distance) * step;
}


static void update_npc_ai(MueangGame *game, float dt)
{
    MueangPlayer *p = &game->player;
    int guard_alerted = 0;

    for (int i = 0; i < MUEANG_NPC_COUNT; ++i) {
        MueangNPC *n = &game->npcs[i];

        if (!n->alive)
            continue;

        /* Civilians keep their existing patrol behavior. */
        if (n->type == 1) {
            n->patrol_timer += dt;

            n->x = n->patrol_origin_x +
                sinf(n->patrol_timer * 0.45f + (float)i) * 1.25f;

            n->z = n->patrol_origin_z +
                cosf(n->patrol_timer * 0.35f + (float)i) * 0.85f;

            continue;
        }

        float dx = p->x - n->x;
        float dz = p->z - n->z;
        float d = sqrtf(dx * dx + dz * dz);

        /* Detect nearby bodies. */
        for (int j = 0; j < MUEANG_NPC_COUNT; ++j) {
            const MueangNPC *body = &game->npcs[j];

            if (!body->alive && !body->body_hidden &&
                distance_xz(n->x, n->z, body->x, body->z) < 5.0f) {

                n->awareness = fmaxf(n->awareness, 0.75f);
                n->last_seen_x = p->x;
                n->last_seen_z = p->z;
                break;
            }
        }

        /* Player detection. */
        if (d < NPC_DETECTION_RANGE) {
            float detection = 1.0f - d / NPC_DETECTION_RANGE;

            if (p->stealth_enabled)
                detection *= 0.30f;

            if (p->crouching)
                detection *= 0.45f;

            if (game->crowd_blending)
                detection *= 0.25f;

            if (p->attacking || p->air_assassination)
                detection = 1.0f;

            n->awareness += detection * dt * 0.85f;

            n->last_seen_x = p->x;
            n->last_seen_z = p->z;
            n->lost_sight_timer = 0.0f;
        } else {
            n->awareness -= dt * 0.22f;

            if (n->state == 2 || n->state == 4)
                n->lost_sight_timer += dt;
        }

        n->awareness = clampf(n->awareness, 0.0f, 1.0f);

        /* Enter pursuit. */
        if (n->awareness >= 0.72f) {
            n->state = 2;
            n->search_timer = 0.0f;
        }

        /* State 2: Chase player or last known position. */
        if (n->state == 2) {
            guard_alerted = 1;

            if (d < NPC_DETECTION_RANGE) {
                n->last_seen_x = p->x;
                n->last_seen_z = p->z;
            }

            float target_x = n->last_seen_x;
            float target_z = n->last_seen_z;

            float target_distance = distance_xz(
                n->x, n->z, target_x, target_z
            );

            if (target_distance > 1.8f) {
                move_npc_towards(
                    n, target_x, target_z, 2.2f, dt
                );
            }

            /* Attack only when the player is actually nearby. */
            if (d < 2.0f) {
                n->attack_timer += dt;

                if (n->attack_timer > 1.2f) {
                    n->attack_timer = 0.0f;
                    p->detected = 1;

                    p->health -= 20.0f +
                        (i == 0 ? 5.0f * game->rival_rank : 0.0f);

                    if (p->health <= 0.0f) {
                        if (i == 0 && game->rival_rank < 5)
                            ++game->rival_rank;

                        respawn_player(game);
                        break;
                    }
                }
            } else {
                n->attack_timer = 0.0f;
            }

            /* Lost target: search after a short grace period. */
            if (n->lost_sight_timer > 1.0f &&
                n->awareness < 0.25f) {

                n->state = 4;
                n->search_timer = 0.0f;
                n->attack_timer = 0.0f;
            }

            continue;
        }

        /* State 1: Investigate suspicious activity. */
        if (n->awareness > 0.05f) {
            if (n->state == 0)
                n->state = 1;

            if (n->state == 1) {
                float investigate_distance = distance_xz(
                    n->x, n->z,
                    n->last_seen_x, n->last_seen_z
                );

                if (investigate_distance > 1.2f) {
                    move_npc_towards(
                        n,
                        n->last_seen_x,
                        n->last_seen_z,
                        1.5f,
                        dt
                    );
                }
            }

            continue;
        }

        /* State 4: Search around the last known position. */
        if (n->state == 4) {
            n->search_timer += dt;

            float phase = n->search_timer * 1.8f + (float)i;
            float radius = 1.2f + n->search_timer * 0.35f;

            float search_x = n->last_seen_x +
                cosf(phase) * radius;

            float search_z = n->last_seen_z +
                sinf(phase) * radius;

            move_npc_towards(
                n, search_x, search_z, 1.25f, dt
            );

            if (n->search_timer >= 4.5f) {
                n->state = 5;
                n->search_timer = 0.0f;
            }

            continue;
        }

        /* State 5: Walk naturally back to patrol origin. */
        if (n->state == 5) {
            float home_distance = distance_xz(
                n->x, n->z,
                n->patrol_origin_x,
                n->patrol_origin_z
            );

            if (home_distance > 0.35f) {
                move_npc_towards(
                    n,
                    n->patrol_origin_x,
                    n->patrol_origin_z,
                    1.8f,
                    dt
                );
            } else {
                n->x = n->patrol_origin_x;
                n->z = n->patrol_origin_z;
                n->state = 0;
                n->awareness = 0.0f;
                n->lost_sight_timer = 0.0f;
                n->search_timer = 0.0f;
            }

            continue;
        }

        /* State 0: Normal patrol. */
        if (n->state == 0) {
            n->patrol_timer += dt;

            float patrol_x = n->patrol_origin_x +
                sinf(n->patrol_timer * 0.35f + (float)i) * 1.8f;

            float patrol_z = n->patrol_origin_z +
                cosf(n->patrol_timer * 0.28f + (float)i) * 1.2f;

            move_npc_towards(
                n, patrol_x, patrol_z, 1.0f, dt
            );
        } else {
            /* Suspicion has faded: begin natural return. */
            n->state = 5;
            n->search_timer = 0.0f;
        }
    }

    game->alert_level = guard_alerted ? 1.0f : 0.0f;

    game->crowd_blending = 0;

    if (p->stealth_enabled && p->grounded) {
        for (int i = 0; i < MUEANG_NPC_COUNT; ++i) {
            MueangNPC *n = &game->npcs[i];

            if (n->alive && n->type == 1 &&
                distance_xz(p->x, p->z, n->x, n->z) < 2.8f) {

                game->crowd_blending = 1;
                break;
            }
        }
    }

    if (!guard_alerted)
        p->detected = 0;
}


/* ================= TERRAIN ================= */

float MueangGame_GroundHeight(float x, float z)
{
    float ground = terrain_height(x, z);

    if (fabsf(x) <= STAIR_X_HALF &&
        z >= STAIR_START_Z &&
        z < STAIR_START_Z + STAIR_STEP_Z * (float)STAIR_COUNT) {

        int step = (int)floorf(
            (z - STAIR_START_Z) / STAIR_STEP_Z
        );

        if (step < 0) step = 0;
        if (step >= STAIR_COUNT) step = STAIR_COUNT - 1;

        float stair_top =
            terrain_height(0.0f, STAIR_START_Z) +
            (float)(step + 1) * stair_rise();

        if (stair_top > ground)
            ground = stair_top;
    }

    if (x >= ISLAND_MIN_X && x <= ISLAND_MAX_X &&
        z >= ISLAND_MIN_Z && z <= ISLAND_MAX_Z &&
        ISLAND_TOP > ground)
        ground = ISLAND_TOP;

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

    /* Synchronization Point ramp collision. */
    {
        float ramp = sync_ramp_height(x, z);

        if (ramp > ground)
            ground = ramp;
    }

    /* Synchronization Point tower platform collision. */
    {
        float dx = x - SYNC_TOWER_X;
        float dz = z - SYNC_TOWER_Z;

        if (dx * dx + dz * dz <= 3.2f * 3.2f) {
            float tower_top =
                sync_tower_base() + SYNC_TOWER_HEIGHT;

            if (tower_top > ground)
                ground = tower_top;
        }
    }

    return ground;
}

/* ================= PLAYER RESET ================= */

static void respawn_player(MueangGame *game)
{
    MueangPlayer *p = &game->player;

    game->carried_body = -1;
/* Reset guard alert after player respawn */
for (int i = 0; i < MUEANG_NPC_COUNT; ++i) {
    MueangNPC *n = &game->npcs[i];

    if (n->alive && n->type == 0) {
    n->awareness = 0.0f;
    n->state = 0;
    n->attack_timer = 0.0f;
    n->last_seen_x = n->patrol_origin_x;
    n->last_seen_z = n->patrol_origin_z;
    n->lost_sight_timer = 0.0f;
    n->search_timer = 0.0f;
 }
}

game->alert_level = 0.0f;
game->crowd_blending = 0;
game->player.detected = 0;

    p->x = 0.0f;
    p->z = 0.0f;
    p->jump_height = 0.0f;
    p->vertical_velocity = 0.0f;
    p->surface_y = MueangGame_GroundHeight(0.0f, 0.0f);
    p->fall_start_y = p->surface_y;
    p->grounded = 1;
    p->health = 100.0f;

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

/* ================= GAME INIT ================= */

void MueangGame_Init(MueangGame *game)
{
    if (!game) return;

    memset(game, 0, sizeof(*game));
    game->carried_body = -1;

    init_npcs(game);

    game->player.move_speed = 3.0f;
    game->player.health = 100.0f;
    game->player.surface_y =
        MueangGame_GroundHeight(0.0f, 0.0f);

    game->player.fall_start_y = game->player.surface_y;
    game->player.grounded = 1;

    game->camera_pitch = 0.18f;
    game->camera_distance = 5.0f;
}

int MueangGame_Heal(MueangGame *game)
{
    if (!game || game->player.health >= 100.0f) return 0;
    game->player.health = fminf(100.0f, game->player.health + 40.0f);
    return 1;
}

void MueangGame_ToggleSight(MueangGame *game)
{
    if (game) game->sight_active = !game->sight_active;
}

int MueangGame_InteractBody(MueangGame *game)
{
    if (!game || !game->player.grounded) return 0;
    if (game->carried_body >= 0 && game->carried_body < MUEANG_NPC_COUNT) {
        MueangNPC *body = &game->npcs[game->carried_body];
        body->x = game->player.x;
        body->z = game->player.z;
        body->body_hidden = in_haystack(body->x, body->z) ||
                            near_tree(body->x, body->z, 1.8f);
        game->carried_body = -1;
        return 1;
    }

    int best = -1;
    float distance = 2.0f;
    for (int i = 0; i < MUEANG_NPC_COUNT; ++i) {
        MueangNPC *body = &game->npcs[i];
        if (body->alive || body->body_hidden) continue;
        float d = distance_xz(game->player.x, game->player.z, body->x, body->z);
        if (d < distance) { distance = d; best = i; }
    }
    if (best < 0) return 0;
    game->carried_body = best;
    return 1;
}

/* ================= MAIN UPDATE ================= */

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

    game->camera_pitch = clampf(
        game->camera_pitch + look_y * 1.5f * dt,
        -0.35f, 0.75f
    );

    float yaw = game->camera_yaw;
    float dir_x = cosf(yaw) * move_x + sinf(yaw) * move_y;
    float dir_z = -sinf(yaw) * move_x + cosf(yaw) * move_y;

    if (length > 0.05f)
        p->yaw = atan2f(move_x, move_y) + yaw;

    int jump_edge = jump_pressed && !p->jump_was_down;
    int action_edge = action_pressed && !p->action_was_down;

    p->jump_was_down = jump_pressed;
    p->action_was_down = action_pressed;

    /* Synchronization camera timer. */
    if (game->sync_timer > 0.0f) {
        game->sync_timer -= dt;

        if (game->sync_timer <= 0.0f) {
            game->sync_timer = 0.0f;
            game->sync_active = 0;
        }
    }

    /* A defeated rival returns stronger after the player leaves the area. */
    MueangNPC *rival = &game->npcs[0];
    if (!rival->alive && game->rival_return_timer > 0.0f) {
        game->rival_return_timer = fmaxf(0.0f, game->rival_return_timer - dt);
    }
    if (!rival->alive && game->rival_return_timer == 0.0f &&
        game->carried_body != 0 &&
        distance_xz(p->x, p->z, rival->patrol_origin_x, rival->patrol_origin_z) > 8.0f) {
        if (game->rival_rank < 5) ++game->rival_rank;
        rival->alive = 1;
        rival->body_hidden = 0;
        rival->x = rival->patrol_origin_x;
        rival->z = rival->patrol_origin_z;
        rival->hit_points = 2 + game->rival_rank;
        rival->state = 0;
        rival->awareness = 0.0f;
        rival->attack_timer = 0.0f;
    }

    /* Detect when player reaches the tower summit. */
    game->sync_prompt =
        (!game->sync_unlocked &&
         p->grounded &&
         near_sync_tower_top(
             p->x, p->z, p->surface_y
         ));

    /* Synchronization interaction takes priority. */
    if (action_edge && game->sync_prompt) {
        game->sync_unlocked = 1;
        game->sync_active = 1;
        game->sync_timer = 5.0f;
        action_edge = 0;
    }

    /* Combat animation timer. */
    if (p->combat_timer > 0.0f) {
        p->combat_timer -= dt;

        if (p->combat_timer <= 0.0f) {
            p->combat_timer = 0.0f;
            p->attacking = 0;
            p->air_assassination = 0;
            p->ledge_takedown = 0;
        }
    }

    if (p->roll_timer > 0.0f)
        p->roll_timer = fmaxf(0.0f, p->roll_timer - dt);

    if (p->slide_timer > 0.0f)
        p->slide_timer = fmaxf(0.0f, p->slide_timer - dt);

    if (p->leap_timer > 0.0f)
        p->leap_timer = fmaxf(0.0f, p->leap_timer - dt);

    if (p->wallrun_timer > 0.0f)
        p->wallrun_timer = fmaxf(0.0f, p->wallrun_timer - dt);

    if (p->grapple_timer > 0.0f)
        p->grapple_timer = fmaxf(0.0f, p->grapple_timer - dt);

    if (p->zipline_timer > 0.0f)
        p->zipline_timer = fmaxf(0.0f, p->zipline_timer - dt);

    if (p->pole_vault_timer > 0.0f)
        p->pole_vault_timer = fmaxf(0.0f, p->pole_vault_timer - dt);

    if (p->traversal_cooldown > 0.0f)
        p->traversal_cooldown -= dt;

    /* Stealth toggle and assassination. */
    if (action_edge) {
        MueangNPC *target =
            find_nearest_target(game, NPC_TAKEDOWN_RANGE);

        if (target) {
            p->attacking = 1;
            p->combat_timer = 0.65f;

            if (!p->grounded) {
                p->air_assassination = 1;
            } else if (p->surface_y > 1.5f) {
                p->ledge_takedown = 1;
            }

            if (--target->hit_points <= 0) {
                defeat_npc(game, target);
            } else {
                target->awareness = 1.0f;
                target->state = 2;
            }
            action_edge = 0;
        } else if (length < 0.12f && p->grounded) {
            p->stealth_enabled = !p->stealth_enabled;
            p->crouching = p->stealth_enabled;
            p->stealth_timer = 0.25f;
            action_edge = 0;
        }
    }

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

    /* Grapple. */
    if (action_edge && !p->grounded &&
        p->traversal_mode == 0 &&
        p->traversal_cooldown <= 0.0f &&
        near_grapple_anchor(p->x, p->z)) {

        p->traversal_mode = 2;
        p->grapple_timer = 1.35f;
        p->vertical_velocity = 3.2f;
        p->traversal_cooldown = 0.35f;
    }

    /* Zipline. */
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

    /* Pole vault. */
    if (jump_edge && sprint_pressed &&
        p->grounded && p->on_beam &&
        p->traversal_mode == 0) {

        p->traversal_mode = 4;
        p->pole_vault_timer = 0.85f;
        p->vertical_velocity = 9.2f;
        p->fall_start_y = p->surface_y;
        p->grounded = 0;
        p->traversal_cooldown = 0.35f;
    }

    /* Leap of Faith. */
    if (action_edge && jump_pressed &&
        p->grounded && p->surface_y > 8.0f &&
        length > 0.08f && p->traversal_mode == 0 &&
        near_island_edge_in_direction(
            p->x, p->z, dir_x, dir_z)) {

        p->grounded = 0;
        p->fall_start_y = p->surface_y;
        p->jump_height = 0.0f;
        p->vertical_velocity = -1.0f;
        p->leap_of_faith = 1;
        p->leap_timer = 1.80f;
        p->descent_active = 0;

    } else if (action_edge && p->grounded &&
               length > 0.15f &&
               p->traversal_mode == 0) {
        p->slide_timer = 0.72f;
    }

    /* Wall run. */
    if (jump_edge && sprint_pressed &&
        p->grounded && p->traversal_mode == 0 &&
        p->traversal_cooldown <= 0.0f &&
        length > 0.35f &&
        (near_house_wall(p->x, p->z) ||
         near_tree(p->x, p->z, 1.15f))) {

        p->traversal_mode = 1;
        p->wallrun_timer = 1.15f;
        p->wall_side = (p->x >= 0.0f) ? 1 : -1;
        p->grounded = 0;
        p->fall_start_y = p->surface_y;
        p->jump_height = 0.65f;
        p->vertical_velocity = 0.8f;
        p->traversal_cooldown = 0.45f;

    } else if (jump_edge && p->grounded &&
               !p->leap_of_faith &&
               p->traversal_mode == 0) {

        p->vertical_velocity = 7.2f;
        p->fall_start_y = p->surface_y;
        p->grounded = 0;
        p->descent_active = 0;
    }

    float speed = p->move_speed *
        (sprint_pressed ? 1.75f : 1.0f);

    if (p->slide_timer > 0.0f) speed *= 1.55f;
    if (p->on_beam) speed *= 0.62f;
    if (p->traversal_mode == 1) speed *= 1.8f;
    if (p->traversal_mode == 2) speed *= 1.45f;
    if (p->traversal_mode == 4) speed *= 2.0f;
    if (p->crouching) speed *= 0.55f;

    if (p->traversal_mode == 3) {
        p->x += 4.55f * dt;
        p->z += 7.2f * dt;
        p->jump_height -= 1.2f * dt;
        p->vertical_velocity = -1.2f;

        if (p->zipline_timer <= 0.0f) {
            p->traversal_mode = 0;
            p->zipline_timer = 0.0f;
        }

    } else if (p->traversal_mode == 2) {
        p->x += dir_x * speed * dt;
        p->z += dir_z * speed * dt;

        p->jump_height +=
            sinf((1.35f - p->grapple_timer) * 2.4f)
            * 2.6f * dt;

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

    float new_floor =
        MueangGame_GroundHeight(p->x, p->z);

    p->on_beam =
        fabsf(p->x) <= BEAM_HALF_WIDTH &&
        p->z >= BEAM_Z_MIN &&
        p->z <= BEAM_Z_MAX;

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

        } else if (p->traversal_mode == 2 ||
                   p->traversal_mode == 3) {
            /* Traversal modes control vertical movement. */

        } else if (p->descent_active && action_pressed) {
            p->vertical_velocity -= 5.0f * dt;

            if (p->vertical_velocity < -3.0f)
                p->vertical_velocity = -3.0f;

        } else {
            p->descent_active = 0;
            p->vertical_velocity -= 18.0f * dt;
        }

        if (p->traversal_mode != 2 &&
            p->traversal_mode != 3) {
            p->jump_height += p->vertical_velocity * dt;
        }

        float world_y = p->surface_y + p->jump_height;

        int still_over_launch_island =
            p->leap_of_faith &&
            new_floor > p->fall_start_y - 2.0f;

        if (p->traversal_mode == 1) {
            /* Wall run remains above the ground. */

        } else if (!still_over_launch_island &&
                   world_y <= new_floor) {

            float fall_distance =
                p->fall_start_y - new_floor;

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

    for (int i = 0; i < MUEANG_LOCATION_COUNT; ++i) {
        const MueangLocation *location = &locations[i];
        unsigned int bit = 1u << i;
        if (!(game->discovered_locations & bit) &&
            distance_xz(p->x, p->z, location->x, location->z) <= location->radius) {
            game->discovered_locations |= bit;
            ++game->discovered_count;
        }
    }

    if (game->carried_body >= 0 && game->carried_body < MUEANG_NPC_COUNT) {
        game->npcs[game->carried_body].x = p->x;
        game->npcs[game->carried_body].z = p->z;
    }

    /* NPC AI runs after player movement. */
    update_npc_ai(game, dt);
}
