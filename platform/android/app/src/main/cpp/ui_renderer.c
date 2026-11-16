
#include "ui_renderer.h"
#include <math.h>
#include <string.h>

#define MAX_VERTICES 12000
#define PI2 6.2831853f

typedef struct {
    float x,y,r,g,b,a;
} Vertex;

static Vertex vertices[MAX_VERTICES];
static int vertex_count;

static const char *VS =
    "attribute vec2 aPosition;"
    "attribute vec4 aColor;"
    "varying vec4 vColor;"
    "void main(){"
    "gl_Position=vec4(aPosition,0.0,1.0);"
    "vColor=aColor;"
    "}";

static const char *FS =
    "precision mediump float;"
    "varying vec4 vColor;"
    "void main(){"
    "gl_FragColor=vColor;"
    "}";

static void vertex(
    float x,float y,
    float r,float g,float b,float a)
{
    if(vertex_count>=MAX_VERTICES)return;

    Vertex *v=&vertices[vertex_count++];

    v->x=x; v->y=y;
    v->r=r; v->g=g;
    v->b=b; v->a=a;
}

static void triangle(
    float x1,float y1,
    float x2,float y2,
    float x3,float y3,
    float r,float g,float b,float a)
{
    vertex(x1,y1,r,g,b,a);
    vertex(x2,y2,r,g,b,a);
    vertex(x3,y3,r,g,b,a);
}

static void rect(
    float x1,float y1,
    float x2,float y2,
    float r,float g,float b,float a)
{
    triangle(
        x1,y1,x2,y1,x2,y2,
        r,g,b,a);

    triangle(
        x1,y1,x2,y2,x1,y2,
        r,g,b,a);
}

static void disc(
    float x,float y,float radius,
    float r,float g,float b,float a)
{
    for(int i=0;i<20;i++){
        float a0=i*PI2/20.0f;
        float a1=(i+1)*PI2/20.0f;

        triangle(
            x,y,
            x+cosf(a0)*radius,
            y+sinf(a0)*radius,
            x+cosf(a1)*radius,
            y+sinf(a1)*radius,
            r,g,b,a);
    }
}

static GLuint shader(GLenum type,const char *src)
{
    GLuint s=glCreateShader(type);

    if(!s)return 0;

    glShaderSource(s,1,&src,NULL);
    glCompileShader(s);

    GLint ok=0;
    glGetShaderiv(s,GL_COMPILE_STATUS,&ok);

    if(!ok){
        glDeleteShader(s);
        return 0;
    }

    return s;
}

int UIRenderer_Init(UIRenderer *ui)
{
    memset(ui,0,sizeof(*ui));

    GLuint vs=shader(GL_VERTEX_SHADER,VS);
    GLuint fs=shader(GL_FRAGMENT_SHADER,FS);

    if(!vs||!fs){
        if(vs)glDeleteShader(vs);
        if(fs)glDeleteShader(fs);
        return 0;
    }

    ui->program=glCreateProgram();

    glAttachShader(ui->program,vs);
    glAttachShader(ui->program,fs);

    glBindAttribLocation(
        ui->program,0,"aPosition");

    glBindAttribLocation(
        ui->program,1,"aColor");

    glLinkProgram(ui->program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok=0;

    glGetProgramiv(
        ui->program,
        GL_LINK_STATUS,
        &ok);

    if(!ok){
        glDeleteProgram(ui->program);
        memset(ui,0,sizeof(*ui));
        return 0;
    }

    ui->position=glGetAttribLocation(
        ui->program,"aPosition");

    ui->color=glGetAttribLocation(
        ui->program,"aColor");

    ui->ready=1;

    return 1;
}

static void tree(float x,float y,float s)
{
    rect(
        x-s*.035f,y-s*.48f,
        x+s*.035f,y+s*.10f,
        .20f,.12f,.06f,1);

    triangle(
        x,y+s*.66f,
        x-s*.40f,y-s*.02f,
        x+s*.40f,y-s*.02f,
        .025f,.13f,.06f,1);

    triangle(
        x,y+s*.42f,
        x-s*.48f,y-s*.22f,
        x+s*.48f,y-s*.22f,
        .035f,.19f,.08f,1);

    disc(
        x,y+s*.14f,s*.22f,
        .035f,.20f,.085f,1);
}

static void waterfall(float t)
{
    /* Tropical sky */
    rect(
        -1,-1,1,1,
        .035f,.10f,.13f,1);

    rect(
        -1,.10f,1,1,
        .24f,.47f,.48f,1);

    /* Sunlight */
    disc(
        .08f,.72f,.48f,
        1.0f,.74f,.40f,.055f);

    disc(
        .08f,.72f,.28f,
        1.0f,.85f,.58f,.10f);

    disc(
        .08f,.72f,.095f,
        1.0f,.91f,.68f,.95f);

    /* Sun rays */
    for(int i=0;i<8;i++){
        float x=-.62f+i*.18f+
            sinf(t*.22f+i)*.025f;

        rect(
            x,.98f,
            x+.035f,-.08f,
            .98f,.82f,.52f,.035f);
    }

    /* Cliffs */
    triangle(
        -1,.02f,-.62f,.72f,-.20f,.03f,
        .12f,.23f,.19f,1);

    triangle(
        .34f,.02f,.73f,.78f,1,.06f,
        .10f,.21f,.17f,1);

    /* Waterfall */
    rect(
        -.17f,.30f,.17f,-.62f,
        .44f,.73f,.77f,.88f);

    rect(
        -.10f,.29f,-.045f,-.63f,
        .78f,.94f,.94f,.72f);

    rect(
        .045f,.29f,.12f,-.61f,
        .68f,.88f,.93f,.65f);

    /* Animated water streams */
    for(int i=0;i<7;i++){
        float x=-.15f+i*.047f+
            sinf(t*1.5f+i)*.008f;

        float y=.18f-
            fmodf(t*.25f+i*.13f,.78f);

        rect(
            x,y,x+.010f,y-.20f,
            .78f,.94f,.96f,.48f);
    }

    /* Jungle */
    tree(-.72f,.24f,.60f);
    tree(-.92f,.04f,.82f);
    tree(-.48f,.08f,.43f);

    tree(.72f,.25f,.62f);
    tree(.93f,.02f,.85f);
    tree(.49f,.06f,.43f);

    /* Pool */
    rect(
        -1,-1,1,-.57f,
        .015f,.12f,.13f,1);

    /* Water reflections */
    for(int i=0;i<9;i++){
        float x=-.72f+i*.18f;

        rect(
            x,-.78f+sinf(t+i)*.01f,
            x+.10f,-.774f+sinf(t+i)*.01f,
            .60f,.88f,.84f,.20f);
    }

    /* Mist */
    for(int i=0;i<8;i++){
        float x=fmodf(
            i*.31f+t*.035f+2.0f,
            2.0f)-1.0f;

        disc(
            x,-.36f+sinf(t*.35f+i)*.03f,
            .12f,
            .76f,.90f,.84f,.055f);
    }

    /* Water droplets */
    for(int i=0;i<20;i++){
        float x=-.30f+
            fmodf(i*.137f+t*.045f,.60f);

        float y=-.54f+
            fmodf(i*.071f+t*.19f,.62f);

        disc(
            x,y,.007f,
            .88f,.98f,1.0f,.48f);
    }
}

void UIRenderer_Draw(
    UIRenderer *ui,
    int width,
    int height,
    float time_seconds)
{
    if(!ui||!ui->ready||
       width<=0||height<=0)return;

    vertex_count=0;

    waterfall(time_seconds);

    glViewport(0,0,width,height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glUseProgram(ui->program);

    glBindBuffer(GL_ARRAY_BUFFER,0);

    glEnable(GL_BLEND);

    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA);

    glVertexAttribPointer(
        ui->position,2,GL_FLOAT,
        GL_FALSE,sizeof(Vertex),
        &vertices[0].x);

    glVertexAttribPointer(
        ui->color,4,GL_FLOAT,
        GL_FALSE,sizeof(Vertex),
        &vertices[0].r);

    glEnableVertexAttribArray(ui->position);
    glEnableVertexAttribArray(ui->color);

    glDrawArrays(
        GL_TRIANGLES,
        0,
        vertex_count);

    glDisableVertexAttribArray(ui->position);
    glDisableVertexAttribArray(ui->color);

    glDisable(GL_BLEND);
    glBindBuffer(GL_ARRAY_BUFFER,0);
}

void UIRenderer_Release(UIRenderer *ui)
{
    if(!ui)return;

    if(ui->program)
        glDeleteProgram(ui->program);

    memset(ui,0,sizeof(*ui));
}
