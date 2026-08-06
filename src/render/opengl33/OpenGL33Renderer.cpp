#include "OpenGL33Shaders.hpp"

#include <new>

#if 0 // Retired native Renderer adapter; shader catalog only
#include "../DeviceRenderer.hpp"
#endif

namespace Aero::Render {
namespace {

#if 0 // Retired native Renderer adapter; shader catalog only
Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotInitialized, message);
}

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

Base::Status OutOfMemory(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::OutOfMemory, message);
}
#endif

Graphics::ShaderDescriptor Shader(
    Graphics::ShaderStage stage,
    const std::uint8_t* source,
    std::uint32_t sourceSize,
    std::uint64_t stableId) noexcept {
    Graphics::ShaderDescriptor descriptor;
    descriptor.stage = stage;
    descriptor.language = Graphics::ShaderLanguage::Glsl330;
    descriptor.bytecode = source;
    descriptor.bytecodeSize = sourceSize;
    descriptor.entryPoint = Base::StringView("main");
    descriptor.stableId = stableId;
    return descriptor;
}

static const std::uint8_t RectangleVertex[] = R"GLSL(
#version 330 core
layout(location = 0) in vec2 inputPosition;
layout(std140) uniform AeroUniform0 {
    vec4 rects[64];
    vec4 colors[64];
    vec4 cornerRadii[64];
    vec4 transform0;
    vec4 transform1;
    vec4 clipRect[32];
    vec4 clipInverse[32];
    vec4 clipTranslation[32];
    uint clipCount;
    uint instanceMode;
    float strokeThickness;
    float clipPadding;
};
out vec4 vertexColor;
out vec2 localPosition;
out vec2 rectangleSize;
out float cornerRadius;
out vec2 canvasPosition;
void main() {
    uint rectIndex = instanceMode == 2u ? uint(gl_InstanceID) : 0u;
    vec4 activeRect = rects[rectIndex];
    if (instanceMode == 1u) {
        if (gl_InstanceID == 0) {
            activeRect.w = strokeThickness;
        } else if (gl_InstanceID == 1) {
            activeRect.y += activeRect.w - strokeThickness;
            activeRect.w = strokeThickness;
        } else if (gl_InstanceID == 2) {
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
    vec2 local = activeRect.xy + inputPosition * activeRect.zw;
    vec2 transformed = vec2(
        local.x * transform0.x + local.y * transform0.z + transform1.x,
        local.x * transform0.y + local.y * transform0.w + transform1.y);
    vec2 ndc = vec2(
        (transformed.x / transform1.z) * 2.0 - 1.0,
        1.0 - (transformed.y / transform1.w) * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
    vertexColor = colors[rectIndex];
    localPosition = inputPosition * activeRect.zw;
    rectangleSize = activeRect.zw;
    cornerRadius = cornerRadii[rectIndex].x;
    canvasPosition = transformed;
}
)GLSL";

static const std::uint8_t RectangleFragment[] = R"GLSL(
#version 330 core
layout(std140) uniform AeroUniform0 {
    vec4 rects[64];
    vec4 colors[64];
    vec4 cornerRadii[64];
    vec4 transform0;
    vec4 transform1;
    vec4 clipRect[32];
    vec4 clipInverse[32];
    vec4 clipTranslation[32];
    uint clipCount;
    uint instanceMode;
    float strokeThickness;
    float clipPadding;
};
in vec4 vertexColor;
in vec2 localPosition;
in vec2 rectangleSize;
in float cornerRadius;
in vec2 canvasPosition;
out vec4 outputColor;
void main() {
    for (uint index = 0u; index < clipCount; ++index) {
        vec2 relative = canvasPosition - clipTranslation[index].xy;
        vec2 local = vec2(
            relative.x * clipInverse[index].x +
                relative.y * clipInverse[index].z,
            relative.x * clipInverse[index].y +
                relative.y * clipInverse[index].w);
        vec2 minimumValue = clipRect[index].xy;
        vec2 maximumValue = minimumValue + clipRect[index].zw;
        if (local.x < minimumValue.x || local.y < minimumValue.y ||
            local.x > maximumValue.x || local.y > maximumValue.y) {
            discard;
        }
    }
    float coverage = 1.0;
    if (cornerRadius > 0.0) {
        vec2 halfSize = rectangleSize * 0.5;
        vec2 cornerDelta = abs(localPosition - halfSize) -
            (halfSize - cornerRadius);
        float signedDistance = length(max(cornerDelta, vec2(0.0))) +
            min(max(cornerDelta.x, cornerDelta.y), 0.0) - cornerRadius;
        coverage = clamp(
            0.5 - signedDistance /
                max(fwidth(signedDistance), 0.0001),
            0.0,
            1.0);
        if (coverage <= 0.0) {
            discard;
        }
    }
    outputColor = vertexColor;
    outputColor.a *= coverage;
}
)GLSL";

static const std::uint8_t ImageVertex[] = R"GLSL(
#version 330 core
layout(location = 0) in vec2 inputPosition;
layout(std140) uniform AeroUniform0 {
    vec4 rects[64];
    vec4 sourceUvs[64];
    vec4 tints[64];
    vec4 transform0;
    vec4 transform1;
    vec4 clipRect[32];
    vec4 clipInverse[32];
    vec4 clipTranslation[32];
    uint clipCount;
    vec3 clipPadding;
};
out vec2 textureCoordinate;
out vec4 vertexTint;
out vec2 canvasPosition;
void main() {
    uint index = uint(gl_InstanceID);
    vec4 rectangle = rects[index];
    vec4 sourceUv = sourceUvs[index];
    vec2 local = rectangle.xy + inputPosition * rectangle.zw;
    vec2 transformed = vec2(
        local.x * transform0.x + local.y * transform0.z + transform1.x,
        local.x * transform0.y + local.y * transform0.w + transform1.y);
    gl_Position = vec4(
        (transformed.x / transform1.z) * 2.0 - 1.0,
        1.0 - (transformed.y / transform1.w) * 2.0,
        0.0,
        1.0);
    textureCoordinate =
        sourceUv.xy + inputPosition * sourceUv.zw;
    vertexTint = tints[index];
    canvasPosition = transformed;
}
)GLSL";

static const std::uint8_t ImageFragment[] = R"GLSL(
#version 330 core
layout(std140) uniform AeroUniform0 {
    vec4 rects[64];
    vec4 sourceUvs[64];
    vec4 tints[64];
    vec4 transform0;
    vec4 transform1;
    vec4 clipRect[32];
    vec4 clipInverse[32];
    vec4 clipTranslation[32];
    uint clipCount;
    vec3 clipPadding;
};
uniform sampler2D AeroTexture0;
in vec2 textureCoordinate;
in vec4 vertexTint;
in vec2 canvasPosition;
out vec4 outputColor;
void main() {
    for (uint index = 0u; index < clipCount; ++index) {
        vec2 relative = canvasPosition - clipTranslation[index].xy;
        vec2 local = vec2(
            relative.x * clipInverse[index].x +
                relative.y * clipInverse[index].z,
            relative.x * clipInverse[index].y +
                relative.y * clipInverse[index].w);
        vec2 minimumValue = clipRect[index].xy;
        vec2 maximumValue = minimumValue + clipRect[index].zw;
        if (local.x < minimumValue.x || local.y < minimumValue.y ||
            local.x > maximumValue.x || local.y > maximumValue.y) {
            discard;
        }
    }
    outputColor = texture(AeroTexture0, textureCoordinate) * vertexTint;
}
)GLSL";

static const std::uint8_t MaskVertex[] = R"GLSL(
#version 330 core
layout(location = 0) in vec2 inputPosition;
layout(std140) uniform AeroUniform0 {
    vec4 maskRect;
    vec4 transform0;
    vec4 transform1;
    vec4 mask0;
    vec4 mask1;
    vec4 geometry0;
    vec4 geometry1;
    vec4 geometry2;
    vec4 relativeInverse0;
    vec4 relativeInverse1;
};
out vec2 localPosition;
void main() {
    vec2 local = maskRect.xy + inputPosition * maskRect.zw;
    vec2 transformed = vec2(
        local.x * transform0.x + local.y * transform0.z + transform1.x,
        local.x * transform0.y + local.y * transform0.w + transform1.y);
    gl_Position = vec4(
        (transformed.x / transform1.z) * 2.0 - 1.0,
        1.0 - (transformed.y / transform1.w) * 2.0,
        0.0,
        1.0);
    localPosition = local;
}
)GLSL";

static const std::uint8_t MaskFragment[] = R"GLSL(
#version 330 core
layout(std140) uniform AeroUniform0 {
    vec4 maskRect;
    vec4 transform0;
    vec4 transform1;
    vec4 mask0;
    vec4 mask1;
    vec4 geometry0;
    vec4 geometry1;
    vec4 geometry2;
    vec4 relativeInverse0;
    vec4 relativeInverse1;
};
uniform sampler2D AeroTexture0;
in vec2 localPosition;
out vec4 outputColor;

float alignmentFactor(float value) {
    if (value < 1.5) return 0.0;
    if (value > 2.5) return 1.0;
    return 0.5;
}

vec2 applyRelativeInverse(vec2 point) {
    return vec2(
        point.x * relativeInverse0.x +
            point.y * relativeInverse0.z + relativeInverse1.x,
        point.x * relativeInverse0.y +
            point.y * relativeInverse0.w + relativeInverse1.y);
}

float imageMaskAlpha(vec2 point) {
    vec4 sourceUv = geometry0;
    vec4 cell = geometry1;
    vec2 imageSize = geometry2.zw;
    if (sourceUv.z <= 0.0 || sourceUv.w <= 0.0 ||
        cell.z <= 0.0 || cell.w <= 0.0 ||
        imageSize.x <= 0.0 || imageSize.y <= 0.0) {
        return 0.0;
    }

    float tileMode = mask1.x;
    vec2 tileIndex = floor((point - cell.xy) / cell.zw);
    if (tileMode < 0.5) {
        tileIndex = vec2(0.0);
        if (point.x < cell.x || point.y < cell.y ||
            point.x > cell.x + cell.z || point.y > cell.y + cell.w) {
            return 0.0;
        }
    }
    vec2 tileOrigin = cell.xy + tileIndex * cell.zw;
    vec4 drawRect = vec4(tileOrigin, cell.zw);
    vec4 fittedUv = sourceUv;
    vec2 sourceSize = imageSize * abs(sourceUv.zw);
    float stretch = mask1.y;
    float alignX = alignmentFactor(mask1.z);
    float alignY = alignmentFactor(mask1.w);

    if (stretch < 0.5) {
        drawRect.xy += (drawRect.zw - sourceSize) * vec2(alignX, alignY);
        drawRect.zw = sourceSize;
    } else if (stretch > 1.5 && stretch < 2.5) {
        float scale = min(
            drawRect.z / sourceSize.x,
            drawRect.w / sourceSize.y);
        vec2 fittedSize = sourceSize * scale;
        drawRect.xy += (drawRect.zw - fittedSize) * vec2(alignX, alignY);
        drawRect.zw = fittedSize;
    } else if (stretch > 2.5) {
        float scale = max(
            drawRect.z / sourceSize.x,
            drawRect.w / sourceSize.y);
        vec2 drawnSize = sourceSize * scale;
        vec2 visibleFraction = drawRect.zw / drawnSize;
        fittedUv.xy += fittedUv.zw *
            (vec2(1.0) - visibleFraction) * vec2(alignX, alignY);
        fittedUv.zw *= visibleFraction;
    }

    if (drawRect.z <= 0.0 || drawRect.w <= 0.0 ||
        point.x < drawRect.x || point.y < drawRect.y ||
        point.x > drawRect.x + drawRect.z ||
        point.y > drawRect.y + drawRect.w) {
        return 0.0;
    }
    vec2 unit = (point - drawRect.xy) / drawRect.zw;
    bool oddColumn = mod(abs(tileIndex.x), 2.0) >= 1.0;
    bool oddRow = mod(abs(tileIndex.y), 2.0) >= 1.0;
    if (oddColumn && ((tileMode > 1.5 && tileMode < 2.5) || tileMode > 3.5)) {
        unit.x = 1.0 - unit.x;
    }
    if (oddRow && ((tileMode > 2.5 && tileMode < 3.5) || tileMode > 3.5)) {
        unit.y = 1.0 - unit.y;
    }
    vec2 uv = fittedUv.xy + unit * fittedUv.zw;
    return texture(AeroTexture0, uv).a * mask0.w;
}

float linearGradientAlpha(vec2 point) {
    vec2 startPoint = geometry0.xy;
    vec2 direction = geometry0.zw - startPoint;
    float denominator = dot(direction, direction);
    float position = denominator > 1.0e-12
        ? dot(point - startPoint, direction) / denominator
        : 0.0;
    return texture(AeroTexture0, vec2(clamp(position, 0.0, 1.0), 0.5)).a *
        mask0.w;
}

float radialGradientAlpha(vec2 point) {
    vec2 center = geometry1.xy;
    vec2 origin = geometry1.zw;
    vec2 radius = geometry2.xy;
    if (radius.x <= 0.0 || radius.y <= 0.0) return 0.0;
    vec2 normalizedPoint = (point - center) / radius;
    vec2 normalizedOrigin = (origin - center) / radius;
    vec2 ray = normalizedPoint - normalizedOrigin;
    float a = dot(ray, ray);
    float position = 0.0;
    if (a > 1.0e-12) {
        float b = 2.0 * dot(normalizedOrigin, ray);
        float c = dot(normalizedOrigin, normalizedOrigin) - 1.0;
        float discriminant = max(b * b - 4.0 * a * c, 0.0);
        float root = (-b + sqrt(discriminant)) / (2.0 * a);
        position = root > 1.0e-6 ? 1.0 / root : length(normalizedPoint);
    }
    return texture(AeroTexture0, vec2(clamp(position, 0.0, 1.0), 0.5)).a *
        mask0.w;
}

void main() {
    vec2 unit = applyRelativeInverse(localPosition / maskRect.zw);
    float alpha;
    if (mask0.x < 2.5) {
        alpha = imageMaskAlpha(unit * maskRect.zw);
    } else {
        vec2 point = mask0.y > 0.5 ? unit * maskRect.zw : unit;
        alpha = mask0.x < 3.5
            ? linearGradientAlpha(point)
            : radialGradientAlpha(point);
    }
    outputColor = vec4(1.0, 1.0, 1.0, clamp(alpha, 0.0, 1.0));
}
)GLSL";

static const std::uint8_t EffectVertex[] = R"GLSL(
#version 330 core
layout(location = 0) in vec2 inputPosition;
layout(std140) uniform AeroUniform0 {
    vec4 viewport;
    vec4 filter0;
    vec4 filter1;
    vec4 tint;
};
out vec2 textureCoordinate;
void main() {
    gl_Position = vec4(
        inputPosition.x * 2.0 - 1.0,
        1.0 - inputPosition.y * 2.0,
        0.0,
        1.0);
    textureCoordinate = inputPosition;
}
)GLSL";

static const std::uint8_t EffectFragment[] = R"GLSL(
#version 330 core
layout(std140) uniform AeroUniform0 {
    vec4 viewport;
    vec4 filter0;
    vec4 filter1;
    vec4 tint;
};
uniform sampler2D AeroTexture0;
in vec2 textureCoordinate;
out vec4 outputColor;

vec4 sampleOrTransparent(vec2 uv) {
    if (uv.x < 0.0 || uv.y < 0.0 || uv.x > 1.0 || uv.y > 1.0) {
        return vec4(0.0);
    }
    return texture(AeroTexture0, uv);
}

void main() {
    vec2 center = textureCoordinate - filter0.zw;
    vec2 stepValue = filter0.xy;
    vec4 blurred =
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
        outputColor = vec4(tint.rgb, clamp(blurred.a * tint.a, 0.0, 1.0));
    } else {
        outputColor = blurred * tint;
    }
}
)GLSL";

static const std::uint8_t MeshVertex[] = R"GLSL(
#version 330 core
layout(location = 0) in vec2 inputPosition;
layout(location = 1) in vec4 inputColor;
layout(location = 2) in float inputCoverage;
layout(std140) uniform AeroUniform0 {
    vec4 tints[64];
    vec4 transform0;
    vec4 transform1;
    vec4 clipRect[32];
    vec4 clipInverse[32];
    vec4 clipTranslation[32];
    uint clipCount;
    vec3 clipPadding;
};
out vec4 vertexColor;
out float vertexCoverage;
out vec2 canvasPosition;
void main() {
    vec2 transformed = vec2(
        inputPosition.x * transform0.x +
            inputPosition.y * transform0.z + transform1.x,
        inputPosition.x * transform0.y +
            inputPosition.y * transform0.w + transform1.y);
    gl_Position = vec4(
        (transformed.x / transform1.z) * 2.0 - 1.0,
        1.0 - (transformed.y / transform1.w) * 2.0,
        0.0,
        1.0);
    vertexColor = inputColor * tints[uint(gl_InstanceID)];
    vertexCoverage = inputCoverage;
    canvasPosition = transformed;
}
)GLSL";

static const std::uint8_t MeshFragment[] = R"GLSL(
#version 330 core
layout(std140) uniform AeroUniform0 {
    vec4 tints[64];
    vec4 transform0;
    vec4 transform1;
    vec4 clipRect[32];
    vec4 clipInverse[32];
    vec4 clipTranslation[32];
    uint clipCount;
    vec3 clipPadding;
};
in vec4 vertexColor;
in float vertexCoverage;
in vec2 canvasPosition;
out vec4 outputColor;
void main() {
    for (uint index = 0u; index < clipCount; ++index) {
        vec2 relative = canvasPosition - clipTranslation[index].xy;
        vec2 local = vec2(
            relative.x * clipInverse[index].x +
                relative.y * clipInverse[index].z,
            relative.x * clipInverse[index].y +
                relative.y * clipInverse[index].w);
        vec2 minimumValue = clipRect[index].xy;
        vec2 maximumValue = minimumValue + clipRect[index].zw;
        if (local.x < minimumValue.x || local.y < minimumValue.y ||
            local.x > maximumValue.x || local.y > maximumValue.y) {
            discard;
        }
    }
    outputColor = vertexColor;
    outputColor.a *= clamp(vertexCoverage, 0.0, 1.0);
}
)GLSL";

static const std::uint8_t GlyphVertex[] = R"GLSL(
#version 330 core
layout(location = 0) in vec2 inputPosition;
layout(location = 1) in vec2 inputUv;
layout(std140) uniform AeroUniform0 {
    vec4 tints[64];
    vec4 transform0;
    vec4 transform1;
    vec4 clipRect[32];
    vec4 clipInverse[32];
    vec4 clipTranslation[32];
    uint clipCount;
    vec3 clipPadding;
};
out vec2 textureCoordinate;
out vec4 vertexTint;
out vec2 canvasPosition;
void main() {
    vec2 transformed = vec2(
        inputPosition.x * transform0.x +
            inputPosition.y * transform0.z + transform1.x,
        inputPosition.x * transform0.y +
            inputPosition.y * transform0.w + transform1.y);
    gl_Position = vec4(
        (transformed.x / transform1.z) * 2.0 - 1.0,
        1.0 - (transformed.y / transform1.w) * 2.0,
        0.0,
        1.0);
    textureCoordinate = inputUv;
    vertexTint = tints[uint(gl_InstanceID)];
    canvasPosition = transformed;
}
)GLSL";

static const std::uint8_t GlyphFragment[] = R"GLSL(
#version 330 core
layout(std140) uniform AeroUniform0 {
    vec4 tints[64];
    vec4 transform0;
    vec4 transform1;
    vec4 clipRect[32];
    vec4 clipInverse[32];
    vec4 clipTranslation[32];
    uint clipCount;
    vec3 clipPadding;
};
uniform sampler2D AeroTexture0;
in vec2 textureCoordinate;
in vec4 vertexTint;
in vec2 canvasPosition;
out vec4 outputColor;
void main() {
    for (uint index = 0u; index < clipCount; ++index) {
        vec2 relative = canvasPosition - clipTranslation[index].xy;
        vec2 local = vec2(
            relative.x * clipInverse[index].x +
                relative.y * clipInverse[index].z,
            relative.x * clipInverse[index].y +
                relative.y * clipInverse[index].w);
        vec2 minimumValue = clipRect[index].xy;
        vec2 maximumValue = minimumValue + clipRect[index].zw;
        if (local.x < minimumValue.x || local.y < minimumValue.y ||
            local.x > maximumValue.x || local.y > maximumValue.y) {
            discard;
        }
    }
    float distance = texture(AeroTexture0, textureCoordinate).r;
    float smoothing = max(fwidth(distance), 1.0 / 512.0);
    outputColor = vertexTint;
    outputColor.a *= smoothstep(0.5 - smoothing, 0.5 + smoothing, distance);
}
)GLSL";

template <std::size_t Size>
std::uint32_t ShaderSize(
    const std::uint8_t (&)[Size]) noexcept {
    return static_cast<std::uint32_t>(Size - 1U);
}

} // namespace

FrameShaderSet MakeOpenGL33FrameShaderSet() noexcept {
    FrameShaderSet shaders;
    shaders.rectangleVertex = Shader(
        Graphics::ShaderStage::Vertex,
        RectangleVertex,
        ShaderSize(RectangleVertex),
        UINT64_C(0x33001001));
    shaders.rectangleFragment = Shader(
        Graphics::ShaderStage::Fragment,
        RectangleFragment,
        ShaderSize(RectangleFragment),
        UINT64_C(0x33001002));
    shaders.imageVertex = Shader(
        Graphics::ShaderStage::Vertex,
        ImageVertex,
        ShaderSize(ImageVertex),
        UINT64_C(0x33001011));
    shaders.imageFragment = Shader(
        Graphics::ShaderStage::Fragment,
        ImageFragment,
        ShaderSize(ImageFragment),
        UINT64_C(0x33001012));
    shaders.maskVertex = Shader(
        Graphics::ShaderStage::Vertex,
        MaskVertex,
        ShaderSize(MaskVertex),
        UINT64_C(0x33001013));
    shaders.maskFragment = Shader(
        Graphics::ShaderStage::Fragment,
        MaskFragment,
        ShaderSize(MaskFragment),
        UINT64_C(0x33001014));
    shaders.effectVertex = Shader(
        Graphics::ShaderStage::Vertex,
        EffectVertex,
        ShaderSize(EffectVertex),
        UINT64_C(0x33001015));
    shaders.effectFragment = Shader(
        Graphics::ShaderStage::Fragment,
        EffectFragment,
        ShaderSize(EffectFragment),
        UINT64_C(0x33001016));
    shaders.meshVertex = Shader(
        Graphics::ShaderStage::Vertex,
        MeshVertex,
        ShaderSize(MeshVertex),
        UINT64_C(0x33001021));
    shaders.meshFragment = Shader(
        Graphics::ShaderStage::Fragment,
        MeshFragment,
        ShaderSize(MeshFragment),
        UINT64_C(0x33001022));
    shaders.glyphVertex = Shader(
        Graphics::ShaderStage::Vertex,
        GlyphVertex,
        ShaderSize(GlyphVertex),
        UINT64_C(0x33001031));
    shaders.glyphFragment = Shader(
        Graphics::ShaderStage::Fragment,
        GlyphFragment,
        ShaderSize(GlyphFragment),
        UINT64_C(0x33001032));
    shaders.colorFormat = Graphics::GraphicsTextureFormat::Bgra8Unorm;
    return shaders;
}

#if 0 // Retired native Renderer adapter; shader catalog only
struct OpenGL33Renderer::Impl {
    Impl(
        Graphics::GraphicsDevice& device,
        Base::IAllocator* allocator) noexcept
        : renderer(
              device,
              MakeOpenGL33FrameShaderSet(),
              allocator) {}

    DeviceRenderer renderer;
    Graphics::FenceValue lastSubmittedFence = 0U;
    bool initialized = false;
};

OpenGL33Renderer::OpenGL33Renderer(
    Graphics::GraphicsDevice& device,
    Graphics::OpenGL33GraphicsBackend& graphicsBackend,
    Graphics::SurfaceSession& surface,
    Graphics::GlContextGeneration contextGeneration,
    Base::IAllocator* allocator) noexcept
    : device_(&device),
      graphicsBackend_(&graphicsBackend),
      surface_(&surface),
      contextGeneration_(contextGeneration),
      allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {}

OpenGL33Renderer::~OpenGL33Renderer() noexcept {
    Shutdown();
}

Base::Result<void>
OpenGL33Renderer::Initialize() noexcept {
    if (IsInitialized()) {
        return {};
    }
    if (device_ == nullptr || graphicsBackend_ == nullptr ||
        surface_ == nullptr || contextGeneration_ == 0U ||
        device_->Backend().IsDeviceLost() ||
        surface_->State() != Graphics::SurfaceState::Ready) {
        return NotInitialized(
            "OpenGL render adapter requires a ready device, context, and surface");
    }
    if (impl_ == nullptr) {
        void* memory = allocator_->Allocate({
            sizeof(Impl),
            alignof(Impl),
            Base::MemoryTag::Render});
        if (memory == nullptr) {
            return OutOfMemory(
                "Failed to allocate OpenGL render adapter state");
        }
        impl_ = new (memory) Impl(
            *device_, allocator_);
    }
    Base::Result<void> initialized = impl_->renderer.Initialize();
    if (!initialized) {
        Shutdown();
        return initialized;
    }
    impl_->renderer.SetBatchingEnabled(
        batchingEnabled_);
    impl_->initialized = true;
    return {};
}

void OpenGL33Renderer::Shutdown() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    impl_->renderer.Shutdown();
    impl_->~Impl();
    allocator_->Deallocate(
        impl_,
        sizeof(Impl),
        alignof(Impl),
        Base::MemoryTag::Render);
    impl_ = nullptr;
}

bool OpenGL33Renderer::IsInitialized() const noexcept {
    return impl_ != nullptr && impl_->initialized &&
        impl_->renderer.IsInitialized();
}

Base::Result<void>
OpenGL33Renderer::RegisterImage(
    Render::RenderImageId image,
    Graphics::ResourceHandle texture,
    Graphics::ResourceHandle sampler) noexcept {
    return IsInitialized()
        ? impl_->renderer.RegisterImage(image, texture, sampler)
        : Base::Result<void>(NotInitialized(
            "OpenGL render adapter is not initialized"));
}

Base::Result<void>
OpenGL33Renderer::UnregisterImage(
    Render::RenderImageId image) noexcept {
    return IsInitialized()
        ? impl_->renderer.UnregisterImage(image)
        : Base::Result<void>(NotInitialized(
            "OpenGL render adapter is not initialized"));
}

Base::Result<void>
OpenGL33Renderer::RegisterMesh(
    Render::RenderMeshId mesh,
    Graphics::ResourceHandle vertexBuffer,
    Graphics::ResourceHandle indexBuffer,
    std::uint32_t indexCount,
    Graphics::IndexType indexType) noexcept {
    return IsInitialized()
        ? impl_->renderer.RegisterMesh(
            mesh,
            vertexBuffer,
            indexBuffer,
            indexCount,
            indexType)
        : Base::Result<void>(NotInitialized(
            "OpenGL render adapter is not initialized"));
}

Base::Result<void>
OpenGL33Renderer::UnregisterMesh(
    Render::RenderMeshId mesh) noexcept {
    return IsInitialized()
        ? impl_->renderer.UnregisterMesh(mesh)
        : Base::Result<void>(NotInitialized(
            "OpenGL render adapter is not initialized"));
}

Base::Result<void>
OpenGL33Renderer::RegisterGlyphRun(
    Render::RenderGlyphRunId glyphRun,
    Graphics::ResourceHandle vertexBuffer,
    Graphics::ResourceHandle indexBuffer,
    std::uint32_t indexCount,
    Graphics::ResourceHandle atlasTexture,
    Graphics::ResourceHandle sampler,
    Graphics::IndexType indexType) noexcept {
    return IsInitialized()
        ? impl_->renderer.RegisterGlyphRun(
            glyphRun,
            vertexBuffer,
            indexBuffer,
            indexCount,
            atlasTexture,
            sampler,
            indexType)
        : Base::Result<void>(NotInitialized(
            "OpenGL render adapter is not initialized"));
}

Base::Result<void>
OpenGL33Renderer::UnregisterGlyphRun(
    Render::RenderGlyphRunId glyphRun) noexcept {
    return IsInitialized()
        ? impl_->renderer.UnregisterGlyphRun(glyphRun)
        : Base::Result<void>(NotInitialized(
            "OpenGL render adapter is not initialized"));
}

Aero::Render::Detail::TextResources* OpenGL33Renderer::GetTextResources() noexcept {
    return IsInitialized() ? impl_->renderer.GetTextResources() : nullptr;
}

Aero::Render::Detail::MeshResources* OpenGL33Renderer::GetMeshResources() noexcept {
    return IsInitialized() ? impl_->renderer.GetMeshResources() : nullptr;
}

Aero::Render::Detail::ImageResources* OpenGL33Renderer::GetImageResources() noexcept {
    return IsInitialized() ? impl_->renderer.GetImageResources() : nullptr;
}

Base::Result<void> OpenGL33Renderer::RenderOffscreen(
    const void* rendererToken,
    const Integration::RenderFrame& plan) noexcept {
    if (!IsInitialized()) {
        return NotInitialized(
            "OpenGL render adapter is not initialized");
    }
    if (device_->Backend().IsDeviceLost()) {
        return InvalidState(
            "Cannot render offscreen on a lost OpenGL device");
    }
    Base::Result<std::uint32_t> collected =
        device_->CollectGarbage();
    if (!collected) return collected.GetStatus();
    Base::Result<Graphics::CommandList> recorded =
        impl_->renderer.RecordOffscreen(rendererToken, plan);
    if (!recorded) return recorded.GetStatus();
    if (recorded.Value().CommandCount() == 0U) return {};
    Base::Result<Graphics::FenceValue> submitted =
        device_->Submit(recorded.Value());
    if (!submitted) return submitted.GetStatus();
    impl_->lastSubmittedFence = submitted.Value();
    return {};
}

Base::Result<void> OpenGL33Renderer::Render(
    const void* rendererToken,
    const Integration::RenderFrame& plan,
    Graphics::LoadOperation load) noexcept {
    if (!IsInitialized()) {
        return NotInitialized(
            "OpenGL render adapter is not initialized");
    }
    if (device_->Backend().IsDeviceLost() ||
        surface_->State() != Graphics::SurfaceState::Ready) {
        return InvalidState(
            "Cannot render a RenderFrame to a lost OpenGL surface");
    }
    Base::Result<std::uint32_t> collected =
        device_->CollectGarbage();
    if (!collected) {
        return collected.GetStatus();
    }

    Base::Result<Graphics::SurfaceFrame> acquired =
        surface_->AcquireFrame();
    if (!acquired) {
        return acquired.GetStatus();
    }
    Graphics::SurfaceFrame frame = acquired.Value();
    if (frame.target.width == 0U ||
        frame.target.height == 0U ||
        (!frame.target.defaultFramebuffer &&
         frame.target.colorTarget == 0U)) {
        static_cast<void>(surface_->DiscardFrame(frame));
        return InvalidArgument(
            "OpenGL surface frame has no render target");
    }

    Graphics::OpenGL33ExternalRenderTargetDescriptor external;
    external.framebuffer = static_cast<Graphics::GlUInt>(
        frame.target.colorTarget);
    external.depthStencilTexture = static_cast<Graphics::GlUInt>(
        frame.target.depthStencilTarget);
    external.texture.width = frame.target.width;
    external.texture.height = frame.target.height;
    external.texture.format = frame.target.colorFormat;
    external.texture.sampleCount = frame.target.sampleCount;
    external.texture.usage =
        Graphics::TextureUsageBit(Graphics::TextureUsage::RenderTarget);
    external.contextGeneration = contextGeneration_;
    external.stableId = frame.target.stableId;
    external.defaultFramebuffer =
        frame.target.defaultFramebuffer;
    Base::Result<Graphics::ResourceHandle> imported =
        Graphics::ImportOpenGL33ExternalRenderTarget(
            *device_, *graphicsBackend_, external);
    if (!imported) {
        static_cast<void>(surface_->DiscardFrame(frame));
        return imported.GetStatus();
    }

    Base::Result<Graphics::CommandList> recorded =
        impl_->renderer.RecordOnscreen(
            rendererToken, plan,
            {imported.Value(),
             frame.target.width,
             frame.target.height,
             load});
    if (!recorded) {
        static_cast<void>(surface_->DiscardFrame(frame));
        static_cast<void>(
            device_->DestroyResource(imported.Value(), 0U));
        return recorded.GetStatus();
    }
    Base::Result<Graphics::FenceValue> submitted =
        device_->Submit(recorded.Value());
    if (!submitted) {
        static_cast<void>(surface_->DiscardFrame(frame));
        static_cast<void>(
            device_->DestroyResource(imported.Value(), 0U));
        return submitted.GetStatus();
    }
    impl_->lastSubmittedFence = submitted.Value();
    Base::Result<void> presented =
        surface_->Present(frame, submitted.Value());
    Base::Result<void> destroyed =
        device_->DestroyResource(
            imported.Value(), submitted.Value());
    if (!presented) {
        return presented;
    }
    if (!destroyed) {
        return destroyed;
    }
    return {};
}

void OpenGL33Renderer::ReleaseRenderer(
    const void* rendererToken) noexcept {
    if (impl_ != nullptr) {
        impl_->renderer.ReleaseRenderer(rendererToken);
    }
}

Graphics::FenceValue
OpenGL33Renderer::LastSubmittedFence() const noexcept {
    return impl_ != nullptr ? impl_->lastSubmittedFence : 0U;
}

OpenGL33RendererStatistics
OpenGL33Renderer::LastSubmitStatistics() const noexcept {
    return impl_ != nullptr
        ? impl_->renderer.LastStatistics()
        : OpenGL33RendererStatistics{};
}

void OpenGL33Renderer::SetBatchingEnabled(
    bool enabled) noexcept {
    batchingEnabled_ = enabled;
    if (impl_ != nullptr) {
        impl_->renderer.SetBatchingEnabled(
            enabled);
    }
}
#endif

} // namespace Aero::Render
