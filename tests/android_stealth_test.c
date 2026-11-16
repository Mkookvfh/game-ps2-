#include "game.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    MueangGame game;
    MueangGame_Init(&game);
    for (int i = 0; i < MUEANG_NPC_COUNT; ++i) {
        game.npcs[i].alive = 0;
        game.npcs[i].body_hidden = 1;
    }
    game.npcs[0].alive = 1;
    game.npcs[0].type = 0;
    game.npcs[0].x = 10;
    game.npcs[0].z = 0;
    game.npcs[1].body_hidden = 0;
    game.npcs[1].x = 11;
    game.npcs[1].z = 0;
    MueangGame_Update(&game, 0, 0, 0, 0, 0, 0, 0, .05f);
    assert(game.npcs[0].state == 2);

    MueangGame_ToggleSight(&game);
    assert(game.sight_active);
    MueangGame_ToggleSight(&game);
    assert(!game.sight_active);
    game.player.x = game.npcs[1].x;
    game.player.z = game.npcs[1].z;
    assert(MueangGame_InteractBody(&game));
    assert(game.carried_body == 1);
    game.player.x = -12;
    game.player.z = -8;
    assert(MueangGame_InteractBody(&game));
    assert(game.carried_body == -1 && game.npcs[1].body_hidden);

    game.npcs[0].state = 0;
    game.npcs[0].awareness = 0;
    game.npcs[0].x = 10;
    game.npcs[0].z = 0;
    MueangGame_Update(&game, 0, 0, 0, 0, 0, 0, 0, .05f);
    assert(game.npcs[0].state == 0);
    assert(!MueangGame_InteractBody(&game));
    puts("sight and body hiding checks passed");
    return 0;
}
