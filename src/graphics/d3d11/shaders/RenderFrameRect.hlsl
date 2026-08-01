struct VSInput {
    float2 position : ATTR0;
    uint instanceId : SV_InstanceID;
};

#ifndef AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES
#define AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES 64
#endif

cbuffer RectConstants : register(b0) {
    float4 rects[AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES];
    float4 colors[AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES];
    float4 cornerRadii[AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES];
    float4 transform0;
    float4 transform1;
    float4 clipRect[32];
    float4 clipInverse[32];
    float4 clipTranslation[32];
    uint clipCount;
    uint instanceMode;
    float strokeThickness;
    float clipPadding;
};

struct VSOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 localPosition : TEXCOORD0;
    float2 rectangleSize : TEXCOORD1;
    float cornerRadius : TEXCOORD2;
};

VSOutput vs_main(VSInput input) {
    const uint rectIndex = instanceMode == 2U ? input.instanceId : 0U;
    float4 activeRect = rects[rectIndex];
    if (instanceMode == 1U) {
        if (input.instanceId == 0U) {
            activeRect.w = strokeThickness;
        } else if (input.instanceId == 1U) {
            activeRect.y += activeRect.w - strokeThickness;
            activeRect.w = strokeThickness;
        } else if (input.instanceId == 2U) {
            activeRect.y += strokeThickness;
            activeRect.z = strokeThickness;
            activeRect.w -= strokeThickness * 2.0;
        } else {
            activeRect.x += activeRect.z - strokeThickness;
            activeRect.y += strokeThickness;
            activeRect.z = strokeThickness;
            activeRect.w -= strokeThickness * 2.0;
        }
    }
    const float2 local = activeRect.xy + input.position * activeRect.zw;
    const float2 transformed = float2(
        local.x * transform0.x + local.y * transform0.z + transform1.x,
        local.x * transform0.y + local.y * transform0.w + transform1.y);
    const float2 ndc = float2(
        (transformed.x / transform1.z) * 2.0 - 1.0,
        1.0 - (transformed.y / transform1.w) * 2.0);

    VSOutput output;
    output.position = float4(ndc, 0.0, 1.0);
    output.color = colors[rectIndex];
    output.localPosition = input.position * activeRect.zw;
    output.rectangleSize = activeRect.zw;
    output.cornerRadius = cornerRadii[rectIndex].x;
    return output;
}

float4 ps_main(VSOutput input) : SV_Target {
    [loop]
    for (uint index = 0; index < clipCount; ++index) {
        const float2 relative = input.position.xy - clipTranslation[index].xy;
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
    float coverage = 1.0;
    if (input.cornerRadius > 0.0) {
        const float2 halfSize = input.rectangleSize * 0.5;
        const float2 cornerDelta = abs(input.localPosition - halfSize) -
            (halfSize - input.cornerRadius);
        const float signedDistance = length(max(cornerDelta, 0.0)) +
            min(max(cornerDelta.x, cornerDelta.y), 0.0) - input.cornerRadius;
        // The RenderFrame pipeline uses non-premultiplied source-alpha
        // blending, so analytic coverage belongs in alpha. fwidth keeps the
        // rounded edge stable under the active affine transform.
        coverage = saturate(0.5 - signedDistance /
            max(fwidth(signedDistance), 0.0001));
        if (coverage <= 0.0) {
            discard;
        }
    }
    float4 color = input.color;
    color.a *= coverage;
    return color;
}
