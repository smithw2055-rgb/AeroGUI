struct VSInput {
    float2 position : ATTR0;
    uint instanceId : SV_InstanceID;
};

#ifndef AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES
#define AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES 64
#endif

cbuffer ImageConstants : register(b0) {
    float4 rects[AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES];
    float4 sourceUvs[AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES];
    float4 tints[AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES];
    float4 transform0;
    float4 transform1;
    float4 clipRect[32];
    float4 clipInverse[32];
    float4 clipTranslation[32];
    uint clipCount;
    float3 clipPadding;
};

Texture2D sourceTexture : register(t0);
SamplerState sourceSampler : register(s0);

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 tint : COLOR0;
    float2 canvasPosition : TEXCOORD1;
};

VSOutput vs_main(VSInput input) {
    const float4 rect = rects[input.instanceId];
    const float4 sourceUv = sourceUvs[input.instanceId];
    const float2 local = rect.xy + input.position * rect.zw;
    const float2 transformed = float2(
        local.x * transform0.x + local.y * transform0.z + transform1.x,
        local.x * transform0.y + local.y * transform0.w + transform1.y);
    const float2 ndc = float2(
        (transformed.x / transform1.z) * 2.0 - 1.0,
        1.0 - (transformed.y / transform1.w) * 2.0);

    VSOutput output;
    output.position = float4(ndc, 0.0, 1.0);
    output.uv = sourceUv.xy + input.position * sourceUv.zw;
    output.tint = tints[input.instanceId];
    output.canvasPosition = transformed;
    return output;
}

float4 ps_main(VSOutput input) : SV_Target {
    [loop]
    for (uint index = 0; index < clipCount; ++index) {
        const float2 relative =
            input.canvasPosition - clipTranslation[index].xy;
        const float2 local = float2(
            relative.x * clipInverse[index].x +
                relative.y * clipInverse[index].z,
            relative.x * clipInverse[index].y +
                relative.y * clipInverse[index].w);
        const float2 minimum = clipRect[index].xy;
        const float2 maximum = minimum + clipRect[index].zw;
        if (local.x < minimum.x || local.y < minimum.y ||
            local.x > maximum.x || local.y > maximum.y) {
            discard;
        }
    }
    return sourceTexture.Sample(sourceSampler, input.uv) * input.tint;
}
