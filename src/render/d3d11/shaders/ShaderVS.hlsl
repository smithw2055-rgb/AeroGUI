// AeroGUI dynamic vertex shader permutations
cbuffer ViewportBuffer : register(b0) {
    float2 viewportSize;
    float2 viewportPad;
};

cbuffer TextureBuffer : register(b1) {
    float2 textureSize;
    float2 texturePad;
};

struct In {
    float2 position : POSITION;
#if HAS_COLOR
    float4 color : COLOR;
#endif
#if HAS_UV0
    float2 uv0 : TEXCOORD0;
#endif
#if HAS_UV1
    float2 uv1 : TEXCOORD1;
#endif
#if HAS_COVERAGE
    float coverage : COVERAGE;
#endif
};

struct Out {
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 uv0 : TEXCOORD0;
    float4 st1 : TEXCOORD1;
    float coverage : COVERAGE;
};

Out vs_main(In i) {
    Out o;
    const float2 ndc = float2(
        i.position.x / viewportSize.x * 2.0 - 1.0,
        1.0 - i.position.y / viewportSize.y * 2.0);
    o.position = float4(ndc, 0.0, 1.0);

#if HAS_COLOR
    o.color = i.color;
#else
    o.color = float4(1.0, 1.0, 1.0, 1.0);
#endif

#if HAS_UV0
    o.uv0 = i.uv0;
#elif HAS_UV1
    o.uv0 = i.uv1;
#else
    o.uv0 = float2(0.0, 0.0);
#endif

#if HAS_UV1
    float2 ts = max(textureSize.xy, float2(1.0, 1.0));
    o.st1 = float4(i.uv1 * ts, 1.0 / (3.0 * ts));
#elif HAS_UV0
    float2 ts = max(textureSize.xy, float2(1.0, 1.0));
    o.st1 = float4(i.uv0 * ts, 1.0 / (3.0 * ts));
#else
    o.st1 = float4(0.0, 0.0, 0.0, 0.0);
#endif

#if HAS_COVERAGE
    o.coverage = i.coverage;
#else
    o.coverage = 1.0;
#endif

    return o;
}
