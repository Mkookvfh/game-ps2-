#include "character3d.h"
#include <math.h>

/* This module intentionally uses the renderer's existing shader and uniforms.
   All geometry is generated from reusable low-poly boxes: no external assets. */

static void mat_identity(float *m) {
    for (int i=0;i<16;i++) m[i]=0.0f;
    m[0]=m[5]=m[10]=m[15]=1.0f;
}
static void mat_mul(float *o,const float *a,const float *b) {
    float t[16];
    for(int c=0;c<4;c++) for(int r=0;r<4;r++) {
        t[c*4+r]=0.0f;
        for(int k=0;k<4;k++) t[c*4+r]+=a[k*4+r]*b[c*4+k];
    }
    for(int i=0;i<16;i++) o[i]=t[i];
}
static void box_vertices(float *v) {
    static const float p[8][3]={
        {-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},{.5f,.5f,-.5f},{-.5f,.5f,-.5f},
        {-.5f,-.5f,.5f},{.5f,-.5f,.5f},{.5f,.5f,.5f},{-.5f,.5f,.5f}};
    static const int f[6][6]={
        {0,1,2,0,2,3},{5,4,7,5,7,6},{4,0,3,4,3,7},
        {1,5,6,1,6,2},{3,2,6,3,6,7},{4,5,1,4,1,0}};
    static const float n[6][3]={
        {0,0,-1},{0,0,1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}};
    int k=0;
    for(int a=0;a<6;a++) for(int b=0;b<6;b++) {
        int q=f[a][b];
        for(int j=0;j<3;j++) v[k++]=p[q][j];
        for(int j=0;j<3;j++) v[k++]=n[a][j];
    }
}
static void part(Renderer3D *r,const float *vp,const float *cube,
                 float x,float y,float z,float sx,float sy,float sz,
                 float yaw,float cr,float cg,float cb) {
    float t[16],rot[16],sc[16],a[16],model[16],mvp[16];
    mat_identity(t); t[12]=x;t[13]=y;t[14]=z;
    mat_identity(rot);
    rot[0]=cosf(yaw);rot[2]=-sinf(yaw);
    rot[8]=sinf(yaw);rot[10]=cosf(yaw);
    mat_identity(sc);sc[0]=sx;sc[5]=sy;sc[10]=sz;
    mat_mul(a,rot,sc);mat_mul(model,t,a);mat_mul(mvp,vp,model);
    glUniformMatrix4fv(r->mvp,1,GL_FALSE,mvp);
    glUniformMatrix4fv(r->model,1,GL_FALSE,model);
    glUniform3f(r->color,cr,cg,cb);
    glVertexAttribPointer(r->position,3,GL_FLOAT,GL_FALSE,6*sizeof(float),cube);
    glVertexAttribPointer(r->normal,3,GL_FLOAT,GL_FALSE,6*sizeof(float),cube+3);
    glEnableVertexAttribArray(r->position);
    glEnableVertexAttribArray(r->normal);
    glDrawArrays(GL_TRIANGLES,0,36);
}
static void human(Renderer3D *r,const float *vp,const float *cube,
                  float x,float y,float z,float yaw,int kind,int variant,
                  int crouch,int moving) {
    float skin[3]={.60f,.42f,.30f};
    float cloth[3],trim[3],cape[3]={.48f,.035f,.045f};
    float sway=moving ? .12f : 0.0f;
    if(kind==0) { /* protagonist */
        cloth[0]=.78f;cloth[1]=.76f;cloth[2]=.69f;
        trim[0]=.20f;trim[1]=.16f;trim[2]=.13f;
    } else if(kind==1) { /* guards: varied armor palettes */
        static const float pal[4][3]={{.16f,.22f,.30f},{.30f,.19f,.12f},
            {.20f,.28f,.20f},{.34f,.25f,.16f}};
        for(int j=0;j<3;j++) cloth[j]=pal[variant%4][j];
        trim[0]=.62f;trim[1]=.48f;trim[2]=.22f;
    } else { /* civilians: varied clothes */
        static const float pal[6][3]={{.43f,.24f,.15f},{.20f,.34f,.31f},
            {.47f,.39f,.20f},{.34f,.22f,.38f},{.25f,.30f,.43f},{.48f,.30f,.25f}};
        for(int j=0;j<3;j++) cloth[j]=pal[variant%6][j];
        trim[0]=.30f;trim[1]=.23f;trim[2]=.16f;
    }
    float bodyH=crouch?.72f:1.02f;
    float bodyY=y+(crouch?.78f:1.02f);
    part(r,vp,cube,x,bodyY,z,.52f,bodyH,.32f,yaw,cloth[0],cloth[1],cloth[2]);
    /* chest/waist armor and belt */
    part(r,vp,cube,x,bodyY-.12f,z-.018f,.55f,.20f,.34f,yaw,trim[0],trim[1],trim[2]);
    part(r,vp,cube,x,y+(crouch? .48f:.58f),z-.19f,.56f,.10f,.055f,yaw,.12f,.09f,.07f);
    /* head and hood / hair */
    part(r,vp,cube,x,y+(crouch?1.28f:1.78f),z,.32f,.34f,.30f,yaw,skin[0],skin[1],skin[2]);
    if(kind==0) {
        part(r,vp,cube,x,y+(crouch?1.40f:1.91f),z-.015f,.39f,.30f,.36f,yaw,.18f,.17f,.16f);
        part(r,vp,cube,x,y+(crouch?1.24f:1.73f),z-.18f,.30f,.10f,.08f,yaw,.72f,.70f,.64f);
        /* long red cape, split into upper and lower panels */
        part(r,vp,cube,x,y+(crouch?.72f:1.12f),z+.22f,.62f,crouch?.62f:1.18f,.12f,yaw,cape[0],cape[1],cape[2]);
        part(r,vp,cube,x,y+(crouch?.28f:.48f),z+.22f,.58f,.48f,.13f,yaw,.38f,.025f,.035f);
        /* shoulder guards */
        part(r,vp,cube,x-.31f,bodyY+.28f,z,.22f,.20f,.34f,yaw,.30f,.27f,.22f);
        part(r,vp,cube,x+.31f,bodyY+.28f,z,.22f,.20f,.34f,yaw,.30f,.27f,.22f);
        /* sheathed blade */
        part(r,vp,cube,x+.34f,y+.76f,z+.04f,.09f,.78f,.10f,yaw,.12f,.12f,.14f);
    } else if(kind==1) {
        /* helmet, brow, shoulder plates and spear */
        part(r,vp,cube,x,y+1.83f,z,.38f,.25f,.36f,yaw,.30f,.32f,.35f);
        part(r,vp,cube,x,y+1.72f,z-.17f,.34f,.07f,.08f,yaw,trim[0],trim[1],trim[2]);
        part(r,vp,cube,x-.31f,bodyY+.25f,z,.24f,.25f,.35f,yaw,.32f,.34f,.37f);
        part(r,vp,cube,x+.31f,bodyY+.25f,z,.24f,.25f,.35f,yaw,.32f,.34f,.37f);
        part(r,vp,cube,x+.48f,y+1.05f,z-.04f,.07f,1.45f,.07f,yaw,.34f,.24f,.12f);
        part(r,vp,cube,x+.48f,y+1.83f,z-.04f,.16f,.28f,.16f,yaw,.62f,.62f,.58f);
    } else {
        /* civilian hair/head wrap and shoulder shawl */
        part(r,vp,cube,x,y+1.82f,z,.36f,.16f,.34f,yaw,trim[0],trim[1],trim[2]);
        part(r,vp,cube,x,y+1.43f,z+.02f,.58f,.12f,.36f,yaw,trim[0],trim[1],trim[2]);
        if(variant%2==0)
            part(r,vp,cube,x,y+.95f,z+.19f,.42f,.78f,.08f,yaw,.34f,.20f,.12f);
    }
    /* articulated-looking arms and legs; simple pose variation */
    part(r,vp,cube,x-.36f,bodyY-.05f,z-.01f,.17f,.64f,.18f,yaw,sway>.0f?.48f:cloth[0],cloth[1],cloth[2]);
    part(r,vp,cube,x+.36f,bodyY-.05f,z-.01f,.17f,.64f,.18f,yaw,cloth[0],cloth[1],cloth[2]);
    part(r,vp,cube,x-.16f,y+.28f,z,.20f,.55f,.22f,yaw+sway,trim[0],trim[1],trim[2]);
    part(r,vp,cube,x+.16f,y+.28f,z,.20f,.55f,.22f,yaw-sway,trim[0],trim[1],trim[2]);
    part(r,vp,cube,x-.16f,y+.06f,z-.02f,.22f,.12f,.34f,yaw,.16f,.13f,.11f);
    part(r,vp,cube,x+.16f,y+.06f,z-.02f,.22f,.12f,.34f,yaw,.16f,.13f,.11f);
}
void Character3D_Draw(Renderer3D *r,const MueangGame *g,
                      const float *vp,float player_y) {
    if(!r||!g||!vp)return;
    float cube[36*6];box_vertices(cube);
    for(int i=0;i<MUEANG_NPC_COUNT;i++) {
        const MueangNPC *n=&g->npcs[i];
        if(!n->alive||n->state==3) continue;
        float y=MueangGame_GroundHeight(n->x,n->z);
        int guard=(n->type==0);
        int variant=(i*3+(int)(n->patrol_timer*0.2f))&7;
        human(r,vp,cube,n->x,y,n->z,n->yaw,guard?1:2,variant,0,
              n->state==2);
    }
    const MueangPlayer *p=&g->player;
    int crouch=p->crouching;
    int moving=(p->move_speed>0.1f);
    human(r,vp,cube,p->x,player_y,p->z,p->yaw,0,0,crouch,moving);
}
