#include "glb_anim_importer.h"
#include "humanoid_standard.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>

#define JSON_TOKENS 30000
#define JSON_DEPTH 64
#define GLB_LIMIT (16u * 1024u * 1024u)

typedef struct { int start, end, parent, size; unsigned char type; } JTok;
typedef struct { const char *s; JTok *t; int n, cap, pos; } JParser;
typedef struct { size_t offset, length, stride; int component, count, type_dim; } Accessor;

enum { JT_OBJECT=1, JT_ARRAY=2, JT_STRING=3, JT_PRIMITIVE=4 };

static uint32_t rd32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static int add_token(JParser *p, int start, int end, int parent, int type) {
    int id;
    if (p->n >= p->cap) return -1;
    id=p->n++; p->t[id].start=start; p->t[id].end=end;
    p->t[id].parent=parent; p->t[id].size=0; p->t[id].type=(unsigned char)type;
    if (parent>=0) p->t[parent].size++;
    return id;
}
static void ws(JParser *p) { while (p->s[p->pos]==' ' || p->s[p->pos]=='\n' || p->s[p->pos]=='\r' || p->s[p->pos]=='\t') p->pos++; }
static int parse_value(JParser *p, int parent, int depth) {
    int id, start; char c;
    if (depth>JSON_DEPTH) return -1;
    ws(p); c=p->s[p->pos]; start=p->pos;
    if (c=='{' || c=='[') {
        int close=(c=='{')?'}':']';
        id=add_token(p,start,start+1,parent,c=='{'?JT_OBJECT:JT_ARRAY); if(id<0)return -1;
        p->pos++;
        for (;;) {
            ws(p); c=p->s[p->pos];
            if (c==close) { p->pos++; p->t[id].end=p->pos; return id; }
            if (!c) return -1;
            if (c==',') { p->pos++; continue; }
            if (c==':') { p->pos++; continue; }
            if (parse_value(p,id,depth+1)<0) return -1;
        }
    }
    if (c=='"') {
        p->pos++;
        while (p->s[p->pos]) {
            if (p->s[p->pos]=='\\') { p->pos+=2; continue; }
            if (p->s[p->pos++]=='"') break;
        }
        if (p->s[p->pos-1]!='"') return -1;
        return add_token(p,start,p->pos,parent,JT_STRING);
    }
    while ((c=p->s[p->pos]) && c!=',' && c!=']' && c!='}' && c!=' ' && c!='\n' && c!='\r' && c!='\t') p->pos++;
    if (p->pos==start) return -1;
    return add_token(p,start,p->pos,parent,JT_PRIMITIVE);
}
static int parse_json(const char *s, JTok **out, int *count) {
    JParser p; int root;
    memset(&p,0,sizeof(p)); p.s=s; p.cap=JSON_TOKENS;
    p.t=(JTok*)calloc((size_t)p.cap,sizeof(JTok)); if(!p.t)return 0;
    root=parse_value(&p,-1,0); ws(&p);
    if(root<0 || p.s[p.pos]!=0 || p.t[root].type!=JT_OBJECT){free(p.t);return 0;}
    *out=p.t; *count=p.n; return 1;
}
static int skip_token(const JTok *t,int n,int i) {
    int end;
    if(i<0||i>=n)return n;
    end=t[i].end; i++;
    while(i<n && t[i].start<end)i++;
    return i;
}
static int str_eq(const char *s,const JTok *t,const char *v) {
    size_t n=strlen(v); int len=t->end-t->start-2;
    return t->type==JT_STRING && len==(int)n && memcmp(s+t->start+1,v,n)==0;
}
static int obj_get(const char *s,const JTok *t,int n,int obj,const char *key) {
    int i;
    if(obj<0||obj>=n||t[obj].type!=JT_OBJECT)return -1;
    i=obj+1;
    while(i<n && t[i].start<t[obj].end) {
        int val=i+1;
        if(val>=n)return -1;
        if(str_eq(s,&t[i],key))return val;
        i=skip_token(t,n,val);
    }
    return -1;
}
static int arr_get(const JTok *t,int n,int arr,int wanted) {
    int i=arr+1,k=0;
    if(arr<0||arr>=n||t[arr].type!=JT_ARRAY||wanted<0)return -1;
    while(i<n&&t[i].start<t[arr].end) { if(k++==wanted)return i; i=skip_token(t,n,i); }
    return -1;
}
static int token_int(const char *s,const JTok *t,int i,int *out) {
    char buf[40],*end; long v; int len;
    if(i<0||t[i].type!=JT_PRIMITIVE)return 0;
    len=t[i].end-t[i].start;if(len<=0||len>=39)return 0;
    memcpy(buf,s+t[i].start,(size_t)len);buf[len]=0;v=strtol(buf,&end,10);
    if(*end||v<0||v>2147483647L) return 0;
    *out=(int)v;
    return 1;
}
static int get_int(const char*s,const JTok*t,int n,int obj,const char*k,int*out) { return token_int(s,t,obj_get(s,t,n,obj,k),out); }
static int accessor_dim(const char *s,const JTok*t,int n,int a) {
    int x=obj_get(s,t,n,a,"type");
    if(x<0)return 0;
    if(str_eq(s,&t[x],"SCALAR"))return 1;
    if(str_eq(s,&t[x],"VEC3"))return 3;
    if(str_eq(s,&t[x],"VEC4"))return 4;
    return 0;
}
static int read_accessor(const char*s,const JTok*t,int n,int accessors,int views,int index,Accessor*out) {
    int a=arr_get(t,n,accessors,index),vi,view,off=0,voff=0,len=0,stride=0,comp=0,count=0,dim;
    if(a<0||!get_int(s,t,n,a,"bufferView",&vi)||!get_int(s,t,n,a,"componentType",&comp)||!get_int(s,t,n,a,"count",&count))return 0;
    view=arr_get(t,n,views,vi);if(view<0)return 0;
    (void)get_int(s,t,n,a,"byteOffset",&off);
    (void)get_int(s,t,n,view,"byteOffset",&voff);
    if(!get_int(s,t,n,view,"byteLength",&len))return 0;
    (void)get_int(s,t,n,view,"byteStride",&stride);
    dim=accessor_dim(s,t,n,a);
    if(comp!=5126||count<=0||dim==0||off<0||voff<0||len<0)return 0;
    if(stride==0)stride=dim*4;
    if(stride<dim*4)return 0;
    if((size_t)off+(size_t)(count-1)*(size_t)stride+(size_t)dim*4>(size_t)len)return 0;
    out->offset=(size_t)voff+(size_t)off;out->length=(size_t)voff+(size_t)len;out->stride=(size_t)stride;
    out->component=comp;out->count=count;out->type_dim=dim;return 1;
}
static int copy_accessor(const unsigned char*bin,size_t binlen,const Accessor*a,float*out) {
    if(a->offset>binlen||a->length>binlen||a->offset+(size_t)(a->count-1)*a->stride+(size_t)a->type_dim*4>a->length)return 0;
    for(int i=0;i<a->count;i++)for(int k=0;k<a->type_dim;k++) {
        float f;
        memcpy(&f,bin+a->offset+(size_t)i*a->stride+(size_t)k*4,4);
        if(!isfinite(f))return 0;
        out[(size_t)i*(size_t)a->type_dim+(size_t)k]=f;
    }
    return 1;
}
static int node_name_map(const char*s,const JTok*t,int n,int nodes,int **out_ids,int **out_indices,int *out_count) {
    int total=t[nodes].size, *ids, *indices;
    ids=(int*)malloc((size_t)total*sizeof(int));indices=(int*)malloc((size_t)total*sizeof(int));
    if(!ids||!indices){free(ids);free(indices);return 0;}
    for(int i=0;i<total;i++) {
        int node=arr_get(t,n,nodes,i),name=obj_get(s,t,n,node,"name");
        char buf[128];int len=0;
        if(name>=0&&t[name].type==JT_STRING) {
            len=t[name].end-t[name].start-2;if(len>127)len=127;
            if(len>0) memcpy(buf,s+t[name].start+1,(size_t)len);
            buf[len]=0;
            ids[i]=HumanoidStandard_MapBoneName(buf);
        } else ids[i]=MUEANG_HUMANOID_UNKNOWN;
        indices[i]=i;
    }
    *out_ids=ids;*out_indices=indices;*out_count=total;return 1;
}
void GLBAnimImporter_Release(MueangGLBAnimation*a) {
    if(!a)return;
    for(int i=0;i<MUEANG_GLB_ANIM_MAX_TRACKS;i++){free(a->owned_times[i]);free(a->owned_values[i]);}
    memset(a,0,sizeof(*a));
}
int GLBAnimImporter_Load(AAssetManager*assets,const char*path,int anim_index,int loop,MueangGLBAnimation*out) {
    AAsset*asset=NULL;unsigned char*file=NULL;JTok*toks=NULL;int nt=0;char*json=NULL;
    size_t flen=0,bin_off=0,bin_len=0;const unsigned char*bin=NULL;
    int root,views,accessors,nodes,animations,animation,samplers,channels,*node_ids=NULL,*node_indices=NULL,node_count=0;
    int imported=0,ignored=0;float duration=0.0f;int ok=0;
    if(!assets||!path||!out||anim_index<0)return 0;
    GLBAnimImporter_Release(out);
    asset=AAssetManager_open(assets,path,AASSET_MODE_BUFFER);if(!asset)goto done;
    {off_t l=AAsset_getLength(asset);if(l<20||l>(off_t)GLB_LIMIT)goto done;flen=(size_t)l;}
    file=(unsigned char*)malloc(flen);if(!file)goto done;
    if(AAsset_read(asset,file,flen)!=(int)flen)goto done;
    if(memcmp(file,"glTF",4)||rd32(file+4)!=2||rd32(file+8)!=(uint32_t)flen)goto done;
    {uint32_t jl=rd32(file+12);if(rd32(file+16)!=0x4E4F534A||jl>flen-20)goto done;
     json=(char*)malloc((size_t)jl+1);if(!json)goto done;memcpy(json,file+20,jl);json[jl]=0;
     {size_t p=20u+jl;if(p+8u>flen||rd32(file+p+4)!=0x004E4942)goto done;bin_len=rd32(file+p);bin_off=p+8u;if(bin_len>flen-bin_off)goto done;bin=file+bin_off;}}
    if(!parse_json(json,&toks,&nt)) goto done;
    root=0;
    views=obj_get(json,toks,nt,root,"bufferViews");accessors=obj_get(json,toks,nt,root,"accessors");
    nodes=obj_get(json,toks,nt,root,"nodes");animations=obj_get(json,toks,nt,root,"animations");
    if(views<0||accessors<0||nodes<0||animations<0||toks[nodes].type!=JT_ARRAY||toks[animations].type!=JT_ARRAY)goto done;
    animation=arr_get(toks,nt,animations,anim_index);if(animation<0)goto done;
    samplers=obj_get(json,toks,nt,animation,"samplers");channels=obj_get(json,toks,nt,animation,"channels");
    if(samplers<0||channels<0||toks[samplers].type!=JT_ARRAY||toks[channels].type!=JT_ARRAY)goto done;
    if(!node_name_map(json,toks,nt,nodes,&node_ids,&node_indices,&node_count))goto done;
    for(int ci=0;ci<toks[channels].size && imported<MUEANG_GLB_ANIM_MAX_TRACKS;ci++) {
        int ch=arr_get(toks,nt,channels,ci),sampler_id=0,target,path_tok,node_id,si,input,output,interp;
        int source_bone;Accessor ia,oa;float *times=NULL,*values=NULL;MueangClipPath clip_path;MueangClipInterpolation clip_interp;
        if(ch<0||!get_int(json,toks,nt,ch,"sampler",&sampler_id)){ignored++;continue;}
        target=obj_get(json,toks,nt,ch,"target");if(target<0||!get_int(json,toks,nt,target,"node",&node_id)){ignored++;continue;}
        if(node_id<0||node_id>=node_count){ignored++;continue;}source_bone=node_ids[node_id];
        if(source_bone<0||source_bone>=MUEANG_HUMANOID_BONE_COUNT){ignored++;continue;}
        path_tok=obj_get(json,toks,nt,target,"path");
        if(path_tok<0){ignored++;continue;}
        if(str_eq(json,&toks[path_tok],"translation"))clip_path=MUEANG_CLIP_TRANSLATION;
        else if(str_eq(json,&toks[path_tok],"rotation"))clip_path=MUEANG_CLIP_ROTATION;
        else if(str_eq(json,&toks[path_tok],"scale"))clip_path=MUEANG_CLIP_SCALE;
        else {ignored++;continue;}
        si=arr_get(toks,nt,samplers,sampler_id);if(si<0){ignored++;continue;}
        if(!get_int(json,toks,nt,si,"input",&input)||!get_int(json,toks,nt,si,"output",&output)){ignored++;continue;}
        interp=obj_get(json,toks,nt,si,"interpolation");
        if(interp>=0&&str_eq(json,&toks[interp],"STEP"))clip_interp=MUEANG_CLIP_STEP;
        else if(interp<0||str_eq(json,&toks[interp],"LINEAR"))clip_interp=MUEANG_CLIP_LINEAR;
        else {ignored++;continue;}
        if(!read_accessor(json,toks,nt,accessors,views,input,&ia)||!read_accessor(json,toks,nt,accessors,views,output,&oa)||ia.type_dim!=1||oa.type_dim!=(clip_path==MUEANG_CLIP_ROTATION?4:3)||ia.count!=oa.count){ignored++;continue;}
        if(ia.count>10000){ignored++;continue;}
        times=(float*)malloc((size_t)ia.count*sizeof(float));values=(float*)malloc((size_t)oa.count*(size_t)oa.type_dim*sizeof(float));
        if(!times||!values||!copy_accessor(bin,bin_len,&ia,times)||!copy_accessor(bin,bin_len,&oa,values)){free(times);free(values);ignored++;continue;}
        int valid=1;for(int k=0;k<ia.count;k++){if(times[k]<0.0f||(k>0&&times[k]<=times[k-1])){valid=0;break;}}
        if(!valid){free(times);free(values);ignored++;continue;}
        out->owned_times[imported]=times;out->owned_values[imported]=values;
        out->tracks[imported].target_bone=source_bone;out->tracks[imported].path=clip_path;
        out->tracks[imported].interpolation=clip_interp;out->tracks[imported].key_count=ia.count;
        out->tracks[imported].times=times;out->tracks[imported].values=values;
        if(times[ia.count-1]>duration) duration=times[ia.count-1];
        imported++;
    }
    if(imported<=0)goto done;
    out->clip.tracks=out->tracks;out->clip.track_count=imported;out->clip.duration=duration;out->clip.loop=loop?1:0;
    out->imported_channels=imported;out->ignored_channels=ignored;out->detected_rig=MUEANG_RIG_UNKNOWN;ok=1;
done:
    if(!ok)GLBAnimImporter_Release(out);
    free(node_ids);free(node_indices);free(toks);free(json);free(file);if(asset)AAsset_close(asset);return ok;
}
