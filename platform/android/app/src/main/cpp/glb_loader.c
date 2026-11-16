
#include "glb_loader.h"
#include "renderer3d.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>

#define GLB_PART_COUNT 27
#define GLB_FLOATS_PER_VERTEX 6
#define GLB_INDEX_SIZE 2

typedef struct {
    uint32_t position_offset;
    uint32_t vertex_count;
    uint32_t color_offset;
    uint32_t index_offset;
    uint32_t index_count;
} GLBPartInfo;

typedef struct {
    GLuint vbo;
    GLsizei vertex_count;
    float color[3];
} GLBPart;

static GLBPart g_parts[GLB_PART_COUNT];
static int g_loaded = 0;

/* Offsets for MUEANG_ISARA_Main_Assassin_02.glb */
static const GLBPartInfo g_info[GLB_PART_COUNT] = {
    {144,    8, 240,     0,  36},
    {272,    8, 368,     0,  36},
    {400,    8, 496,     0,  36},
    {528,    8, 368,     0,  36},
    {624,    8, 496,     0,  36},
    {720,    8, 240,     0,  36},

    {4656, 162, 6600,  816, 960},
    {7248, 162, 9192,  816, 960},
    {9840,   8, 9936,    0,  36},

    {9968,   8, 240,     0,  36},
    {10064,  8, 10160,   0,  36},
    {11152, 42, 11656, 10192,240},

    {11824,  8, 240,     0,  36},
    {11920,  8, 10160,   0,  36},
    {12016, 42, 11656, 10192,240},

    {12520,  8, 368,     0,  36},
    {12616,  8, 10160,   0,  36},
    {12712,  8, 9936,    0,  36},

    {12808,  8, 368,     0,  36},
    {12904,  8, 10160,   0,  36},
    {13000,  8, 9936,    0,  36},

    {13096,  8, 13192,   0,  36},
    {13224,  8, 13192,   0,  36},
    {13320,  8, 13192,   0,  36},

    {13416,  8, 368,     0,  36},
    {13512,  8, 496,     0,  36},
    {13608,  8, 10160,   0,  36}
};

static uint32_t read_u32(const unsigned char *p)
{
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

/* GLB model uses UNSIGNED_SHORT indices (16-bit). */
static uint16_t read_u16(const unsigned char *p)
{
    return (uint16_t)p[0]
        | ((uint16_t)p[1] << 8);
}

static void matrix_identity(float *m)
{
    memset(m, 0, sizeof(float) * 16);
    m[0] = 1.0f;
    m[5] = 1.0f;
    m[10] = 1.0f;
    m[15] = 1.0f;
}

static void matrix_multiply(
    float *out,
    const float *a,
    const float *b)
{
    float t[16];

    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            t[c * 4 + r] = 0.0f;

            for (int k = 0; k < 4; k++) {
                t[c * 4 + r] +=
                    a[k * 4 + r] * b[c * 4 + k];
            }
        }
    }

    memcpy(out, t, sizeof(t));
}

static int build_part(
    int part_index,
    const unsigned char *bin,
    size_t bin_size)
{
    const GLBPartInfo *info = &g_info[part_index];

    size_t pos_bytes =
        (size_t)info->vertex_count * 12;

    size_t color_bytes =
        (size_t)info->vertex_count * 4;

    size_t index_bytes =
        (size_t)info->index_count * GLB_INDEX_SIZE;

    if ((size_t)info->position_offset + pos_bytes > bin_size)
        return 0;

    if ((size_t)info->color_offset + color_bytes > bin_size)
        return 0;

    if ((size_t)info->index_offset + index_bytes > bin_size)
        return 0;

    const unsigned char *positions =
        bin + info->position_offset;

    const unsigned char *colors =
        bin + info->color_offset;

    const unsigned char *indices =
        bin + info->index_offset;

    if (info->index_count % 3 != 0)
        return 0;

    size_t float_count =
        (size_t)info->index_count * GLB_FLOATS_PER_VERTEX;

    GLfloat *vertices =
        (GLfloat *)malloc(float_count * sizeof(GLfloat));

    if (!vertices)
        return 0;

    float cr = colors[0] / 255.0f;
    float cg = colors[1] / 255.0f;
    float cb = colors[2] / 255.0f;

    for (uint32_t i = 0; i < info->index_count; i += 3) {

        uint32_t ia = read_u16(indices + i * GLB_INDEX_SIZE);
        uint32_t ib = read_u16(indices + (i + 1) * GLB_INDEX_SIZE);
        uint32_t ic = read_u16(indices + (i + 2) * GLB_INDEX_SIZE);

        if (ia >= info->vertex_count ||
            ib >= info->vertex_count ||
            ic >= info->vertex_count) {
            free(vertices);
            return 0;
        }

        float a[3], b[3], c[3];

        memcpy(a, positions + ia * 12, 12);
        memcpy(b, positions + ib * 12, 12);
        memcpy(c, positions + ic * 12, 12);

        float ux = b[0] - a[0];
        float uy = b[1] - a[1];
        float uz = b[2] - a[2];

        float vx = c[0] - a[0];
        float vy = c[1] - a[1];
        float vz = c[2] - a[2];

        float nx = uy * vz - uz * vy;
        float ny = uz * vx - ux * vz;
        float nz = ux * vy - uy * vx;

        float length =
            sqrtf(nx * nx + ny * ny + nz * nz);

        if (length > 0.00001f) {
            nx /= length;
            ny /= length;
            nz /= length;
        } else {
            nx = 0.0f;
            ny = 1.0f;
            nz = 0.0f;
        }

        const float *points[3] = {a, b, c};

        for (int v = 0; v < 3; v++) {
            int base = (int)(i + v) * 6;

            vertices[base + 0] = points[v][0];
            vertices[base + 1] = points[v][1];
            vertices[base + 2] = points[v][2];

            vertices[base + 3] = nx;
            vertices[base + 4] = ny;
            vertices[base + 5] = nz;
        }
    }

    GLuint vbo = 0;

    glGenBuffers(1, &vbo);

    if (!vbo) {
        free(vertices);
        return 0;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        float_count * sizeof(GLfloat),
        vertices,
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    free(vertices);

    g_parts[part_index].vbo = vbo;
    g_parts[part_index].vertex_count =
        (GLsizei)info->index_count;

    g_parts[part_index].color[0] = cr;
    g_parts[part_index].color[1] = cg;
    g_parts[part_index].color[2] = cb;

    return 1;
}

void GLBLoader_Release(struct Renderer3D *renderer)
{
    (void)renderer;

    for (int i = 0; i < GLB_PART_COUNT; i++) {
        if (g_parts[i].vbo) {
            glDeleteBuffers(1, &g_parts[i].vbo);
            g_parts[i].vbo = 0;
        }

        g_parts[i].vertex_count = 0;
    }

    g_loaded = 0;
}

int GLBLoader_Load(
    struct Renderer3D *renderer,
    AAssetManager *asset_manager,
    const char *asset_path)
{
    (void)renderer;

    if (!asset_manager || !asset_path)
        return 0;

    GLBLoader_Release(renderer);

    AAsset *asset = AAssetManager_open(
        asset_manager,
        asset_path,
        AASSET_MODE_BUFFER
    );

    if (!asset)
        return 0;

    off_t asset_length = AAsset_getLength(asset);

    if (asset_length < 32 ||
        asset_length > 16 * 1024 * 1024) {
        AAsset_close(asset);
        return 0;
    }

    unsigned char *file =
        (unsigned char *)malloc((size_t)asset_length);

    if (!file) {
        AAsset_close(asset);
        return 0;
    }

    int read_count = AAsset_read(
        asset,
        file,
        (size_t)asset_length
    );

    AAsset_close(asset);

    if (read_count != asset_length) {
        free(file);
        return 0;
    }

    if (memcmp(file, "glTF", 4) != 0 ||
        read_u32(file + 4) != 2 ||
        read_u32(file + 8) != (uint32_t)asset_length) {
        free(file);
        return 0;
    }

    uint32_t json_length = read_u32(file + 12);
    uint32_t json_type = read_u32(file + 16);

    if (json_type != 0x4E4F534A ||
        json_length > (uint32_t)asset_length - 28) {
        free(file);
        return 0;
    }

    size_t bin_header = 20u + json_length;

    if (bin_header + 8u > (size_t)asset_length) {
        free(file);
        return 0;
    }

    uint32_t bin_length = read_u32(file + bin_header);
    uint32_t bin_type = read_u32(file + bin_header + 4);

    if (bin_type != 0x004E4942 ||
        bin_header + 8u + bin_length > (size_t)asset_length ||
        bin_length < 13704u) {
        free(file);
        return 0;
    }

    const unsigned char *bin =
        file + bin_header + 8u;

    int success = 1;

    for (int i = 0; i < GLB_PART_COUNT; i++) {
        if (!build_part(i, bin, bin_length)) {
            success = 0;
            break;
        }
    }

    free(file);

    if (!success) {
        GLBLoader_Release(renderer);
        return 0;
    }

    g_loaded = 1;
    return 1;
}

int GLBLoader_Draw(
    struct Renderer3D *renderer,
    const float *view_projection,
    float x,
    float y,
    float z,
    float yaw,
    float bob)
{
    if (!renderer || !view_projection || !g_loaded)
        return 0;

    float translation[16];
    float rotation[16];
    float model[16];
    float mvp[16];

    matrix_identity(translation);
    translation[12] = x;
    translation[13] = y + bob;
    translation[14] = z;

    matrix_identity(rotation);

    float c = cosf(yaw);
    float s = sinf(yaw);

    rotation[0] = c;
    rotation[2] = -s;
    rotation[8] = s;
    rotation[10] = c;

    matrix_multiply(model, translation, rotation);
    matrix_multiply(mvp, view_projection, model);

    glUniformMatrix4fv(
        renderer->mvp, 1, GL_FALSE, mvp
    );

    glUniformMatrix4fv(
        renderer->model, 1, GL_FALSE, model
    );

    for (int i = 0; i < GLB_PART_COUNT; i++) {
        GLBPart *part = &g_parts[i];

        if (!part->vbo || part->vertex_count <= 0)
            continue;

        glUniform3f(
            renderer->color,
            part->color[0],
            part->color[1],
            part->color[2]
        );

        glBindBuffer(GL_ARRAY_BUFFER, part->vbo);

        glVertexAttribPointer(
            renderer->position,
            3,
            GL_FLOAT,
            GL_FALSE,
            GLB_FLOATS_PER_VERTEX * sizeof(float),
            (const void *)0
        );

        glVertexAttribPointer(
            renderer->normal,
            3,
            GL_FLOAT,
            GL_FALSE,
            GLB_FLOATS_PER_VERTEX * sizeof(float),
            (const void *)(3 * sizeof(float))
        );

        glEnableVertexAttribArray(renderer->position);
        glEnableVertexAttribArray(renderer->normal);

        glDrawArrays(
            GL_TRIANGLES,
            0,
            part->vertex_count
        );
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return 1;
}
