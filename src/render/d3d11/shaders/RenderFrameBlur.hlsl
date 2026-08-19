// Vertex layout matches UiFrameEncoder::Vertex2D (24 bytes):
//   float2 position @0, uint32 color (RGBA8) @8, float2 uv0 @12, float coverage @20
cbuffer ViewportBuffer : register(b0) {
    float2 viewportSize;
    float2 viewportPad;
};

cbuffer TextureSizeBuffer : register(b1) {
    float2 textureSize;
    float2 texturePad;
};

Texture2D sourceTexture : register(t0);
SamplerState sourceSampler : register(s0);

struct VSInput {
    float2 position : POSITION;
    float4 color : COLOR;
    float2 uv0 : TEXCOORD0;
    float coverage : COVERAGE;
};

struct VSOutput {
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 uv0 : TEXCOORD0;
    float coverage : COVERAGE;
};

VSOutput vs_main(VSInput input) {
    VSOutput output;
    const float2 ndc = float2(
        input.position.x / viewportSize.x * 2.0 - 1.0,
        1.0 - input.position.y / viewportSize.y * 2.0);
    output.position = float4(ndc, 0.0, 1.0);
    output.color = input.color;
    output.uv0 = input.uv0;
    output.coverage = input.coverage;
    return output;
}

float4 ps_main(VSOutput input) : SV_Target {
    const float2 texel = 1.0 / textureSize;
    float4 sum = sourceTexture.Sample(sourceSampler, input.uv0) * 4.0;
    sum += sourceTexture.Sample(sourceSampler, input.uv0 + float2(-texel.x, 0.0)) * 2.0;
    sum += sourceTexture.Sample(sourceSampler, input.uv0 + float2(texel.x, 0.0)) * 2.0;
    sum += sourceTexture.Sample(sourceSampler, input.uv0 + float2(0.0, -texel.y)) * 2.0;
    sum += sourceTexture.Sample(sourceSampler, input.uv0 + float2(0.0, texel.y)) * 2.0;
    sum += sourceTexture.Sample(sourceSampler, input.uv0 + float2(-texel.x, -texel.y));
    sum += sourceTexture.Sample(sourceSampler, input.uv0 + float2(texel.x, -texel.y));
    sum += sourceTexture.Sample(sourceSampler, input.uv0 + float2(-texel.x, texel.y));
    sum += sourceTexture.Sample(sourceSampler, input.uv0 + float2(texel.x, texel.y));
    return (sum / 16.0) * (input.color * input.coverage);
}
