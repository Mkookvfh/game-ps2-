#include "progress.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROGRESS_VALUE_COUNT 10

int MueangProgress_Save(const char *path, const MueangGame *game,
                        const MueangResourceSystem *resources)
{
    if (!path || !game || !resources) return 0;
    size_t size = strlen(path) + 5;
    char *temp = malloc(size);
    if (!temp) return 0;
    snprintf(temp, size, "%s.tmp", path);
    FILE *file = fopen(temp, "w");
    if (!file) { free(temp); return 0; }
    int ok = fprintf(file, "MUEANG1 %d %d %d %d %d %d %d %d %d %u\n",
        resources->inventory[0], resources->inventory[1], resources->inventory[2],
        resources->inventory[3], resources->inventory[4], resources->potions,
        resources->tool_level, resources->backpack_level, game->rival_rank,
        game->discovered_locations) > 0;
    if (fflush(file) != 0) ok = 0;
    if (fclose(file) != 0) ok = 0;
    if (ok && rename(temp, path) != 0) ok = 0;
    if (!ok) remove(temp);
    free(temp);
    return ok;
}

int MueangProgress_Load(const char *path, MueangGame *game,
                        MueangResourceSystem *resources)
{
    if (!path || !game || !resources) return 0;
    FILE *file = fopen(path, "r");
    if (!file) return 0;
    char line[256];
    int ok = fgets(line, sizeof(line), file) != NULL &&
             strchr(line, '\n') != NULL && fgetc(file) == EOF && !ferror(file);
    fclose(file);
    if (!ok || strncmp(line, "MUEANG1 ", 8) != 0) return 0;
    long values[PROGRESS_VALUE_COUNT];
    char *cursor = line + 8;
    for (int i = 0; i < PROGRESS_VALUE_COUNT; ++i) {
        char *end;
        errno = 0;
        values[i] = strtol(cursor, &end, 10);
        if (errno || end == cursor || (*end && !isspace((unsigned char)*end))) return 0;
        cursor = end;
    }
    while (isspace((unsigned char)*cursor)) ++cursor;
    if (*cursor || values[5] < 0 || values[5] > MUEANG_RESOURCE_MAX_STACK ||
        values[6] < 0 || values[6] > 2 || values[7] < 0 || values[7] > 2 ||
        values[8] < 0 || values[8] > 5 || values[9] < 0 ||
        values[9] >= (1u << MUEANG_LOCATION_COUNT)) return 0;
    int capacity = MUEANG_RESOURCE_MAX_STACK + (int)values[7] * 50;
    for (int i = 0; i < MUEANG_RESOURCE_ITEM_COUNT; ++i)
        if (values[i] < 0 || values[i] > capacity) return 0;

    for (int i = 0; i < MUEANG_RESOURCE_ITEM_COUNT; ++i)
        resources->inventory[i] = (int)values[i];
    resources->potions = (int)values[5];
    resources->tool_level = (int)values[6];
    resources->backpack_level = (int)values[7];
    game->rival_rank = (int)values[8];
    game->npcs[0].hit_points = 2 + game->rival_rank;
    game->discovered_locations = (unsigned int)values[9];
    game->discovered_count = 0;
    for (int i = 0; i < MUEANG_LOCATION_COUNT; ++i)
        if (game->discovered_locations & (1u << i)) ++game->discovered_count;
    return 1;
}
