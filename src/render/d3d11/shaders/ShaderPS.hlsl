// AeroGUI Uber Pixel Shader Permutations (aligned with reference architecture)
#if PAINT_SOLID
    #define HAS_COLOR 1
#endif

#if PAINT_LINEAR || PAINT_RADIAL || PAINT_PATTERN
    #define HAS_UV0 1
#endif

#if EFFECT_PATH_AA
    #define HAS_COVERAGE 1
#endif

#if EFFECT_SDF || EFFECT_SDF_LCD
    #define HAS_GLYPHS 1
#endif

#if EFFECT_OPACITY
    #define HAS_IMAGE 1
#endif

#if EFFECT_SHADOW || EFFECT_BLUR || EFFECT_MASK
    #define HAS_IMAGE 1
    #define HAS_SHADOW 1
#endif

#if EFFECT_DOWNSAMPLE || EFFECT_UPSAMPLE
    #define HAS_PATTERN 1
#endif

Texture2D pattern : register(t0);
SamplerState patternSampler : register(s0);

Texture2D ramps : register(t1);
SamplerState rampsSampler : register(s1);

Texture2D image : register(t2);
SamplerState imageSampler : register(s2);

Texture2D glyphs : register(t3);
SamplerState glyphsSampler : register(s3);

Texture2D shadow : register(t4);
SamplerState shadowSampler : register(s4);

cbuffer PaintBuffer : register(b0) {
    float4 uPaint0;
    float4 uPaint1;
};

cbuffer EffectBuffer : register(b1) {
    float4 uEffect0;
    float4 uEffect1;
};

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

struct In {
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 uv0 : TEXCOORD0;
    float4 st1 : TEXCOORD1;
    float coverage : COVERAGE;
};

struct Out {
    float4 color : SV_Target0;
#if EFFECT_SDF_LCD
    float4 alpha : SV_Target1;
#endif
};

Out ps_main(In i) {
    Out o;

    /////////////////////////////////////////////////////
    // Fetch paint color and opacity
    /////////////////////////////////////////////////////
#if PAINT_SOLID
    float4 paint = i.color;
    float opacity_ = 1.0;

#elif PAINT_LINEAR
    float4 paint = ramps.Sample(rampsSampler, float2(i.uv0.x, 0.5)) * i.color;
    float opacity_ = uPaint0.x;

#elif PAINT_RADIAL
    float dd = uPaint1.x * i.uv0.x - uPaint1.y * i.uv0.y;
    float inside = i.uv0.x * i.uv0.x + i.uv0.y * i.uv0.y - dd * dd;
    float u = uPaint0.x * i.uv0.x + uPaint0.y * i.uv0.y +
        uPaint0.z * sqrt(max(inside, 0.0));
    float4 paint = ramps.Sample(rampsSampler, float2(u, uPaint1.z)) * i.color;
    float opacity_ = uPaint0.w;

#elif PAINT_PATTERN
    float2 uv = i.uv0;
#if REPEAT_PATTERN
    uv = frac(uv);
#elif MIRROR_PATTERN
    uv = abs(uv - 2.0 * floor((uv - 1.0) * 0.5) - 2.0);
#elif MIRRORU_PATTERN
    uv.x = abs(uv.x - 2.0 * floor((uv.x - 1.0) * 0.5) - 2.0);
    uv.y = frac(uv.y);
#elif MIRRORV_PATTERN
    uv.x = frac(uv.x);
    uv.y = abs(uv.y - 2.0 * floor((uv.y - 1.0) * 0.5) - 2.0);
#elif CLAMP_PATTERN
    uv = saturate(uv);
#endif
    float4 paint = pattern.Sample(patternSampler, uv) * i.color;
    float opacity_ = (uPaint0.x > 0.0) ? uPaint0.x : 1.0;

#else
    float4 paint = i.color;
    float opacity_ = 1.0;
#endif

    /////////////////////////////////////////////////////
    // Apply selected effect
    /////////////////////////////////////////////////////
#if EFFECT_RGBA
    o.color = uPaint0;

#elif EFFECT_MASK
    // Opacity mask composition: source in 'image' (t2), mask in 'shadow' (t4)
    float maskAlpha = shadow.Sample(shadowSampler, i.uv0).a;
    o.color = image.Sample(imageSampler, i.uv0) * (maskAlpha * i.color * i.coverage);

#elif EFFECT_CLEAR
    o.color = float4(0.0, 0.0, 0.0, 0.0);

#elif EFFECT_PATH
    o.color = opacity_ * paint;

#elif EFFECT_PATH_AA
    // PPAA: per-vertex coverage modulates color
    o.color = (opacity_ * i.coverage) * paint;

#elif EFFECT_OPACITY
    // In-place opacity pass for nested visuals without offscreen overhead
#if PAINT_PATTERN
    o.color = (opacity_ * i.coverage) * paint;
#else
    o.color = image.Sample(imageSampler, i.uv0) * (opacity_ * paint * i.coverage);
#endif

#elif EFFECT_SDF
    // Single-channel text rasterization (SDF)
    #define SDF_SCALE 7.96875
    #define SDF_BIAS 0.50196078431
    #define SDF_AA_FACTOR 0.65
    #define SDF_BASE_MIN 0.125
    #define SDF_BASE_MAX 0.25
    #define SDF_BASE_DEV -0.65

    float dist = SDF_SCALE * (glyphs.Sample(glyphsSampler, i.uv0).r - SDF_BIAS);
    float2 grad = ddx(i.st1.xy);
    float gradLen = max(length(grad), 1.0 / 512.0);
    float scale = 1.0 / gradLen;
    float base = SDF_BASE_DEV * (1.0 - (clamp(scale, SDF_BASE_MIN, SDF_BASE_MAX) - SDF_BASE_MIN) / (SDF_BASE_MAX - SDF_BASE_MIN));
    float range = SDF_AA_FACTOR * gradLen;
    float alpha = smoothstep(base - range, base + range, dist);
    o.color = (alpha * opacity_ * i.coverage) * paint;

#elif EFFECT_SDF_LCD
    // Subpixel ClearType LCD text with Dual Source Blending (Target0=color, Target1=alpha)
    #define SDF_SCALE 7.96875
    #define SDF_BIAS 0.50196078431
    #define SDF_AA_FACTOR 0.65
    #define SDF_BASE_MIN 0.125
    #define SDF_BASE_MAX 0.25
    #define SDF_BASE_DEV -0.65

    float2 grad = ddx(i.st1.xy);
    float gradLen = max(length(grad), 1.0 / 512.0);
    float scale = 1.0 / gradLen;
    float base = SDF_BASE_DEV * (1.0 - (clamp(scale, SDF_BASE_MIN, SDF_BASE_MAX) - SDF_BASE_MIN) / (SDF_BASE_MAX - SDF_BASE_MIN));
    float range = SDF_AA_FACTOR * gradLen;

    float2 offset = float2(i.st1.z, 0.0);
    float red = SDF_SCALE * (glyphs.Sample(glyphsSampler, i.uv0 - offset).r - SDF_BIAS);
    float green = SDF_SCALE * (glyphs.Sample(glyphsSampler, i.uv0).r - SDF_BIAS);
    float blue = SDF_SCALE * (glyphs.Sample(glyphsSampler, i.uv0 + offset).r - SDF_BIAS);
    float3 dist3 = float3(red, green, blue);
    float3 a3 = smoothstep(base - range, base + range, dist3);
    o.color = float4((opacity_ * i.coverage) * paint.rgb * a3.rgb, a3.g * i.coverage);
    o.alpha = float4((opacity_ * paint.a * i.coverage) * a3.rgb, a3.g * i.coverage);

#elif EFFECT_DOWNSAMPLE
    // 4-tap box filter downsample pass
    float2 texSize = uPaint0.xy;
    float2 delta = 1.0 / max(texSize, float2(1.0, 1.0));
    o.color = (
        pattern.Sample(patternSampler, i.uv0 + float2(-delta.x, -delta.y)) +
        pattern.Sample(patternSampler, i.uv0 + float2( delta.x, -delta.y)) +
        pattern.Sample(patternSampler, i.uv0 + float2(-delta.x,  delta.y)) +
        pattern.Sample(patternSampler, i.uv0 + float2( delta.x,  delta.y))
    ) * 0.25;

#elif EFFECT_UPSAMPLE
    // Upsample pass with bilinear filtering and blend
    o.color = lerp(image.Sample(imageSampler, i.uv0), pattern.Sample(patternSampler, i.uv0), i.color.a);

#elif EFFECT_SHADOW
    float2 texSize = uPaint0.xy;
    float2 texel = 1.0 / max(texSize, float2(1.0, 1.0));
    float sumA = 0.0;
    [unroll]
    for (int sIdx = 0; sIdx < 33; ++sIdx) {
        sumA += shadow.Sample(shadowSampler, i.uv0 + kGaussianKernel[sIdx].xy * texel).a * kGaussianKernel[sIdx].z;
    }
    o.color = i.color * (sumA * i.coverage);

#elif EFFECT_BLUR
    float2 texSize = uPaint0.xy;
    float2 texel = 1.0 / max(texSize, float2(1.0, 1.0));
    float4 sum = float4(0.0, 0.0, 0.0, 0.0);
    [unroll]
    for (int bIdx = 0; bIdx < 33; ++bIdx) {
        sum += image.Sample(imageSampler, i.uv0 + kGaussianKernel[bIdx].xy * texel) * kGaussianKernel[bIdx].z;
    }
    o.color = sum * (i.color * i.coverage);

#else
    o.color = opacity_ * paint;
#endif

    return o;
}
