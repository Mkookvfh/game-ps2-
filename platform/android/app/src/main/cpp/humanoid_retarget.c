#include "humanoid_retarget.h"
#include <math.h>
#include <string.h>

#define MAT4 16
#define EPSILON 1.0e-7f

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static void mat_mul(float out[16], const float a[16], const float b[16]) {
    float t[16];
    int c, r, k;
    for (c = 0; c < 4; ++c) {
        for (r = 0; r < 4; ++r) {
            t[c * 4 + r] = 0.0f;
            for (k = 0; k < 4; ++k)
                t[c * 4 + r] += a[k * 4 + r] * b[c * 4 + k];
        }
    }
    memcpy(out, t, sizeof(t));
}

/* General 4x4 inverse using pivoted Gauss-Jordan elimination. */
static int mat_inverse(float out[16], const float in[16]) {
    float a[4][8];
    int r, c, k;
    for (r = 0; r < 4; ++r) {
        for (c = 0; c < 4; ++c) a[r][c] = in[c * 4 + r];
        for (c = 0; c < 4; ++c) a[r][c + 4] = (r == c) ? 1.0f : 0.0f;
    }
    for (c = 0; c < 4; ++c) {
        int pivot = c;
        float best = fabsf(a[c][c]);
        for (r = c + 1; r < 4; ++r) {
            float v = fabsf(a[r][c]);
            if (v > best) { best = v; pivot = r; }
        }
        if (best < EPSILON) return 0;
        if (pivot != c) {
            for (k = 0; k < 8; ++k) {
                float tmp = a[c][k]; a[c][k] = a[pivot][k]; a[pivot][k] = tmp;
            }
        }
        {
            float d = a[c][c];
            for (k = 0; k < 8; ++k) a[c][k] /= d;
        }
        for (r = 0; r < 4; ++r) {
            float f;
            if (r == c) continue;
            f = a[r][c];
            for (k = 0; k < 8; ++k) a[r][k] -= f * a[c][k];
        }
    }
    for (r = 0; r < 4; ++r)
        for (c = 0; c < 4; ++c) out[c * 4 + r] = a[r][c + 4];
    return 1;
}

static int is_clavicle_name(const char *name) {
    char n[96];
    int j = 0;
    const char *p = name;
    if (!p) return 0;
    while (*p && j < 95) {
        unsigned char c = (unsigned char)*p++;
        if (c == ':' || c == '|') { j = 0; continue; }
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) n[j++] = (char)c;
    }
    n[j] = '\0';
    return strstr(n, "clavicle") != 0 || strstr(n, "shoulder") != 0;
}

int HumanoidRetarget_Init(MueangHumanoidRetargeter *r,
                          const char *const *names, int count,
                          float source_height, float target_height) {
    int i;
    if (!r || !names || count <= 0 || count > MUEANG_RETARGET_MAX_SOURCE_BONES)
        return 0;
    memset(r, 0, sizeof(*r));
    r->source_count = count;
    r->source_rig = HumanoidStandard_DetectRig(names, count);
    r->source_height = source_height > EPSILON ? source_height : 1.0f;
    r->target_height = target_height > EPSILON ? target_height : 1.0f;
    r->blend = 1.0f;
    for (i = 0; i < MUEANG_HUMANOID_BONE_COUNT; ++i) r->target_to_source[i] = -1;
    for (i = 0; i < count; ++i) {
        int target = HumanoidStandard_MapBoneName(names[i]);
        r->source_to_target[i] = target;
        if (target < 0 || target >= MUEANG_HUMANOID_BONE_COUNT) continue;
        if (r->target_to_source[target] < 0) {
            r->target_to_source[target] = i;
        } else if (is_clavicle_name(names[r->target_to_source[target]]) &&
                   !is_clavicle_name(names[i])) {
            /* Prefer the actual upper-arm joint over the clavicle anchor. */
            r->target_to_source[target] = i;
        }
    }
    r->mapped_count = HumanoidStandard_CountMapped(r->source_to_target, count);
    return r->mapped_count > 0;
}

void HumanoidRetarget_SetBlend(MueangHumanoidRetargeter *r, float blend) {
    if (r) r->blend = clamp01(blend);
}

static void blend_matrix(float out[16], const float a[16], const float b[16], float t) {
    int i;
    float x[3], y[3], z[3], len;
    for (i = 0; i < 16; ++i) out[i] = a[i] + (b[i] - a[i]) * t;
    /* Keep the interpolated rotation basis normalized to limit shear. */
    x[0]=out[0]; x[1]=out[1]; x[2]=out[2];
    len=sqrtf(x[0]*x[0]+x[1]*x[1]+x[2]*x[2]);
    if (len > EPSILON) { out[0]/=len; out[1]/=len; out[2]/=len; }
    y[0]=out[4]; y[1]=out[5]; y[2]=out[6];
    len=sqrtf(y[0]*y[0]+y[1]*y[1]+y[2]*y[2]);
    if (len > EPSILON) { out[4]/=len; out[5]/=len; out[6]/=len; }
    z[0]=out[8]; z[1]=out[9]; z[2]=out[10];
    len=sqrtf(z[0]*z[0]+z[1]*z[1]+z[2]*z[2]);
    if (len > EPSILON) { out[8]/=len; out[9]/=len; out[10]/=len; }
    out[3]=out[7]=out[11]=0.0f;
    out[15]=1.0f;
}

int HumanoidRetarget_ApplyPose(const MueangHumanoidRetargeter *r,
                               const float *source_bind,
                               const float *source_pose,
                               const float *target_bind,
                               const float *fallback_pose,
                               float *out_pose) {
    int target, applied = 0;
    float scale;
    if (!r || !source_bind || !source_pose || !target_bind ||
        !fallback_pose || !out_pose || r->source_count <= 0) return 0;
    scale = r->target_height / r->source_height;
    memcpy(out_pose, fallback_pose,
           sizeof(float) * MUEANG_HUMANOID_BONE_COUNT * MAT4);
    for (target = 0; target < MUEANG_HUMANOID_BONE_COUNT; ++target) {
        int source = r->target_to_source[target];
        float inv_bind[16], delta[16], mapped[16];
        const float *sb, *sp, *tb, *fb;
        if (source < 0 || source >= r->source_count) continue;
        /* Cape joints are game-specific; keep their procedural fallback. */
        if (target >= MUEANG_HUMANOID_CAPE_ROOT) continue;
        sb = source_bind + (size_t)source * MAT4;
        sp = source_pose + (size_t)source * MAT4;
        tb = target_bind + (size_t)target * MAT4;
        fb = fallback_pose + (size_t)target * MAT4;
        if (!mat_inverse(inv_bind, sb)) continue;
        mat_mul(delta, inv_bind, sp);
        /* Scale retargeted root/body translation for differing character height. */
        delta[12] *= scale;
        delta[13] *= scale;
        delta[14] *= scale;
        mat_mul(mapped, tb, delta);
        blend_matrix(out_pose + (size_t)target * MAT4, fb, mapped, r->blend);
        ++applied;
    }
    return applied;
}
