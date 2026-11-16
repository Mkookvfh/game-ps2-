#include "rigged_model.h"
#define CGLTF_IMPLEMENTATION
#include "third_party/cgltf.h"
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_GLB_SIZE (16u * 1024u * 1024u)
#define MAX_NODES 512
#define MAX_JOINTS 128
#define MAX_VERTICES 300000

typedef struct {
    float position[3], normal[3], color[3], weight[4];
    unsigned int joint[4];
} SourceVertex;

typedef struct {
    float translation[3], rotation[4], scale[3];
} BindNode;

struct MueangRiggedModel {
    unsigned char *bytes;
    cgltf_data *data;
    cgltf_skin *skin;
    BindNode *bind;
    SourceVertex *source;
    size_t vertex_count;
};

static int finite_values(const float *v, size_t count)
{
    for (size_t i = 0; i < count; ++i) if (!isfinite(v[i])) return 0;
    return 1;
}

/* Validate spans before cgltf_validate reads indices from buffer views. */
static int safe_buffers(const cgltf_data *data)
{
    if (data->buffers_count != 1 || data->buffers[0].uri ||
        !data->buffers[0].data || data->buffers[0].size > data->bin_size) return 0;
    for (size_t i = 0; i < data->buffer_views_count; ++i) {
        const cgltf_buffer_view *v = &data->buffer_views[i];
        if (v->buffer != &data->buffers[0] || v->has_meshopt_compression ||
            v->offset > v->buffer->size || v->size > v->buffer->size - v->offset) return 0;
    }
    for (size_t i = 0; i < data->accessors_count; ++i) {
        const cgltf_accessor *a = &data->accessors[i];
        size_t component = cgltf_component_size(a->component_type);
        size_t element = cgltf_calc_size(a->type, a->component_type);
        if (!component || !element || !a->count || a->count > MAX_VERTICES ||
            a->is_sparse || !a->buffer_view || a->stride < element ||
            (a->buffer_view->offset + a->offset) % component || a->stride % component ||
            a->offset > a->buffer_view->size || element > a->buffer_view->size - a->offset ||
            a->count - 1 > (a->buffer_view->size - a->offset - element) / a->stride) return 0;
    }
    return 1;
}

static void identity(float *m)
{
    memset(m, 0, 16 * sizeof(*m));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void multiply(float *out, const float *a, const float *b)
{
    float v[16];
    for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) {
        v[c*4+r] = 0;
        for (int k = 0; k < 4; ++k) v[c*4+r] += a[k*4+r] * b[c*4+k];
    }
    memcpy(out, v, sizeof(v));
}

static int valid_animations(const cgltf_data *data)
{
    size_t total_keys = 0;
    if (!data->animations_count || data->animations_count > 64) return 0;
    for (size_t i = 0; i < data->animations_count; ++i) {
        const cgltf_animation *a = &data->animations[i];
        if (!a->channels_count || a->channels_count > MAX_NODES * 3) return 0;
        for (size_t j = 0; j < a->channels_count; ++j) {
            const cgltf_animation_channel *ch = &a->channels[j];
            const cgltf_animation_sampler *s = ch->sampler;
            if (!ch->target_node || ch->target_node->has_matrix || !s || !s->input || !s->output ||
                (s->interpolation != cgltf_interpolation_type_linear && s->interpolation != cgltf_interpolation_type_step) ||
                s->input->type != cgltf_type_scalar || s->input->component_type != cgltf_component_type_r_32f ||
                s->input->count > 10000 || s->input->count > 250000 - total_keys ||
                s->input->count != s->output->count || s->output->component_type != cgltf_component_type_r_32f ||
                (ch->target_path != cgltf_animation_path_type_translation &&
                 ch->target_path != cgltf_animation_path_type_rotation &&
                 ch->target_path != cgltf_animation_path_type_scale) ||
                s->output->type != (ch->target_path == cgltf_animation_path_type_rotation ? cgltf_type_vec4 : cgltf_type_vec3)) return 0;
            total_keys += s->input->count;
            float last = -1.0f;
            for (size_t k = 0; k < s->input->count; ++k) {
                float time, value[4];
                size_t dim = ch->target_path == cgltf_animation_path_type_rotation ? 4 : 3;
                if (!cgltf_accessor_read_float(s->input, k, &time, 1) || !isfinite(time) || time < 0 || time <= last ||
                    !cgltf_accessor_read_float(s->output, k, value, dim) || !finite_values(value, dim)) return 0;
                if (dim == 4 && value[0]*value[0] + value[1]*value[1] + value[2]*value[2] + value[3]*value[3] < 1e-10f) return 0;
                last = time;
            }
        }
    }
    return 1;
}

static int read_source(MueangRiggedModel *model, const cgltf_mesh *mesh)
{
    size_t total = 0;
    for (size_t p = 0; p < mesh->primitives_count; ++p) {
        const cgltf_primitive *prim = &mesh->primitives[p];
        const cgltf_accessor *pos = cgltf_find_accessor(prim, cgltf_attribute_type_position, 0);
        const cgltf_accessor *normal = cgltf_find_accessor(prim, cgltf_attribute_type_normal, 0);
        const cgltf_accessor *joints = cgltf_find_accessor(prim, cgltf_attribute_type_joints, 0);
        const cgltf_accessor *weights = cgltf_find_accessor(prim, cgltf_attribute_type_weights, 0);
        const cgltf_accessor *color = cgltf_find_accessor(prim, cgltf_attribute_type_color, 0);
        if (prim->type != cgltf_primitive_type_triangles || prim->targets_count || prim->has_draco_mesh_compression ||
            !pos || pos->type != cgltf_type_vec3 || pos->component_type != cgltf_component_type_r_32f ||
            !joints || joints->type != cgltf_type_vec4 || joints->normalized ||
            (joints->component_type != cgltf_component_type_r_8u && joints->component_type != cgltf_component_type_r_16u) ||
            !weights || weights->type != cgltf_type_vec4 ||
            (normal && normal->type != cgltf_type_vec3) ||
            (color && color->type != cgltf_type_vec3 && color->type != cgltf_type_vec4) ||
            cgltf_find_accessor(prim, cgltf_attribute_type_joints, 1) ||
            cgltf_find_accessor(prim, cgltf_attribute_type_weights, 1) ||
            (prim->material && (prim->material->alpha_mode != cgltf_alpha_mode_opaque ||
             prim->material->pbr_metallic_roughness.base_color_texture.texture))) return 0;
        size_t count = prim->indices ? prim->indices->count : pos->count;
        if (count % 3 || count > MAX_VERTICES - total) return 0;
        total += count;
    }
    if (!total) return 0;
    model->source = calloc(total, sizeof(*model->source));
    if (!model->source) return 0;
    model->vertex_count = total;
    size_t cursor = 0;
    for (size_t p = 0; p < mesh->primitives_count; ++p) {
        const cgltf_primitive *prim = &mesh->primitives[p];
        const cgltf_accessor *pos = cgltf_find_accessor(prim, cgltf_attribute_type_position, 0);
        const cgltf_accessor *normal = cgltf_find_accessor(prim, cgltf_attribute_type_normal, 0);
        const cgltf_accessor *joints = cgltf_find_accessor(prim, cgltf_attribute_type_joints, 0);
        const cgltf_accessor *weights = cgltf_find_accessor(prim, cgltf_attribute_type_weights, 0);
        const cgltf_accessor *color = cgltf_find_accessor(prim, cgltf_attribute_type_color, 0);
        size_t count = prim->indices ? prim->indices->count : pos->count;
        for (size_t k = 0; k < count; ++k) {
            size_t index = prim->indices ? cgltf_accessor_read_index(prim->indices, k) : k;
            SourceVertex *v = &model->source[cursor++];
            float rgba[4] = {1,1,1,1};
            if (index >= pos->count || !cgltf_accessor_read_float(pos,index,v->position,3) ||
                !cgltf_accessor_read_uint(joints,index,v->joint,4) ||
                !cgltf_accessor_read_float(weights,index,v->weight,4) ||
                (normal && !cgltf_accessor_read_float(normal,index,v->normal,3)) ||
                (color && !cgltf_accessor_read_float(color,index,rgba,4)) ||
                !finite_values(v->position,3) || !finite_values(v->normal,3) ||
                !finite_values(v->weight,4) || !finite_values(rgba,4)) return 0;
            float sum = 0;
            for (int i = 0; i < 4; ++i) {
                if (v->joint[i] >= model->skin->joints_count || v->weight[i] < 0) return 0;
                sum += v->weight[i];
            }
            if (!isfinite(sum) || sum < 1e-6f) return 0;
            for (int i = 0; i < 4; ++i) v->weight[i] /= sum;
            for (int i = 0; i < 3; ++i)
                v->color[i] = rgba[i] * (prim->material ? prim->material->pbr_metallic_roughness.base_color_factor[i] : 1.0f);
            if (!finite_values(v->color, 3)) return 0;
        }
        if (!normal) {
            for (size_t k = cursor-count; k < cursor; k += 3) {
                SourceVertex *a = &model->source[k], *b = a+1, *c = a+2;
                float ux=b->position[0]-a->position[0], uy=b->position[1]-a->position[1], uz=b->position[2]-a->position[2];
                float vx=c->position[0]-a->position[0], vy=c->position[1]-a->position[1], vz=c->position[2]-a->position[2];
                float n[3]={uy*vz-uz*vy,uz*vx-ux*vz,ux*vy-uy*vx};
                float len=sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
                if (len < 1e-8f) { n[0]=0; n[1]=1; n[2]=0; } else for (int q=0;q<3;q++) n[q]/=len;
                memcpy(a->normal,n,sizeof(n)); memcpy(b->normal,n,sizeof(n)); memcpy(c->normal,n,sizeof(n));
            }
        }
    }
    return 1;
}

MueangRiggedModel *RiggedModel_Load(const void *glb, size_t size)
{
    if (!glb || size < 20 || size > MAX_GLB_SIZE) return NULL;
    MueangRiggedModel *model = calloc(1, sizeof(*model));
    if (!model) return NULL;
    model->bytes = malloc(size);
    if (!model->bytes) goto fail;
    memcpy(model->bytes, glb, size);
    cgltf_options options = {0};
    if (cgltf_parse(&options, model->bytes, size, &model->data) != cgltf_result_success ||
        model->data->file_type != cgltf_file_type_glb ||
        model->data->buffers_count != 1 || model->data->buffers[0].uri ||
        model->data->extensions_required_count || model->data->textures_count ||
        cgltf_load_buffers(&options, model->data, NULL) != cgltf_result_success ||
        !safe_buffers(model->data) || cgltf_validate(model->data) != cgltf_result_success ||
        !model->data->nodes_count || model->data->nodes_count > MAX_NODES ||
        !valid_animations(model->data)) goto fail;

    cgltf_node *mesh_node = NULL;
    for (size_t i = 0; i < model->data->nodes_count; ++i) {
        cgltf_node *node = &model->data->nodes[i];
        size_t depth = 0;
        for (cgltf_node *parent = node->parent; parent; parent = parent->parent)
            if (++depth > model->data->nodes_count) goto fail;
        if (!finite_values(node->translation, 3) || !finite_values(node->rotation, 4) ||
            !finite_values(node->scale, 3) || (node->has_matrix && !finite_values(node->matrix, 16))) goto fail;
        if (node->mesh) {
            if (mesh_node || !node->skin) goto fail;
            mesh_node = node;
        }
    }
    if (!mesh_node || !mesh_node->skin || !mesh_node->skin->joints_count ||
        mesh_node->skin->joints_count > MAX_JOINTS) goto fail;
    model->skin = mesh_node->skin;
    if (model->skin->inverse_bind_matrices &&
        (model->skin->inverse_bind_matrices->type != cgltf_type_mat4 ||
         model->skin->inverse_bind_matrices->component_type != cgltf_component_type_r_32f ||
         model->skin->inverse_bind_matrices->count != model->skin->joints_count)) goto fail;
    for (size_t i = 0; i < model->skin->joints_count; ++i) {
        float inverse[16];
        if (!model->skin->joints[i] ||
            (model->skin->inverse_bind_matrices &&
             (!cgltf_accessor_read_float(model->skin->inverse_bind_matrices, i, inverse, 16) ||
              !finite_values(inverse, 16)))) goto fail;
    }
    model->bind = calloc(model->data->nodes_count, sizeof(*model->bind));
    if (!model->bind) goto fail;
    for (size_t i = 0; i < model->data->nodes_count; ++i) {
        memcpy(model->bind[i].translation, model->data->nodes[i].translation, sizeof(model->bind[i].translation));
        memcpy(model->bind[i].rotation, model->data->nodes[i].rotation, sizeof(model->bind[i].rotation));
        memcpy(model->bind[i].scale, model->data->nodes[i].scale, sizeof(model->bind[i].scale));
    }
    if (!read_source(model, mesh_node->mesh)) goto fail;
    return model;
fail:
    RiggedModel_Free(model);
    return NULL;
}

void RiggedModel_Free(MueangRiggedModel *model)
{
    if (!model) return;
    free(model->source);
    free(model->bind);
    if (model->data) cgltf_free(model->data);
    free(model->bytes);
    free(model);
}

size_t RiggedModel_VertexCount(const MueangRiggedModel *model)
{
    return model ? model->vertex_count : 0;
}

int RiggedModel_FindAnimation(const MueangRiggedModel *model, const char *name)
{
    if (!model || !name) return -1;
    for (size_t i = 0; i < model->data->animations_count; ++i) {
        const char *a = model->data->animations[i].name, *b = name;
        if (!a) continue;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { ++a; ++b; }
        if (!*a && !*b) return (int)i;
    }
    return -1;
}

static void normalize_quat(float *q)
{
    float length = sqrtf(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
    if (length < 1e-8f) { q[0]=q[1]=q[2]=0; q[3]=1; return; }
    for (int i=0;i<4;++i) q[i] /= length;
}

static void interpolate(const cgltf_animation_channel *ch, float time)
{
    const cgltf_animation_sampler *s = ch->sampler;
    size_t last = s->input->count - 1, low = 0, high = last;
    float t0, t1, a[4] = {0}, b[4] = {0}, out[4] = {0};
    cgltf_accessor_read_float(s->input,0,&t0,1);
    cgltf_accessor_read_float(s->input,last,&t1,1);
    if (time <= t0) high = low = 0;
    else if (time >= t1) low = high = last;
    else while (high - low > 1) {
        size_t mid = low + (high-low)/2;
        float tm; cgltf_accessor_read_float(s->input,mid,&tm,1);
        if (time < tm) high = mid; else low = mid;
    }
    cgltf_accessor_read_float(s->input,low,&t0,1);
    cgltf_accessor_read_float(s->input,high,&t1,1);
    size_t dim = ch->target_path == cgltf_animation_path_type_rotation ? 4 : 3;
    cgltf_accessor_read_float(s->output,low,a,dim);
    cgltf_accessor_read_float(s->output,high,b,dim);
    float u = low == high || s->interpolation == cgltf_interpolation_type_step ? 0 : (time-t0)/(t1-t0);
    if (dim == 4) {
        normalize_quat(a); normalize_quat(b);
        float dot = 0; for (int i=0;i<4;++i) dot += a[i]*b[i];
        if (dot < 0) { dot = -dot; for (int i=0;i<4;++i) b[i] = -b[i]; }
        if (dot < .9995f) {
            float angle=acosf(fminf(1.0f,dot)), divisor=sinf(angle);
            float wa=sinf((1-u)*angle)/divisor, wb=sinf(u*angle)/divisor;
            for (int i=0;i<4;++i) out[i]=a[i]*wa+b[i]*wb;
        } else for (int i=0;i<4;++i) out[i]=a[i]+u*(b[i]-a[i]);
        normalize_quat(out);
        memcpy(ch->target_node->rotation,out,4*sizeof(float));
    } else {
        for (int i=0;i<3;++i) out[i]=a[i]+u*(b[i]-a[i]);
        float *target = ch->target_path == cgltf_animation_path_type_translation ? ch->target_node->translation : ch->target_node->scale;
        memcpy(target,out,3*sizeof(float));
    }
}

int RiggedModel_Sample(MueangRiggedModel *model, int animation, float seconds,
                       MueangRiggedVertex *vertices, size_t capacity)
{
    if (!model || !vertices || capacity < model->vertex_count || !isfinite(seconds) ||
        animation < -1 || (animation >= 0 && (size_t)animation >= model->data->animations_count)) return 0;
    for (size_t i=0;i<model->data->nodes_count;++i) {
        cgltf_node *node = &model->data->nodes[i];
        memcpy(node->translation,model->bind[i].translation,sizeof(node->translation));
        memcpy(node->rotation,model->bind[i].rotation,sizeof(node->rotation));
        memcpy(node->scale,model->bind[i].scale,sizeof(node->scale));
    }
    if (animation >= 0) {
        cgltf_animation *clip = &model->data->animations[animation];
        float duration=0;
        for (size_t i=0;i<clip->channels_count;++i) {
            const cgltf_accessor *input=clip->channels[i].sampler->input;
            float end; cgltf_accessor_read_float(input,input->count-1,&end,1);
            if (end>duration) duration=end;
        }
        float time = duration > 0 ? fmodf(fmaxf(seconds,0),duration) : 0;
        for (size_t i=0;i<clip->channels_count;++i) interpolate(&clip->channels[i],time);
    }
    float palette[MAX_JOINTS][16];
    for (size_t i=0;i<model->skin->joints_count;++i) {
        float world[16], inverse[16];
        cgltf_node_transform_world(model->skin->joints[i],world);
        if (model->skin->inverse_bind_matrices)
            cgltf_accessor_read_float(model->skin->inverse_bind_matrices,i,inverse,16);
        else identity(inverse);
        multiply(palette[i],world,inverse);
        if (!finite_values(palette[i],16)) return 0;
    }
    for (size_t i=0;i<model->vertex_count;++i) {
        const SourceVertex *src=&model->source[i];
        float m[16]={0};
        for (int j=0;j<4;++j) for (int k=0;k<16;++k) m[k]+=palette[src->joint[j]][k]*src->weight[j];
        if (!finite_values(m,16)) return 0;
        for (int r=0;r<3;++r)
            vertices[i].position[r]=m[r]*src->position[0]+m[4+r]*src->position[1]+m[8+r]*src->position[2]+m[12+r];
        if (!finite_values(vertices[i].position,3)) return 0;
        /* Cofactor matrix is the inverse transpose up to a scalar. */
        float a=m[0],b=m[4],c=m[8],d=m[1],e=m[5],f=m[9],g=m[2],h=m[6],j=m[10];
        float cof[9]={e*j-f*h, f*g-d*j, d*h-e*g, c*h-b*j, a*j-c*g, b*g-a*h, b*f-c*e, c*d-a*f, a*e-b*d};
        float det=a*cof[0]+b*cof[1]+c*cof[2];
        float sign=det < 0 ? -1.0f : 1.0f;
        for (int r=0;r<3;++r) vertices[i].normal[r]=sign*(cof[r*3]*src->normal[0]+cof[r*3+1]*src->normal[1]+cof[r*3+2]*src->normal[2]);
        float *n=vertices[i].normal;
        float length=sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        if (!isfinite(length)) return 0;
        if (length < 1e-8f) { n[0]=0; n[1]=1; n[2]=0; }
        else for (int k=0;k<3;++k) n[k]/=length;
        memcpy(vertices[i].color,src->color,sizeof(src->color));
    }
    return 1;
}
