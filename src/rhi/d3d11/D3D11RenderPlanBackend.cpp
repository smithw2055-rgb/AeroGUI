#include <Aero/Rhi/D3D11Backend.hpp>

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Vector.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>

#include "AeroD3D11RenderPlanPixelShader.hpp"
#include "AeroD3D11RenderPlanVertexShader.hpp"

namespace Aero::Rhi {
namespace {

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotInitialized, message);
}

Base::Status OutOfMemory(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::OutOfMemory, message);
}

Base::Status Unsupported(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::Unsupported, message);
}

Core::Transform2D IdentityTransform() noexcept {
    return {};
}

Core::Transform2D Translation(double x, double y) noexcept {
    Core::Transform2D value;
    value.dx = x;
    value.dy = y;
    return value;
}

// Transforms use row-vector affine form: (x, y, 1) * M.
Core::Transform2D Compose(
    const Core::Transform2D& first,
    const Core::Transform2D& second) noexcept {
    Core::Transform2D output;
    output.m11 = first.m11 * second.m11 + first.m12 * second.m21;
    output.m12 = first.m11 * second.m12 + first.m12 * second.m22;
    output.m21 = first.m21 * second.m11 + first.m22 * second.m21;
    output.m22 = first.m21 * second.m12 + first.m22 * second.m22;
    output.dx = first.dx * second.m11 + first.dy * second.m21 + second.dx;
    output.dy = first.dx * second.m12 + first.dy * second.m22 + second.dy;
    return output;
}

void TransformPoint(
    const Core::Transform2D& transform,
    double x,
    double y,
    double& outputX,
    double& outputY) noexcept {
    outputX = x * transform.m11 + y * transform.m21 + transform.dx;
    outputY = x * transform.m12 + y * transform.m22 + transform.dy;
}

Core::Rect TransformBounds(
    const Core::Transform2D& transform,
    Core::Rect rect) noexcept {
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
    double x3 = 0.0;
    double y3 = 0.0;
    TransformPoint(transform, rect.x, rect.y, x0, y0);
    TransformPoint(transform, rect.x + rect.width, rect.y, x1, y1);
    TransformPoint(transform, rect.x, rect.y + rect.height, x2, y2);
    TransformPoint(transform, rect.x + rect.width, rect.y + rect.height, x3, y3);
    const double minimumX = std::fmin(std::fmin(x0, x1), std::fmin(x2, x3));
    const double minimumY = std::fmin(std::fmin(y0, y1), std::fmin(y2, y3));
    const double maximumX = std::fmax(std::fmax(x0, x1), std::fmax(x2, x3));
    const double maximumY = std::fmax(std::fmax(y0, y1), std::fmax(y2, y3));
    return {minimumX, minimumY, maximumX - minimumX, maximumY - minimumY};
}

Core::Rect IntersectRect(Core::Rect left, Core::Rect right) noexcept {
    const double x0 = std::fmax(left.x, right.x);
    const double y0 = std::fmax(left.y, right.y);
    const double x1 = std::fmin(left.x + left.width, right.x + right.width);
    const double y1 = std::fmin(left.y + left.height, right.y + right.height);
    return {x0, y0, std::fmax(0.0, x1 - x0), std::fmax(0.0, y1 - y0)};
}

bool IsEmpty(Core::Rect rect) noexcept {
    return rect.width <= 0.0 || rect.height <= 0.0;
}

bool FitsFloat(double value) noexcept {
    return std::isfinite(value) &&
        value >= -static_cast<double>(std::numeric_limits<float>::max()) &&
        value <= static_cast<double>(std::numeric_limits<float>::max());
}

constexpr std::uint32_t MaxShaderClips = 32U;

struct ClipState final {
    Core::Rect rect;
    Core::Transform2D transform;
    Core::Rect bounds;
};

struct ShaderRectConstants final {
    float rect[4]{};
    float color[4]{};
    float transform0[4]{};
    float transform1[4]{};
    float clipRect[MaxShaderClips][4]{};
    float clipInverse[MaxShaderClips][4]{};
    float clipTranslation[MaxShaderClips][4]{};
    std::uint32_t clipCount = 0U;
    std::uint32_t instanceMode = 0U;
    float strokeThickness = 0.0F;
    float padding = 0.0F;
};

static_assert(sizeof(ShaderRectConstants) % 16U == 0U,
    "D3D11 constant buffers must be float4 aligned");

Base::Result<void> PushClipState(
    Base::Vector<ClipState>& clips,
    Core::Rect rect,
    const Core::Transform2D& transform) noexcept {
    if (clips.Size() >= MaxShaderClips) {
        return Unsupported("D3D11 RenderPlan clip nesting exceeds shader capacity");
    }
    const double determinant = transform.m11 * transform.m22 -
        transform.m12 * transform.m21;
    if (!std::isfinite(determinant) || std::fabs(determinant) < 1.0e-12) {
        return Unsupported("D3D11 RenderPlan cannot clip through a singular transform");
    }
    Core::Rect bounds = TransformBounds(transform, rect);
    if (!Core::IsValidLayoutRect(bounds)) {
        return InvalidArgument("D3D11 RenderPlan clip bounds are invalid");
    }
    if (!clips.Empty()) {
        bounds = IntersectRect(clips[clips.Size() - 1U].bounds, bounds);
    }
    return clips.TryPushBack({rect, transform, bounds});
}

struct NodeState final {
    Core::RenderNodeId id = Core::InvalidRenderNodeId;
    Core::Transform2D transform;
    ClipState clip;
    std::uint32_t parentIndex = UINT32_MAX;
};

Base::Result<void> AppendDraw(
    GraphicsCommandEncoder& encoder,
    ResourceHandle uniformBuffer,
    const ShaderRectConstants& constants,
    Core::Rect scissor,
    std::uint32_t instanceCount = 1U) noexcept {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&constants);
    Base::Result<void> uploaded = encoder.UploadBuffer(
        uniformBuffer, 0U, {bytes, static_cast<std::uint32_t>(sizeof(constants))});
    if (!uploaded) {
        return uploaded;
    }

    Base::Result<void> encoded = encoder.SetScissor(scissor);
    if (encoded) encoded = encoder.Draw(4U, instanceCount);
    return encoded;
}

} // namespace

struct D3D11RenderPlanBackend::Impl final {
    Impl(
        RhiDevice& device,
        D3D11GraphicsBackend& graphics,
        Base::IAllocator* allocator) noexcept
        : device(&device), graphics(&graphics), resources(device, graphics), nodes(allocator), transforms(allocator),
          clips(allocator), opacities(allocator), nodePath(allocator) {}

    RhiDevice* device = nullptr;
    D3D11GraphicsBackend* graphics = nullptr;
    GraphicsResourceFactory resources;
    ResourceHandle vertexBuffer;
    ResourceHandle uniformBuffer;
    ResourceHandle pipeline;
    Base::Vector<NodeState> nodes;
    Base::Vector<Core::Transform2D> transforms;
    Base::Vector<ClipState> clips;
    Base::Vector<double> opacities;
    Base::Vector<std::uint32_t> nodePath;
    FenceValue lastSubmittedFence = 0U;
    D3D11RenderPlanSubmitStatistics lastSubmitStatistics;
    bool initialized = false;
};

D3D11RenderPlanBackend::D3D11RenderPlanBackend(
    RhiDevice& device,
    D3D11GraphicsBackend& graphics,
    D3D11SurfacePresenter& presenter,
    Base::IAllocator* allocator) noexcept
    : device_(&device),
      graphics_(&graphics),
      presenter_(&presenter),
      allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()) {}

D3D11RenderPlanBackend::~D3D11RenderPlanBackend() noexcept {
    Shutdown();
}

Base::Result<void> D3D11RenderPlanBackend::Initialize() noexcept {
    if (impl_ != nullptr && impl_->initialized) {
        return {};
    }
    if (device_ == nullptr || graphics_ == nullptr || presenter_ == nullptr ||
        !graphics_->IsInitialized() || graphics_->IsDeviceLost()) {
        return NotInitialized("D3D11 RenderPlan backend requires a ready graphics backend");
    }
    if (impl_ == nullptr) {
        void* memory = allocator_->Allocate({
            sizeof(Impl), alignof(Impl), Base::MemoryTag::Render});
        if (memory == nullptr) {
            return OutOfMemory("Failed to allocate D3D11 RenderPlan backend state");
        }
        impl_ = new (memory) Impl(*device_, *graphics_, allocator_);
    }

    BufferDescriptor vertexDescriptor;
    vertexDescriptor.sizeBytes = 32U;
    vertexDescriptor.usage = BufferUsage::Vertex;
    Base::Result<ResourceHandle> vertex = impl_->resources.CreateBuffer(vertexDescriptor);
    if (!vertex) {
        return vertex.GetStatus();
    }
    impl_->vertexBuffer = vertex.Value();

    BufferDescriptor uniformDescriptor;
    uniformDescriptor.sizeBytes = sizeof(ShaderRectConstants);
    uniformDescriptor.usage = BufferUsage::Uniform;
    Base::Result<ResourceHandle> uniform = impl_->resources.CreateBuffer(uniformDescriptor);
    if (!uniform) {
        Shutdown();
        return uniform.GetStatus();
    }
    impl_->uniformBuffer = uniform.Value();

    PipelineDescriptor pipelineDescriptor;
    pipelineDescriptor.vertexShader.stage = ShaderStage::Vertex;
    pipelineDescriptor.vertexShader.language = ShaderLanguage::Dxbc;
    pipelineDescriptor.vertexShader.bytecode = AeroD3D11RenderPlanVertexShader;
    pipelineDescriptor.vertexShader.bytecodeSize = sizeof(AeroD3D11RenderPlanVertexShader);
    pipelineDescriptor.vertexShader.entryPoint = Base::StringView("vs_main");
    pipelineDescriptor.vertexShader.stableId = UINT64_C(0xD3111001);
    pipelineDescriptor.fragmentShader.stage = ShaderStage::Fragment;
    pipelineDescriptor.fragmentShader.language = ShaderLanguage::Dxbc;
    pipelineDescriptor.fragmentShader.bytecode = AeroD3D11RenderPlanPixelShader;
    pipelineDescriptor.fragmentShader.bytecodeSize = sizeof(AeroD3D11RenderPlanPixelShader);
    pipelineDescriptor.fragmentShader.entryPoint = Base::StringView("ps_main");
    pipelineDescriptor.fragmentShader.stableId = UINT64_C(0xD3111002);
    pipelineDescriptor.vertexLayout.bufferCount = 1U;
    pipelineDescriptor.vertexLayout.attributeCount = 1U;
    pipelineDescriptor.vertexLayout.buffers[0].stride = 8U;
    pipelineDescriptor.vertexLayout.attributes[0].location = 0U;
    pipelineDescriptor.vertexLayout.attributes[0].bufferSlot = 0U;
    pipelineDescriptor.vertexLayout.attributes[0].format = VertexFormat::Float2;
    pipelineDescriptor.vertexLayout.attributes[0].offset = 0U;
    pipelineDescriptor.topology = PrimitiveTopology::TriangleStrip;
    pipelineDescriptor.blend.enabled = true;
    pipelineDescriptor.blend.color.source = BlendFactor::SourceAlpha;
    pipelineDescriptor.blend.color.destination = BlendFactor::OneMinusSourceAlpha;
    pipelineDescriptor.blend.alpha.source = BlendFactor::One;
    pipelineDescriptor.blend.alpha.destination = BlendFactor::OneMinusSourceAlpha;
    pipelineDescriptor.colorFormat = GraphicsTextureFormat::Bgra8Unorm;
    pipelineDescriptor.raster.scissorEnabled = true;
    Base::Result<ResourceHandle> pipeline = impl_->resources.CreatePipeline(pipelineDescriptor);
    if (!pipeline) {
        Shutdown();
        return pipeline.GetStatus();
    }
    impl_->pipeline = pipeline.Value();
    impl_->initialized = true;
    return {};
}

void D3D11RenderPlanBackend::Shutdown() noexcept {
    if (impl_ == nullptr) {
        return;
    }
    const FenceValue retireFence = graphics_ != nullptr
        ? graphics_->LastSubmittedFence()
        : 0U;
    if (device_ != nullptr) {
        if (impl_->pipeline.IsValid()) {
            static_cast<void>(device_->DestroyResource(impl_->pipeline, retireFence));
        }
        if (impl_->uniformBuffer.IsValid()) {
            static_cast<void>(device_->DestroyResource(impl_->uniformBuffer, retireFence));
        }
        if (impl_->vertexBuffer.IsValid()) {
            static_cast<void>(device_->DestroyResource(impl_->vertexBuffer, retireFence));
        }
    }
    impl_->~Impl();
    allocator_->Deallocate(impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Render);
    impl_ = nullptr;
}

bool D3D11RenderPlanBackend::IsInitialized() const noexcept {
    return impl_ != nullptr && impl_->initialized;
}

FenceValue D3D11RenderPlanBackend::LastSubmittedFence() const noexcept {
    return impl_ != nullptr ? impl_->lastSubmittedFence : 0U;
}

D3D11RenderPlanSubmitStatistics
D3D11RenderPlanBackend::LastSubmitStatistics() const noexcept {
    return impl_ != nullptr ? impl_->lastSubmitStatistics
                            : D3D11RenderPlanSubmitStatistics{};
}

Base::Result<void> D3D11RenderPlanBackend::Submit(
    const Core::RenderPlan& plan) noexcept {
    if (!IsInitialized()) {
        return NotInitialized("D3D11 RenderPlan backend is not initialized");
    }
    if (graphics_->IsDeviceLost()) {
        return InvalidState("Cannot submit a RenderPlan to a lost D3D11 device");
    }

    Base::Result<D3D11SurfaceFrame> acquired = presenter_->AcquireFrame();
    if (!acquired) {
        return acquired.GetStatus();
    }
    D3D11SurfaceFrame frame = acquired.Value();
    const std::uint32_t width = frame.surface.target.width;
    const std::uint32_t height = frame.surface.target.height;
    if (width == 0U || height == 0U) {
        static_cast<void>(presenter_->DiscardFrame(frame));
        return InvalidArgument("D3D11 surface frame has an empty render target");
    }

    GraphicsCommandEncoder encoder(allocator_);
    static constexpr float UnitQuad[] = {
        0.0F, 0.0F, 1.0F, 0.0F,
        0.0F, 1.0F, 1.0F, 1.0F};
    const auto* vertexBytes = reinterpret_cast<const std::uint8_t*>(UnitQuad);
    Base::Result<void> encoded = encoder.UploadBuffer(
        impl_->vertexBuffer, 0U,
        {vertexBytes, static_cast<std::uint32_t>(sizeof(UnitQuad))});
    if (!encoded) {
        static_cast<void>(presenter_->DiscardFrame(frame));
        return encoded;
    }
    RenderPassDescriptor pass;
    pass.renderArea = {
        0.0, 0.0, static_cast<double>(width), static_cast<double>(height)};
    pass.colorAttachmentCount = 1U;
    pass.colorAttachments[0].target = frame.renderTarget;
    pass.colorAttachments[0].load = LoadOperation::Clear;
    pass.colorAttachments[0].store = StoreOperation::Store;
    pass.colorAttachments[0].clearColor = {0.0F, 0.0F, 0.0F, 0.0F};
    encoded = encoder.BeginRenderPass(pass);
    if (!encoded) {
        static_cast<void>(presenter_->DiscardFrame(frame));
        return encoded;
    }
    D3D11RenderPlanSubmitStatistics submissionStatistics;
    submissionStatistics.renderPassCount = 1U;
    encoded = encoder.BindPipeline(impl_->pipeline);
    if (encoded) {
        submissionStatistics.pipelineBindingCount = 1U;
        encoded = encoder.BindVertexBuffer(0U, impl_->vertexBuffer);
    }
    if (encoded) {
        submissionStatistics.vertexBufferBindingCount = 1U;
        encoded = encoder.BindUniformBuffer(
            0U, impl_->uniformBuffer, 0U,
            static_cast<std::uint32_t>(sizeof(ShaderRectConstants)));
    }
    if (encoded) {
        submissionStatistics.uniformBufferBindingCount = 1U;
    } else {
        static_cast<void>(presenter_->DiscardFrame(frame));
        return encoded;
    }

    impl_->nodes.Clear();
    Core::RenderNodeId previousId = Core::InvalidRenderNodeId;
    const Core::Rect targetClip = {
        0.0, 0.0, static_cast<double>(width), static_cast<double>(height)};
    const Base::Span<const Core::RenderCommand> commands = plan.Commands();
    for (const Core::RenderNodeSnapshot& node : plan.Nodes()) {
        if (node.id == Core::InvalidRenderNodeId || node.id <= previousId ||
            !Core::IsValidLayoutRect(node.layoutSlot) ||
            !Core::IsValidLayoutRect(node.clip) ||
            !Core::IsValidLayoutSize(node.renderSize) ||
            node.commandOffset > commands.Size() ||
            node.commandCount > commands.Size() - node.commandOffset) {
            encoded = InvalidArgument("D3D11 RenderPlan contains an invalid node snapshot");
            break;
        }
        previousId = node.id;

        Core::Transform2D parentTransform = IdentityTransform();
        Core::Rect parentClip = targetClip;
        std::uint32_t parentIndex = UINT32_MAX;
        if (node.parentId != Core::InvalidRenderNodeId) {
            const NodeState* parent = nullptr;
            for (std::uint32_t index = impl_->nodes.Size(); index > 0U; --index) {
                const NodeState& candidate = impl_->nodes[index - 1U];
                if (candidate.id == node.parentId) {
                    parent = &candidate;
                    parentIndex = index - 1U;
                    break;
                }
            }
            if (parent == nullptr) {
                encoded = InvalidState("D3D11 RenderPlan node parent is unavailable");
                break;
            }
            parentTransform = parent->transform;
            parentClip = parent->clip.bounds;
        }

        const Core::Transform2D nodeTransform = Compose(
            Translation(node.layoutSlot.x, node.layoutSlot.y), parentTransform);
        const Core::Rect nodeBounds = TransformBounds(parentTransform, node.clip);
        if (!Core::IsValidLayoutRect(nodeBounds)) {
            encoded = InvalidArgument("D3D11 RenderPlan node clip bounds are invalid");
            break;
        }
        ClipState nodeClip{node.clip, parentTransform,
            IntersectRect(parentClip, nodeBounds)};
        Base::Result<void> appendedNode = impl_->nodes.TryPushBack(
            {node.id, nodeTransform, nodeClip, parentIndex});
        if (!appendedNode) {
            encoded = appendedNode;
            break;
        }

        impl_->transforms.Clear();
        impl_->clips.Clear();
        impl_->opacities.Clear();
        impl_->nodePath.Clear();
        std::uint32_t nodePathIndex = impl_->nodes.Size() - 1U;
        while (true) {
            if (impl_->nodePath.Size() >= MaxShaderClips) {
                encoded = Unsupported(
                    "D3D11 RenderPlan layout clip nesting exceeds shader capacity");
                break;
            }
            Base::Result<void> pathAppended = impl_->nodePath.TryPushBack(nodePathIndex);
            if (!pathAppended) {
                encoded = pathAppended;
                break;
            }
            const std::uint32_t nextParent =
                impl_->nodes[nodePathIndex].parentIndex;
            if (nextParent == UINT32_MAX) {
                break;
            }
            nodePathIndex = nextParent;
        }
        if (!encoded) {
            break;
        }
        for (std::uint32_t index = impl_->nodePath.Size(); index > 0U; --index) {
            Base::Result<void> pushed = impl_->clips.TryPushBack(
                impl_->nodes[impl_->nodePath[index - 1U]].clip);
            if (!pushed) {
                encoded = pushed;
                break;
            }
        }
        if (!encoded) {
            break;
        }
        if (!(impl_->transforms.TryPushBack(nodeTransform)) ||
            !(impl_->opacities.TryPushBack(1.0))) {
            encoded = OutOfMemory("Failed to allocate D3D11 RenderPlan state stack");
            break;
        }
        const std::uint32_t baseClipCount = impl_->clips.Size();

        for (std::uint32_t commandIndex = 0U;
             commandIndex < node.commandCount;
             ++commandIndex) {
            const Core::RenderCommand& command =
                commands[node.commandOffset + commandIndex];
            switch (command.kind) {
            case Core::RenderCommandKind::PushClip: {
                if (!Core::IsValidLayoutRect(command.rect)) {
                    encoded = InvalidArgument("D3D11 RenderPlan contains an invalid clip");
                    break;
                }
                Base::Result<void> pushed = PushClipState(
                    impl_->clips, command.rect,
                    impl_->transforms[impl_->transforms.Size() - 1U]);
                if (!pushed) encoded = pushed;
                break;
            }
            case Core::RenderCommandKind::PopClip:
                if (impl_->clips.Size() <= baseClipCount) {
                    encoded = InvalidState("D3D11 RenderPlan clip stack underflow");
                } else {
                    impl_->clips.PopBack();
                }
                break;
            case Core::RenderCommandKind::PushOpacity: {
                if (!Core::IsValidOpacity(command.scalar)) {
                    encoded = InvalidArgument("D3D11 RenderPlan contains invalid opacity");
                    break;
                }
                Base::Result<void> pushed = impl_->opacities.TryPushBack(
                    impl_->opacities[impl_->opacities.Size() - 1U] * command.scalar);
                if (!pushed) encoded = pushed;
                break;
            }
            case Core::RenderCommandKind::PopOpacity:
                if (impl_->opacities.Size() <= 1U) {
                    encoded = InvalidState("D3D11 RenderPlan opacity stack underflow");
                } else {
                    impl_->opacities.PopBack();
                }
                break;
            case Core::RenderCommandKind::PushTransform: {
                if (!Core::IsFinite(command.transform)) {
                    encoded = InvalidArgument("D3D11 RenderPlan contains an invalid transform");
                    break;
                }
                Base::Result<void> pushed = impl_->transforms.TryPushBack(Compose(
                    command.transform,
                    impl_->transforms[impl_->transforms.Size() - 1U]));
                if (!pushed) encoded = pushed;
                break;
            }
            case Core::RenderCommandKind::PopTransform:
                if (impl_->transforms.Size() <= 1U) {
                    encoded = InvalidState("D3D11 RenderPlan transform stack underflow");
                } else {
                    impl_->transforms.PopBack();
                }
                break;
            case Core::RenderCommandKind::FillRect:
            case Core::RenderCommandKind::StrokeRect: {
                if (!Core::IsValidLayoutRect(command.rect) ||
                    !Core::IsFinite(command.color) ||
                    (command.kind == Core::RenderCommandKind::StrokeRect &&
                     (!std::isfinite(command.scalar) || command.scalar < 0.0))) {
                    encoded = InvalidArgument("D3D11 RenderPlan contains invalid rectangle geometry");
                    break;
                }
                const Core::Rect clip =
                    impl_->clips[impl_->clips.Size() - 1U].bounds;
                if (IsEmpty(clip) || IsEmpty(command.rect)) {
                    break;
                }
                const Core::Transform2D& transform =
                    impl_->transforms[impl_->transforms.Size() - 1U];
                const double opacity = impl_->opacities[impl_->opacities.Size() - 1U];
                if (!FitsFloat(command.rect.x) || !FitsFloat(command.rect.y) ||
                    !FitsFloat(command.rect.width) || !FitsFloat(command.rect.height) ||
                    !FitsFloat(transform.m11) || !FitsFloat(transform.m12) ||
                    !FitsFloat(transform.m21) || !FitsFloat(transform.m22) ||
                    !FitsFloat(transform.dx) || !FitsFloat(transform.dy) ||
                    !FitsFloat(opacity)) {
                    encoded = InvalidArgument("D3D11 RenderPlan values exceed shader precision");
                    break;
                }
                auto appendRectangle = [&](
                    Core::Rect rect,
                    std::uint32_t instanceCount = 1U,
                    float strokeThickness = 0.0F) noexcept -> Base::Result<void> {
                    ShaderRectConstants constants;
                    constants.rect[0] = static_cast<float>(rect.x);
                    constants.rect[1] = static_cast<float>(rect.y);
                    constants.rect[2] = static_cast<float>(rect.width);
                    constants.rect[3] = static_cast<float>(rect.height);
                    constants.color[0] = command.color.red;
                    constants.color[1] = command.color.green;
                    constants.color[2] = command.color.blue;
                    constants.color[3] = static_cast<float>(command.color.alpha * opacity);
                    constants.transform0[0] = static_cast<float>(transform.m11);
                    constants.transform0[1] = static_cast<float>(transform.m12);
                    constants.transform0[2] = static_cast<float>(transform.m21);
                    constants.transform0[3] = static_cast<float>(transform.m22);
                    constants.transform1[0] = static_cast<float>(transform.dx);
                    constants.transform1[1] = static_cast<float>(transform.dy);
                    constants.transform1[2] = static_cast<float>(width);
                    constants.transform1[3] = static_cast<float>(height);
                    constants.clipCount = impl_->clips.Size();
                    constants.instanceMode = instanceCount == 4U ? 1U : 0U;
                    constants.strokeThickness = strokeThickness;
                    for (std::uint32_t clipIndex = 0U;
                         clipIndex < impl_->clips.Size();
                         ++clipIndex) {
                        const ClipState& clipState = impl_->clips[clipIndex];
                        const Core::Transform2D& clipTransform =
                            clipState.transform;
                        const double determinant =
                            clipTransform.m11 * clipTransform.m22 -
                            clipTransform.m12 * clipTransform.m21;
                        const double inverseM11 = clipTransform.m22 / determinant;
                        const double inverseM12 = -clipTransform.m12 / determinant;
                        const double inverseM21 = -clipTransform.m21 / determinant;
                        const double inverseM22 = clipTransform.m11 / determinant;
                        if (!FitsFloat(clipState.rect.x) ||
                            !FitsFloat(clipState.rect.y) ||
                            !FitsFloat(clipState.rect.width) ||
                            !FitsFloat(clipState.rect.height) ||
                            !FitsFloat(inverseM11) || !FitsFloat(inverseM12) ||
                            !FitsFloat(inverseM21) || !FitsFloat(inverseM22) ||
                            !FitsFloat(clipTransform.dx) ||
                            !FitsFloat(clipTransform.dy)) {
                            return InvalidArgument(
                                "D3D11 RenderPlan clip values exceed shader precision");
                        }
                        constants.clipRect[clipIndex][0] =
                            static_cast<float>(clipState.rect.x);
                        constants.clipRect[clipIndex][1] =
                            static_cast<float>(clipState.rect.y);
                        constants.clipRect[clipIndex][2] =
                            static_cast<float>(clipState.rect.width);
                        constants.clipRect[clipIndex][3] =
                            static_cast<float>(clipState.rect.height);
                        constants.clipInverse[clipIndex][0] =
                            static_cast<float>(inverseM11);
                        constants.clipInverse[clipIndex][1] =
                            static_cast<float>(inverseM12);
                        constants.clipInverse[clipIndex][2] =
                            static_cast<float>(inverseM21);
                        constants.clipInverse[clipIndex][3] =
                            static_cast<float>(inverseM22);
                        constants.clipTranslation[clipIndex][0] =
                            static_cast<float>(clipTransform.dx);
                        constants.clipTranslation[clipIndex][1] =
                            static_cast<float>(clipTransform.dy);
                    }
                    Base::Result<void> result = AppendDraw(
                        encoder, impl_->uniformBuffer, constants, clip,
                        instanceCount);
                    if (result) {
                        ++submissionStatistics.drawCallCount;
                        submissionStatistics.rectangleInstanceCount += instanceCount;
                        ++submissionStatistics.uniformBufferUploadCount;
                    }
                    return result;
                };

                if (command.kind == Core::RenderCommandKind::FillRect ||
                    command.scalar == 0.0 ||
                    command.scalar * 2.0 >= std::fmin(command.rect.width, command.rect.height)) {
                    encoded = appendRectangle(command.rect);
                } else {
                    // Border segments share transform, opacity, and clip state,
                    // so D3D11 emits them as one four-instance draw.
                    encoded = appendRectangle(
                        command.rect, 4U, static_cast<float>(command.scalar));
                }
                break;
            }
            }
            if (!encoded) {
                break;
            }
        }
        if (!encoded || impl_->transforms.Size() != 1U ||
            impl_->clips.Size() != baseClipCount || impl_->opacities.Size() != 1U) {
            if (encoded) {
                encoded = InvalidState("D3D11 RenderPlan node has unbalanced state stacks");
            }
            break;
        }
    }

    if (encoded) {
        encoded = encoder.EndRenderPass();
    }
    Base::Result<GraphicsCommandBuffer> finished = encoded
        ? encoder.Finish()
        : Base::Result<GraphicsCommandBuffer>(encoded.GetStatus());
    if (!finished) {
        static_cast<void>(presenter_->DiscardFrame(frame));
        return finished.GetStatus();
    }
    Base::Result<FenceValue> submitted = presenter_->SubmitAndPresent(frame, finished.Value());
    if (!submitted) {
        return submitted.GetStatus();
    }
    impl_->lastSubmittedFence = submitted.Value();
    impl_->lastSubmitStatistics = submissionStatistics;
    return {};
}

} // namespace Aero::Rhi
