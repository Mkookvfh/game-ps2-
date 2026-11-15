
#include "renderer3d.h"
#include <math.h>
#include <string.h>

#define PI 3.14159265f
#define TERRAIN_SIZE 32
#define TERRAIN_STEP 2.5f
#define TERRAIN_VERTICES (TERRAIN_SIZE*TERRAIN_SIZE*6)

static GLfloat terrain[TERRAIN_VERTICES * 6];
static GLfloat cube[36 * 6];
static int terrain_ready = 0;

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
" float fog=clamp((vDistance-24.0)/65.0,0.0,0.78);"
" vec3 fogColor=vec3(0.58,0.72,0.79);"
" gl_FragColor=vec4(mix(base,fogColor,fog),1.0);"
"}";

static void identity(float *m)
{
    memset(m,0,16*sizeof(float));
    m[0]=m[5]=m[10]=m[15]=1;
}

static void multiply(float *o,const float *a,const float *b)
{
    float t[16];

    for(int c=0;c<4;c++)
        for(int r=0;r<4;r++){
            t[c*4+r]=0;
            for(int k=0;k<4;k++)
                t[c*4+r]+=a[k*4+r]*b[c*4+k];
        }

    memcpy(o,t,sizeof(t));
}

static void perspective(float *m,float fov,float aspect,float near,float far)
{
    memset(m,0,16*sizeof(float));

    float f=1.0f/tanf(fov*0.5f);

    m[0]=f/aspect;
    m[5]=f;
    m[10]=(far+near)/(near-far);
    m[11]=-1;
    m[14]=(2*far*near)/(near-far);
}

static void look_at(float *m,
    float ex,float ey,float ez,
    float tx,float ty,float tz)
{
    float fx=tx-ex, fy=ty-ey, fz=tz-ez;
    float fl=sqrtf(fx*fx+fy*fy+fz*fz);

    fx/=fl; fy/=fl; fz/=fl;

    float sx= -fz, sy=0, sz=fx;
    float sl=sqrtf(sx*sx+sz*sz);

    if(sl<0.001f){sx=1;sz=0;sl=1;}

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
    glShaderSource(sh,1,&src,NULL);
    glCompileShader(sh);

    GLint ok=0;
    glGetShaderiv(sh,GL_COMPILE_STATUS,&ok);

    if(!ok){
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

static void put_vertex(int *index,
    float x,float y,float z,
    float nx,float ny,float nz)
{
    int i=(*index)++;

    terrain[i*6]=x;
    terrain[i*6+1]=y;
    terrain[i*6+2]=z;
    terrain[i*6+3]=nx;
    terrain[i*6+4]=ny;
    terrain[i*6+5]=nz;
}

static void build_terrain(void)
{
    int index=0;

    for(int z=0;z<TERRAIN_SIZE;z++){
        for(int x=0;x<TERRAIN_SIZE;x++){

            float x0=(x-TERRAIN_SIZE/2)*TERRAIN_STEP;
            float z0=(z-TERRAIN_SIZE/2)*TERRAIN_STEP;
            float x1=x0+TERRAIN_STEP;
            float z1=z0+TERRAIN_STEP;

            float y00=height_at(x0,z0);
            float y10=height_at(x1,z0);
            float y01=height_at(x0,z1);
            float y11=height_at(x1,z1);

            float nx=-(y10-y00)/TERRAIN_STEP;
            float nz=-(y01-y00)/TERRAIN_STEP;
            float len=sqrtf(nx*nx+1+nz*nz);

            nx/=len; nz/=len;

            put_vertex(&index,x0,y00,z0,nx,1/len,nz);
            put_vertex(&index,x1,y10,z0,nx,1/len,nz);
            put_vertex(&index,x0,y01,z1,nx,1/len,nz);

            put_vertex(&index,x0,y01,z1,nx,1/len,nz);
            put_vertex(&index,x1,y10,z0,nx,1/len,nz);
            put_vertex(&index,x1,y11,z1,nx,1/len,nz);
        }
    }

    terrain_ready=1;
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

            for(int a=0;a<3;a++)
                cube[k++]=p[v][a];

            for(int a=0;a<3;a++)
                cube[k++]=n[f][a];
        }
}

static void draw_mesh(Renderer3D *r,
    const GLfloat *vertices,int count,
    const float *model,const float *vp,
    float red,float green,float blue)
{
    float mvp[16];
    multiply(mvp,vp,model);

    glUniformMatrix4fv(r->mvp,1,GL_FALSE,mvp);
    glUniformMatrix4fv(r->model,1,GL_FALSE,model);
    glUniform3f(r->color,red,green,blue);

    glVertexAttribPointer(r->position,3,GL_FLOAT,
        GL_FALSE,6*sizeof(float),vertices);

    glVertexAttribPointer(r->normal,3,GL_FLOAT,
        GL_FALSE,6*sizeof(float),vertices+3);

    glEnableVertexAttribArray(r->position);
    glEnableVertexAttribArray(r->normal);

    glDrawArrays(GL_TRIANGLES,0,count);
}

static void draw_cube(Renderer3D *r,const float *vp,
    float x,float y,float z,
    float sx,float sy,float sz,
    float yaw,float red,float green,float blue)
{
    float t[16],rot[16],scale[16],a[16],model[16];

    identity(t);
    t[12]=x; t[13]=y; t[14]=z;

    identity(rot);
    rot[0]=cosf(yaw);
    rot[2]=-sinf(yaw);
    rot[8]=sinf(yaw);
    rot[10]=cosf(yaw);

    identity(scale);
    scale[0]=sx; scale[5]=sy; scale[10]=sz;

    multiply(a,rot,scale);
    multiply(model,t,a);

    draw_mesh(r,cube,36,model,vp,red,green,blue);
}


static void mountain_triangle(
    GLfloat *v, int *index,
    const float *a,
    const float *b,
    const float *c)
{
    float ux = b[0] - a[0];
    float uy = b[1] - a[1];
    float uz = b[2] - a[2];

    float vx = c[0] - a[0];
    float vy = c[1] - a[1];
    float vz = c[2] - a[2];

    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;

    float len = sqrtf(nx*nx + ny*ny + nz*nz);
    if (len > 0.0001f) {
        nx /= len;
        ny /= len;
        nz /= len;
    }

    const float *p[3] = {a, b, c};

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++)
            v[(*index)++] = p[i][j];

        v[(*index)++] = nx;
        v[(*index)++] = ny;
        v[(*index)++] = nz;
    }
}

static void draw_mountain(
    Renderer3D *r,
    const float *vp,
    float x, float y, float z,
    float sx, float sy, float sz)
{
    GLfloat v[18 * 6];
    int index = 0;

    const float a[3] = {-1, 0, -1};
    const float b[3] = { 1, 0, -1};
    const float c[3] = { 1, 0,  1};
    const float d[3] = {-1, 0,  1};
    const float peak[3] = {0, 1.8f, 0};

    mountain_triangle(v, &index, a, b, peak);
    mountain_triangle(v, &index, b, c, peak);
    mountain_triangle(v, &index, c, d, peak);
    mountain_triangle(v, &index, d, a, peak);

    mountain_triangle(v, &index, a, c, b);
    mountain_triangle(v, &index, a, d, c);

    float model[16];
    identity(model);

    model[0] = sx;
    model[5] = sy;
    model[10] = sz;

    model[12] = x;
    model[13] = y;
    model[14] = z;

    draw_mesh(
        r, v, 18, model, vp,
        0.38f, 0.37f, 0.34f
    );
}

int Renderer3D_Init(Renderer3D *r)
{
    GLuint vs=compile(GL_VERTEX_SHADER,VS);
    GLuint fs=compile(GL_FRAGMENT_SHADER,FS);

    if(!vs||!fs) return 0;

    r->program=glCreateProgram();
    glAttachShader(r->program,vs);
    glAttachShader(r->program,fs);
    glLinkProgram(r->program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok=0;
    glGetProgramiv(r->program,GL_LINK_STATUS,&ok);

    if(!ok) return 0;

    r->position=glGetAttribLocation(r->program,"aPosition");
    r->normal=glGetAttribLocation(r->program,"aNormal");
    r->mvp=glGetUniformLocation(r->program,"uMVP");
    r->model=glGetUniformLocation(r->program,"uModel");
    r->color=glGetUniformLocation(r->program,"uColor");
    r->camera=glGetUniformLocation(r->program,"uCamera");

    build_cube();
    build_terrain();

    return 1;
}

void Renderer3D_Draw(Renderer3D *r,
    const MueangGame *g,int width,int height)
{
    if(!r||!g||width<=0||height<=0) return;

    glViewport(0,0,width,height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);

    glClearColor(0.55f,0.72f,0.82f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    glUseProgram(r->program);

    float px=g->player.x;
    float pz=g->player.z;

    float targetY=height_at(px,pz)+1.2f;
    float distance=7.5f;

    float yaw=g->camera_yaw;
    float pitch=g->camera_pitch;

    float cx=px-sinf(yaw)*cosf(pitch)*distance;
    float cy=targetY+sinf(pitch)*distance+1.0f;
    float cz=pz-cosf(yaw)*cosf(pitch)*distance;

    float projection[16],view[16],vp[16];

    perspective(projection,PI/3.0f,
        (float)width/(float)height,0.1f,180.0f);

    look_at(view,cx,cy,cz,px,targetY,pz);
    multiply(vp,projection,view);

    glUniform3f(r->camera,cx,cy,cz);

    float identityModel[16];
    identity(identityModel);

    draw_mesh(r,terrain,TERRAIN_VERTICES,
        identityModel,vp,0.24f,0.40f,0.20f);

    /* Ancient village house */
    float hx=px+5.0f;
    float hz=pz-5.0f;
    float hy=height_at(hx,hz);

    draw_cube(r,vp,hx,hy+1.2f,hz,
        3.2f,2.4f,3.0f,0,
        0.62f,0.43f,0.27f);

    draw_cube(r,vp,hx,hy+2.7f,hz,
        3.8f,0.35f,3.6f,0.35f,
        0.36f,0.15f,0.10f);

    /* Trees */
    for(int i=0;i<9;i++){
        float tx=px+(float)((i%3)-1)*9.0f+7.0f;
        float tz=pz+(float)(i/3)*8.0f-8.0f;
        float ty=height_at(tx,tz);

        draw_cube(r,vp,tx,ty+1.0f,tz,
            0.35f,2.0f,0.35f,0,
            0.30f,0.18f,0.09f);

        draw_cube(r,vp,tx,ty+2.6f,tz,
            1.8f,2.0f,1.8f,0,
            0.13f,0.32f,0.14f);
    }

    /* Player prototype: body, head and cape */
    float py=height_at(px,pz);

    draw_cube(r,vp,px,py+0.95f,pz,
        0.55f,1.35f,0.38f,g->player.yaw,
        0.78f,0.76f,0.70f);

    draw_cube(r,vp,px,py+1.85f,pz,
        0.42f,0.42f,0.42f,g->player.yaw,
        0.82f,0.78f,0.66f);

    draw_cube(r,vp,px,py+0.95f,pz-0.28f,
        0.72f,1.25f,0.12f,g->player.yaw,
        0.55f,0.08f,0.06f);

    glDisableVertexAttribArray(r->position);
    glDisableVertexAttribArray(r->normal);

    glDisable(GL_DEPTH_TEST);
}
