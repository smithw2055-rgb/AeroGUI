#include "render/RenderDeviceState.hpp"
#include "render/RenderTargetState.hpp"
#include "render/opengl33/OpenGL33RenderDevice.hpp"
#include "render/opengl33/OpenGL33Shaders.hpp"

#include <new>
#include <utility>

namespace Aero::Render {
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

class OpenGL33TargetState final : public RenderTargetBase {
public:
    OpenGL33TargetState(
        Base::Ref<Aero::RenderDevice> owner,
        Graphics::OpenGL33RenderDevice& device,
        const OpenGL33EmbeddedTargetOptions& options,
        Base::IAllocator& allocator) noexcept
        : RenderTargetBase(std::move(owner), RenderTargetKind::Embedded),
          device_(&device),
          options_(options),
          allocator_(&allocator) {}

    ~OpenGL33TargetState() noexcept override = default;

    Base::Result<void> Initialize() noexcept {
        if (device_ == nullptr || !device_->IsReady() ||
            options_.acquireTarget == nullptr) {
            health_ = RenderTargetState::Lost;
            return NotInitialized("OpenGL target requires a ready render device");
        }
        deviceGeneration_ = RenderDeviceBase::BackendGeneration(*device_);
        health_ = RenderTargetState::Ready;
        return {};
    }

    Base::Result<FrameTarget> AcquireFrameTarget() noexcept override {
        if (!IsReady()) return InvalidState("OpenGL target is not ready");
        Base::Result<void> current = device_->MakeCurrent();
        if (!current) return current.GetStatus();

        ::Aero::Render::OpenGL33::EmbeddedTarget target;
        void* targetContext = options_.targetContext != nullptr
            ? options_.targetContext
            : options_.callbackContext;
        Base::Status acquired = options_.acquireTarget(targetContext, &target);
        if (!acquired.IsOk()) return acquired;
        if (target.width == 0U || target.height == 0U ||
            (!target.defaultFramebuffer && target.framebuffer == 0U)) {
            return InvalidArgument("OpenGL embedded target is invalid");
        }

        Graphics::OpenGL33RenderTargetBinding external;
        external.framebuffer = static_cast<Graphics::GlUInt>(target.framebuffer);
        external.depthStencilTexture =
            static_cast<Graphics::GlUInt>(target.depthStencilTexture);
        external.texture.width = target.width;
        external.texture.height = target.height;
        external.texture.format = Graphics::GraphicsTextureFormat::Bgra8Unorm;
        external.texture.sampleCount = 1U;
        external.texture.usage =
            Graphics::TextureUsageBit(Graphics::TextureUsage::RenderTarget);
        external.contextGeneration = device_->ContextGeneration();
        external.stableId = target.stableId;
        external.defaultFramebuffer = target.defaultFramebuffer;
        Base::Result<Graphics::ResourceHandle> imported =
            Graphics::ImportOpenGL33ExternalRenderTarget(
                *device_, *device_, external);
        if (!imported) return imported.GetStatus();

        return FrameTarget{
            imported.Value(), target.width, target.height,
            options_.clearBeforeRender
                ? Graphics::LoadOperation::Clear
                : Graphics::LoadOperation::Load};
    }

    Base::Result<void> RetireFrameTarget(
        const FrameTarget& target) noexcept override {
        const Graphics::FenceValue retireFence =
            device_->LastSubmittedFence();
        return static_cast<RenderDeviceBase&>(*device_).DestroyResource(
            target.color, retireFence);
    }

    Base::Result<void> ResizeBackend(
        std::uint32_t, std::uint32_t) noexcept override {
        return {};
    }

    void NotifyBackendLost() noexcept override {
        health_ = RenderTargetState::Lost;
    }

    Base::Result<void> RestoreBackend() noexcept override {
        if (health_ != RenderTargetState::Lost) {
            return InvalidState("Only a lost OpenGL target can be restored");
        }
        return Initialize();
    }

    RenderTargetState BackendState() const noexcept override {
        if (device_ == nullptr) return RenderTargetState::Shutdown;
        if (!device_->IsReady() ||
            deviceGeneration_ != RenderDeviceBase::BackendGeneration(*device_)) {
            return RenderTargetState::Lost;
        }
        return health_;
    }

private:
    bool IsReady() const noexcept {
        return BackendState() == RenderTargetState::Ready &&
            device_->IsReady();
    }

    Graphics::OpenGL33RenderDevice* device_ = nullptr;
    OpenGL33EmbeddedTargetOptions options_;
    Base::IAllocator* allocator_ = nullptr;
    std::uint64_t deviceGeneration_ = 0U;
    RenderTargetState health_ = RenderTargetState::Shutdown;
};

Graphics::OpenGL33RenderDevice* DeviceStateFrom(
    const Base::Ref<Aero::RenderDevice>& device) noexcept {
    if (!device || RenderDeviceBase::Backend(*device) !=
            Aero::RenderBackendKind::OpenGL33) {
        return nullptr;
    }
    return static_cast<Graphics::OpenGL33RenderDevice*>(device.Get());
}

} // namespace

Base::Result<Base::Ref<Aero::RenderDevice>> OpenGL33::CreateDevice(
    const ::Aero::Render::OpenGL33::DeviceOptions& options,
    Base::IAllocator* allocator) noexcept {
    if (options.resolve == nullptr || options.makeCurrent == nullptr ||
        options.isCurrent == nullptr || options.contextGeneration == nullptr ||
        options.contextGeneration(options.callbackContext) == 0U) {
        return InvalidArgument("OpenGL device options are incomplete");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    Base::Result<Base::Ref<Graphics::OpenGL33RenderDevice>> made =
        Base::MakeRefWithAllocator<Graphics::OpenGL33RenderDevice>(
            selected, options, &selected);
    if (!made) return made.GetStatus();
    Base::Ref<Graphics::OpenGL33RenderDevice> state =
        std::move(made).Value();
    Base::Result<void> initialized = state->Initialize();
    if (!initialized) {
        return initialized.GetStatus();
    }
    return Base::Ref<Aero::RenderDevice>(std::move(state));
}

Base::Result<Base::Ref<Aero::RenderTarget>> CreateOpenGL33EmbeddedTarget(
    Base::Ref<Aero::RenderDevice> device,
    const OpenGL33EmbeddedTargetOptions& options,
    Base::IAllocator* allocator) noexcept {
    Graphics::OpenGL33RenderDevice* state = DeviceStateFrom(device);
    if (state == nullptr || options.acquireTarget == nullptr) {
        return InvalidArgument(
            "OpenGL embedded target requires a matching device and target callback");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    Base::Result<Base::Ref<OpenGL33TargetState>> made =
        Base::MakeRefWithAllocator<OpenGL33TargetState>(
            selected, device, *state, options, selected);
    if (!made) return made.GetStatus();
    Base::Ref<OpenGL33TargetState> target =
        std::move(made).Value();
    Base::Result<void> initialized = target->Initialize();
    if (!initialized) {
        return initialized.GetStatus();
    }
    return Base::Ref<Aero::RenderTarget>(std::move(target));
}

} // namespace Aero::Render
