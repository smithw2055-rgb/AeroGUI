struct VSInput {
    float2 position : ATTR0;
};

cbuffer MaskConstants : register(b0) {
    float4 maskRect;
    float4 transform0;
    float4 transform1;
    float4 mask0;
    float4 mask1;
    float4 geometry0;
    float4 geometry1;
    float4 geometry2;
    float4 relativeInverse0;
    float4 relativeInverse1;
};

Texture2D sourceTexture : register(t0);
SamplerState sourceSampler : register(s0);

struct VSOutput {
    float4 position : SV_Position;
    float2 localPosition : TEXCOORD0;
};

VSOutput vs_main(VSInput input) {
    const float2 local = maskRect.xy + input.position * maskRect.zw;
    const float2 transformed = float2(
        local.x * transform0.x + local.y * transform0.z + transform1.x,
        local.x * transform0.y + local.y * transform0.w + transform1.y);
    const float2 ndc = float2(
        (transformed.x / transform1.z) * 2.0 - 1.0,
        1.0 - (transformed.y / transform1.w) * 2.0);

    VSOutput output;
    output.position = float4(ndc, 0.0, 1.0);
    output.localPosition = local;
    return output;
}

float alignmentFactor(float value) {
    if (value < 1.5) return 0.0;
    if (value > 2.5) return 1.0;
    return 0.5;
}

float2 applyRelativeInverse(float2 samplePoint) {
    return float2(
        samplePoint.x * relativeInverse0.x +
            samplePoint.y * relativeInverse0.z + relativeInverse1.x,
        samplePoint.x * relativeInverse0.y +
            samplePoint.y * relativeInverse0.w + relativeInverse1.y);
}

float imageMaskAlpha(float2 samplePoint) {
    const float4 sourceUv = geometry0;
    const float4 cell = geometry1;
    const float2 imageSize = geometry2.zw;
    if (sourceUv.z <= 0.0 || sourceUv.w <= 0.0 ||
        cell.z <= 0.0 || cell.w <= 0.0 ||
        imageSize.x <= 0.0 || imageSize.y <= 0.0) {
        return 0.0;
    }

    const float tileMode = mask1.x;
    float2 tileIndex = floor((samplePoint - cell.xy) / cell.zw);
    if (tileMode < 0.5) {
        tileIndex = float2(0.0, 0.0);
        if (samplePoint.x < cell.x || samplePoint.y < cell.y ||
            samplePoint.x > cell.x + cell.z ||
            samplePoint.y > cell.y + cell.w) {
            return 0.0;
        }
    }
    const float2 tileOrigin = cell.xy + tileIndex * cell.zw;
    float4 drawRect = float4(tileOrigin, cell.zw);
    float4 fittedUv = sourceUv;
    const float2 sourceSize = imageSize * abs(sourceUv.zw);
    const float stretch = mask1.y;
    const float alignX = alignmentFactor(mask1.z);
    const float alignY = alignmentFactor(mask1.w);

    if (stretch < 0.5) {
        drawRect.xy += (drawRect.zw - sourceSize) * float2(alignX, alignY);
        drawRect.zw = sourceSize;
    } else if (stretch > 1.5 && stretch < 2.5) {
        const float scale = min(
            drawRect.z / sourceSize.x,
            drawRect.w / sourceSize.y);
        const float2 fittedSize = sourceSize * scale;
        drawRect.xy += (drawRect.zw - fittedSize) * float2(alignX, alignY);
        drawRect.zw = fittedSize;
    } else if (stretch > 2.5) {
        const float scale = max(
            drawRect.z / sourceSize.x,
            drawRect.w / sourceSize.y);
        const float2 drawnSize = sourceSize * scale;
        const float2 visibleFraction = drawRect.zw / drawnSize;
        fittedUv.xy += fittedUv.zw *
            (float2(1.0, 1.0) - visibleFraction) *
            float2(alignX, alignY);
        fittedUv.zw *= visibleFraction;
    }

    if (drawRect.z <= 0.0 || drawRect.w <= 0.0 ||
        samplePoint.x < drawRect.x || samplePoint.y < drawRect.y ||
        samplePoint.x > drawRect.x + drawRect.z ||
        samplePoint.y > drawRect.y + drawRect.w) {
        return 0.0;
    }
    float2 unit = (samplePoint - drawRect.xy) / drawRect.zw;
    const bool oddColumn = fmod(abs(tileIndex.x), 2.0) >= 1.0;
    const bool oddRow = fmod(abs(tileIndex.y), 2.0) >= 1.0;
    if (oddColumn && (tileMode > 1.5 && tileMode < 2.5 || tileMode > 3.5)) {
        unit.x = 1.0 - unit.x;
    }
    if (oddRow && (tileMode > 2.5 && tileMode < 3.5 || tileMode > 3.5)) {
        unit.y = 1.0 - unit.y;
    }
    const float2 uv = fittedUv.xy + unit * fittedUv.zw;
    return sourceTexture.Sample(sourceSampler, uv).a * mask0.w;
}

float linearGradientAlpha(float2 samplePoint) {
    const float2 startPoint = geometry0.xy;
    const float2 direction = geometry0.zw - startPoint;
    const float denominator = dot(direction, direction);
    const float position = denominator > 1.0e-12
        ? dot(samplePoint - startPoint, direction) / denominator
        : 0.0;
    return sourceTexture.Sample(
        sourceSampler, float2(saturate(position), 0.5)).a * mask0.w;
}

float radialGradientAlpha(float2 samplePoint) {
    const float2 center = geometry1.xy;
    const float2 origin = geometry1.zw;
    const float2 radius = geometry2.xy;
    if (radius.x <= 0.0 || radius.y <= 0.0) return 0.0;
    const float2 normalizedPoint = (samplePoint - center) / radius;
    const float2 normalizedOrigin = (origin - center) / radius;
    const float2 ray = normalizedPoint - normalizedOrigin;
    const float a = dot(ray, ray);
    float position = 0.0;
    if (a > 1.0e-12) {
        const float b = 2.0 * dot(normalizedOrigin, ray);
        const float c = dot(normalizedOrigin, normalizedOrigin) - 1.0;
        const float discriminant = max(b * b - 4.0 * a * c, 0.0);
        const float root = (-b + sqrt(discriminant)) / (2.0 * a);
        position = root > 1.0e-6 ? 1.0 / root : length(normalizedPoint);
    }
    return sourceTexture.Sample(
        sourceSampler, float2(saturate(position), 0.5)).a * mask0.w;
}

float4 ps_main(VSOutput input) : SV_Target {
    float2 unit = input.localPosition / maskRect.zw;
    unit = applyRelativeInverse(unit);
    if (mask0.x < 2.5) {
        return float4(1.0, 1.0, 1.0,
            saturate(imageMaskAlpha(unit * maskRect.zw)));
    }

    float2 samplePoint = unit;
    if (mask0.y > 0.5) samplePoint *= maskRect.zw;
    const float alpha = mask0.x < 3.5
        ? linearGradientAlpha(samplePoint)
        : radialGradientAlpha(samplePoint);
    return float4(1.0, 1.0, 1.0, saturate(alpha));
}
