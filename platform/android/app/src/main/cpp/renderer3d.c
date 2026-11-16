
#include "renderer3d.h"
#include <android/log.h>
#include <math.h>
#include <string.h>
#include "character3d.h"
#include "glb_loader.h"
#include "rigged_renderer.h"

#define PI 3.14159265f
#define TERRAIN_SIZE 32
#define TERRAIN_STEP 2.5f
#define TERRAIN_VERTICES (TERRAIN_SIZE*TERRAIN_SIZE*6)

static GLfloat terrain[TERRAIN_VERTICES * 6];
static GLfloat cube[36 * 6];

static const char *VS =
"attribute vec3 aPosition;attribute vec3 aNormal;"
"uniform mat4 uMVP;uniform mat4 uModel;uniform vec3 uCamera;"
"varying vec3 vNormal;varying float vDistance;"
"void main(){vec4 world=uModel*vec4(aPosition,1.0);"
"gl_Position=uMVP*vec4(aPosition,1.0);"
"vNormal=mat3(uModel)*aNormal;vDistance=length(world.xyz-uCamera);}";

static const char *FS =
"precision mediump float;uniform vec3 uColor;"
"varying vec3 vNormal;varying float vDistance;"
"void main(){float l=max(dot(normalize(vNormal),normalize(vec3(-0.4,0.85,0.3))),0.0);"
"vec3 base=uColor*(0.38+l*0.62);float fog=clamp((vDistance-24.0)/75.0,0.0,0.78);"
"gl_FragColor=vec4(mix(base,vec3(0.58,0.72,0.79),fog),1.0);}";

static void identity(float *m){
    memset(m,0,64);
    m[0]=m[5]=m[10]=m[15]=1.0f;
}

static void multiply(float *o,const float *a,const float *b){
    float t[16];
    for(int c=0;c<4;c++)
        for(int r=0;r<4;r++){
            t[c*4+r]=0;
            for(int k=0;k<4;k++)
                t[c*4+r]+=a[k*4+r]*b[c*4+k];
        }
    memcpy(o,t,sizeof(t));
}

static void perspective(float *m,float fov,float aspect,float n,float f){
    memset(m,0,64);
    float q=1.0f/tanf(fov*0.5f);
    m[0]=q/aspect;
    m[5]=q;
    m[10]=(f+n)/(n-f);
    m[11]=-1;
    m[14]=(2*f*n)/(n-f);
}

static void look_at(float *m,float ex,float ey,float ez,
                    float tx,float ty,float tz){
    float fx=tx-ex,fy=ty-ey,fz=tz-ez;
    float fl=sqrtf(fx*fx+fy*fy+fz*fz);
    if(fl<0.0001f)fl=1;
    fx/=fl;fy/=fl;fz/=fl;

    float sx=-fz,sy=0,sz=fx;
    float sl=sqrtf(sx*sx+sz*sz);
    if(sl<0.001f){sx=1;sz=0;sl=1;}
    sx/=sl;sz/=sl;

    float ux=sy*fz-sz*fy;
    float uy=sz*fx-sx*fz;
    float uz=sx*fy-sy*fx;

    identity(m);
    m[0]=sx;m[4]=sy;m[8]=sz;
    m[1]=ux;m[5]=uy;m[9]=uz;
    m[2]=-fx;m[6]=-fy;m[10]=-fz;
    m[12]=-(sx*ex+sy*ey+sz*ez);
    m[13]=-(ux*ex+uy*ey+uz*ez);
    m[14]=fx*ex+fy*ey+fz*ez;
}

static GLuint compile(GLenum type,const char *src){
    GLuint sh=glCreateShader(type);
    if(!sh)return 0;

    glShaderSource(sh,1,&src,NULL);
    glCompileShader(sh);

    GLint ok=0;
    glGetShaderiv(sh,GL_COMPILE_STATUS,&ok);

    if(!ok){
        char log[512];
        glGetShaderInfoLog(sh,sizeof(log),NULL,log);
        __android_log_print(ANDROID_LOG_ERROR,
            "MUEANG_ISARA","Shader: %s",log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static float height_at(float x,float z){
    return sinf(x*0.075f)*1.5f+
           cosf(z*0.09f)*1.2f+
           sinf((x+z)*0.045f);
}

static void put_vertex(int *i,float x,float y,float z,
                       float nx,float ny,float nz){
    int k=(*i)++;
    terrain[k*6]=x;
    terrain[k*6+1]=y;
    terrain[k*6+2]=z;
    terrain[k*6+3]=nx;
    terrain[k*6+4]=ny;
    terrain[k*6+5]=nz;
}

static void build_terrain(float cx,float cz){
    int i=0;
    float ox=floorf(cx/TERRAIN_STEP)*TERRAIN_STEP-40.0f;
    float oz=floorf(cz/TERRAIN_STEP)*TERRAIN_STEP-40.0f;

    for(int z=0;z<TERRAIN_SIZE;z++)
        for(int x=0;x<TERRAIN_SIZE;x++){
            float x0=ox+x*TERRAIN_STEP;
            float z0=oz+z*TERRAIN_STEP;
            float x1=x0+TERRAIN_STEP;
            float z1=z0+TERRAIN_STEP;

            float y00=height_at(x0,z0);
            float y10=height_at(x1,z0);
            float y01=height_at(x0,z1);
            float y11=height_at(x1,z1);

            float gx=(height_at(x0+0.25f,z0)-
                      height_at(x0-0.25f,z0))*2;
            float gz=(height_at(x0,z0+0.25f)-
                      height_at(x0,z0-0.25f))*2;

            float len=sqrtf(gx*gx+1+gz*gz);
            float nx=-gx/len,ny=1/len,nz=-gz/len;

            put_vertex(&i,x0,y00,z0,nx,ny,nz);
            put_vertex(&i,x1,y10,z0,nx,ny,nz);
            put_vertex(&i,x0,y01,z1,nx,ny,nz);

            put_vertex(&i,x0,y01,z1,nx,ny,nz);
            put_vertex(&i,x1,y10,z0,nx,ny,nz);
            put_vertex(&i,x1,y11,z1,nx,ny,nz);
        }
}

static void build_cube(void){
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
    for(int a=0;a<6;a++)
        for(int b=0;b<6;b++){
            int v=f[a][b];
            for(int c=0;c<3;c++)cube[k++]=p[v][c];
            for(int c=0;c<3;c++)cube[k++]=n[a][c];
        }
}

static void draw_mesh(Renderer3D*r,const GLfloat*v,int count,
                      const float*model,const float*vp,
                      float cr,float cg,float cb){
    float mvp[16];
    multiply(mvp,vp,model);

    glUniformMatrix4fv(r->mvp,1,GL_FALSE,mvp);
    glUniformMatrix4fv(r->model,1,GL_FALSE,model);
    glUniform3f(r->color,cr,cg,cb);

    glVertexAttribPointer(r->position,3,GL_FLOAT,GL_FALSE,
                          6*sizeof(float),v);
    glVertexAttribPointer(r->normal,3,GL_FLOAT,GL_FALSE,
                          6*sizeof(float),v+3);

    glEnableVertexAttribArray(r->position);
    glEnableVertexAttribArray(r->normal);
    glDrawArrays(GL_TRIANGLES,0,count);
}

static void draw_cube(Renderer3D*r,const float*vp,
                      float x,float y,float z,
                      float sx,float sy,float sz,float yaw,
                      float cr,float cg,float cb){
    float t[16],rot[16],sc[16],a[16],m[16];

    identity(t);
    t[12]=x;t[13]=y;t[14]=z;

    identity(rot);
    rot[0]=cosf(yaw);
    rot[2]=-sinf(yaw);
    rot[8]=sinf(yaw);
    rot[10]=cosf(yaw);

    identity(sc);
    sc[0]=sx;sc[5]=sy;sc[10]=sz;

    multiply(a,rot,sc);
    multiply(m,t,a);

    draw_mesh(r,cube,36,m,vp,cr,cg,cb);
}

static void triangle(GLfloat*v,int*i,
                     const float*a,const float*b,const float*c){
    float ux=b[0]-a[0],uy=b[1]-a[1],uz=b[2]-a[2];
    float vx=c[0]-a[0],vy=c[1]-a[1],vz=c[2]-a[2];

    float nx=uy*vz-uz*vy;
    float ny=uz*vx-ux*vz;
    float nz=ux*vy-uy*vx;
    float l=sqrtf(nx*nx+ny*ny+nz*nz);

    if(l>0.0001f){
        nx/=l;ny/=l;nz/=l;
    }

    const float*p[3]={a,b,c};

    for(int q=0;q<3;q++){
        for(int j=0;j<3;j++)v[(*i)++]=p[q][j];
        v[(*i)++]=nx;
        v[(*i)++]=ny;
        v[(*i)++]=nz;
    }
}

static void draw_pyramid(Renderer3D*r,const float*vp,
                         float x,float y,float z,
                         float sx,float sy,float sz,int inverted){
    GLfloat v[108];
    int i=0;

    const float a[3]={-1,0,-1};
    const float b[3]={1,0,-1};
    const float c[3]={1,0,1};
    const float d[3]={-1,0,1};
    const float tip[3]={0,inverted?-2.2f:1.8f,0};

    triangle(v,&i,a,b,tip);
    triangle(v,&i,b,c,tip);
    triangle(v,&i,c,d,tip);
    triangle(v,&i,d,a,tip);
    triangle(v,&i,a,b,c);
    triangle(v,&i,a,c,d);

    float m[16];
    identity(m);
    m[0]=sx;m[5]=sy;m[10]=sz;
    m[12]=x;m[13]=y;m[14]=z;

    draw_mesh(r,v,18,m,vp,
        inverted?0.25f:0.38f,
        inverted?0.24f:0.37f,
        inverted?0.23f:0.34f);
}

static void draw_tree(Renderer3D*r,const float*vp,
                      float x,float z,float s){
    float y=height_at(x,z);

    draw_cube(r,vp,x,y+.85f*s,z,
        .28f*s,1.7f*s,.28f*s,0,.30f,.18f,.09f);

    draw_cube(r,vp,x,y+2.05f*s,z,
        1.55f*s,1.85f*s,1.55f*s,0,.12f,.30f,.13f);

    draw_cube(r,vp,x,y+2.75f*s,z,
        1.05f*s,1.25f*s,1.05f*s,0,.16f,.37f,.16f);
}

static void draw_floating_island(Renderer3D*r,const float*vp){
    const float iz=39.0f,by=19.0f;

    draw_cube(r,vp,0,by,iz,14,2.2f,11,0,.30f,.39f,.23f);

    draw_pyramid(r,vp,0,by-1,iz,6.7f,7,5.2f,1);
    draw_pyramid(r,vp,-4,by-1.1f,iz+1,2.8f,4.4f,2.5f,1);
    draw_pyramid(r,vp,4.2f,by-1.1f,iz-.8f,2.7f,4.8f,2.4f,1);
    draw_pyramid(r,vp,0,by+1,iz-1,4.5f,6.2f,4,0);

    draw_cube(r,vp,-4,20.95f,37,.24f,1.7f,.24f,0,.30f,.18f,.09f);
    draw_cube(r,vp,-4,22.15f,37,1.25f,1.65f,1.25f,0,.12f,.30f,.13f);

    draw_cube(r,vp,4,20.95f,40,.24f,1.7f,.24f,0,.30f,.18f,.09f);
    draw_cube(r,vp,4,22.15f,40,1.35f,1.75f,1.35f,0,.12f,.30f,.13f);

    draw_cube(r,vp,-4.4f,11.4f,39.2f,.48f,13,.12f,0,.20f,.62f,.78f);
    draw_cube(r,vp,0,10.8f,44.35f,.38f,14,.12f,0,.24f,.68f,.82f);
    draw_cube(r,vp,4.3f,11.7f,38.6f,.42f,12.4f,.12f,0,.18f,.57f,.76f);

    draw_cube(r,vp,-4.4f,4.8f,39.2f,2.5f,.35f,1.5f,0,.68f,.79f,.80f);
    draw_cube(r,vp,0,4.5f,44.2f,2.8f,.30f,1.4f,0,.70f,.82f,.83f);
    draw_cube(r,vp,4.3f,4.9f,38.6f,2.4f,.32f,1.5f,0,.66f,.78f,.81f);
}

static void draw_access_stairs(Renderer3D*r,const float*vp){
    const float start=10,stepz=.84f;
    const float base=height_at(0,start);
    const float top=20.10f;
    const float rise=(top-base)/28;

    for(int i=0;i<28;i++){
        float z=start+(i+.5f)*stepz;
        float y=base+(i+1)*rise;

        draw_cube(r,vp,0,y-.20f,z,
            4.1f,.40f,1.02f,0,.48f,.43f,.35f);

        if((i%2)==0){
            draw_cube(r,vp,-2.05f,y+.38f,z,
                .22f,.76f,.22f,0,.34f,.31f,.27f);
            draw_cube(r,vp,2.05f,y+.38f,z,
                .22f,.76f,.22f,0,.34f,.31f,.27f);
        }
    }

    draw_cube(r,vp,0,20.16f,33.15f,
        4.4f,.28f,1.1f,0,.52f,.47f,.37f);

    draw_cube(r,vp,-2.8f,21.35f,34,
        .35f,2.5f,.35f,0,.42f,.36f,.27f);

    draw_cube(r,vp,2.8f,21.35f,34,
        .35f,2.5f,.35f,0,.42f,.36f,.27f);
}

static void draw_parkour_props(Renderer3D*r,const float*vp){
    float beam_y=height_at(0,4.5f)+.55f;

    draw_cube(r,vp,0,beam_y,4.5f,
        .42f,.22f,8.2f,0,.43f,.25f,.12f);

    draw_cube(r,vp,-.22f,beam_y-.20f,4.5f,
        .10f,.42f,8.2f,0,.30f,.19f,.10f);

    draw_cube(r,vp,.22f,beam_y-.20f,4.5f,
        .10f,.42f,8.2f,0,.30f,.19f,.10f);

    draw_cube(r,vp,0,height_at(0,54)+.55f,54,
        3.8f,1.1f,3.8f,0,.58f,.43f,.16f);

    draw_cube(r,vp,0,height_at(0,54)+1.05f,54,
        2.8f,.65f,2.8f,0,.68f,.52f,.20f);

    draw_cube(r,vp,10,height_at(10,51)+.12f,51,
        6.8f,.24f,6.8f,0,.18f,.48f,.66f);
}

static void draw_locations(Renderer3D *r, const float *vp)
{
    const MueangLocation *waterfall = MueangGame_GetLocation(0);
    const MueangLocation *temple = MueangGame_GetLocation(1);
    const MueangLocation *river = MueangGame_GetLocation(3);
    const MueangLocation *bamboo = MueangGame_GetLocation(4);
    const MueangLocation *flowers = MueangGame_GetLocation(5);
    float y = height_at(waterfall->x,waterfall->z);
    draw_cube(r,vp,waterfall->x,y+3.0f,waterfall->z,5,6,1,0,.30f,.31f,.28f);
    draw_cube(r,vp,waterfall->x,y+2.8f,waterfall->z-0.6f,2.2f,5.6f,.16f,0,.18f,.65f,.84f);
    y = height_at(temple->x,temple->z);
    draw_cube(r,vp,temple->x,y+.25f,temple->z,6,.5f,5,0,.52f,.46f,.36f);
    for (int i=0;i<4;++i) {
        float x = temple->x + (i&1 ? 2.0f : -2.0f);
        float z = temple->z + (i&2 ? 1.5f : -1.5f);
        draw_cube(r,vp,x,y+1.8f,z,.45f,3.2f,.45f,0,.65f,.59f,.45f);
    }
    draw_cube(r,vp,temple->x,y+3.5f,temple->z,5.2f,.45f,4.2f,0,.45f,.40f,.30f);
    y = height_at(river->x,river->z);
    draw_cube(r,vp,river->x,y+.15f,river->z,8,.15f,4,0,.08f,.53f,.43f);
    for (int i=0;i<7;++i) {
        float x=bamboo->x+(float)(i%3-1)*1.5f;
        float z=bamboo->z+(float)(i/3-1)*1.5f;
        y=height_at(x,z);
        draw_cube(r,vp,x,y+2,z,.16f,4,.16f,0,.25f,.48f,.18f);
        draw_cube(r,vp,x,y+4,z,.65f,.50f,.65f,0,.17f,.40f,.14f);
    }
    for (int i=0;i<8;++i) {
        float x=flowers->x+(float)(i%4-1)*1.2f;
        float z=flowers->z+(float)(i/4)*1.5f;
        y=height_at(x,z);
        draw_cube(r,vp,x,y+.35f,z,.14f,.7f,.14f,0,.18f,.50f,.12f);
        draw_cube(r,vp,x,y+.75f,z,.45f,.18f,.45f,0,.85f,.38f,.65f);
    }
}

int Renderer3D_Init(Renderer3D*r,AAssetManager *assets){
    if(!r)return 0;

    memset(r,0,sizeof(*r));

    GLuint vs=compile(GL_VERTEX_SHADER,VS);
    GLuint fs=compile(GL_FRAGMENT_SHADER,FS);

    if(!vs||!fs){
        if(vs)glDeleteShader(vs);
        if(fs)glDeleteShader(fs);
        return 0;
    }

    r->program=glCreateProgram();

    if(!r->program){
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }

    glAttachShader(r->program,vs);
    glAttachShader(r->program,fs);
    glLinkProgram(r->program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok=0;
    glGetProgramiv(r->program,GL_LINK_STATUS,&ok);

    if(!ok){
        char log[512];
        glGetProgramInfoLog(r->program,512,NULL,log);

        __android_log_print(ANDROID_LOG_ERROR,
            "MUEANG_ISARA","Link: %s",log);

        glDeleteProgram(r->program);
        r->program=0;
        return 0;
    }

    r->position=glGetAttribLocation(r->program,"aPosition");
    r->normal=glGetAttribLocation(r->program,"aNormal");

    r->mvp=glGetUniformLocation(r->program,"uMVP");
    r->model=glGetUniformLocation(r->program,"uModel");
    r->color=glGetUniformLocation(r->program,"uColor");
    r->camera=glGetUniformLocation(r->program,"uCamera");

    if(r->position<0||r->normal<0||r->mvp<0||
       r->model<0||r->color<0||r->camera<0){
        glDeleteProgram(r->program);
        memset(r,0,sizeof(*r));
        return 0;
    }

    build_cube();

    if (!GLBLoader_Load(r, assets,
            "models/MUEANG_ISARA_Main_Assassin_02.glb")) {
        __android_log_print(ANDROID_LOG_WARN,
            "MUEANG_ISARA",
            "Main Assassin GLB unavailable; using procedural fallback");
    }

    /* Optional rigged GLB; the existing model remains available as fallback. */
    const char *rig_path = "models/MUEANG_ISARA_Rigged.glb";
    AAsset *rig_asset = assets ? AAssetManager_open(assets, rig_path, AASSET_MODE_BUFFER) : NULL;
    int rig_ready = 0;
    if (rig_asset) {
        AAsset_close(rig_asset);
        rig_ready = RiggedRenderer_Init(assets, rig_path);
        if (!rig_ready)
            __android_log_print(ANDROID_LOG_WARN, "MUEANG_ISARA",
                "Custom rigged GLB unsupported or invalid; trying prototype");
    }
    if (!rig_ready && !RiggedRenderer_Init(assets, "models/MUEANG_ISARA_Rigged_Prototype.glb"))
        __android_log_print(ANDROID_LOG_WARN, "MUEANG_ISARA",
            "Prototype rig unavailable; using existing character");

    return 1;
}

void Renderer3D_Release(Renderer3D *r)
{
    if (!r) return;

    GLBLoader_Release(r);
    RiggedRenderer_Release();

    if (r->program) {
        glDeleteProgram(r->program);
        r->program = 0;
    }

    r->position = -1;
    r->normal = -1;
    r->mvp = -1;
    r->model = -1;
    r->color = -1;
    r->camera = -1;
}

void Renderer3D_Draw(Renderer3D*r,const MueangGame*g,
                     const MueangResourceSystem *resources,int w,int h){
    if(!r||!g||!r->program||w<=0||h<=0)return;

    glViewport(0,0,w,h);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    glClearColor(.55f,.72f,.82f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glUseProgram(r->program);

    float px=g->player.x;
    float pz=g->player.z;

    build_terrain(px,pz);

    float floor_y=MueangGame_GroundHeight(px,pz);
    float player_y=g->player.grounded?
        floor_y:(g->player.surface_y+g->player.jump_height);

    float targetY=player_y+1.2f;

    float dist=g->camera_distance;
    if(dist<3||dist>14)dist=7.5f;

    float yaw=g->camera_yaw;
    float pitch=g->camera_pitch;

    float cx=px-sinf(yaw)*cosf(pitch)*dist;
    float cy=targetY+sinf(pitch)*dist+1;
    float cz=pz-cosf(yaw)*cosf(pitch)*dist;

    float proj[16],view[16],vp[16];

    perspective(proj,PI/3,(float)w/(float)h,.1f,220);
    look_at(view,cx,cy,cz,px,targetY,pz);
    multiply(vp,proj,view);

    glUniform3f(r->camera,cx,cy,cz);

    float id[16];
    identity(id);

    draw_mesh(r,terrain,TERRAIN_VERTICES,id,vp,
        .24f,.40f,.20f);

    draw_pyramid(r,vp,-28,height_at(-28,-25),-25,9,12,8,0);
    draw_pyramid(r,vp,28,height_at(28,-25),-25,10,14,9,0);
    draw_pyramid(r,vp,-28,height_at(-28,25),25,11,13,9,0);
    draw_pyramid(r,vp,28,height_at(28,25),25,10,15,9,0);

    float hy=height_at(5,-5);

    draw_cube(r,vp,5,hy+1.2f,-5,
        3.2f,2.4f,3,0,.62f,.43f,.27f);

    draw_cube(r,vp,5,hy+2.7f,-5,
        3.8f,.35f,3.6f,.35f,.36f,.15f,.10f);

    draw_tree(r,vp,-12,-8,1);
    draw_tree(r,vp,-5,-11,.9f);
    draw_tree(r,vp,2,-13,1.1f);
    draw_tree(r,vp,11,-9,1);
    draw_tree(r,vp,15,1,.9f);
    draw_tree(r,vp,-15,8,1.1f);
    draw_tree(r,vp,-8,14,.9f);
    draw_tree(r,vp,9,13,1);
    draw_tree(r,vp,16,17,1);

    /* Mark the active harvest nodes, using the same positions as gameplay. */
    if (resources) {
        static const float colors[MUEANG_RESOURCE_ITEM_COUNT][3] = {
            {.22f,.75f,.18f}, {.63f,.34f,.12f}, {.65f,.68f,.70f},
            {.85f,.42f,.12f}, {.80f,.70f,.25f}
        };
        for (int i = 0; i < MUEANG_RESOURCE_NODE_COUNT; ++i) {
            const MueangResourceNode *node = &resources->nodes[i];
            if (!node->active || node->item < 0 ||
                node->item >= MUEANG_RESOURCE_ITEM_COUNT) continue;
            const float *color = colors[node->item];
            draw_cube(r,vp,node->x,height_at(node->x,node->z)+4.2f,node->z,
                      .42f,.42f,.42f,PI*.25f,color[0],color[1],color[2]);
        }
    }

    draw_access_stairs(r,vp);
    draw_floating_island(r,vp);
    draw_parkour_props(r,vp);
    draw_locations(r,vp);

    if (g->sight_active) {
        for (int i = 0; i < MUEANG_NPC_COUNT; ++i) {
            const MueangNPC *npc = &g->npcs[i];
            if (!npc->alive || npc->type != 0) continue;
            draw_cube(r,vp,npc->x,MueangGame_GroundHeight(npc->x,npc->z)+2.3f,npc->z,
                      .36f,.36f,.36f,PI*.25f,1.0f,.25f,.12f);
        }
    }

    /* Modular Character System */
    Character3D_Draw(r, g, vp, player_y);

    glDisableVertexAttribArray(r->position);
    glDisableVertexAttribArray(r->normal);
    glDisable(GL_DEPTH_TEST);
}
