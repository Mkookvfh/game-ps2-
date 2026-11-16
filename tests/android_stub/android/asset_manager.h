#ifndef TEST_ANDROID_ASSET_MANAGER_H
#define TEST_ANDROID_ASSET_MANAGER_H
#include <stddef.h>
#include <sys/types.h>
typedef struct AAssetManager { int unused; } AAssetManager;
typedef struct AAsset AAsset;
#define AASSET_MODE_BUFFER 3
AAsset *AAssetManager_open(AAssetManager *manager, const char *path, int mode);
off_t AAsset_getLength(AAsset *asset);
int AAsset_read(AAsset *asset, void *buffer, size_t count);
void AAsset_close(AAsset *asset);
#endif
