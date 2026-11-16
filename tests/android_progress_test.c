#include "progress.h"
#include <assert.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    assert(argc == 2);
    MueangGame game, restored;
    MueangResourceSystem resources, restored_resources;
    MueangGame_Init(&game);
    MueangResource_Init(&resources);
    resources.backpack_level = 1;
    resources.tool_level = 2;
    resources.inventory[MUEANG_ITEM_WOOD] = 140;
    resources.potions = 3;
    game.rival_rank = 3;
    game.discovered_locations = (1u << 0) | (1u << 4);
    assert(MueangProgress_Save(argv[1], &game, &resources));
    MueangGame_Init(&restored);
    MueangResource_Init(&restored_resources);
    assert(MueangProgress_Load(argv[1], &restored, &restored_resources));
    assert(restored_resources.inventory[MUEANG_ITEM_WOOD] == 140);
    assert(restored_resources.potions == 3 && restored_resources.tool_level == 2);
    assert(restored_resources.backpack_level == 1 && restored.rival_rank == 3);
    assert(restored.npcs[0].hit_points == 5 && restored.discovered_count == 2);
    assert(restored.discovered_locations == game.discovered_locations);

    FILE *bad = fopen(argv[1], "w");
    assert(bad);
    assert(fputs("MUEANG1 0 999999999999999999999999 0 0 0 0 0 0 0 0\n", bad) >= 0);
    assert(fclose(bad) == 0);
    assert(!MueangProgress_Load(argv[1], &restored, &restored_resources));
    assert(restored_resources.inventory[MUEANG_ITEM_WOOD] == 140 && restored.rival_rank == 3);
    assert(remove(argv[1]) == 0);
    puts("progress round-trip and corrupt-file checks passed");
    return 0;
}
