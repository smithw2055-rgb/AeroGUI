#include "render/RenderDeviceState.hpp"
#include "render/RenderTargetState.hpp"
#include "gui/ViewRenderer.hpp"
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

class OpenGL33TargetState final : public Aero::RenderTarget::Access {
public:
    OpenGL33TargetState(
        Graphics::OpenGL33RenderDevice& device,
        const OpenGL33EmbeddedTargetOptions& options,
        Base::IAllocator& allocator) noexcept
        : Aero::RenderTarget::Access(RenderTargetKind::Embedded),
          device_(&device),
          options_(options),
          allocator_(&allocator) {}

    ~OpenGL33TargetState() noexcept override = default;

    Base::Result<void> Initialize() noexcept {
        if (device_ == nullptr || !device_->IsReady() ||
            options_.acquireTarget == nullptr) {
            health_ = SurfaceHealth::Lost;
            return NotInitialized("OpenGL target requires a ready render device");
        }
        deviceGeneration_ = device_->BackendGeneration();
        health_ = SurfaceHealth::Ready;
        return {};
    }

    Base::Result<void> Render(
        ::Aero::ViewRenderer& renderer,
        const ::Aero::Render::RenderFrame& frame) noexcept override {
        if (!IsReady()) return InvalidState("OpenGL target is not ready");
        Base::Result<void> current = device_->MakeCurrent();
        if (!current) return current.GetStatus();

        ::Aero::Render::OpenGL33EmbeddedTarget target;
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

        Base::Result<Graphics::FenceValue> submitted =
            renderer.RenderOnscreenFrame(frame,
                 {imported.Value(), target.width, target.height,
                  options_.clearBeforeRender
                      ? Graphics::LoadOperation::Clear
                      : Graphics::LoadOperation::Load});
        const Graphics::FenceValue retireFence =
            device_->LastSubmittedFence();
        Base::Result<void> retired =
            static_cast<Aero::RenderDevice::Access&>(*device_).DestroyResource(
                imported.Value(), retireFence);
        if (!submitted) return submitted.GetStatus();
        return retired;
    }

    Base::Result<void> Resize(std::uint32_t, std::uint32_t) noexcept override {
        return {};
    }

    void NotifySurfaceLost() noexcept override {
        health_ = SurfaceHealth::Lost;
    }

    Base::Result<void> RestoreSurface() noexcept override {
        if (health_ != SurfaceHealth::Lost) {
            return InvalidState("Only a lost OpenGL target can be restored");
        }
        return Initialize();
    }

    SurfaceHealth GetSurfaceHealth() const noexcept override {
        if (device_ == nullptr) return SurfaceHealth::Shutdown;
        if (!device_->IsReady() ||
            deviceGeneration_ != device_->BackendGeneration()) {
            return SurfaceHealth::Lost;
        }
        return health_;
    }

private:
    bool IsReady() const noexcept {
        return GetSurfaceHealth() == SurfaceHealth::Ready &&
            device_->IsReady();
    }

    Graphics::OpenGL33RenderDevice* device_ = nullptr;
    OpenGL33EmbeddedTargetOptions options_;
    Base::IAllocator* allocator_ = nullptr;
    std::uint64_t deviceGeneration_ = 0U;
    SurfaceHealth health_ = SurfaceHealth::Shutdown;
};

Graphics::OpenGL33RenderDevice* DeviceStateFrom(
    const Base::Ref<Aero::RenderDevice>& device) noexcept {
    if (!device || Aero::RenderDevice::Access::Backend(*device) !=
            RenderBackendKind::OpenGL33) {
        return nullptr;
    }
    return static_cast<Graphics::OpenGL33RenderDevice*>(
        Aero::RenderDevice::Access::BackendState(*device));
}

} // namespace

Base::Result<Base::Ref<Aero::RenderDevice>> CreateOpenGL33Device(
    const ::Aero::Render::OpenGL33DeviceOptions& options,
    Base::IAllocator* allocator) noexcept {
    if (options.resolve == nullptr || options.makeCurrent == nullptr ||
        options.isCurrent == nullptr || options.contextGeneration == nullptr ||
        options.contextGeneration(options.callbackContext) == 0U) {
        return InvalidArgument("OpenGL device options are incomplete");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();
    auto* state = new (std::nothrow)
        Graphics::OpenGL33RenderDevice(options, &selected);
    if (state == nullptr) return OutOfMemory("Unable to allocate OpenGL render device");
    Base::Result<void> initialized = state->Initialize();
    if (!initialized) {
        delete state;
        return initialized.GetStatus();
    }
    return AdoptRenderDevice(state, &selected);
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
    auto* target = new (std::nothrow)
        OpenGL33TargetState(*state, options, selected);
    if (target == nullptr) return OutOfMemory("Unable to allocate OpenGL target state");
    Base::Result<void> initialized = target->Initialize();
    if (!initialized) {
        delete target;
        return initialized.GetStatus();
    }
    return AdoptRenderTarget(
        std::move(device), target, RenderTargetKind::Embedded, &selected);
}

} // namespace Aero::Render
