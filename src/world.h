#ifndef MUEANG_WORLD_H
#define MUEANG_WORLD_H

typedef struct {
    const char *name;
    const char *era_name;
    unsigned char sky_r, sky_g, sky_b;
    unsigned char ground_r, ground_g, ground_b;
    unsigned char accent_r, accent_g, accent_b;
} WorldEra;

typedef struct {
    const char *name;
    const WorldEra *eras;
    int era_count;
} WorldCity;

void world_init(void);
void world_next_city(void);
void world_next_era(void);
const WorldCity *world_current_city(void);
const WorldEra *world_current_era(void);
int world_city_index(void);
int world_era_index(void);

#endif
