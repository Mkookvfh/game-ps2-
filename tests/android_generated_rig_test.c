#include "rigged_model.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    assert(argc == 3);
    FILE *f = fopen(argv[1], "rb");
    assert(f && fseek(f,0,SEEK_END) == 0);
    long size = ftell(f);
    assert(size > 0 && fseek(f,0,SEEK_SET) == 0);
    void *bytes = malloc((size_t)size);
    assert(bytes && fread(bytes,1,(size_t)size,f) == (size_t)size);
    fclose(f);
    MueangRiggedModel *model = RiggedModel_Load(bytes,(size_t)size);
    free(bytes);
    assert(model);
    const char *names[] = {"Idle","Walk","Jump","Roll","Slide","WallRun","Grapple","Vault","Attack","Crouch"};
    size_t count = RiggedModel_VertexCount(model);
    assert(count > 1000);
    MueangRiggedVertex *a = calloc(count,sizeof(*a)), *b = calloc(count,sizeof(*b));
    assert(a && b);
    for (size_t i=0;i<sizeof(names)/sizeof(names[0]);++i) {
        int clip = RiggedModel_FindAnimation(model,names[i]);
        assert(clip >= 0 && RiggedModel_Sample(model,clip,.25f,b,count));
        for (size_t j=0;j<count;++j)
            for (int k=0;k<3;++k) assert(isfinite(b[j].position[k]) && isfinite(b[j].normal[k]));
    }
    int walk = RiggedModel_FindAnimation(model,"Walk");
    assert(RiggedModel_Sample(model,walk,0,a,count));
    assert(RiggedModel_Sample(model,walk,.25f,b,count));
    int moved = 0;
    for (size_t i=0;i<count;++i)
        if (fabsf(a[i].position[0]-b[i].position[0]) + fabsf(a[i].position[1]-b[i].position[1]) +
            fabsf(a[i].position[2]-b[i].position[2]) > .001f) ++moved;
    assert(moved > 100);
    free(a); free(b); RiggedModel_Free(model);
    f = fopen(argv[2], "rb");
    assert(f && fseek(f,0,SEEK_END) == 0);
    size = ftell(f);
    assert(size > 0 && fseek(f,0,SEEK_SET) == 0);
    bytes = malloc((size_t)size);
    assert(bytes && fread(bytes,1,(size_t)size,f) == (size_t)size);
    fclose(f);
    assert(!RiggedModel_Load(bytes,(size_t)size));
    free(bytes);
    puts("generated character rig and gameplay clips passed");
    return 0;
}
