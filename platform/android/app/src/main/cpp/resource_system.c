
#include "resource_system.h"
#include <math.h>
#include <string.h>

#define HARVEST_RANGE 1.85f
#define RESPAWN_DISTANCE 3.25f
#define HARVEST_SECONDS 1.25f
#define RESPAWN_SECONDS 45.0f

static const float NODE_X[MUEANG_RESOURCE_NODE_COUNT] = {
    -12.0f, -5.0f, 2.0f, 11.0f,
    15.0f, -15.0f, -8.0f, 9.0f
};

static const float NODE_Z[MUEANG_RESOURCE_NODE_COUNT] = {
    -8.0f, -11.0f, -13.0f, -9.0f,
    1.0f, 8.0f, 14.0f, 13.0f
};

static const int NODE_ITEM[MUEANG_RESOURCE_NODE_COUNT] = {
    MUEANG_ITEM_WOOD,
    MUEANG_ITEM_HERB,
    MUEANG_ITEM_STONE,
    MUEANG_ITEM_WOOD,
    MUEANG_ITEM_IRON_ORE,
    MUEANG_ITEM_FIBER,
    MUEANG_ITEM_HERB,
    MUEANG_ITEM_IRON_ORE
};

static const int NODE_QUANTITY[MUEANG_RESOURCE_NODE_COUNT] = {
    2, 2, 1, 2, 1, 2, 2, 1
};

static float clamp_dt(float dt)
{
    if (!isfinite(dt) || dt < 0.0f) return 0.0f;
    if (dt > 0.05f) return 0.05f;
    return dt;
}

static float distance_sq(
    float ax, float az,
    float bx, float bz)
{
    float dx = ax - bx;
    float dz = az - bz;
    return dx * dx + dz * dz;
}

static int find_nearest(
    const MueangResourceSystem *system,
    float px,
    float pz)
{
    int best = -1;
    float best_distance_sq =
        HARVEST_RANGE * HARVEST_RANGE;

    for (int i = 0; i < MUEANG_RESOURCE_NODE_COUNT; ++i) {
        const MueangResourceNode *node =
            &system->nodes[i];

        if (!node->active) continue;

        float d2 = distance_sq(
            px, pz, node->x, node->z
        );

        if (d2 <= best_distance_sq) {
            best_distance_sq = d2;
            best = i;
        }
    }

    return best;
}

void MueangResource_Init(MueangResourceSystem *system)
{
    if (!system) return;

    memset(system, 0, sizeof(*system));

    system->nearest_node = -1;
    system->last_status = MUEANG_HARVEST_NONE;

    for (int i = 0; i < MUEANG_RESOURCE_NODE_COUNT; ++i) {
        system->nodes[i].x = NODE_X[i];
        system->nodes[i].z = NODE_Z[i];
        system->nodes[i].item = NODE_ITEM[i];
        system->nodes[i].quantity = NODE_QUANTITY[i];
        system->nodes[i].active = 1;
    }
}

void MueangResource_Update(
    MueangResourceSystem *system,
    float player_x,
    float player_z,
    int interact_held,
    float dt)
{
    if (!system ||
        !isfinite(player_x) ||
        !isfinite(player_z)) {
        return;
    }

    dt = clamp_dt(dt);

    /* Resource respawn */
    for (int i = 0; i < MUEANG_RESOURCE_NODE_COUNT; ++i) {
        MueangResourceNode *node = &system->nodes[i];

        if (node->active) continue;

        float d2 = distance_sq(
            player_x, player_z,
            node->x, node->z
        );

        if (d2 >= RESPAWN_DISTANCE * RESPAWN_DISTANCE) {
            node->respawn_timer += dt;

            if (node->respawn_timer >= RESPAWN_SECONDS) {
                node->active = 1;
                node->respawn_timer = 0.0f;
                node->progress = 0.0f;
            }
        } else {
            node->respawn_timer = 0.0f;
        }
    }

    int previous_node = system->nearest_node;
    system->nearest_node = find_nearest(
        system, player_x, player_z
    );

    if (previous_node != system->nearest_node)
        system->harvest_progress = 0.0f;

    system->last_status = MUEANG_HARVEST_NONE;

    if (system->nearest_node < 0) {
        system->harvest_progress = 0.0f;
        return;
    }

    system->last_status = MUEANG_HARVEST_NEARBY;

    if (!interact_held) {
        system->harvest_progress = 0.0f;
        return;
    }

    system->last_status = MUEANG_HARVESTING;

    system->harvest_progress +=
        dt / (HARVEST_SECONDS / (1.0f + 0.25f * system->tool_level));

    if (system->harvest_progress < 1.0f) return;

    MueangResourceNode *node =
        &system->nodes[system->nearest_node];

    int *count = &system->inventory[node->item];

    if (*count >
        MueangResource_GetCapacity(system) - node->quantity) {

        system->harvest_progress = 1.0f;
        system->last_status =
            MUEANG_HARVEST_INVENTORY_FULL;
        return;
    }

    *count += node->quantity;

    node->active = 0;
    node->respawn_timer = 0.0f;
    node->progress = 0.0f;

    system->harvest_progress = 0.0f;
    system->last_status = MUEANG_HARVEST_COMPLETE;
    system->nearest_node = -1;
}

int MueangResource_GetCount(
    const MueangResourceSystem *system,
    MueangResourceItem item)
{
    if (!system ||
        item < 0 ||
        item >= MUEANG_RESOURCE_ITEM_COUNT) {
        return 0;
    }

    return system->inventory[item];
}

int MueangResource_GetNearestNode(
    const MueangResourceSystem *system)
{
    return system ? system->nearest_node : -1;
}

float MueangResource_GetProgress(
    const MueangResourceSystem *system)
{
    if (!system) return 0.0f;

    if (system->harvest_progress < 0.0f)
        return 0.0f;

    if (system->harvest_progress > 1.0f)
        return 1.0f;

    return system->harvest_progress;
}

MueangHarvestStatus MueangResource_GetStatus(
    const MueangResourceSystem *system)
{
    if (!system) return MUEANG_HARVEST_NONE;

    return (MueangHarvestStatus)system->last_status;
}

const char *MueangResource_GetItemName(
    MueangResourceItem item)
{
    static const char *names[] = {
        "Herb",
        "Wood",
        "Stone",
        "Iron Ore",
        "Fiber"
    };

    if (item < 0 || item >= MUEANG_RESOURCE_ITEM_COUNT)
        return "Unknown";

    return names[item];
}

int MueangResource_GetCapacity(const MueangResourceSystem *system)
{
    return MUEANG_RESOURCE_MAX_STACK + (system ? system->backpack_level * 50 : 0);
}

int MueangResource_Craft(MueangResourceSystem *system, MueangRecipe recipe)
{
    static const int costs[MUEANG_RECIPE_COUNT][MUEANG_RESOURCE_ITEM_COUNT] = {
        {2, 0, 0, 0, 1}, /* potion */
        {0, 3, 2, 1, 0}, /* tool */
        {0, 2, 0, 0, 3}  /* backpack */
    };

    if (!system || recipe < 0 || recipe >= MUEANG_RECIPE_COUNT)
        return 0;
    if ((recipe == MUEANG_RECIPE_POTION && system->potions >= MUEANG_RESOURCE_MAX_STACK) ||
        (recipe == MUEANG_RECIPE_TOOL && system->tool_level >= 2) ||
        (recipe == MUEANG_RECIPE_BACKPACK && system->backpack_level >= 2))
        return 0;

    for (int i = 0; i < MUEANG_RESOURCE_ITEM_COUNT; ++i)
        if (system->inventory[i] < costs[recipe][i]) return 0;

    for (int i = 0; i < MUEANG_RESOURCE_ITEM_COUNT; ++i)
        system->inventory[i] -= costs[recipe][i];

    if (recipe == MUEANG_RECIPE_POTION) ++system->potions;
    else if (recipe == MUEANG_RECIPE_TOOL) ++system->tool_level;
    else ++system->backpack_level;
    return 1;
}
