#ifndef MUEANG_RIGGED_MODEL_H
#define MUEANG_RIGGED_MODEL_H

#include <stddef.h>

typedef struct MueangRiggedModel MueangRiggedModel;

typedef struct {
    float position[3];
    float normal[3];
    float color[3];
} MueangRiggedVertex;

/* The model owns a copy of the GLB. Unsupported features cause load failure. */
MueangRiggedModel *RiggedModel_Load(const void *glb, size_t size);
void RiggedModel_Free(MueangRiggedModel *model);
size_t RiggedModel_VertexCount(const MueangRiggedModel *model);
int RiggedModel_FindAnimation(const MueangRiggedModel *model, const char *name);
/* animation == -1 selects bind pose. Looping clips use seconds modulo duration. */
int RiggedModel_Sample(MueangRiggedModel *model, int animation, float seconds,
                       MueangRiggedVertex *vertices, size_t capacity);

#endif
