
#include "humanoid_animation_runtime.h"

#include <math.h>
#include <string.h>

static float runtime_clamp(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

void HumanoidRuntime_Init(
    MueangHumanoidAnimationRuntime *runtime)
{
    if (!runtime) return;

    memset(runtime, 0, sizeof(*runtime));

    HumanoidClip_PoseIdentity(
        &runtime->sampled_pose
    );

    HumanoidClip_PoseIdentity(
        &runtime->output_pose
    );

    runtime->initialized = 1;
    runtime->blend = 1.0f;
}

void HumanoidRuntime_Release(
    MueangHumanoidAnimationRuntime *runtime)
{
    if (!runtime || !runtime->initialized) return;

    GLBAnimImporter_Release(
        &runtime->imported
    );

    memset(runtime, 0, sizeof(*runtime));
}

int HumanoidRuntime_LoadGLB(
    MueangHumanoidAnimationRuntime *runtime,
    AAssetManager *assets,
    const char *asset_path,
    int animation_index,
    int loop)
{
    if (!runtime ||
        !runtime->initialized ||
        !assets ||
        !asset_path ||
        animation_index < 0)
    {
        return 0;
    }

    GLBAnimImporter_Release(
        &runtime->imported
    );

    runtime->loaded = 0;
    runtime->playing = 0;
    runtime->time_seconds = 0.0f;
    runtime->last_sample_count = 0;

    if (!GLBAnimImporter_Load(
            assets,
            asset_path,
            animation_index,
            loop,
            &runtime->imported))
    {
        HumanoidClip_PoseIdentity(
            &runtime->sampled_pose
        );

        HumanoidClip_PoseIdentity(
            &runtime->output_pose
        );

        return 0;
    }

    runtime->loaded = 1;

    HumanoidClip_PoseIdentity(
        &runtime->sampled_pose
    );

    HumanoidClip_PoseIdentity(
        &runtime->output_pose
    );

    return 1;
}

void HumanoidRuntime_Play(
    MueangHumanoidAnimationRuntime *runtime)
{
    if (!runtime ||
        !runtime->initialized ||
        !runtime->loaded)
    {
        return;
    }

    runtime->playing = 1;
}

void HumanoidRuntime_Pause(
    MueangHumanoidAnimationRuntime *runtime)
{
    if (!runtime) return;

    runtime->playing = 0;
}

void HumanoidRuntime_Seek(
    MueangHumanoidAnimationRuntime *runtime,
    float time_seconds)
{
    if (!runtime ||
        !runtime->loaded ||
        !isfinite(time_seconds))
    {
        return;
    }

    runtime->time_seconds =
        time_seconds < 0.0f ? 0.0f : time_seconds;
}

void HumanoidRuntime_SetBlend(
    MueangHumanoidAnimationRuntime *runtime,
    float blend)
{
    if (!runtime) return;

    runtime->blend = runtime_clamp(blend);
}

int HumanoidRuntime_Update(
    MueangHumanoidAnimationRuntime *runtime,
    float delta_seconds,
    const MueangHumanoidPose *fallback_pose,
    MueangHumanoidPose *out_pose)
{
    MueangHumanoidPose identity;

    const MueangHumanoidPose *fallback =
        fallback_pose;

    int sampled;

    if (!runtime ||
        !runtime->initialized ||
        !out_pose)
    {
        return 0;
    }

    if (!fallback)
    {
        HumanoidClip_PoseIdentity(&identity);
        fallback = &identity;
    }

    /* No imported animation: preserve fallback pose. */
    if (!runtime->loaded)
    {
        *out_pose = *fallback;
        runtime->output_pose = *out_pose;
        runtime->last_sample_count = 0;

        return 0;
    }

    /* Advance animation clock. */
    if (runtime->playing &&
        isfinite(delta_seconds) &&
        delta_seconds > 0.0f)
    {
        runtime->time_seconds += delta_seconds;
    }

    /* Sample imported animation. */
    sampled = HumanoidClip_Sample(
        &runtime->imported.clip,
        runtime->time_seconds,
        &runtime->sampled_pose
    );

    runtime->last_sample_count = sampled;

    if (sampled <= 0)
    {
        *out_pose = *fallback;
        runtime->output_pose = *out_pose;

        return 0;
    }

    /* Full animation contribution. */
    if (runtime->blend >= 0.9999f)
    {
        *out_pose = runtime->sampled_pose;
    }

    /* Full fallback contribution. */
    else if (runtime->blend <= 0.0001f)
    {
        *out_pose = *fallback;
    }

    /* Blend between fallback and imported clip. */
    else
    {
        HumanoidClip_BlendPose(
            fallback,
            &runtime->sampled_pose,
            runtime->blend,
            out_pose
        );
    }

    runtime->output_pose = *out_pose;

    return sampled;
}
