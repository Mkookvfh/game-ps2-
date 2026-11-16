#include "character3d.h"
#include <math.h>

/* Local-space character assembly: every part offset rotates with the root. */
static void ident(float *m){for(int i=0;i<16;i++)m[i]=0; m[0]=m[5]=m[10]=m[15]=1;}
static void mul(float *o,const float *a,const float *b){
 float t[16];
 for(int c=0;c<4;c++)for(int r=0;r<4;r++){t[c*4+r]=0;for(int k=0;k<4;k++)t[c*4+r]+=a[k*4+r]*b[c*4+k];}
 for(int i=0;i<16;i++)o[i]=t[i];
}
static void make_cube(float *v){
 static const float p[8][3]={{-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{.5f,.5f,-.5f},{-.5f,.5f,-.5f},{-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f}};
 static const int f[6][6]={{0,1,2,0,2,3},{5,4,7,5,7,6},{4,0,3,4,3,7},{1,5,6,1,6,2},{3,2,6,3,6,7},{4,5,1,4,1,0}};
 static const float n[6][3]={{0,0,-1},{0,0,1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}};
 int k=0; for(int a=0;a<6;a++)for(int b=0;b<6;b++){int q=f[a][b];for(int j=0;j<3;j++)v[k++]=p[q][j];for(int j=0;j<3;j++)v[k++]=n[a][j];}
}
static void draw_part(Renderer3D*r,const float*vp,const float*cube,
 float rx,float ry,float rz,float lx,float ly,float lz,
 float sx,float sy,float sz,float yaw,float cr,float cg,float cb){
 float c=cosf(yaw),s=sinf(yaw);
 float x=rx+lx*c+lz*s, z=rz-lx*s+lz*c;
 float t[16],rot[16],sc[16],a[16],model[16],mvp[16];
 ident(t);t[12]=x;t[13]=ry+ly;t[14]=z;
 ident(rot);rot[0]=c;rot[2]=-s;rot[8]=s;rot[10]=c;
 ident(sc);sc[0]=sx;sc[5]=sy;sc[10]=sz;
 mul(a,rot,sc);mul(model,t,a);mul(mvp,vp,model);
 glUniformMatrix4fv(r->mvp,1,GL_FALSE,mvp);glUniformMatrix4fv(r->model,1,GL_FALSE,model);glUniform3f(r->color,cr,cg,cb);
 glVertexAttribPointer(r->position,3,GL_FLOAT,GL_FALSE,6*sizeof(float),cube);
 glVertexAttribPointer(r->normal,3,GL_FLOAT,GL_FALSE,6*sizeof(float),cube+3);
 glEnableVertexAttribArray(r->position);glEnableVertexAttribArray(r->normal);glDrawArrays(GL_TRIANGLES,0,36);
}
static void human(Renderer3D*r,const float*vp,const float*cube,float x,float y,float z,float yaw,int kind,int variant,int crouch,int moving){
 float cloth[3],trim[3],skin[3]={.60f,.42f,.30f},cape[3]={.48f,.035f,.045f};
 if(kind==0){cloth[0]=.78f;cloth[1]=.76f;cloth[2]=.69f;trim[0]=.20f;trim[1]=.16f;trim[2]=.13f;}
 else if(kind==1){static const float p[4][3]={{.16f,.22f,.30f},{.30f,.19f,.12f},{.20f,.28f,.20f},{.34f,.25f,.16f}};for(int j=0;j<3;j++)cloth[j]=p[variant%4][j];trim[0]=.62f;trim[1]=.48f;trim[2]=.22f;}
 else{static const float p[6][3]={{.43f,.24f,.15f},{.20f,.34f,.31f},{.47f,.39f,.20f},{.34f,.22f,.38f},{.25f,.30f,.43f},{.48f,.30f,.25f}};for(int j=0;j<3;j++)cloth[j]=p[variant%6][j];trim[0]=.30f;trim[1]=.23f;trim[2]=.16f;}
 float bh=crouch?.72f:1.02f, by=crouch?.78f:1.02f, sway=moving?.10f:0;
 /* torso, waist, face */
 draw_part(r,vp,cube,x,y,z,0,by,0,.54f,bh,.36f,yaw,cloth[0],cloth[1],cloth[2]);
 draw_part(r,vp,cube,x,y,z,0,by-.10f,-.015f,.56f,.20f,.37f,yaw,trim[0],trim[1],trim[2]);
 draw_part(r,vp,cube,x,y,z,0,crouch?1.28f:1.78f,0,.34f,.36f,.32f,yaw,skin[0],skin[1],skin[2]);
 if(kind==0){
  draw_part(r,vp,cube,x,y,z,0,crouch?1.42f:1.92f,-.02f,.40f,.30f,.37f,yaw,.18f,.17f,.16f);
  draw_part(r,vp,cube,x,y,z,0,crouch?.72f:1.12f,.24f,.64f,crouch?.62f:1.20f,.14f,yaw,cape[0],cape[1],cape[2]);
  draw_part(r,vp,cube,x,y,z,0,crouch?.28f:.48f,.24f,.60f,.48f,.14f,yaw,.38f,.025f,.035f);
  draw_part(r,vp,cube,x,y,z,-.31f,by+.25f,0,.24f,.22f,.36f,yaw,.30f,.27f,.22f);
  draw_part(r,vp,cube,x,y,z,.31f,by+.25f,0,.24f,.22f,.36f,yaw,.30f,.27f,.22f);
  draw_part(r,vp,cube,x,y,z,.35f,.78f,.05f,.09f,.78f,.10f,yaw,.12f,.12f,.14f);
 }else if(kind==1){
  draw_part(r,vp,cube,x,y,z,0,1.84f,0,.40f,.26f,.38f,yaw,.30f,.32f,.35f);
  draw_part(r,vp,cube,x,y,z,-.31f,by+.24f,0,.25f,.25f,.36f,yaw,.32f,.34f,.37f);
  draw_part(r,vp,cube,x,y,z,.31f,by+.24f,0,.25f,.25f,.36f,yaw,.32f,.34f,.37f);
  draw_part(r,vp,cube,x,y,z,.48f,1.05f,-.04f,.07f,1.45f,.07f,yaw,.34f,.24f,.12f);
 }else{
  draw_part(r,vp,cube,x,y,z,0,1.82f,0,.38f,.17f,.36f,yaw,trim[0],trim[1],trim[2]);
  draw_part(r,vp,cube,x,y,z,0,1.43f,.02f,.60f,.13f,.38f,yaw,trim[0],trim[1],trim[2]);
  if(variant%2==0)draw_part(r,vp,cube,x,y,z,0,.96f,.20f,.44f,.78f,.09f,yaw,.34f,.20f,.12f);
 }
 /* Arms overlap torso slightly; offsets are rotated with root yaw. */
 draw_part(r,vp,cube,x,y,z,-.285f,by-.04f,-.01f,.22f,.68f,.22f,yaw,cloth[0],cloth[1],cloth[2]);
 draw_part(r,vp,cube,x,y,z,.285f,by-.04f,-.01f,.22f,.68f,.22f,yaw,cloth[0],cloth[1],cloth[2]);
 /* Legs */
 draw_part(r,vp,cube,x,y,z,-.145f,.31f,0,.23f,.64f,.24f,yaw+sway,trim[0],trim[1],trim[2]);
 draw_part(r,vp,cube,x,y,z,.145f,.31f,0,.23f,.64f,.24f,yaw-sway,trim[0],trim[1],trim[2]);
 draw_part(r,vp,cube,x,y,z,-.145f,.06f,-.02f,.24f,.15f,.36f,yaw,.16f,.13f,.11f);
 draw_part(r,vp,cube,x,y,z,.145f,.06f,-.02f,.24f,.15f,.36f,yaw,.16f,.13f,.11f);
}
void Character3D_Draw(Renderer3D*r,const MueangGame*g,const float*vp,float player_y){
 if(!r||!g||!vp)return;float cube[216];make_cube(cube);
 for(int i=0;i<MUEANG_NPC_COUNT;i++){const MueangNPC*n=&g->npcs[i];if(!n->alive||n->state==3)continue;
  float gy=MueangGame_GroundHeight(n->x,n->z);int guard=n->type==0;int v=(i*3+(int)(n->patrol_timer*.2f))&7;
  human(r,vp,cube,n->x,gy,n->z,n->yaw,guard?1:2,v,0,n->state==2);
 }
 const MueangPlayer*p=&g->player;
 human(r,vp,cube,p->x,player_y,p->z,p->yaw,0,0,p->crouching,p->move_speed>.1f);
}
