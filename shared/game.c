
#include "game.h"
#include <math.h>

static float clampf(float v, float min, float max)
{
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

void MueangGame_Init(MueangGame *game)
{
    if (!game) return;

    game->player.x = 0.0f;
    game->player.z = 0.0f;
    game->player.yaw = 0.0f;
    game->player.move_speed = 3.0f;

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

    game->camera_yaw += look_x * 2.2f * dt;

    game->camera_pitch = clampf(
        game->camera_pitch + look_y * 1.5f * dt,
        -0.35f,
        0.75f
    );

    float yaw = game->camera_yaw;

    game->player.x +=
        (cosf(yaw) * move_x + sinf(yaw) * move_y)
        * game->player.move_speed * dt;

    game->player.z +=
        (-sinf(yaw) * move_x + cosf(yaw) * move_y)
        * game->player.move_speed * dt;

    if (length > 0.05f) {
        game->player.yaw = atan2f(move_x, move_y) + yaw;
    }
}
