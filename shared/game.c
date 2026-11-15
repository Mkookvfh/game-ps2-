#include "game.h"
#include <math.h>

static float clampf(float v, float min, float max)
{
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

/* Must match the terrain height formula in renderer3d.c. */
static float terrain_height(float x, float z)
{
    return sinf(x * 0.075f) * 1.5f
         + cosf(z * 0.09f) * 1.2f
         + sinf((x + z) * 0.045f) * 1.0f;
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
    game->player.grounded = 1;
    game->player.jump_was_down = 0;

    game->camera_yaw = 0.0f;
    game->camera_pitch = 0.18f;
    game->camera_distance = 5.0f;
}

void MueangGame_Update(
    MueangGame *game,
    float move_x,
    float move_y,
    float look_x,
    float look_y,
    int jump_pressed,
    int sprint_pressed,
    float dt
)
{
    if (!game) return;

    dt = clampf(dt, 0.0f, 0.05f);

    float length = sqrtf(
        move_x * move_x + move_y * move_y
    );

    if (length > 1.0f) {
        move_x /= length;
        move_y /= length;
    }

    /* Camera */
    game->camera_yaw += look_x * 2.2f * dt;

    game->camera_pitch = clampf(
        game->camera_pitch + look_y * 1.5f * dt,
        -0.35f,
        0.75f
    );

    /* Movement and sprint */
    float speed = game->player.move_speed;

    if (sprint_pressed) {
        speed *= 1.75f;
    }

    float yaw = game->camera_yaw;

    game->player.x +=
        (cosf(yaw) * move_x + sinf(yaw) * move_y)
        * speed * dt;

    game->player.z +=
        (-sinf(yaw) * move_x + cosf(yaw) * move_y)
        * speed * dt;

    if (length > 0.05f) {
        game->player.yaw = atan2f(move_x, move_y) + yaw;
    }

    /* Jump: trigger once when the button is first pressed. */
    if (jump_pressed &&
        !game->player.jump_was_down &&
        game->player.grounded) {

        game->player.vertical_velocity = 7.2f;
        game->player.grounded = 0;
    }

    game->player.jump_was_down = jump_pressed;

    /* Gravity and landing */
    if (!game->player.grounded) {
        game->player.vertical_velocity -= 18.0f * dt;

        game->player.jump_height +=
            game->player.vertical_velocity * dt;

        if (game->player.jump_height <= 0.0f) {
            game->player.jump_height = 0.0f;
            game->player.vertical_velocity = 0.0f;
            game->player.grounded = 1;
        }
    }

    /* Keep the player above the terrain. */
    (void)terrain_height(game->player.x, game->player.z);
}