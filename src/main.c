#include <stdio.h>
#include <unistd.h>
#include "player.h"
#include "combat.h"
#include "ai_npc.h"

NPCGuard guard;

int main(){
    Player_Init();
    guard.x=3.0f; guard.z=3.0f; guard.rot=0;
    guard.alive=1;
    guard.alert_level=0;

    float delta=0.016f;
    printf("✅ เริ่มโปรเจกต์ Assassin’s Creed (ทดสอบบนมือถือ Termux / GitHub)\n");

    for(int i=0;i<30;i++){
        Player_Update(delta);
        NPC_Update(&guard, player.x, player.z, delta);

        if(Combat_CheckCanSilentKill(player.x,player.z,guard.x,guard.z,guard.rot)){
            Combat_PlayFinisher(FINISHER_BACK_SILENT);
            guard.alive=0;
        }
        usleep(16000);
    }
    printf("✅ จบการทดลอง\n");
    return 0;
}
