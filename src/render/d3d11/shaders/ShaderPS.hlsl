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

struct In {
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 uv0 : TEXCOORD0;
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
    o.color = (opacity_ * i.coverage) * paint;

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
    // Single-channel signed distance field text
    float dist = glyphs.Sample(glyphsSampler, i.uv0).r;
    float smoothing = max(fwidth(dist), 1.0 / 512.0);
    float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, dist);
    o.color = (alpha * opacity_ * i.coverage) * paint;

#elif EFFECT_SDF_LCD
    // Subpixel ClearType LCD text with Dual Source Blending (Target0=color, Target1=alpha)
    float2 grad = ddx(i.uv0);
    float2 offset = grad * float2(0.333333, 0.0);
    float4 red = glyphs.Sample(glyphsSampler, i.uv0 - offset);
    float4 green = glyphs.Sample(glyphsSampler, i.uv0);
    float4 blue = glyphs.Sample(glyphsSampler, i.uv0 + offset);
    float3 dist3 = float3(red.r, green.r, blue.r);
    float sm = max(fwidth(dist3.g), 1.0 / 512.0);
    float3 a3 = smoothstep(0.5 - sm, 0.5 + sm, dist3);
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
    float blendVal = uEffect0.w;
    float4 img = image.Sample(imageSampler, i.uv0);
    float shadowA = shadow.Sample(shadowSampler, i.uv0).a;
    o.color = (img + (1.0 - img.a) * (uEffect0 * shadowA)) * (opacity_ * paint.a);

#elif EFFECT_BLUR
    float blendVal = uEffect0.w;
    o.color = lerp(image.Sample(imageSampler, i.uv0), shadow.Sample(shadowSampler, i.uv0), blendVal) * (opacity_ * paint.a);

#else
    o.color = opacity_ * paint;
#endif

    return o;
}
