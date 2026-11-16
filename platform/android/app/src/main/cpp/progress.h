#ifndef MUEANG_PROGRESS_H
#define MUEANG_PROGRESS_H

#include "game.h"
#include "resource_system.h"

/* Persists inventory, upgrades, rival rank and discoveries. Returns 1 on success. */
int MueangProgress_Save(const char *path, const MueangGame *game,
                        const MueangResourceSystem *resources);
int MueangProgress_Load(const char *path, MueangGame *game,
                        MueangResourceSystem *resources);

#endif
