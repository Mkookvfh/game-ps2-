#include "resource_system.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void tick(MueangResourceSystem *s, float x, float z, int held, int frames)
{
    for (int i = 0; i < frames; ++i)
        MueangResource_Update(s, x, z, held, 0.05f);
}

int main(void)
{
    MueangResourceSystem s;
    MueangResource_Init(&s);
    assert(MueangResource_GetCapacity(&s) == 99);
    tick(&s, -12, -8, 1, 10);
    assert(MueangResource_GetNearestNode(&s) == 0);
    assert(MueangResource_GetProgress(&s) > 0.0f);
    tick(&s, 0, 0, 0, 1);
    assert(MueangResource_GetProgress(&s) == 0.0f);
    tick(&s, -12, -8, 1, 26);
    assert(s.inventory[MUEANG_ITEM_WOOD] == 2 && !s.nodes[0].active);
    tick(&s, 0, 0, 0, 901);
    assert(s.nodes[0].active);

    s.inventory[MUEANG_ITEM_HERB] = 2;
    s.inventory[MUEANG_ITEM_FIBER] = 0;
    assert(!MueangResource_Craft(&s, MUEANG_RECIPE_POTION));
    assert(s.inventory[MUEANG_ITEM_HERB] == 2 && s.potions == 0);
    s.inventory[MUEANG_ITEM_FIBER] = 4;
    assert(MueangResource_Craft(&s, MUEANG_RECIPE_POTION));
    assert(s.potions == 1 && s.inventory[MUEANG_ITEM_HERB] == 0 &&
           s.inventory[MUEANG_ITEM_FIBER] == 3);
    assert(MueangResource_Craft(&s, MUEANG_RECIPE_BACKPACK));
    assert(MueangResource_GetCapacity(&s) == 149);
    s.inventory[MUEANG_ITEM_WOOD] = 3;
    s.inventory[MUEANG_ITEM_STONE] = 2;
    s.inventory[MUEANG_ITEM_IRON_ORE] = 1;
    assert(MueangResource_Craft(&s, MUEANG_RECIPE_TOOL));
    assert(s.tool_level == 1 && s.inventory[MUEANG_ITEM_WOOD] == 0);
    tick(&s, -12, -8, 1, 21);
    assert(!s.nodes[0].active && s.inventory[MUEANG_ITEM_WOOD] == 2);

    s.potions = 99;
    s.inventory[MUEANG_ITEM_HERB] = 2;
    s.inventory[MUEANG_ITEM_FIBER] = 1;
    assert(!MueangResource_Craft(&s, MUEANG_RECIPE_POTION));
    assert(s.inventory[MUEANG_ITEM_HERB] == 2 && s.inventory[MUEANG_ITEM_FIBER] == 1);
    assert(!MueangResource_Craft(&s, (MueangRecipe)-1));
    puts("resource and crafting checks passed");
    return 0;
}
