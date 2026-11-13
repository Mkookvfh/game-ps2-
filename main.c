#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <libpad.h>
#include <gsKit.h>
#include <dmaKit.h>

/* MUEANG ISARA - PS2SDK prototype 0.5
 * Hierarchical articulated character rig, animation poses, contextual
 * vault/mantle parkour, authored climb surfaces and simple collision.
 * Source only: must be built with a real PS2SDK toolchain and tested.
 */
#define SW 640.0f
#define SH 448.0f
#define PI 3.14159265f
#define WORLD 18.0f
#define CAM_LERP 0.12f
#define DEADZONE 18

typedef struct { float x,y,z; } V3;
typedef struct { float x,y; } V2;
typedef struct { float x,z,w,d,h; } Box;
static GSGLOBAL *gs;
static unsigned char padBuf[256] __attribute__((aligned(64)));
static int padReady=0;
static V3 player={0,0,0}, cam={0,3,-5.5f};
static float yaw=0, jumpY=0, jumpV=0, walkPhase=0, climbY=0, animClock=0;
static int attackFlash=0, climbing=0, grounded=1;
static int shadowSense=0, senseFrames=0, vaultFrames=0, mantleFrames=0;
static float vaultStartX=0, vaultStartZ=0, mantleStartX=0, mantleStartZ=0, mantleStartY=0;
static float climbHeight=0.0f;
static u16 previousButtons=0xffff;

/* Buildings are also collision volumes. Climbable faces are deliberately
 * authored separately so the prototype never tries to climb every wall. */
static Box buildings[] = {
 {-13,5,4,4,7},{-7,9,3,4,10},{5,8,4,4,8},{11,3,4,5,12},
 {-13,-9,4,5,9},{-7,-14,4,4,6},{7,-13,4,4,11},{13,-8,3,4,7}
};
static int buildingCount=8;
static Box climbables[] = { {-7,9,3,4,10}, {11,3,4,5,12}, {7,-13,4,4,11} };
static int climbCount=3;
/* Waist-high cover: collision is separate from tall buildings so it can be
 * vaulted. Coordinates are deliberately sparse for the PS2 prototype. */
static Box cover[] = { {-2.0f,4.0f,1.8f,0.55f,0.72f}, {3.2f,-3.0f,0.55f,1.8f,0.68f} };
static int coverCount=2;

static float clampf(float v,float lo,float hi){return v<lo?lo:(v>hi?hi:v);}
static float approach(float a,float b,float t){return a+(b-a)*t;}
static u64 col(int r,int g,int b){return GS_SETREG_RGBAQ(r,g,b,0x80,0);}
static int pressed(u16 now,u16 mask){return (!(now&mask) && (previousButtons&mask));}

static void camera_update(void){
    float desiredX=player.x-sinf(yaw)*5.2f;
    float desiredZ=player.z-cosf(yaw)*5.2f;
    float desiredY=2.8f+jumpY*0.18f+(climbing?climbY*0.08f:0);
    cam.x=approach(cam.x,desiredX,CAM_LERP);
    cam.y=approach(cam.y,desiredY,CAM_LERP);
    cam.z=approach(cam.z,desiredZ,CAM_LERP);
}
static int project(V3 p,V2 *o,float *depth){
    float sy=sinf(yaw), cy=cosf(yaw);
    float dx=p.x-cam.x,dy=p.y-cam.y,dz=p.z-cam.z;
    float rightX=cy,rightZ=-sy, forwardX=sy,forwardZ=cy;
    float cx=dx*rightX+dz*rightZ;
    float cz=dx*forwardX+dz*forwardZ;
    if(cz<0.25f)return 0;
    o->x=SW*0.5f+cx*(300.0f/cz);
    o->y=SH*0.52f-dy*(290.0f/cz);
    *depth=cz; return 1;
}
static void triangle(V3 a,V3 b,V3 c,u64 color){
    V2 pa,pb,pc; float za,zb,zc;
    if(!project(a,&pa,&za)||!project(b,&pb,&zb)||!project(c,&pc,&zc))return;
    if((pa.x<-250&&pb.x<-250&&pc.x<-250)||(pa.x>890&&pb.x>890&&pc.x>890))return;
    gsKit_prim_triangle_gouraud_3d(gs,pa.x,pa.y,(u32)(za*32),pb.x,pb.y,(u32)(zb*32),pc.x,pc.y,(u32)(zc*32),color,color,color);
}
static void quad(V3 a,V3 b,V3 c,V3 d,u64 k){triangle(a,b,c,k);triangle(a,c,d,k);}
static void box_draw(Box b,int s){
 float x=b.x,z=b.z,w=b.w,d=b.d,h=b.h;
 V3 a={x,0,z},bb={x+w,0,z},c={x+w,h,z},dd={x,h,z};
 V3 e={x,0,z+d},f={x+w,0,z+d},g={x+w,h,z+d},hh={x,h,z+d};
 quad(a,bb,c,dd,col(s,s,s+7)); quad(bb,f,g,c,col(s-12,s-12,s-5));
 quad(f,e,hh,g,col(s-20,s-18,s-12)); quad(e,a,dd,hh,col(s-8,s-8,s));
 quad(dd,c,g,hh,col(s+8,s+8,s+12));
}
static void draw_world(void){
 int i; quad((V3){-WORLD,0,-WORLD},(V3){WORLD,0,-WORLD},(V3){WORLD,0,WORLD},(V3){-WORLD,0,WORLD},col(43,46,48));
 for(i=-16;i<=16;i+=4){
  quad((V3){i,0.02f,-WORLD},(V3){i+0.1f,0.02f,-WORLD},(V3){i+0.1f,0.02f,WORLD},(V3){i,0.02f,WORLD},col(62,64,65));
  quad((V3){-WORLD,0.02f,i},(V3){WORLD,0.02f,i},(V3){WORLD,0.02f,i+0.1f},(V3){-WORLD,0.02f,i+0.1f},col(62,64,65));
 }
 for(i=0;i<buildingCount;i++){
  float dx=(buildings[i].x+buildings[i].w*0.5f)-player.x;
  float dz=(buildings[i].z+buildings[i].d*0.5f)-player.z;
  /* Sector-style visibility cull: distant blocks are not submitted. */
  if(dx*dx+dz*dz<27.0f*27.0f)box_draw(buildings[i],88+(i%3)*7);
 }
 for(i=0;i<coverCount;i++)box_draw(cover[i],65);
 if(shadowSense){
  for(i=0;i<climbCount;i++){
   Box b=climbables[i]; float x=b.x+b.w*0.5f,z=b.z+b.d*0.5f;
   /* Bright vertical marker identifies authored climb routes. */
   quad((V3){x-0.08f,0.05f,z},(V3){x+0.08f,0.05f,z},
        (V3){x+0.08f,b.h-0.25f,z},(V3){x-0.08f,b.h-0.25f,z},col(30,155,210));
  }
 }
}

/* Character renderer 0.5: faceted hood, layered cloak and articulated rig.
 * Geometry is intentionally procedural so the PS2 does not need a runtime
 * model importer. The same proportions are documented in assets/character. */
static V3 vadd(V3 a,V3 b){return (V3){a.x+b.x,a.y+b.y,a.z+b.z};}
static V3 vsub(V3 a,V3 b){return (V3){a.x-b.x,a.y-b.y,a.z-b.z};}
static V3 vmul(V3 a,float k){return (V3){a.x*k,a.y*k,a.z*k};}
static V3 vcross(V3 a,V3 b){return (V3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static V3 vnorm(V3 a){float n=sqrtf(a.x*a.x+a.y*a.y+a.z*a.z);return n>0.0001f?vmul(a,1.0f/n):(V3){0,1,0};}
/* Six-sided tapered limb/boot/forearm segment. */
static void body_segment(V3 a,V3 b,float ra,float rb,u64 shade){
 V3 axis=vnorm(vsub(b,a)), ref=(fabsf(axis.y)<0.85f)?(V3){0,1,0}:(V3){0,0,1};
 V3 u=vnorm(vcross(axis,ref)),v=vnorm(vcross(axis,u));
 V3 ar[6],br[6]; int i;
 for(i=0;i<6;i++){
  float t=(float)i*1.04719755f;
  V3 radial=vadd(vmul(u,cosf(t)),vmul(v,sinf(t)));
  ar[i]=vadd(a,vmul(radial,ra));br[i]=vadd(b,vmul(radial,rb));
 }
 for(i=0;i<6;i++){int j=(i+1)%6;u64 c=shade-(u64)(i*0x00030303);
  quad(ar[i],ar[j],br[j],br[i],c);triangle(a,ar[j],ar[i],shade);triangle(b,br[i],br[j],shade);}
}
static void hood_head(float x,float y,float z){
 V3 crown={x,y+2.10f,z},left={x-0.23f,y+1.78f,z},right={x+0.23f,y+1.78f,z};
 V3 back={x,y+1.72f,z-0.23f},front={x,y+1.76f,z+0.22f};
 triangle(crown,left,back,col(5,6,9));triangle(crown,back,right,col(8,9,13));
 triangle(crown,right,front,col(3,4,7));triangle(crown,front,left,col(10,11,15));
 /* Face opening and lower hood rim */
 triangle(left,right,(V3){x,y+1.67f,z+0.12f},col(1,2,4));
 quad((V3){x-0.20f,y+1.76f,z+0.18f},(V3){x+0.20f,y+1.76f,z+0.18f},
      (V3){x+0.16f,y+1.69f,z+0.20f},(V3){x-0.16f,y+1.69f,z+0.20f},col(30,31,35));
}
/* Compact hierarchical rig. Each child offset is relative to its parent;
 * pose evaluation accumulates the hierarchy into world-space joint points.
 * The PS2 renderer draws rigid low-poly bone sections between those joints,
 * avoiding a costly general-purpose vertex skinning pass. */
enum { J_ROOT, J_PELVIS, J_CHEST, J_NECK, J_HEAD,
       J_L_SHOULDER, J_L_ELBOW, J_L_HAND, J_R_SHOULDER, J_R_ELBOW, J_R_HAND,
       J_L_HIP, J_L_KNEE, J_L_FOOT, J_R_HIP, J_R_KNEE, J_R_FOOT, J_COUNT };
static const signed char jointParent[J_COUNT] = {
 -1, J_ROOT, J_PELVIS, J_CHEST, J_NECK,
 J_CHEST, J_L_SHOULDER, J_L_ELBOW, J_CHEST, J_R_SHOULDER, J_R_ELBOW,
 J_PELVIS, J_L_HIP, J_L_KNEE, J_PELVIS, J_R_HIP, J_R_KNEE
};
typedef struct { V3 local[J_COUNT]; V3 world[J_COUNT]; } RigPose;
static void rig_evaluate(RigPose *r){
 int i; for(i=0;i<J_COUNT;i++){
  int p=jointParent[i];
  r->world[i]=(p<0)?r->local[i]:vadd(r->world[p],r->local[i]);
 }
}
static void draw_hero(void){
 float x=player.x,z=player.z,y=jumpY;
 float gait=climbing?0.0f:sinf(walkPhase), swing=gait*0.30f;
 float breath=sinf(animClock)*0.012f;
 float armSwing=swing*0.85f;
 RigPose rig;
 memset(&rig,0,sizeof(rig));
 rig.local[J_ROOT]=(V3){x,y,z};
 rig.local[J_PELVIS]=(V3){0,0.91f,0};
 rig.local[J_CHEST]=(V3){0,0.52f+breath,0};
 rig.local[J_NECK]=(V3){0,0.27f,0};
 rig.local[J_HEAD]=(V3){0,0.18f,0};
 rig.local[J_L_SHOULDER]=(V3){-0.29f,0.00f,0};
 rig.local[J_L_ELBOW]=(V3){-0.14f-armSwing*0.45f,-0.30f,0.03f};
 rig.local[J_L_HAND]=(V3){-0.04f-armSwing,-0.27f,0.05f};
 rig.local[J_R_SHOULDER]=(V3){0.29f,0.00f,0};
 rig.local[J_R_ELBOW]=(V3){0.14f+armSwing*0.45f,-0.30f,0.03f};
 rig.local[J_R_HAND]=(V3){0.04f+armSwing,-0.27f,0.05f};
 rig.local[J_L_HIP]=(V3){-0.17f,0.03f,0};
 rig.local[J_L_KNEE]=(V3){-0.02f+swing*0.45f,-0.42f,fabsf(swing)*0.12f};
 rig.local[J_L_FOOT]=(V3){-0.01f+swing*0.55f,-0.40f,0.10f+fabsf(swing)*0.18f};
 rig.local[J_R_HIP]=(V3){0.17f,0.03f,0};
 rig.local[J_R_KNEE]=(V3){0.02f-swing*0.45f,-0.42f,fabsf(swing)*0.12f};
 rig.local[J_R_FOOT]=(V3){0.01f-swing*0.55f,-0.40f,0.10f+fabsf(swing)*0.18f};
 if(climbing){
  rig.local[J_L_HAND].y=0.34f; rig.local[J_R_HAND].y=0.12f;
  rig.local[J_L_HAND].z+=0.13f; rig.local[J_R_HAND].z+=0.13f;
 } else if(jumpY>0.05f){
  rig.local[J_L_HAND].y=-0.05f; rig.local[J_R_HAND].y=-0.05f;
  rig.local[J_L_HAND].x-=0.10f; rig.local[J_R_HAND].x+=0.10f;
 }
 rig_evaluate(&rig);
 /* Cloak shell and animated split tails. */
 quad((V3){x-0.37f,1.48f+y,z-0.08f},(V3){x+0.37f,1.48f+y,z-0.08f},
      (V3){x+0.42f,0.20f+y,z-0.24f},(V3){x-0.42f,0.20f+y,z-0.24f},col(7,8,12));
 triangle((V3){x-0.39f,1.43f+y,z-0.07f},(V3){x-0.12f,1.32f+y,z-0.12f},
          (V3){x-0.34f+gait*0.07f,0.12f+y,z-0.30f},col(13,14,19));
 triangle((V3){x+0.39f,1.43f+y,z-0.07f},(V3){x+0.12f,1.32f+y,z-0.12f},
          (V3){x+0.34f-gait*0.07f,0.12f+y,z-0.30f},col(10,11,16));
 quad((V3){x-0.28f,1.48f+y,z+0.02f},(V3){x+0.28f,1.48f+y,z+0.02f},
      (V3){x+0.22f,0.91f+y,z+0.05f},(V3){x-0.22f,0.91f+y,z+0.05f},col(20,21,26));
 quad((V3){x-0.34f,1.47f+y,z+0.02f},(V3){x+0.34f,1.47f+y,z+0.02f},
      (V3){x+0.28f,1.34f+y,z+0.08f},(V3){x-0.28f,1.34f+y,z+0.08f},col(9,10,15));
 quad((V3){x-0.24f,0.98f+y,z+0.09f},(V3){x+0.24f,0.98f+y,z+0.09f},
      (V3){x+0.23f,0.88f+y,z+0.10f},(V3){x-0.23f,0.88f+y,z+0.10f},col(48,39,29));
 hood_head(x,y,z);
 /* Bone sections follow the evaluated rig hierarchy. */
 body_segment(rig.world[J_L_HIP],rig.world[J_L_KNEE],0.115f,0.095f,col(17,18,23));
 body_segment(rig.world[J_L_KNEE],rig.world[J_L_FOOT],0.090f,0.075f,col(12,13,18));
 body_segment(rig.world[J_R_HIP],rig.world[J_R_KNEE],0.115f,0.095f,col(20,21,26));
 body_segment(rig.world[J_R_KNEE],rig.world[J_R_FOOT],0.090f,0.075f,col(14,15,20));
 body_segment(rig.world[J_L_FOOT],vadd(rig.world[J_L_FOOT],(V3){0,-0.04f,0.20f}),0.085f,0.075f,col(5,6,9));
 body_segment(rig.world[J_R_FOOT],vadd(rig.world[J_R_FOOT],(V3){0,-0.04f,0.20f}),0.085f,0.075f,col(5,6,9));
 body_segment(rig.world[J_L_SHOULDER],rig.world[J_L_ELBOW],0.125f,0.095f,col(11,12,17));
 body_segment(rig.world[J_L_ELBOW],rig.world[J_L_HAND],0.095f,0.065f,col(18,19,24));
 body_segment(rig.world[J_R_SHOULDER],rig.world[J_R_ELBOW],0.125f,0.095f,col(14,15,20));
 body_segment(rig.world[J_R_ELBOW],rig.world[J_R_HAND],0.095f,0.065f,col(18,19,24));
 body_segment((V3){x-0.20f,1.42f+y,z-0.01f},(V3){x+0.18f,1.00f+y,z-0.02f},0.025f,0.018f,col(55,43,31));
 body_segment((V3){x+0.27f,1.12f+y,z-0.18f},(V3){x+0.42f,0.62f+y,z-0.20f},0.045f,0.035f,col(58,58,62));
}
static void draw_hud(void){
 gsKit_prim_sprite(gs,10,10,220,34,1,col(20,22,25));
 gsKit_prim_sprite(gs,16,17,205,27,1,col(125,35,30));
 if(climbing)gsKit_prim_sprite(gs,470,12,630,39,1,col(35,85,115));
 if(shadowSense)gsKit_prim_sprite(gs,470,42,630,58,1,col(20,115,165));
 if(vaultFrames)gsKit_prim_sprite(gs,470,62,630,78,1,col(125,90,25));
 if(attackFlash)gsKit_prim_sprite(gs,470,12,630,39,1,col(100,24,18));
}

/* Circle/AABB horizontal collision. Returns a corrected candidate position. */
static int blocked(float x,float z){
 int i; for(i=0;i<buildingCount;i++){
  Box b=buildings[i];
  if(x>b.x-0.38f&&x<b.x+b.w+0.38f&&z>b.z-0.38f&&z<b.z+b.d+0.38f)return 1;
 }
 for(i=0;i<coverCount;i++){
  Box b=cover[i];
  if(x>b.x-0.30f&&x<b.x+b.w+0.30f&&z>b.z-0.30f&&z<b.z+b.d+0.30f)return 1;
 }
 return 0;
}
/* Detect a waist-high obstacle directly ahead for a contextual vault. */
static int find_vault_target(float *tx,float *tz){
 int i; float fx=sinf(yaw),fz=cosf(yaw);
 for(i=0;i<coverCount;i++){
  Box b=cover[i]; float cx=b.x+b.w*0.5f,cz=b.z+b.d*0.5f;
  float along=(cx-player.x)*fx+(cz-player.z)*fz;
  float side=fabsf((cx-player.x)*fz-(cz-player.z)*fx);
  if(along>0.15f&&along<1.15f&&side<0.85f){*tx=cx;*tz=cz;return 1;}
 }
 return 0;
}
/* Detect an authored climbable wall only when close and facing its front/back face. */
static int find_climb_surface(float *wallZ,float *wallX,float *height){
 int i; float fx=sinf(yaw),fz=cosf(yaw);
 for(i=0;i<climbCount;i++){
  Box b=climbables[i];
  if(fabsf(fx)>fabsf(fz)){
   float face=(fx>0)?b.x-0.42f:b.x+b.w+0.42f;
   if(fabsf(player.x-face)<0.65f&&player.z>b.z-0.3f&&player.z<b.z+b.d+0.3f){*wallX=face;*wallZ=player.z;*height=b.h;return 1;}
  }else{
   float face=(fz>0)?b.z-0.42f:b.z+b.d+0.42f;
   if(fabsf(player.z-face)<0.65f&&player.x>b.x-0.3f&&player.x<b.x+b.w+0.3f){*wallZ=face;*wallX=player.x;*height=b.h;return 1;}
  }
 }
 return 0;
}
static void init_pad(void){
 int s,t=0; padInit(0);
 if(padPortOpen(0,0,padBuf)){
  do{s=padGetState(0,0);if(s==PAD_STATE_STABLE||s==PAD_STATE_FINDCTP1)break;DelayThread(10000);}while(++t<100);
  padReady=1;
 }
}
static void update_game(void){
 padButtonStatus p; u16 b=0xffff; int hasPad=0;
 float move=0,turn=0,wx,wz,vx,vz,wallH=0;
 if(padReady&&padRead(0,0,&p)){b=p.btns;hasPad=1;}
 if(hasPad){
  /* Right stick rotates camera; D-pad remains a reliable fallback. */
  if(p.rjoy_h<128-DEADZONE)turn=-((128-p.rjoy_h-DEADZONE)/110.0f)*0.055f;
  else if(p.rjoy_h>128+DEADZONE)turn=((p.rjoy_h-128-DEADZONE)/110.0f)*0.055f;
  else {if(!(b&PAD_LEFT))turn=-0.045f;if(!(b&PAD_RIGHT))turn=0.045f;}
  yaw+=turn;
  if(pressed(b,PAD_SELECT)){shadowSense=!shadowSense;}
  if(p.ljoy_v<128-DEADZONE)move=(128-p.ljoy_v-DEADZONE)/110.0f;
  else if(p.ljoy_v>128+DEADZONE)move=-(p.ljoy_v-128-DEADZONE)/110.0f;
  else {if(!(b&PAD_UP))move=1.0f;if(!(b&PAD_DOWN))move=-1.0f;}
  move=clampf(move,-1,1);
  if(pressed(b,PAD_L1) && !vaultFrames && !climbing && find_vault_target(&vx,&vz)){
   vaultFrames=24;vaultStartX=player.x;vaultStartZ=player.z;grounded=0;jumpV=0;
  }
  if(mantleFrames){
   float t=(19.0f-(float)mantleFrames)/18.0f;
   float fx=sinf(yaw),fz=cosf(yaw);
   player.x=mantleStartX+fx*0.72f*t;player.z=mantleStartZ+fz*0.72f*t;
   jumpY=mantleStartY+(0.30f*sinf(t*PI))+(climbHeight-1.65f-mantleStartY)*t;
   mantleFrames--; if(mantleFrames==0){jumpY=0;climbY=0;grounded=1;}
  }else if(vaultFrames){
   float t=(25.0f-(float)vaultFrames)/24.0f;
   player.x=vaultStartX+sinf(yaw)*1.55f*t;
   player.z=vaultStartZ+cosf(yaw)*1.55f*t;
   jumpY=sinf(t*PI)*0.92f;
   vaultFrames--;
   if(vaultFrames==0){jumpY=0;grounded=1;}
  }else if((b&PAD_R1)==0 && find_climb_surface(&wz,&wx,&wallH) && !climbing){climbing=1;climbY=0.0f;climbHeight=wallH;}
  if(climbing){
   if((b&PAD_R1) || !(b&PAD_TRIANGLE) || !(b&PAD_CIRCLE)){climbing=0;}
   else {
    climbY+=move*0.055f;climbY=clampf(climbY,0,climbHeight-1.65f);
    player.x=approach(player.x,wx,0.25f);player.z=approach(player.z,wz,0.25f);
    /* At the lip, mantle onto the roof with a short eased transition. */
    if(climbY>=climbHeight-1.66f && move>0.15f){
     mantleFrames=18;mantleStartX=player.x;mantleStartZ=player.z;mantleStartY=climbY;
     climbing=0;vaultFrames=0;
    }
   }
  }else if(!vaultFrames&&!mantleFrames){
   float nx=player.x+sinf(yaw)*move*0.075f,nz=player.z+cosf(yaw)*move*0.075f;
   if(!blocked(nx,player.z))player.x=nx;
   if(!blocked(player.x,nz))player.z=nz;
   if(move!=0)walkPhase+=0.22f*fabsf(move);
   if(pressed(b,PAD_CROSS)&&grounded){jumpV=0.17f;grounded=0;}
  }
  if(pressed(b,PAD_SQUARE))attackFlash=16;
  if(pressed(b,PAD_CIRCLE)&&climbing)climbing=0;
 }
 if(!climbing&&!vaultFrames&&!mantleFrames){jumpY+=jumpV;jumpV-=0.0105f;if(jumpY<=0){jumpY=0;jumpV=0;grounded=1;}}
 else if(climbing){jumpY=climbY;grounded=0;}
 player.x=clampf(player.x,-WORLD+1,WORLD-1);player.z=clampf(player.z,-WORLD+1,WORLD-1);
 if(attackFlash>0)attackFlash--;
 animClock+=0.055f;
 previousButtons=b;
 camera_update();
}
int main(int argc,char **argv){
 (void)argc;(void)argv; SifInitRpc(0);init_pad();
 gs=gsKit_init_global();gs->PSM=GS_PSM_CT32;gs->PSMZ=GS_PSMZ_16S;gs->ZBuffering=GS_SETTING_ON;
 dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF,D_CTRL_STS_UNSPEC,D_CTRL_STD_OFF,D_CTRL_RCYC_8,1<<DMA_CHANNEL_GIF);
 dmaKit_chan_init(DMA_CHANNEL_GIF);gsKit_init_screen(gs);gsKit_mode_switch(gs,GS_ONESHOT);
 while(1){update_game();gsKit_clear(gs,col(27,33,42));draw_world();draw_hero();draw_hud();gsKit_queue_exec(gs);gsKit_sync_flip(gs);}
 return 0;
}
