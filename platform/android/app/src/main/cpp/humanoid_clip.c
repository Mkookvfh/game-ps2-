#include "humanoid_clip.h"
#include <math.h>
#include <string.h>

#define EPS 1.0e-7f
static const int g_parent[MUEANG_HUMANOID_BONE_COUNT] = {
    -1,0,1,2,3,4,3,6,7,3,9,10,1,12,13,1,15,16,3,18,19
};

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
static void quat_normalize(float q[4]) {
    float n = sqrtf(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
    if (n < EPS) { q[0]=q[1]=q[2]=0.0f; q[3]=1.0f; return; }
    for (int i=0;i<4;i++) q[i] /= n;
}
static void quat_slerp(float out[4], const float a[4], const float b0[4], float t) {
    float b[4], dot=0.0f;
    for (int i=0;i<4;i++) b[i]=b0[i];
    for (int i=0;i<4;i++) dot += a[i]*b[i];
    if (dot < 0.0f) { dot=-dot; for (int i=0;i<4;i++) b[i]=-b[i]; }
    if (dot > 0.9995f) {
        for (int i=0;i<4;i++) out[i]=a[i]+(b[i]-a[i])*t;
        quat_normalize(out); return;
    }
    if (dot > 1.0f) dot=1.0f;
    {
        float theta=acosf(dot), sn=sinf(theta);
        float wa=sinf((1.0f-t)*theta)/sn, wb=sinf(t*theta)/sn;
        for (int i=0;i<4;i++) out[i]=wa*a[i]+wb*b[i];
    }
    quat_normalize(out);
}
static int find_segment(const float *times, int count, float t, int *lo, int *hi, float *u) {
    int l=0, r=count-1;
    if (!times || count<=0 || !lo || !hi || !u) return 0;
    if (count==1 || t<=times[0]) { *lo=*hi=0; *u=0; return 1; }
    if (t>=times[count-1]) { *lo=*hi=count-1; *u=0; return 1; }
    while (r-l>1) { int m=l+(r-l)/2; if (times[m] <= t) l=m; else r=m; }
    if (!(times[r] > times[l])) return 0;
    *lo=l; *hi=r; *u=clamp01((t-times[l])/(times[r]-times[l]));
    return 1;
}
static void sample_track(const MueangClipTrack *tr, float time, float out[4]) {
    int lo,hi,dim=(tr->path==MUEANG_CLIP_ROTATION)?4:3;
    float u;
    const float *a,*b;
    out[0]=out[1]=out[2]=0.0f; out[3]=1.0f;
    if (!find_segment(tr->times,tr->key_count,time,&lo,&hi,&u)) return;
    a=tr->values+(size_t)lo*(size_t)dim;
    b=tr->values+(size_t)hi*(size_t)dim;
    if (lo==hi || tr->interpolation==MUEANG_CLIP_STEP) {
        for (int k=0;k<dim;k++) out[k]=a[k];
    } else if (tr->path==MUEANG_CLIP_ROTATION) {
        quat_slerp(out,a,b,u);
    } else {
        for (int k=0;k<dim;k++) out[k]=a[k]+(b[k]-a[k])*u;
    }
    if (tr->path==MUEANG_CLIP_ROTATION) quat_normalize(out);
}

void HumanoidClip_PoseIdentity(MueangHumanoidPose *p) {
    if (!p) return;
    for (int i=0;i<MUEANG_HUMANOID_BONE_COUNT;i++) {
        p->translation[i][0]=p->translation[i][1]=p->translation[i][2]=0.0f;
        p->rotation[i][0]=p->rotation[i][1]=p->rotation[i][2]=0.0f; p->rotation[i][3]=1.0f;
        p->scale[i][0]=p->scale[i][1]=p->scale[i][2]=1.0f;
    }
}

int HumanoidClip_Sample(const MueangHumanoidClip *clip, float time, MueangHumanoidPose *out) {
    int sampled=0;
    if (!clip || !out || !clip->tracks || clip->track_count<=0 ||
        clip->track_count>MUEANG_CLIP_MAX_TRACKS) return 0;
    HumanoidClip_PoseIdentity(out);
    if (clip->duration>EPS) {
        if (clip->loop) { time=fmodf(time,clip->duration); if (time<0.0f) time+=clip->duration; }
        else { if (time<0.0f) time=0.0f; if (time>clip->duration) time=clip->duration; }
    } else time=0.0f;
    for (int i=0;i<clip->track_count;i++) {
        const MueangClipTrack *tr=&clip->tracks[i];
        float v[4];
        if (tr->target_bone<0 || tr->target_bone>=MUEANG_HUMANOID_BONE_COUNT ||
            tr->key_count<=0 || !tr->times || !tr->values ||
            (tr->path!=MUEANG_CLIP_TRANSLATION && tr->path!=MUEANG_CLIP_ROTATION && tr->path!=MUEANG_CLIP_SCALE) ||
            (tr->interpolation!=MUEANG_CLIP_STEP && tr->interpolation!=MUEANG_CLIP_LINEAR)) continue;
        sample_track(tr,time,v);
        if (tr->path==MUEANG_CLIP_TRANSLATION) memcpy(out->translation[tr->target_bone],v,3*sizeof(float));
        else if (tr->path==MUEANG_CLIP_ROTATION) memcpy(out->rotation[tr->target_bone],v,4*sizeof(float));
        else memcpy(out->scale[tr->target_bone],v,3*sizeof(float));
        sampled++;
    }
    return sampled;
}

void HumanoidClip_BlendPose(const MueangHumanoidPose *a,const MueangHumanoidPose *b,float alpha,MueangHumanoidPose *out) {
    if (!a || !b || !out) return;
    float t=clamp01(alpha);
    for (int i=0;i<MUEANG_HUMANOID_BONE_COUNT;i++) {
        for (int k=0;k<3;k++) {
            out->translation[i][k]=a->translation[i][k]+(b->translation[i][k]-a->translation[i][k])*t;
            out->scale[i][k]=a->scale[i][k]+(b->scale[i][k]-a->scale[i][k])*t;
        }
        quat_slerp(out->rotation[i],a->rotation[i],b->rotation[i],t);
    }
}

static void mat_mul(float out[16],const float a[16],const float b[16]) {
    float t[16];
    for (int c=0;c<4;c++) for (int r=0;r<4;r++) {
        t[c*4+r]=0.0f;
        for (int k=0;k<4;k++) t[c*4+r]+=a[k*4+r]*b[c*4+k];
    }
    memcpy(out,t,sizeof(t));
}
static void local_matrix(const float tr[3],const float q0[4],const float sc[3],float m[16]) {
    float q[4]={q0[0],q0[1],q0[2],q0[3]};
    float x,y,z,w;
    quat_normalize(q); x=q[0];y=q[1];z=q[2];w=q[3];
    m[0]=(1-2*(y*y+z*z))*sc[0]; m[1]=(2*(x*y+z*w))*sc[0]; m[2]=(2*(x*z-y*w))*sc[0]; m[3]=0;
    m[4]=(2*(x*y-z*w))*sc[1]; m[5]=(1-2*(x*x+z*z))*sc[1]; m[6]=(2*(y*z+x*w))*sc[1]; m[7]=0;
    m[8]=(2*(x*z+y*w))*sc[2]; m[9]=(2*(y*z-x*w))*sc[2]; m[10]=(1-2*(x*x+y*y))*sc[2]; m[11]=0;
    m[12]=tr[0];m[13]=tr[1];m[14]=tr[2];m[15]=1;
}
void HumanoidClip_BuildGlobalMatrices(const MueangHumanoidPose *pose,float out[ MUEANG_HUMANOID_BONE_COUNT ][16]) {
    if (!pose || !out) return;
    for (int i=0;i<MUEANG_HUMANOID_BONE_COUNT;i++) {
        float local[16];
        local_matrix(pose->translation[i],pose->rotation[i],pose->scale[i],local);
        if (g_parent[i]<0) memcpy(out[i],local,sizeof(local));
        else mat_mul(out[i],out[g_parent[i]],local);
    }
}
