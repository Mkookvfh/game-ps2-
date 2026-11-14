#ifndef COMBAT_H
#define COMBAT_H

#define FINISHER_BACK_SILENT    1
#define FINISHER_BACK_FAST      2
#define FINISHER_SIDE           3
#define FINISHER_GRASS_HIDE     4

void Combat_PlayFinisher(int finisher_id);
void Combat_PostKillLookAround();
int Combat_CheckCanSilentKill(float px,float pz,float nx,float nz,float nrot);

#endif
