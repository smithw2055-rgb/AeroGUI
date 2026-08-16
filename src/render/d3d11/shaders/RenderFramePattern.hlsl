// Vertex layout matches UiFrameEncoder::Vertex2D (24 bytes):
//   float2 position @0, uint32 color (RGBA8) @8, float2 uv0 @12, float coverage @20
cbuffer ViewportBuffer : register(b0) {
    float2 viewportSize;
    float2 viewportPad;
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
    float4 color = sourceTexture.Sample(sourceSampler, input.uv0) * input.color;
    color.a *= input.coverage;
    return color;
}