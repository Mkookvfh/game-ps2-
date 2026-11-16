#include "glb_loader.h"
#include "renderer3d.h"
#include "skeleton.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#define PARTS 27
#define INDEX_SIZE 4
#define BONES 21
#define STRIDE 14
#define PI 3.14159265358979323846f
typedef struct {uint32_t position_offset,vertex_count,color_offset,index_offset,index_count;} PartInfo;
typedef struct {GLuint vbo;GLsizei count;float color[3];} Part;
static Part g_parts[PARTS];static GLuint g_program=0;static GLint g_pos=-1,g_normal=-1,g_joints=-1,g_weights=-1,g_mvp=-1,g_model=-1,g_color=-1,g_bones=-1;static int g_loaded=0;
static const PartInfo g_info[PARTS]={
{144,8,240,0,36},{272,8,368,0,36},{400,8,496,0,36},{528,8,368,0,36},{624,8,496,0,36},{720,8,240,0,36},{4656,162,6600,816,960},{7248,162,9192,816,960},{9840,8,9936,0,36},{9968,8,240,0,36},{10064,8,10160,0,36},{11152,42,11656,10192,240},{11824,8,240,0,36},{11920,8,10160,0,36},{12016,42,11656,10192,240},{12520,8,368,0,36},{12616,8,10160,0,36},{12712,8,9936,0,36},{12808,8,368,0,36},{12904,8,10160,0,36},{13000,8,9936,0,36},{13096,8,13192,0,36},{13224,8,13192,0,36},{13320,8,13192,0,36},{13416,8,368,0,36},{13512,8,496,0,36},{13608,8,10160,0,36}};
static const int primary[PARTS]={2,3,3,1,1,1,5,5,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,1,1,1};
static const int secondary[PARTS]={1,2,2,2,1,2,4,4,4,7,8,7,10,11,10,13,14,13,16,17,16,19,20,19,2,2,2};
static uint32_t u32(const unsigned char*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void ident(float*m){memset(m,0,64);m[0]=m[5]=m[10]=m[15]=1;}
static void mul(float*o,const float*a,const float*b){float t[16];for(int c=0;c<4;c++)for(int r=0;r<4;r++){t[c*4+r]=0;for(int k=0;k<4;k++)t[c*4+r]+=a[k*4+r]*b[c*4+k];}memcpy(o,t,64);}
static GLuint shader(GLenum type,const char*src){GLuint s=glCreateShader(type);if(!s)return 0;glShaderSource(s,1,&src,NULL);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){glDeleteShader(s);return 0;}return s;}
static const char *VS=
"attribute vec3 aPosition;attribute vec3 aNormal;attribute vec4 aJoints;attribute vec4 aWeights;"
"uniform mat4 uMVP;uniform mat4 uModel;uniform mat4 uBones[21];uniform vec3 uColor;"
"varying vec3 vNormal;varying vec3 vColor;"
"mat4 B(float i){"
"if(i<0.5)return uBones[0];if(i<1.5)return uBones[1];if(i<2.5)return uBones[2];if(i<3.5)return uBones[3];if(i<4.5)return uBones[4];if(i<5.5)return uBones[5];"
"if(i<6.5)return uBones[6];if(i<7.5)return uBones[7];if(i<8.5)return uBones[8];if(i<9.5)return uBones[9];if(i<10.5)return uBones[10];if(i<11.5)return uBones[11];"
"if(i<12.5)return uBones[12];if(i<13.5)return uBones[13];if(i<14.5)return uBones[14];if(i<15.5)return uBones[15];if(i<16.5)return uBones[16];if(i<17.5)return uBones[17];"
"if(i<18.5)return uBones[18];if(i<19.5)return uBones[19];return uBones[20];}"
"void main(){mat4 skin=B(aJoints.x)*aWeights.x+B(aJoints.y)*aWeights.y+B(aJoints.z)*aWeights.z+B(aJoints.w)*aWeights.w;"
"vec4 p=skin*vec4(aPosition,1.0);gl_Position=uMVP*p;vNormal=normalize(mat3(uModel)*mat3(skin)*aNormal);"
"float l=max(dot(normalize(vNormal),normalize(vec3(-0.4,0.85,0.3))),0.0);vColor=uColor*(0.38+l*0.62);} ";
static const char *FS="precision mediump float;varying vec3 vNormal;varying vec3 vColor;void main(){gl_FragColor=vec4(vColor,1.0);} ";
static float smooth01(float x){x=fmaxf(0,fminf(1,x));return x*x*(3-2*x);}
static void vertex_weights(int part,const float p[3],float*j0,float*j1,float*w0,float*w1){
 int a=primary[part],b=secondary[part];float blend=0.0f;
 switch(part){case 9:case 12:blend=1.0f-smooth01((p[1]-1.18f)/.28f);break;case 10:case 13:blend=1.0f-smooth01((p[1]-.94f)/.22f);break;case 11:case 14:blend=.18f;break;case 15:case 18:blend=1.0f-smooth01((p[1]-.42f)/.30f);break;case 16:case 19:blend=1.0f-smooth01((p[1]-.16f)/.18f);break;case 17:case 20:blend=.15f;break;case 21:blend=.22f;break;case 22:blend=1.0f-smooth01((p[1]-.72f)/.58f);break;case 23:blend=.25f+0.35f*smooth01((.7f-p[1])/.6f);break;case 0:blend=.18f;break;case 1:case 2:case 5:blend=.12f;break;default:blend=.04f;break;}
 *j0=(float)a;*j1=(float)b;*w1=blend;*w0=1.0f-blend;
}
static int build_part(int id,const unsigned char*bin,size_t size){const PartInfo*q=&g_info[id];size_t pb=(size_t)q->vertex_count*12,cb=(size_t)q->vertex_count*4,ib=(size_t)q->index_count*INDEX_SIZE;if((size_t)q->position_offset+pb>size||(size_t)q->color_offset+cb>size||(size_t)q->index_offset+ib>size||q->index_count%3)return 0;
 const unsigned char*pos=bin+q->position_offset,*col=bin+q->color_offset,*idx=bin+q->index_offset;size_t floats=(size_t)q->index_count*STRIDE;GLfloat*v=(GLfloat*)calloc(floats,sizeof(GLfloat));if(!v)return 0;
 for(uint32_t k=0;k<q->index_count;k++){uint32_t ix=u32(idx+(size_t)k*4);if(ix>=q->vertex_count){free(v);return 0;}float p[3];memcpy(p,pos+(size_t)ix*12,12);float*j=v+(size_t)k*STRIDE+6;float*w=j+4;vertex_weights(id,p,&j[0],&j[1],&w[0],&w[1]);w[2]=w[3]=0.0f;}
 for(uint32_t k=0;k<q->index_count;k+=3){uint32_t ia=u32(idx+(size_t)k*4),ibx=u32(idx+(size_t)(k+1)*4),ic=u32(idx+(size_t)(k+2)*4);float a[3],b[3],c[3];memcpy(a,pos+(size_t)ia*12,12);memcpy(b,pos+(size_t)ibx*12,12);memcpy(c,pos+(size_t)ic*12,12);float ux=b[0]-a[0],uy=b[1]-a[1],uz=b[2]-a[2],vx=c[0]-a[0],vy=c[1]-a[1],vz=c[2]-a[2];float nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx,l=sqrtf(nx*nx+ny*ny+nz*nz);if(l>1e-6f){nx/=l;ny/=l;nz/=l;}else{nx=0;ny=1;nz=0;}uint32_t ids[3]={ia,ibx,ic};for(int t=0;t<3;t++){float*out=v+((size_t)k+t)*STRIDE;memcpy(out,pos+(size_t)ids[t]*12,12);out[3]=nx;out[4]=ny;out[5]=nz;}}
 GLuint b=0;glGenBuffers(1,&b);if(!b){free(v);return 0;}glBindBuffer(GL_ARRAY_BUFFER,b);glBufferData(GL_ARRAY_BUFFER,floats*sizeof(float),v,GL_STATIC_DRAW);glBindBuffer(GL_ARRAY_BUFFER,0);free(v);g_parts[id].vbo=b;g_parts[id].count=(GLsizei)q->index_count;g_parts[id].color[0]=col[0]/255.f;g_parts[id].color[1]=col[1]/255.f;g_parts[id].color[2]=col[2]/255.f;return 1;}
void GLBLoader_Release(struct Renderer3D*r){(void)r;for(int i=0;i<PARTS;i++){if(g_parts[i].vbo)glDeleteBuffers(1,&g_parts[i].vbo);memset(&g_parts[i],0,sizeof(g_parts[i]));}if(g_program)glDeleteProgram(g_program);g_program=0;g_loaded=0;}
int GLBLoader_Load(struct Renderer3D*r,AAssetManager*am,const char*path){(void)r;if(!am||!path)return 0;GLBLoader_Release(r);GLuint vs=shader(GL_VERTEX_SHADER,VS),fs=shader(GL_FRAGMENT_SHADER,FS);if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return 0;}g_program=glCreateProgram();glAttachShader(g_program,vs);glAttachShader(g_program,fs);glLinkProgram(g_program);glDeleteShader(vs);glDeleteShader(fs);GLint ok=0;glGetProgramiv(g_program,GL_LINK_STATUS,&ok);if(!ok){GLBLoader_Release(r);return 0;}g_pos=glGetAttribLocation(g_program,"aPosition");g_normal=glGetAttribLocation(g_program,"aNormal");g_joints=glGetAttribLocation(g_program,"aJoints");g_weights=glGetAttribLocation(g_program,"aWeights");g_mvp=glGetUniformLocation(g_program,"uMVP");g_model=glGetUniformLocation(g_program,"uModel");g_color=glGetUniformLocation(g_program,"uColor");g_bones=glGetUniformLocation(g_program,"uBones[0]");if(g_pos<0||g_normal<0||g_joints<0||g_weights<0||g_mvp<0||g_model<0||g_color<0||g_bones<0){GLBLoader_Release(r);return 0;}
 AAsset*a=AAssetManager_open(am,path,AASSET_MODE_BUFFER);if(!a){GLBLoader_Release(r);return 0;}off_t len=AAsset_getLength(a);if(len<32||len>16*1024*1024){AAsset_close(a);GLBLoader_Release(r);return 0;}unsigned char*file=(unsigned char*)malloc((size_t)len);if(!file){AAsset_close(a);GLBLoader_Release(r);return 0;}int got=AAsset_read(a,file,(size_t)len);AAsset_close(a);if(got!=len||memcmp(file,"glTF",4)||u32(file+4)!=2||u32(file+8)!=(uint32_t)len){free(file);GLBLoader_Release(r);return 0;}uint32_t jl=u32(file+12);if(u32(file+16)!=0x4E4F534A||jl>(uint32_t)len-28){free(file);GLBLoader_Release(r);return 0;}size_t bh=20u+jl;if(bh+8u>(size_t)len){free(file);GLBLoader_Release(r);return 0;}uint32_t bl=u32(file+bh);if(u32(file+bh+4)!=0x004E4942||bh+8u+bl>(size_t)len||bl<13704u){free(file);GLBLoader_Release(r);return 0;}const unsigned char*bin=file+bh+8u;int success=1;for(int i=0;i<PARTS;i++)if(!build_part(i,bin,bl)){success=0;break;}free(file);if(!success){GLBLoader_Release(r);return 0;}g_loaded=1;return 1;}
int GLBLoader_Draw(struct Renderer3D*r,const float*vp,float x,float y,float z,float yaw,float bob,int state,float phase,int crouch){(void)r;if(!g_loaded||!vp||!g_program)return 0;float t[16],rot[16],root[16],mvp[16],palette[BONES][16];ident(t);t[12]=x;t[13]=y+bob;t[14]=z;ident(rot);float c=cosf(yaw),s=sinf(yaw);rot[0]=c;rot[2]=-s;rot[8]=s;rot[10]=c;mul(root,t,rot);mul(mvp,vp,root);Skeleton_BuildBonePalette((MueangAnimState)state,phase,crouch,palette);glUseProgram(g_program);glUniformMatrix4fv(g_mvp,1,GL_FALSE,mvp);glUniformMatrix4fv(g_model,1,GL_FALSE,root);glUniformMatrix4fv(g_bones,BONES,GL_FALSE,&palette[0][0]);glEnableVertexAttribArray((GLuint)g_pos);glEnableVertexAttribArray((GLuint)g_normal);glEnableVertexAttribArray((GLuint)g_joints);glEnableVertexAttribArray((GLuint)g_weights);for(int i=0;i<PARTS;i++){Part*p=&g_parts[i];if(!p->vbo||p->count<=0)continue;glUniform3f(g_color,p->color[0],p->color[1],p->color[2]);glBindBuffer(GL_ARRAY_BUFFER,p->vbo);glVertexAttribPointer(g_pos,3,GL_FLOAT,GL_FALSE,STRIDE*sizeof(float),(const void*)0);glVertexAttribPointer(g_normal,3,GL_FLOAT,GL_FALSE,STRIDE*sizeof(float),(const void*)(3*sizeof(float)));glVertexAttribPointer(g_joints,4,GL_FLOAT,GL_FALSE,STRIDE*sizeof(float),(const void*)(6*sizeof(float)));glVertexAttribPointer(g_weights,4,GL_FLOAT,GL_FALSE,STRIDE*sizeof(float),(const void*)(10*sizeof(float)));glDrawArrays(GL_TRIANGLES,0,p->count);}glBindBuffer(GL_ARRAY_BUFFER,0);glDisableVertexAttribArray((GLuint)g_pos);glDisableVertexAttribArray((GLuint)g_normal);glDisableVertexAttribArray((GLuint)g_joints);glDisableVertexAttribArray((GLuint)g_weights);return 1;}
