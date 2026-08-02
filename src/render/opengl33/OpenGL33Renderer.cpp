#include "OpenGL33Renderer.hpp"

#include <new>

#include "../MeshGpuResources.hpp"
#include "../ImageGpuResources.hpp"
#include "../TextGpuResources.hpp"

namespace Aero::Render {
namespace {

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
out vec4 outputColor;
void main() {
    vec2 fragmentPosition = vec2(
        gl_FragCoord.x, transform1.w - gl_FragCoord.y);
    for (uint index = 0u; index < clipCount; ++index) {
        vec2 relative = fragmentPosition - clipTranslation[index].xy;
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
out vec4 outputColor;
void main() {
    vec2 fragmentPosition = vec2(
        gl_FragCoord.x, transform1.w - gl_FragCoord.y);
    for (uint index = 0u; index < clipCount; ++index) {
        vec2 relative = fragmentPosition - clipTranslation[index].xy;
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
out vec4 outputColor;
void main() {
    vec2 fragmentPosition = vec2(
        gl_FragCoord.x, transform1.w - gl_FragCoord.y);
    for (uint index = 0u; index < clipCount; ++index) {
        vec2 relative = fragmentPosition - clipTranslation[index].xy;
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
out vec4 outputColor;
void main() {
    vec2 fragmentPosition = vec2(
        gl_FragCoord.x, transform1.w - gl_FragCoord.y);
    for (uint index = 0u; index < clipCount; ++index) {
        vec2 relative = fragmentPosition - clipTranslation[index].xy;
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
    outputColor = vertexTint;
    outputColor.a *= texture(AeroTexture0, textureCoordinate).r;
}
)GLSL";

template <std::size_t Size>
std::uint32_t ShaderSize(
    const std::uint8_t (&)[Size]) noexcept {
    return static_cast<std::uint32_t>(Size - 1U);
}

} // namespace

RendererShaderSet MakeOpenGL33RendererShaderSet() noexcept {
    RendererShaderSet shaders;
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

struct OpenGL33Renderer::Impl {
    Impl(
        Graphics::GraphicsDevice& device,
        std::uint64_t generation,
        Base::IAllocator* allocator) noexcept
        : renderer(
              device,
              MakeOpenGL33RendererShaderSet(),
              allocator),
          textResources(
              device, renderer, generation, *allocator),
          meshResources(
              device, renderer, generation, *allocator),
          imageResources(
              device, renderer, generation, *allocator) {}

    Renderer renderer;
    Detail::TextGpuResources textResources;
    Detail::MeshGpuResources meshResources;
    Detail::ImageGpuResources imageResources;
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
        ++textGeneration_;
        if (textGeneration_ == 0U) ++textGeneration_;
        impl_ = new (memory) Impl(
            *device_, textGeneration_, allocator_);
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
    impl_->textResources.Shutdown();
    impl_->meshResources.Shutdown();
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

Aero::Internal::TextResources* OpenGL33Renderer::GetTextResources() noexcept {
    return IsInitialized() ? &impl_->textResources.Table() : nullptr;
}

Aero::Internal::MeshResources* OpenGL33Renderer::GetMeshResources() noexcept {
    return IsInitialized() ? &impl_->meshResources.Table() : nullptr;
}

Aero::Internal::ImageResources* OpenGL33Renderer::GetImageResources() noexcept {
    return IsInitialized() ? &impl_->imageResources.Table() : nullptr;
}

Base::Result<void> OpenGL33Renderer::Submit(
    const Integration::RenderFrame& plan) noexcept {
    if (!IsInitialized()) {
        return NotInitialized(
            "OpenGL render adapter is not initialized");
    }
    if (device_->Backend().IsDeviceLost() ||
        surface_->State() != Graphics::SurfaceState::Ready) {
        return InvalidState(
            "Cannot submit a RenderFrame to a lost OpenGL surface");
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
        impl_->renderer.Record(
            plan,
            {imported.Value(),
             frame.target.width,
             frame.target.height});
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

} // namespace Aero::Render
