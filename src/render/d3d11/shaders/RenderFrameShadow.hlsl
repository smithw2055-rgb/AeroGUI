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

static const float3 kGaussianKernel[33] = {
    float3( 0.0000000f,  0.0000000f,  0.0717812f),
    float3( 0.1250000f,  0.0000000f,  0.0693556f),
    float3(-0.1596450f,  0.1462479f,  0.0647477f),
    float3( 0.0244362f, -0.2784383f,  0.0604458f),
    float3( 0.2012222f,  0.2624588f,  0.0564298f),
    float3(-0.3692676f, -0.0653182f,  0.0526806f),
    float3( 0.3498025f, -0.2225157f,  0.0491805f),
    float3(-0.1170021f,  0.4352419f,  0.0459130f),
    float3(-0.2231357f, -0.4296341f,  0.0428625f),
    float3( 0.4841151f,  0.1767981f,  0.0400147f),
    float3(-0.5036411f,  0.2078957f,  0.0373561f),
    float3( 0.2427883f, -0.5188245f,  0.0348742f),
    float3( 0.1794144f,  0.5720013f,  0.0325572f),
    float3(-0.5407570f, -0.3133797f,  0.0303941f),
    float3( 0.6343695f, -0.1394644f,  0.0283747f),
    float3(-0.3871458f,  0.5506751f,  0.0264895f),
    float3(-0.0894397f, -0.6901996f,  0.0247295f),
    float3( 0.5490718f,  0.4627583f,  0.0230865f),
    float3(-0.7388785f,  0.0305549f,  0.0215526f),
    float3( 0.5389551f, -0.5363323f,  0.0201207f),
    float3(-0.0360582f,  0.7797915f,  0.0187839f),
    float3(-0.5128175f, -0.6145268f,  0.0175359f),
    float3( 0.8123596f,  0.1093018f,  0.0163708f),
    float3(-0.6883106f,  0.4789086f,  0.0152831f),
    float3( 0.1880861f, -0.8360614f,  0.0142677f),
    float3( 0.4350333f,  0.7591911f,  0.0133197f),
    float3(-0.8504484f, -0.2713162f,  0.0124348f),
    float3( 0.8261024f, -0.3816803f,  0.0116086f),
    float3(-0.3578882f,  0.8551556f,  0.0108373f),
    float3(-0.3194073f, -0.8880338f,  0.0101173f),
    float3( 0.8499086f,  0.4466882f,  0.0094451f),
    float3(-0.9440346f,  0.2488445f,  0.0088176f),
    float3( 0.5365958f, -0.8345298f,  0.0082317f)
};

float4 ps_main(VSOutput input) : SV_Target {
    const float2 texel = 1.0 / textureSize;
    float sumA = 0.0;
    [unroll]
    for (int i = 0; i < 33; ++i) {
        sumA += sourceTexture.Sample(sourceSampler, input.uv0 + kGaussianKernel[i].xy * texel).a * kGaussianKernel[i].z;
    }
    return input.color * (sumA * input.coverage);
}
