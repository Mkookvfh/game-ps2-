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
    /* Keep the rival's return timer running during this test. */
    game.rival_return_timer = 45;
    assert(!MueangGame_GetLocation(-1));
    assert(!MueangGame_GetLocation(MUEANG_LOCATION_COUNT));
    for (int i = 0; i < MUEANG_LOCATION_COUNT; ++i) {
        const MueangLocation *place = MueangGame_GetLocation(i);
        assert(place && place->name);
        game.player.x = place->x;
        game.player.z = place->z;
        game.player.surface_y = MueangGame_GroundHeight(place->x, place->z);
        game.player.grounded = 1;
        MueangGame_Update(&game, 0, 0, 0, 0, 0, 0, 0, 0);
        assert(game.discovered_locations & (1u << i));
        assert(game.discovered_count == i + 1);
        MueangGame_Update(&game, 0, 0, 0, 0, 0, 0, 0, 0);
        assert(game.discovered_count == i + 1);
    }
    puts("location discovery checks passed");
    return 0;
}
