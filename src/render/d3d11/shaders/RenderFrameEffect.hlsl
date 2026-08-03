struct VSInput {
    float2 position : ATTR0;
};

cbuffer EffectConstants : register(b0) {
    float4 viewport;
    float4 filter0;
    float4 filter1;
    float4 tint;
};

Texture2D sourceTexture : register(t0);
SamplerState sourceSampler : register(s0);

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOutput vs_main(VSInput input) {
    VSOutput output;
    output.position = float4(
        input.position.x * 2.0 - 1.0,
        1.0 - input.position.y * 2.0,
        0.0,
        1.0);
    output.uv = input.position;
    return output;
}

float4 sampleOrTransparent(float2 uv) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) {
        return float4(0.0, 0.0, 0.0, 0.0);
    }
    return sourceTexture.Sample(sourceSampler, uv);
}

float4 ps_main(VSOutput input) : SV_Target {
    const float2 center = input.uv - filter0.zw;
    const float2 stepValue = filter0.xy;
    float4 blurred =
        sampleOrTransparent(center - stepValue * 4.0) * 0.01621622 +
        sampleOrTransparent(center - stepValue * 3.0) * 0.05405405 +
        sampleOrTransparent(center - stepValue * 2.0) * 0.12162162 +
        sampleOrTransparent(center - stepValue) * 0.19459459 +
        sampleOrTransparent(center) * 0.22702703 +
        sampleOrTransparent(center + stepValue) * 0.19459459 +
        sampleOrTransparent(center + stepValue * 2.0) * 0.12162162 +
        sampleOrTransparent(center + stepValue * 3.0) * 0.05405405 +
        sampleOrTransparent(center + stepValue * 4.0) * 0.01621622;
    if (filter1.x > 0.5) {
        return float4(tint.rgb, saturate(blurred.a * tint.a));
    }
    return blurred * tint;
}
