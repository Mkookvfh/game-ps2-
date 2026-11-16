#include "character3d.h"
#include <math.h>

/* MUEANG ISARA - Animation Pack 04 + Cape Attachment Fix
 * Procedural ES 2.0 character poses built from the existing modular cube mesh.
 * Local +Z is forward; local -Z is behind the character.
 */

#define PI 3.14159265f

static void ident(float *m) {
    for (int i=0;i<16;i++) m[i]=0.0f;
    m[0]=m[5]=m[10]=m[15]=1.0f;
}
static void mul(float *o,const float *a,const float *b) {
    float t[16];
    for(int c=0;c<4;c++) for(int r=0;r<4;r++) {
        t[c*4+r]=0.0f;
        for(int k=0;k<4;k++) t[c*4+r]+=a[k*4+r]*b[c*4+k];
    }
    for(int i=0;i<16;i++) o[i]=t[i];
}
static void make_cube(float *v) {
    static const float p[8][3]={
        {-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},
        {.5f,.5f,-.5f},{-.5f,.5f,-.5f},
        {-.5f,-.5f,.5f},{.5f,-.5f,.5f},
        {.5f,.5f,.5f},{-.5f,.5f,.5f}
    };
    static const int f[6][6]={
        {0,1,2,0,2,3},{5,4,7,5,7,6},
        {4,0,3,4,3,7},{1,5,6,1,6,2},
        {3,2,6,3,6,7},{4,5,1,4,1,0}
    };
    static const float n[6][3]={
        {0,0,-1},{0,0,1},{-1,0,0},
        {1,0,0},{0,1,0},{0,-1,0}
    };
    int k=0;
    for(int a=0;a<6;a++) for(int b=0;b<6;b++) {
        int q=f[a][b];
        for(int j=0;j<3;j++) v[k++]=p[q][j];
        for(int j=0;j<3;j++) v[k++]=n[a][j];
    }
}
static void draw_part(Renderer3D *r,const float *vp,const float *cube,
    float rx,float ry,float rz,float lx,float ly,float lz,
    float sx,float sy,float sz,float yaw,float cr,float cg,float cb) {
    float c=cosf(yaw),s=sinf(yaw);
    float x=rx+lx*c+lz*s, z=rz-lx*s+lz*c;
    float t[16],rot[16],sc[16],a[16],model[16],mvp[16];
    ident(t); t[12]=x; t[13]=ry+ly; t[14]=z;
    ident(rot); rot[0]=c; rot[2]=-s; rot[8]=s; rot[10]=c;
    ident(sc); sc[0]=sx; sc[5]=sy; sc[10]=sz;
    mul(a,rot,sc); mul(model,t,a); mul(mvp,vp,model);
    glUniformMatrix4fv(r->mvp,1,GL_FALSE,mvp);
    glUniformMatrix4fv(r->model,1,GL_FALSE,model);
    glUniform3f(r->color,cr,cg,cb);
    glVertexAttribPointer(r->position,3,GL_FLOAT,GL_FALSE,6*sizeof(float),cube);
    glVertexAttribPointer(r->normal,3,GL_FLOAT,GL_FALSE,6*sizeof(float),cube+3);
    glEnableVertexAttribArray(r->position);
    glEnableVertexAttribArray(r->normal);
    glDrawArrays(GL_TRIANGLES,0,36);
}

/* pose: 0 idle/walk, 1 jump/fall, 2 roll, 3 slide,
 * 4 wallrun, 5 grapple/zipline, 6 pole vault, 7 combat */
static void human(Renderer3D *r,const float *vp,const float *cube,
    float x,float y,float z,float yaw,int kind,int variant,int crouch,
    int moving,float phase,int airborne,int pose,float action_t) {

    float cloth[3],trim[3],skin[3]={.60f,.42f,.30f};
    const float cape[3]={.48f,.035f,.045f};

    if(kind==0) {
        cloth[0]=.78f; cloth[1]=.76f; cloth[2]=.69f;
        trim[0]=.20f; trim[1]=.16f; trim[2]=.13f;
    } else if(kind==1) {
        static const float p[4][3]={{.16f,.22f,.30f},{.30f,.19f,.12f},
            {.20f,.28f,.20f},{.34f,.25f,.16f}};
        for(int j=0;j<3;j++) cloth[j]=p[variant%4][j];
        trim[0]=.62f; trim[1]=.48f; trim[2]=.22f;
    } else {
        static const float p[6][3]={{.43f,.24f,.15f},{.20f,.34f,.31f},
            {.47f,.39f,.20f},{.34f,.22f,.38f},{.25f,.30f,.43f},{.48f,.30f,.25f}};
        for(int j=0;j<3;j++) cloth[j]=p[variant%6][j];
        trim[0]=.30f; trim[1]=.23f; trim[2]=.16f;
    }

    float s=sinf(phase), s2=sinf(phase+PI);
    float torso_y=1.02f, torso_h=1.02f, lean=0.0f;
    float arm_l=s*.22f, arm_r=-s*.22f;
    float leg_l=s*.25f, leg_r=s2*.25f;
    float leg_zl=s*.12f, leg_zr=-leg_zl;
    float cape_sway=moving?sinf(phase*.72f)*.045f:0.0f;
    float cape_lift=moving?fabsf(s)*.035f:0.0f;
    float head_y=1.78f, arm_y=0.98f, leg_y=.31f;
    float jump_offset=airborne?.10f:0.0f;

    if(crouch) { torso_y=.78f; torso_h=.72f; head_y=1.28f; arm_y=.72f; }
    if(pose==1) { arm_l=-.48f; arm_r=.48f; leg_l=.16f; leg_r=-.16f; leg_zl=.08f; leg_zr=-.08f; cape_lift=.12f; }
    if(pose==2) { torso_y=.48f; torso_h=.62f; head_y=.92f; arm_y=.45f; leg_y=.18f; lean=.12f; cape_lift=.14f; }
    if(pose==3) { torso_y=.62f; torso_h=.70f; head_y=1.20f; arm_y=.58f; leg_y=.22f; lean=.08f; cape_lift=.10f; }
    if(pose==4) { torso_y=.92f; arm_l=-.55f; arm_r=.55f; leg_l=.42f; leg_r=-.42f; cape_lift=.14f; }
    if(pose==5) { torso_y=.90f; arm_l=-.72f; arm_r=.72f; leg_l=.20f; leg_r=-.20f; cape_lift=.16f; }
    if(pose==6) { torso_y=1.05f; arm_l=-.85f; arm_r=.85f; leg_l=.45f; leg_r=-.35f; cape_lift=.15f; }
    if(pose==7) { arm_l=-.95f; arm_r=.30f; lean=.06f; }

    if(pose==7) {
        float hit=sinf((1.0f-action_t)*PI);
        arm_l-=hit*.22f;
    }

    float bob=(moving && pose==0 && !crouch)?fabsf(s)*.035f:0.0f;
    draw_part(r,vp,cube,x,y,z,0,torso_y+bob+lean,0,
        .54f,torso_h,.36f,yaw,cloth[0],cloth[1],cloth[2]);
    draw_part(r,vp,cube,x,y,z,0,torso_y-.10f+bob+lean,-.015f,
        .56f,.20f,.37f,yaw,trim[0],trim[1],trim[2]);
    draw_part(r,vp,cube,x,y,z,0,head_y+jump_offset,0,
        .34f,.36f,.32f,yaw,skin[0],skin[1],skin[2]);

    if(kind==0) {
        draw_part(r,vp,cube,x,y,z,0,head_y+.14f+jump_offset,-.02f,
            .40f,.30f,.37f,yaw,.18f,.17f,.16f);

        /* Shoulder attachment: overlaps the torso slightly so the cape
           visibly starts at the upper back instead of hovering behind it. */
        draw_part(r,vp,cube,x,y,z,0,torso_y+.34f+bob,-.195f,
            .52f,.24f,.12f,yaw,.38f,.025f,.035f);

        /* Cape panel is pushed into the back surface by a small amount.
           The top stays anchored; only the lower cloth sways and lifts. */
        draw_part(r,vp,cube,x,y,z,cape_sway,torso_y-.02f+bob+cape_lift*.35f,
            -.205f,.60f,crouch?.94f:1.10f,.13f,yaw,cape[0],cape[1],cape[2]);
        draw_part(r,vp,cube,x,y,z,cape_sway*1.25f,torso_y-.48f+bob+cape_lift,
            -.215f,.58f,.42f,.14f,yaw,.38f,.025f,.035f);

        draw_part(r,vp,cube,x,y,z,-.31f,torso_y+.25f+bob,0,
            .24f,.22f,.36f,yaw,.30f,.27f,.22f);
        draw_part(r,vp,cube,x,y,z,.31f,torso_y+.25f+bob,0,
            .24f,.22f,.36f,yaw,.30f,.27f,.22f);
        draw_part(r,vp,cube,x,y,z,.35f,.78f,-.05f,
            .09f,.78f,.10f,yaw,.12f,.12f,.14f);
    } else if(kind==1) {
        draw_part(r,vp,cube,x,y,z,0,1.84f,0,.40f,.26f,.38f,yaw,.30f,.32f,.35f);
        draw_part(r,vp,cube,x,y,z,-.31f,torso_y+.24f,0,.25f,.25f,.36f,yaw,.32f,.34f,.37f);
        draw_part(r,vp,cube,x,y,z,.31f,torso_y+.24f,0,.25f,.25f,.36f,yaw,.32f,.34f,.37f);
        draw_part(r,vp,cube,x,y,z,.48f,1.05f,-.04f,.07f,1.45f,.07f,yaw,.34f,.24f,.12f);
    } else {
        draw_part(r,vp,cube,x,y,z,0,1.82f,0,.38f,.17f,.36f,yaw,trim[0],trim[1],trim[2]);
        draw_part(r,vp,cube,x,y,z,0,1.43f,.02f,.60f,.13f,.38f,yaw,trim[0],trim[1],trim[2]);
        if((variant%2)==0) draw_part(r,vp,cube,x,y,z,0,.96f,.20f,.44f,.78f,.09f,yaw,.34f,.20f,.12f);
    }

    draw_part(r,vp,cube,x,y,z,-.285f,arm_y+bob,-.01f,
        .22f,.68f,.22f,yaw+arm_l,cloth[0],cloth[1],cloth[2]);
    draw_part(r,vp,cube,x,y,z,.285f,arm_y+bob,-.01f,
        .22f,.68f,.22f,yaw+arm_r,cloth[0],cloth[1],cloth[2]);

    draw_part(r,vp,cube,x,y,z,-.145f,leg_y+jump_offset,leg_zl,
        .23f,.64f,.24f,yaw+leg_l,trim[0],trim[1],trim[2]);
    draw_part(r,vp,cube,x,y,z,.145f,leg_y+jump_offset,leg_zr,
        .23f,.64f,.24f,yaw+leg_r,trim[0],trim[1],trim[2]);
    draw_part(r,vp,cube,x,y,z,-.145f,.06f+jump_offset, -.02f+leg_zl,
        .24f,.15f,.36f,yaw+leg_l,.16f,.13f,.11f);
    draw_part(r,vp,cube,x,y,z,.145f,.06f+jump_offset, -.02f+leg_zr,
        .24f,.15f,.36f,yaw+leg_r,.16f,.13f,.11f);
}

void Character3D_Draw(Renderer3D *r,const MueangGame *g,
                      const float *vp,float player_y) {
    if(!r || !g || !vp) return;
    float cube[216]; make_cube(cube);

    for(int i=0;i<MUEANG_NPC_COUNT;i++) {
        const MueangNPC *n=&g->npcs[i];
        if(!n->alive || n->state==3) continue;
        float gy=MueangGame_GroundHeight(n->x,n->z);
        int guard=n->type==0;
        int moving=(n->state==2) || (n->type==1);
        float phase=n->patrol_timer*(moving?7.0f:0.0f);
        human(r,vp,cube,n->x,gy,n->z,n->yaw,guard?1:2,i*3,
              0,moving,phase,0,0,0.0f);
    }

    const MueangPlayer *p=&g->player;
    static float last_x=0.0f,last_z=0.0f,player_phase=0.0f;
    static int valid=0;
    float dx=p->x-last_x,dz=p->z-last_z;
    float travelled=sqrtf(dx*dx+dz*dz);
    if(!valid) { last_x=p->x; last_z=p->z; valid=1; travelled=0.0f; }
    int moving=travelled>0.0008f;
    if(moving) player_phase+=travelled*8.0f;
    last_x=p->x; last_z=p->z;

    int pose=0;
    if(p->attacking || p->air_assassination || p->ledge_takedown) pose=7;
    else if(p->roll_timer>0.0f) pose=2;
    else if(p->slide_timer>0.0f) pose=3;
    else if(p->traversal_mode==1) pose=4;
    else if(p->traversal_mode==2 || p->traversal_mode==3) pose=5;
    else if(p->traversal_mode==4) pose=6;
    else if(!p->grounded) pose=1;

    float phase=player_phase;
    if(pose!=0) phase+=p->combat_timer*5.0f;
    human(r,vp,cube,p->x,player_y,p->z,p->yaw,0,0,p->crouching,
          moving,phase,!p->grounded,pose,p->combat_timer);
}
