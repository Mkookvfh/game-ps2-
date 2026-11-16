#include "rigged_model.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    assert(argc == 2);
    FILE *f = fopen(argv[1], "rb");
    assert(f && fseek(f, 0, SEEK_END) == 0);
    long size = ftell(f);
    assert(size > 0 && fseek(f, 0, SEEK_SET) == 0);
    unsigned char *bytes = malloc((size_t)size);
    assert(bytes && fread(bytes, 1, (size_t)size, f) == (size_t)size);
    fclose(f);
    assert(!RiggedModel_Load(bytes, 19));
    bytes[0] = 'X';
    assert(!RiggedModel_Load(bytes, (size_t)size));
    bytes[0] = 'g';
    MueangRiggedModel *model = RiggedModel_Load(bytes, (size_t)size);
    assert(model);
    free(bytes);
    assert(RiggedModel_VertexCount(model) == 3);
    int walk = RiggedModel_FindAnimation(model, "walk");
    int idle = RiggedModel_FindAnimation(model, "Idle");
    assert(walk >= 0 && idle >= 0 && RiggedModel_FindAnimation(model, "Missing") == -1);
    MueangRiggedVertex v[3];
    assert(!RiggedModel_Sample(model, walk, 0, v, 2));
    assert(!RiggedModel_Sample(model, walk, NAN, v, 3));
    assert(RiggedModel_Sample(model, -1, 0, v, 3));
    assert(fabsf(v[1].position[0] - 1.0f) < .0001f && fabsf(v[1].position[1]) < .0001f);
    assert(RiggedModel_Sample(model, walk, .5f, v, 3));
    assert(fabsf(v[1].position[0] - sqrtf(2.0f)) < .0001f);
    assert(fabsf(v[1].position[1] - 1.0f) < .0001f);
    assert(fabsf(v[2].position[0] - (-.25f + .15f / sqrtf(2.0f))) < .0001f);
    assert(fabsf(v[2].position[1] - (.6f - .65f / sqrtf(2.0f))) < .0001f);
    assert(fabsf(v[1].normal[2] - 1.0f) < .0001f);
    assert(RiggedModel_Sample(model, walk, 1.0f, v, 3));
    assert(fabsf(v[1].position[0] - 1.0f) < .0001f);
    assert(RiggedModel_Sample(model, idle, .5f, v, 3));
    assert(fabsf(v[1].position[0] - 1.25f) < .0001f);
    assert(RiggedModel_Sample(model, walk, 0, v, 3));
    assert(fabsf(v[1].position[0] - 1.0f) < .0001f);
    RiggedModel_Free(model);
    puts("rigged GLB skinning and animation checks passed");
    return 0;
}
