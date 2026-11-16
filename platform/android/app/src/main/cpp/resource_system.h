
#ifndef MUEANG_RESOURCE_SYSTEM_H
#define MUEANG_RESOURCE_SYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#define MUEANG_RESOURCE_NODE_COUNT 8
#define MUEANG_RESOURCE_ITEM_COUNT 5
#define MUEANG_RESOURCE_MAX_STACK 99

typedef enum {
    MUEANG_ITEM_HERB = 0,
    MUEANG_ITEM_WOOD,
    MUEANG_ITEM_STONE,
    MUEANG_ITEM_IRON_ORE,
    MUEANG_ITEM_FIBER
} MueangResourceItem;

typedef enum {
    MUEANG_HARVEST_NONE = 0,
    MUEANG_HARVEST_NEARBY,
    MUEANG_HARVESTING,
    MUEANG_HARVEST_COMPLETE,
    MUEANG_HARVEST_INVENTORY_FULL
} MueangHarvestStatus;

typedef struct {
    float x;
    float z;
    float progress;
    float respawn_timer;
    int item;
    int quantity;
    int active;
} MueangResourceNode;

typedef struct {
    MueangResourceNode nodes[MUEANG_RESOURCE_NODE_COUNT];
    int inventory[MUEANG_RESOURCE_ITEM_COUNT];
    int nearest_node;
    float harvest_progress;
    int last_status;
} MueangResourceSystem;

void MueangResource_Init(MueangResourceSystem *system);

void MueangResource_Update(
    MueangResourceSystem *system,
    float player_x,
    float player_z,
    int interact_held,
    float dt
);

int MueangResource_GetCount(
    const MueangResourceSystem *system,
    MueangResourceItem item
);

int MueangResource_GetNearestNode(
    const MueangResourceSystem *system
);

float MueangResource_GetProgress(
    const MueangResourceSystem *system
);

MueangHarvestStatus MueangResource_GetStatus(
    const MueangResourceSystem *system
);

const char *MueangResource_GetItemName(
    MueangResourceItem item
);

#ifdef __cplusplus
}
#endif

#endif
