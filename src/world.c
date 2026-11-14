#include "world.h"

static const WorldEra paris_eras[] = {
    {"Paris", "French Revolution", 25, 38, 58, 83, 72, 52, 155, 115, 72},
    {"Paris", "Restoration Era", 42, 54, 72, 91, 82, 61, 170, 145, 95}
};

static const WorldEra venice_eras[] = {
    {"Venice", "Renaissance", 35, 68, 91, 76, 91, 69, 180, 145, 92},
    {"Venice", "Later Republic", 48, 65, 77, 83, 86, 70, 150, 135, 110}
};

static const WorldEra rome_eras[] = {
    {"Rome", "Ancient Rome", 75, 57, 43, 112, 91, 61, 190, 155, 95},
    {"Rome", "Late Antiquity", 49, 51, 57, 86, 78, 62, 135, 125, 105}
};

static const WorldEra england_eras[] = {
    {"England", "Viking Age", 45, 66, 75, 66, 83, 57, 160, 135, 92},
    {"England", "Medieval England", 58, 64, 70, 76, 84, 63, 170, 150, 110}
};

static const WorldCity cities[] = {
    {"Paris", paris_eras, 2},
    {"Venice", venice_eras, 2},
    {"Rome", rome_eras, 2},
    {"England", england_eras, 2}
};

#define WORLD_CITY_COUNT ((int)(sizeof(cities) / sizeof(cities[0])))

static int current_city = 0;
static int current_era = 0;

void world_init(void)
{
    current_city = 0;
    current_era = 0;
}

void world_next_city(void)
{
    current_city++;
    if (current_city >= WORLD_CITY_COUNT)
        current_city = 0;

    current_era = 0;
}

void world_next_era(void)
{
    const WorldCity *city = &cities[current_city];

    current_era++;
    if (current_era >= city->era_count)
        current_era = 0;
}

const WorldCity *world_current_city(void)
{
    return &cities[current_city];
}

const WorldEra *world_current_era(void)
{
    return &cities[current_city].eras[current_era];
}

int world_city_index(void)
{
    return current_city;
}

int world_era_index(void)
{
    return current_era;
}
