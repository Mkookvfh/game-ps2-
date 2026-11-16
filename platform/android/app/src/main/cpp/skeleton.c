#include "skeleton.h"
#include <math.h>
#include <string.h>
#define PI 3.14159265358979323846f
static const char *g_names[MUEANG_SKELETON_BONE_COUNT]={
 "Root","Hips","Spine","Chest","Neck","Head",
 "L_UpperArm","L_Forearm","L_Hand","R_UpperArm","R_Forearm","R_Hand",
 "L_Thigh","L_Calf","L_Foot","R_Thigh","R_Calf","R_Foot",
 "Cape_Root","Cape_Mid","Cape_Tip"
};
static const int g_parent[MUEANG_SKELETON_BONE_COUNT]={
 -1,0,1,2,3,4,3,6,7,3,9,10,1,12,13,1,15,16,3,18,19
};
/* Bind-space pivots match the measured proportions of Main_Assassin_02.glb. */
static const float g_pivot[MUEANG_SKELETON_BONE_COUNT][3]={
 {0,0,0},{0,.88f,0},{0,1.00f,0},{0,1.42f,0},{0,1.68f,0},{0,1.86f,0},
 {-.32f,1.58f,0},{-.43f,1.20f,0},{-.48f,.96f,0},
 {.32f,1.58f,0},{.43f,1.20f,0},{.48f,.96f,0},
 {-.18f,.62f,0},{-.18f,.39f,0},{-.18f,.13f,.02f},
 {.18f,.62f,0},{.18f,.39f,0},{.18f,.13f,.02f},
 {0,1.62f,-.22f},{0,.96f,-.28f},{0,.28f,-.36f}
};
/* Each of the 27 separate GLB meshes is rigidly attached to one bone. */
static const unsigned char g_part_bone[MUEANG_SKELETON_PART_COUNT]={
 2,3,3,1,1,1,5,5,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,1,1,1
};
static void identity(float*m){memset(m,0,64);m[0]=m[5]=m[10]=m[15]=1.0f;}
static void mul(float*out,const float*a,const float*b){float t[16];for(int c=0;c<4;c++)for(int r=0;r<4;r++){t[c*4+r]=0;for(int k=0;k<4;k++)t[c*4+r]+=a[k*4+r]*b[c*4+k];}memcpy(out,t,sizeof(t));}
static void translate(float*m,float x,float y,float z){identity(m);m[12]=x;m[13]=y;m[14]=z;}
static void rotate_euler(float*m,float x,float y,float z){
 float cx=cosf(x),sx=sinf(x),cy=cosf(y),sy=sinf(y),cz=cosf(z),sz=sinf(z);
 float rx[16],ry[16],rz[16],a[16];identity(rx);identity(ry);identity(rz);
 rx[5]=cx;rx[6]=sx;rx[9]=-sx;rx[10]=cx;
 ry[0]=cy;ry[2]=-sy;ry[8]=sy;ry[10]=cy;
 rz[0]=cz;rz[1]=sz;rz[4]=-sz;rz[5]=cz;
 mul(a,ry,rx);mul(m,a,rz);
}
static void local_bone(int id,const float angles[ MUEANG_SKELETON_BONE_COUNT ][3],float out[16]){
 float a[16],r[16],b[16],t[16];
 translate(a,g_pivot[id][0],g_pivot[id][1],g_pivot[id][2]);
 rotate_euler(r,angles[id][0],angles[id][1],angles[id][2]);
 translate(b,-g_pivot[id][0],-g_pivot[id][1],-g_pivot[id][2]);
 mul(t,a,r);mul(out,t,b);
}
const char *Skeleton_BoneName(int bone){return (bone>=0&&bone<MUEANG_SKELETON_BONE_COUNT)?g_names[bone]:"Unknown";}
void Skeleton_BuildPartMatrix(int part,MueangAnimState state,float phase,int crouching,float out[16]){
 float a[MUEANG_SKELETON_BONE_COUNT][3];
 float world[MUEANG_SKELETON_BONE_COUNT][16];
 memset(a,0,sizeof(a));
 float s=sinf(phase),c=cosf(phase);
 switch(state){
  case MUEANG_ANIM_WALK:
   a[6][0]=-.43f*s;a[7][0]=.10f+fmaxf(0,-s)*.12f;a[8][0]=.04f;
   a[9][0]= .43f*s;a[10][0]=.10f+fmaxf(0,s)*.12f;a[11][0]=.04f;
   a[12][0]= .58f*s;a[13][0]=fmaxf(0,-s)*.55f;a[14][0]=-.10f*s;
   a[15][0]=-.58f*s;a[16][0]=fmaxf(0,s)*.55f;a[17][0]= .10f*s;
   break;
  case MUEANG_ANIM_JUMP:
   a[2][0]=-.08f;a[6][0]=-.72f;a[7][0]=-.12f;a[9][0]=-.72f;a[10][0]=-.12f;
   a[12][0]=-.28f;a[13][0]=.20f;a[15][0]=-.12f;a[16][0]=.32f;
   break;
  case MUEANG_ANIM_ROLL:
   a[2][0]=.35f;a[2][2]=.48f;a[6][0]=-.55f;a[9][0]=-.55f;
   a[12][0]=.45f;a[13][0]=-.65f;a[15][0]=.45f;a[16][0]=-.65f;
   break;
  case MUEANG_ANIM_SLIDE:
   a[2][0]=.30f;a[12][0]=.55f;a[13][0]=-.78f;a[15][0]=.55f;a[16][0]=-.78f;
   a[6][0]=-.28f;a[9][0]=-.28f;
   break;
  case MUEANG_ANIM_WALL_RUN:
   a[2][0]=-.12f;a[6][0]=-.85f;a[9][0]=.72f;
   a[12][0]=.50f+.18f*s;a[13][0]=-.25f;a[15][0]=-.35f-.18f*s;a[16][0]=.25f;
   break;
  case MUEANG_ANIM_GRAPPLE:
   a[6][0]=-.95f;a[9][0]=-.95f;a[7][0]=-.12f;a[10][0]=-.12f;
   a[12][0]=.22f;a[15][0]=-.22f;
   break;
  case MUEANG_ANIM_VAULT:
   a[2][0]=-.18f;a[6][0]=-.78f;a[9][0]=-.78f;
   a[12][0]=.48f;a[13][0]=-.25f;a[15][0]=-.38f;a[16][0]=.18f;
   break;
  case MUEANG_ANIM_ATTACK:
   a[2][0]=-.06f;a[6][0]=-.20f;a[9][0]=.92f;a[10][0]=-.18f;a[11][0]=-.12f;
   break;
  case MUEANG_ANIM_CROUCH:
   a[2][0]=.20f;a[12][0]=.35f;a[13][0]=-.62f;a[15][0]=.35f;a[16][0]=-.62f;
   a[6][0]=.18f;a[9][0]=.18f;
   break;
  default: break;
 }
 if(crouching && state==MUEANG_ANIM_IDLE){a[2][0]=.16f;a[12][0]=.22f;a[13][0]=-.38f;a[15][0]=.22f;a[16][0]=-.38f;}
 /* Small secondary motion; the cape remains anchored at the shoulders. */
 a[18][1]=sinf(phase*.55f)*.025f;
 a[19][0]=.08f+sinf(phase*.72f)*.075f;
 a[20][0]=.12f+sinf(phase*.62f+.4f)*.16f;
 a[20][1]=sinf(phase*.48f)*.055f;
 for(int i=0;i<MUEANG_SKELETON_BONE_COUNT;i++){
  float local[16];
  if(i==0) identity(world[i]);
  else {local_bone(i,a,local);mul(world[i],world[g_parent[i]],local);}
 }
 if(part<0||part>=MUEANG_SKELETON_PART_COUNT){identity(out);return;}
 memcpy(out,world[g_part_bone[part]],sizeof(float)*16);
 (void)c;
}
