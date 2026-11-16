#include "game.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    MueangGame game;
    MueangGame_Init(&game);
    for (int i = 1; i < MUEANG_NPC_COUNT; ++i) {
        game.npcs[i].alive = 0;
        game.npcs[i].body_hidden = 1;
    }
    game.npcs[0].x = 1;
    game.npcs[0].z = 0;
    MueangGame_Update(&game, 0, 0, 0, 0, 0, 0, 1, 0);
    assert(game.npcs[0].alive && game.npcs[0].hit_points == 1);
    MueangGame_Update(&game, 0, 0, 0, 0, 0, 0, 0, 0);
    MueangGame_Update(&game, 0, 0, 0, 0, 0, 0, 1, 0);
    assert(!game.npcs[0].alive && game.rival_return_timer == 45.0f);
    assert(game.enemies_defeated == 1);
    game.player.x = -40;
    game.player.z = -40;
    game.player.surface_y = MueangGame_GroundHeight(-40, -40);
    game.rival_return_timer = 0.05f;
    MueangGame_Update(&game, 0, 0, 0, 0, 0, 0, 0, 0.05f);
    assert(game.npcs[0].alive && game.rival_rank == 1 && game.npcs[0].hit_points == 3);
    puts("rival return and rank checks passed");
    return 0;
}
