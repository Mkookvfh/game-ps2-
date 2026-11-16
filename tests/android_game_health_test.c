#include "game.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    MueangGame game;
    MueangGame_Init(&game);
    assert(game.player.health == 100.0f);
    assert(!MueangGame_Heal(&game));
    for (int i = 1; i < MUEANG_NPC_COUNT; ++i) game.npcs[i].alive = 0;
    game.npcs[0].x = 1.0f;
    game.npcs[0].z = 0.0f;
    game.npcs[0].state = 2;
    game.npcs[0].awareness = 1.0f;
    for (int i = 0; i < 25; ++i)
        MueangGame_Update(&game, 0, 0, 0, 0, 0, 0, 0, 0.05f);
    assert(game.player.health == 80.0f);
    assert(MueangGame_Heal(&game));
    assert(game.player.health == 100.0f);
    game.player.health = 20.0f;
    game.player.x = 1.0f;
    game.player.z = 0.0f;
    game.npcs[0].x = 1.0f;
    game.npcs[0].z = 0.0f;
    game.npcs[0].attack_timer = 1.2f;
    MueangGame_Update(&game, 0, 0, 0, 0, 0, 0, 0, 0.05f);
    assert(game.player.health == 100.0f);
    assert(game.player.x == 0.0f && game.player.z == 0.0f);
    puts("health and respawn checks passed");
    return 0;
}
