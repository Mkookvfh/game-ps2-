#include "renderer3d.h"
#include <android/log.h>
#include <math.h>
#include <string.h>

#define PI 3.14159265f
#define TERRAIN_SIZE 32
#define TERRAIN_STEP 2.5f
#define TERRAIN_VERTICES (TERRAIN_SIZE*TERRAIN_SIZE*6)

static GLfloat terrain[TERRAIN_VERTICES * 6];
static GLfloat cube[36 * 6];

static const char *VS =
"attribute vec3 aPosition;"
"attribute vec3 aNormal;"
"uniform mat4 uMVP;"
"uniform mat4 uModel;"
"uniform vec3 uCamera;"
"varying vec3 vNormal;"
"varying float vDistance;"
"void main(){"
" vec4 world=uModel*vec4(aPosition,1.0);"
" gl_Position=uMVP*vec4(aPosition,1.0);"
" vNormal=mat3(uModel)*aNormal;"
" vDistance=length(world.xyz-uCamera);"
"}";

static const char *FS =
"precision mediump float;"
"uniform vec3 uColor;"
"varying vec3 vNormal;"
"varying float vDistance;"
"void main(){"
" float light=max(dot(normalize(vNormal),"
" normalize(vec3(-0.4,0.85,0.3))),0.0);"
" float shade=0.38+light*0.62;"
" vec3 base=uColor*shade;"
" float fog=clamp((vDistance-24.0)/75.0,0.0,0.78);"
" vec3 fogColor=vec3(0.58,0.72,0.79);"
" gl_FragColor=vec4(mix(base,fogColor,fog),1.0);"
"}";

static void identity(float *m)
{
    memset(m,0,16*sizeof(float));
    m[0]=m[5]=m[10]=m[15]=1.0f;
}

static void multiply(float *o,const float *a,const float *b)
{
    float t[16];
    for(int c=0;c<4;c++)
        for(int r=0;r<4;r++){
            t[c*4+r]=0.0f;
            for(int k=0;k<4;k++)
                t[c*4+r]+=a[k*4+r]*b[c*4+k];
        }
    memcpy(o,t,sizeof(t));
}

static void perspective(float *m,float fov,float aspect,float near_z,float far_z)
{
    memset(m,0,16*sizeof(float));
    float f=1.0f/tanf(fov*0.5f);
    m[0]=f/aspect;
    m[5]=f;
    m[10]=(far_z+near_z)/(near_z-far_z);
    m[11]=-1.0f;
    m[14]=(2.0f*far_z*near_z)/(near_z-far_z);
}

static void look_at(float *m,float ex,float ey,float ez,
                    float tx,float ty,float tz)
{
    float fx=tx-ex, fy=ty-ey, fz=tz-ez;
    float fl=sqrtf(fx*fx+fy*fy+fz*fz);
    if(fl<0.0001f) fl=1.0f;
    fx/=fl; fy/=fl; fz/=fl;
    float sx=-fz, sy=0.0f, sz=fx;
    float sl=sqrtf(sx*sx+sz*sz);
    if(sl<0.001f){sx=1.0f;sz=0.0f;sl=1.0f;}
    sx/=sl; sz/=sl;
    float ux=sy*fz-sz*fy;
    float uy=sz*fx-sx*fz;
    float uz=sx*fy-sy*fx;
    identity(m);
    m[0]=sx; m[4]=sy; m[8]=sz;
    m[1]=ux; m[5]=uy; m[9]=uz;
    m[2]=-fx; m[6]=-fy; m[10]=-fz;
    m[12]=-(sx*ex+sy*ey+sz*ez);
    m[13]=-(ux*ex+uy*ey+uz*ez);
    m[14]=(fx*ex+fy*ey+fz*ez);
}

static GLuint compile(GLenum type,const char *src)
{
    GLuint sh=glCreateShader(type);
    if(!sh) return 0;
    glShaderSource(sh,1,&src,NULL);
    glCompileShader(sh);
    GLint ok=0;
    glGetShaderiv(sh,GL_COMPILE_STATUS,&ok);
    if(!ok){
        char log[512];
        glGetShaderInfoLog(sh,sizeof(log),NULL,log);
        __android_log_print(ANDROID_LOG_ERROR,"MUEANG_ISARA",
                            "Renderer shader error: %s",log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static float height_at(float x,float z)
{
    return sinf(x*0.075f)*1.5f
         + cosf(z*0.09f)*1.2f
         + sinf((x+z)*0.045f)*1.0f;
}

static void put_vertex(int *index,float x,float y,float z,
                       float nx,float ny,float nz)
{
    int i=(*index)++;
    terrain[i*6]=x; terrain[i*6+1]=y; terrain[i*6+2]=z;
    terrain[i*6+3]=nx; terrain[i*6+4]=ny; terrain[i*6+5]=nz;
}

static void build_terrain(float center_x,float center_z)
{
    int index=0;
    float origin_x=floorf(center_x/TERRAIN_STEP)*TERRAIN_STEP
        -(TERRAIN_SIZE*0.5f)*TERRAIN_STEP;
    float origin_z=floorf(center_z/TERRAIN_STEP)*TERRAIN_STEP
        -(TERRAIN_SIZE*0.5f)*TERRAIN_STEP;
    for(int z=0;z<TERRAIN_SIZE;z++)
        for(int x=0;x<TERRAIN_SIZE;x++){
            float x0=origin_x+(float)x*TERRAIN_STEP;
            float z0=origin_z+(float)z*TERRAIN_STEP;
            float x1=x0+TERRAIN_STEP, z1=z0+TERRAIN_STEP;
            float y00=height_at(x0,z0), y10=height_at(x1,z0);
            float y01=height_at(x0,z1), y11=height_at(x1,z1);
            float gx=(height_at(x0+0.25f,z0)-height_at(x0-0.25f,z0))*2.0f;
            float gz=(height_at(x0,z0+0.25f)-height_at(x0,z0-0.25f))*2.0f;
            float len=sqrtf(gx*gx+1.0f+gz*gz);
            float nx=-gx/len, ny=1.0f/len, nz=-gz/len;
            put_vertex(&index,x0,y00,z0,nx,ny,nz);
            put_vertex(&index,x1,y10,z0,nx,ny,nz);
            put_vertex(&index,x0,y01,z1,nx,ny,nz);
            put_vertex(&index,x0,y01,z1,nx,ny,nz);
            put_vertex(&index,x1,y10,z0,nx,ny,nz);
            put_vertex(&index,x1,y11,z1,nx,ny,nz);
        }
}

static void build_cube(void)
{
    static const float p[8][3]={
        {-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},
        {0.5f,0.5f,-0.5f},{-0.5f,0.5f,-0.5f},
        {-0.5f,-0.5f,0.5f},{0.5f,-0.5f,0.5f},
        {0.5f,0.5f,0.5f},{-0.5f,0.5f,0.5f}
    };
    static const int faces[6][6]={
        {0,1,2,0,2,3},{5,4,7,5,7,6},
        {4,0,3,4,3,7},{1,5,6,1,6,2},
        {3,2,6,3,6,7},{4,5,1,4,1,0}
    };
    static const float n[6][3]={
        {0,0,-1},{0,0,1},{-1,0,0},
        {1,0,0},{0,1,0},{0,-1,0}
    };
    int k=0;
    for(int f=0;f<6;f++)
        for(int j=0;j<6;j++){
            int v=faces[f][j];
            for(int a=0;a<3;a++) cube[k++]=p[v][a];
            for(int a=0;a<3;a++) cube[k++]=n[f][a];
        }
}

static void draw_mesh(Renderer3D *r,const GLfloat *vertices,int count,
                      const float *model,const float *vp,
                      float red,float green,float blue)
{
    float mvp[16];
    multiply(mvp,vp,model);
    glUniformMatrix4fv(r->mvp,1,GL_FALSE,mvp);
    glUniformMatrix4fv(r->model,1,GL_FALSE,model);
    glUniform3f(r->color,red,green,blue);
    glVertexAttribPointer(r->position,3,GL_FLOAT,GL_FALSE,
                          6*sizeof(float),vertices);
    glVertexAttribPointer(r->normal,3,GL_FLOAT,GL_FALSE,
                          6*sizeof(float),vertices+3);
    glEnableVertexAttribArray(r->position);
    glEnableVertexAttribArray(r->normal);
    glDrawArrays(GL_TRIANGLES,0,count);
}

static void draw_cube(Renderer3D *r,const float *vp,
    float x,float y,float z,float sx,float sy,float sz,
    float yaw,float red,float green,float blue)
{
    float t[16],rot[16],scale[16],a[16],model[16];
    identity(t); t[12]=x; t[13]=y; t[14]=z;
    identity(rot);
    rot[0]=cosf(yaw); rot[2]=-sinf(yaw);
    rot[8]=sinf(yaw); rot[10]=cosf(yaw);
    identity(scale); scale[0]=sx; scale[5]=sy; scale[10]=sz;
    multiply(a,rot,scale); multiply(model,t,a);
    draw_mesh(r,cube,36,model,vp,red,green,blue);
}

static void mountain_triangle(GLfloat *v,int *index,
    const float *a,const float *b,const float *c)
{
    float ux=b[0]-a[0], uy=b[1]-a[1], uz=b[2]-a[2];
    float vx=c[0]-a[0], vy=c[1]-a[1], vz=c[2]-a[2];
    float nx=uy*vz-uz*vy, ny=uz*vx-ux*vz, nz=ux*vy-uy*vx;
    float len=sqrtf(nx*nx+ny*ny+nz*nz);
    if(len>0.0001f){nx/=len;ny/=len;nz/=len;}
    const float *p[3]={a,b,c};
    for(int i=0;i<3;i++){
        for(int j=0;j<3;j++) v[(*index)++]=p[i][j];
        v[(*index)++]=nx; v[(*index)++]=ny; v[(*index)++]=nz;
    }
}

static void draw_mountain(Renderer3D *r,const float *vp,
    float x,float y,float z,float sx,float sy,float sz)
{
    GLfloat v[18*6]; int index=0;
    const float a[3]={-1,0,-1},b[3]={1,0,-1};
    const float c[3]={1,0,1},d[3]={-1,0,1},peak[3]={0,1.8f,0};
    mountain_triangle(v,&index,a,b,peak);
    mountain_triangle(v,&index,b,c,peak);
    mountain_triangle(v,&index,c,d,peak);
    mountain_triangle(v,&index,d,a,peak);
    mountain_triangle(v,&index,a,c,b);
    mountain_triangle(v,&index,a,d,c);
    float model[16]; identity(model);
    model[0]=sx; model[5]=sy; model[10]=sz;
    model[12]=x; model[13]=y; model[14]=z;
    draw_mesh(r,v,18,model,vp,0.38f,0.37f,0.34f);
}

static void draw_inverted_rock(Renderer3D *r,const float *vp,
    float x,float y,float z,float sx,float sy,float sz)
{
    GLfloat v[18*6]; int index=0;
    const float a[3]={-1,0,-1},b[3]={1,0,-1};
    const float c[3]={1,0,1},d[3]={-1,0,1},tip[3]={0,-2.2f,0};
    mountain_triangle(v,&index,a,tip,b);
    mountain_triangle(v,&index,b,tip,c);
    mountain_triangle(v,&index,c,tip,d);
    mountain_triangle(v,&index,d,tip,a);
    mountain_triangle(v,&index,a,b,c);
    mountain_triangle(v,&index,a,c,d);
    float model[16]; identity(model);
    model[0]=sx; model[5]=sy; model[10]=sz;
    model[12]=x; model[13]=y; model[14]=z;
    draw_mesh(r,v,18,model,vp,0.25f,0.24f,0.23f);
}

static void draw_tree(Renderer3D *r,const float *vp,
                      float x,float z,float scale)
{
    float y=height_at(x,z);
    draw_cube(r,vp,x,y+0.85f*scale,z,0.28f*scale,1.7f*scale,
        0.28f*scale,0,0.30f,0.18f,0.09f);
    draw_cube(r,vp,x,y+2.05f*scale,z,1.55f*scale,1.85f*scale,
        1.55f*scale,0,0.12f,0.30f,0.13f);
    draw_cube(r,vp,x,y+2.75f*scale,z,1.05f*scale,1.25f*scale,
        1.05f*scale,0,0.16f,0.37f,0.16f);
}

static void draw_floating_island(Renderer3D *r,const float *vp)
{
    const float ix=0.0f, iz=39.0f, base_y=19.0f;
    draw_cube(r,vp,ix,base_y,iz,14.0f,2.2f,11.0f,0,0.30f,0.39f,0.23f);
    draw_inverted_rock(r,vp,ix,base_y-1.0f,iz,6.7f,7.0f,5.2f);
    draw_inverted_rock(r,vp,-4.0f,base_y-1.1f,iz+1.0f,2.8f,4.4f,2.5f);
    draw_inverted_rock(r,vp,4.2f,base_y-1.1f,iz-0.8f,2.7f,4.8f,2.4f);
    draw_mountain(r,vp,ix,base_y+1.0f,iz-1.0f,4.5f,6.2f,4.0f);
    draw_cube(r,vp,-2.4f,base_y+1.8f,iz+3.0f,0.25f,2.0f,0.25f,0,0.29f,0.17f,0.08f);
    draw_cube(r,vp,-2.4f,base_y+3.0f,iz+3.0f,1.8f,2.0f,1.8f,0,0.12f,0.31f,0.13f);
    draw_cube(r,vp,3.2f,base_y+1.8f,iz+2.0f,0.25f,2.0f,0.25f,0,0.29f,0.17f,0.08f);
    draw_cube(r,vp,3.2f,base_y+3.0f,iz+2.0f,1.8f,2.0f,1.8f,0,0.12f,0.31f,0.13f);

    draw_cube(r,vp,-4.4f,11.4f,iz+0.2f,0.48f,13.0f,0.12f,0,0.20f,0.62f,0.78f);
    draw_cube(r,vp,0.0f,10.8f,iz+5.35f,0.38f,14.0f,0.12f,0,0.24f,0.68f,0.82f);
    draw_cube(r,vp,4.3f,11.7f,iz-0.4f,0.42f,12.4f,0.12f,0,0.18f,0.57f,0.76f);
    draw_cube(r,vp,-4.25f,11.4f,iz+0.10f,0.10f,12.6f,0.035f,0,0.62f,0.86f,0.91f);
    draw_cube(r,vp,0.10f,10.8f,iz+5.25f,0.08f,13.6f,0.035f,0,0.67f,0.89f,0.94f);
    draw_cube(r,vp,4.42f,11.7f,iz-0.50f,0.09f,12.0f,0.035f,0,0.60f,0.84f,0.91f);
    draw_cube(r,vp,-4.4f,4.8f,iz+0.2f,2.5f,0.35f,1.5f,0,0.68f,0.79f,0.80f);
    draw_cube(r,vp,0.0f,4.5f,iz+5.2f,2.8f,0.30f,1.4f,0,0.70f,0.82f,0.83f);
    draw_cube(r,vp,4.3f,4.9f,iz-0.4f,2.4f,0.32f,1.5f,0,0.66f,0.78f,0.81f);
}

int Renderer3D_Init(Renderer3D *r)
{
    if(!r) return 0;
    memset(r,0,sizeof(*r));
    GLuint vs=compile(GL_VERTEX_SHADER,VS);
    GLuint fs=compile(GL_FRAGMENT_SHADER,FS);
    if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return 0;}
    r->program=glCreateProgram();
    if(!r->program){glDeleteShader(vs);glDeleteShader(fs);return 0;}
    glAttachShader(r->program,vs); glAttachShader(r->program,fs);
    glLinkProgram(r->program); glDeleteShader(vs); glDeleteShader(fs);
    GLint ok=0; glGetProgramiv(r->program,GL_LINK_STATUS,&ok);
    if(!ok){
        char log[512];
        glGetProgramInfoLog(r->program,sizeof(log),NULL,log);
        __android_log_print(ANDROID_LOG_ERROR,"MUEANG_ISARA",
                            "Renderer link error: %s",log);
        glDeleteProgram(r->program); r->program=0; return 0;
    }
    r->position=glGetAttribLocation(r->program,"aPosition");
    r->normal=glGetAttribLocation(r->program,"aNormal");
    r->mvp=glGetUniformLocation(r->program,"uMVP");
    r->model=glGetUniformLocation(r->program,"uModel");
    r->color=glGetUniformLocation(r->program,"uColor");
    r->camera=glGetUniformLocation(r->program,"uCamera");
    if(r->position<0||r->normal<0||r->mvp<0||r->model<0||
       r->color<0||r->camera<0){
        glDeleteProgram(r->program); memset(r,0,sizeof(*r)); return 0;
    }
    build_cube();
    return 1;
}

void Renderer3D_Draw(Renderer3D *r,const MueangGame *g,int width,int height)
{
    if(!r||!g||!r->program||width<=0||height<=0) return;
    glViewport(0,0,width,height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glClearColor(0.55f,0.72f,0.82f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glUseProgram(r->program);

    float px=g->player.x, pz=g->player.z;
    build_terrain(px,pz);

    float targetY=height_at(px,pz)+1.2f+g->player.jump_height;
    float distance=g->camera_distance;
    if(distance<3.0f||distance>14.0f) distance=7.5f;
    float yaw=g->camera_yaw, pitch=g->camera_pitch;
    float cx=px-sinf(yaw)*cosf(pitch)*distance;
    float cy=targetY+sinf(pitch)*distance+1.0f;
    float cz=pz-cosf(yaw)*cosf(pitch)*distance;

    float projection[16],view[16],vp[16];
    perspective(projection,PI/3.0f,(float)width/(float)height,0.1f,220.0f);
    look_at(view,cx,cy,cz,px,targetY,pz);
    multiply(vp,projection,view);
    glUniform3f(r->camera,cx,cy,cz);

    float identity_model[16]; identity(identity_model);
    draw_mesh(r,terrain,TERRAIN_VERTICES,identity_model,vp,0.24f,0.40f,0.20f);

    draw_mountain(r,vp,-28.0f,height_at(-28.0f,-25.0f),-25.0f,9.0f,12.0f,8.0f);
    draw_mountain(r,vp,28.0f,height_at(28.0f,-25.0f),-25.0f,10.0f,14.0f,9.0f);
    draw_mountain(r,vp,-28.0f,height_at(-28.0f,25.0f),25.0f,11.0f,13.0f,9.0f);
    draw_mountain(r,vp,28.0f,height_at(28.0f,25.0f),25.0f,10.0f,15.0f,9.0f);

    /* Fixed village house */
    {
        const float hx=5.0f,hz=-5.0f;
        float hy=height_at(hx,hz);
        draw_cube(r,vp,hx,hy+1.2f,hz,3.2f,2.4f,3.0f,0,0.62f,0.43f,0.27f);
        draw_cube(r,vp,hx,hy+2.7f,hz,3.8f,0.35f,3.6f,0.35f,0.36f,0.15f,0.10f);
    }

    draw_tree(r,vp,-12.0f,-8.0f,1.0f);
    draw_tree(r,vp,-5.0f,-11.0f,0.9f);
    draw_tree(r,vp,2.0f,-13.0f,1.1f);
    draw_tree(r,vp,11.0f,-9.0f,1.0f);
    draw_tree(r,vp,15.0f,1.0f,0.9f);
    draw_tree(r,vp,-15.0f,8.0f,1.1f);
    draw_tree(r,vp,-8.0f,14.0f,0.9f);
    draw_tree(r,vp,9.0f,13.0f,1.0f);
    draw_tree(r,vp,16.0f,17.0f,1.0f);

    draw_floating_island(r,vp);

    /* Player model follows the jump arc. */
    {
        float py=height_at(px,pz)+g->player.jump_height;
        draw_cube(r,vp,px,py+0.95f,pz,0.55f,1.35f,0.38f,
            g->player.yaw,0.78f,0.76f,0.70f);
        draw_cube(r,vp,px,py+1.85f,pz,0.42f,0.42f,0.42f,
            g->player.yaw,0.82f,0.78f,0.66f);
        draw_cube(r,vp,px,py+0.95f,pz-0.28f,0.72f,1.25f,0.12f,
            g->player.yaw,0.55f,0.08f,0.06f);
    }

    glDisableVertexAttribArray(r->position);
    glDisableVertexAttribArray(r->normal);
    glDisable(GL_DEPTH_TEST);
}
