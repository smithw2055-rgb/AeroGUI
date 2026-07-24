struct VSInput {
    float2 position : ATTR0;
    float2 uv : ATTR1;
};

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

cbuffer TintBuffer : register(b0) {
    float4 tint;
    float4 tintPadding;
};

Texture2D sourceTexture : register(t0);
SamplerState sourceSampler : register(s0);

VSOutput vs_main(VSInput input) {
    VSOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.uv = input.uv;
    return output;
}

float4 ps_main(VSOutput input) : SV_Target {
    return sourceTexture.Sample(sourceSampler, input.uv) * (tint + tintPadding);
}
