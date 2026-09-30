#include "gui/ViewRenderer.hpp"
#include "gui/ViewFrame.hpp"
#include "gui/text/TextPipeline.hpp"
#include "render/RenderTree.hpp"
#include <thread>
#include <new>

namespace Aero {

namespace {

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotInitialized, message);
}

Base::Status WrongThread(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::WrongThread, message);
}

Base::Status DeviceUnavailable(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

// Text resource callbacks
Base::Result<::Aero::Controls::TextBlockLayout*> CreateTextLayout(
    void* context,
    Text::FontManager& fonts,
    const Render::TextConfig& config,
    Base::IAllocator& allocator) noexcept {
    auto* renderer = static_cast<ViewRenderer*>(context);
    if (renderer == nullptr || !renderer->Device()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Renderer or device is null");
    }
    auto* layout = new (std::nothrow) Render::TextRenderer(
        fonts, *renderer->Device(),
        renderer->FrameEncoder(),
        &allocator);
    if (layout == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::OutOfMemory, "Failed to allocate text layout");
    }
    Base::Result<void> init = layout->Initialize(config);
    if (!init) {
        delete layout;
        return init.GetStatus();
    }
    return layout;
}

void DestroyTextLayout(void*, ::Aero::Controls::TextBlockLayout* layout) noexcept {
    delete static_cast<Render::TextRenderer*>(layout);
}

Base::Result<std::uint32_t> CollectTextLayout(void*, ::Aero::Controls::TextBlockLayout* layout) noexcept {
    if (layout != nullptr) {
        return static_cast<Render::TextRenderer*>(layout)->CollectGarbage();
    }
    return 0U;
}

// Image resource callbacks
Base::Result<Render::RenderImageId> CreateImageResource(
    void* context,
    std::uint32_t width,
    std::uint32_t height,
    Base::Span<const std::uint8_t> pixels) noexcept {
    auto* renderer = static_cast<ViewRenderer*>(context);
    if (renderer == nullptr || !renderer->Device() || renderer->FrameEncoder() == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Renderer is null or uninitialized");
    }
    const Render::RenderImageId id = Render::RenderIdAllocator::AllocateImageId();
    const void* data = pixels.Data();
    Ref<Texture> tex = renderer->Device()->CreateTexture(
        "ImageResource", width, height, 1, TextureFormat::RGBA8, pixels.Empty() ? nullptr : &data);
    if (!tex) {
        return Base::Status::Failure(Base::ErrorCode::InternalError, "Failed to create texture for image");
    }
    Base::Result<void> reg = renderer->FrameEncoder()->RegisterImage(id, std::move(tex));
    if (!reg) return reg.GetStatus();
    return id;
}

void ReleaseImageResource(void* context, Render::RenderImageId id) noexcept {
    auto* renderer = static_cast<ViewRenderer*>(context);
    ::Aero::Render::UiFrameEncoder* encoder = renderer != nullptr ? renderer->FrameEncoder() : nullptr;
    if (encoder != nullptr) {
        encoder->UnregisterImage(id);
    }
}

// Mesh resource callbacks
Base::Result<Render::RenderMeshId> CreateMeshResource(
    void* context,
    Base::Span<const Aero::Point> vertices,
    Base::Span<const std::uint32_t> indices) noexcept {
    auto* renderer = static_cast<ViewRenderer*>(context);
    if (renderer == nullptr || renderer->FrameEncoder() == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Renderer is null or uninitialized");
    }
    const Render::RenderMeshId meshId = Render::RenderIdAllocator::AllocateMeshId();
    Base::Result<void> reg = renderer->FrameEncoder()->RegisterMesh(meshId, vertices, indices);
    if (!reg) return reg.GetStatus();
    return meshId;
}

void ReleaseMeshResource(void* context, Render::RenderMeshId meshId) noexcept {
    auto* renderer = static_cast<ViewRenderer*>(context);
    ::Aero::Render::UiFrameEncoder* encoder = renderer != nullptr ? renderer->FrameEncoder() : nullptr;
    if (encoder != nullptr) {
        encoder->UnregisterMesh(meshId);
    }
}

} // namespace

Base::Result<void> ViewRenderer::InitializeRenderResources(
    RenderDevice& device,
    std::uint64_t generation) noexcept {
    if (frameEncoder_.has_value() && frameEncoder_->IsInitialized()) {
        return renderThread_ == std::this_thread::get_id()
            ? Base::Result<void>{}
            : Base::Result<void>(WrongThread(
                  "ViewRenderer resources must stay on their owning render thread"));
    }
    if (generation == 0U || allocator_ == nullptr) {
        return NotInitialized(
            "ViewRenderer requires a ready graphics device and generation");
    }

    frameEncoder_.emplace(device, allocator_);
    Base::Result<void> initialized = frameEncoder_->Initialize();
    if (!initialized) {
        ShutdownRenderResources();
        return initialized;
    }

    textResources_.generation = generation;
    textResources_.context = this;
    textResources_.create = &CreateTextLayout;
    textResources_.destroy = &DestroyTextLayout;
    textResources_.collect = &CollectTextLayout;

    imageResources_.generation = generation;
    imageResources_.context = this;
    imageResources_.create = &CreateImageResource;
    imageResources_.release = &ReleaseImageResource;

    meshResources_.generation = generation;
    meshResources_.context = this;
    meshResources_.create = &CreateMeshResource;
    meshResources_.release = &ReleaseMeshResource;

    renderThread_ = std::this_thread::get_id();
    deviceGeneration_ = generation;
    return {};
}

void ViewRenderer::ShutdownRenderResources() noexcept {
    if (frameEncoder_.has_value()) {
        frameEncoder_->Shutdown();
        frameEncoder_.reset();
    }
    textResources_ = {};
    imageResources_ = {};
    meshResources_ = {};
    renderThread_ = {};
    deviceGeneration_ = 0U;
}

Base::Result<void> ViewRenderer::VerifyRenderResources() const noexcept {
    if (!frameEncoder_.has_value() || !frameEncoder_->IsInitialized()) {
        return NotInitialized("ViewRenderer resources are not initialized");
    }
    if (renderThread_ != std::this_thread::get_id()) {
        return WrongThread(
            "ViewRenderer must render from its owning render thread");
    }
    if (!device_ || device_->State() != RenderDeviceState::Ready) {
        return DeviceUnavailable(
            "ViewRenderer graphics device is unavailable");
    }
    return {};
}

Base::Result<void> ViewRenderer::RenderOffscreenFrame(
    const ::Aero::Render::RenderFrame& frame) noexcept {
    Base::Result<void> ready = VerifyRenderResources();
    if (!ready) return ready.GetStatus();
    return frameEncoder_->RecordOffscreen(frame);
}

Base::Result<void> ViewRenderer::RenderOnscreenFrame(
    const ::Aero::Render::RenderFrame& frame,
    RenderTarget& target) noexcept {
    Base::Result<void> ready = VerifyRenderResources();
    if (!ready) return ready.GetStatus();
    return frameEncoder_->RecordOnscreen(frame, target);
}

::Aero::Render::FrameStatistics
ViewRenderer::LastStatistics() const noexcept {
    return frameEncoder_.has_value() && frameEncoder_->IsInitialized()
        ? frameEncoder_->LastStatistics()
        : ::Aero::Render::FrameStatistics{};
}

::Aero::Render::RenderResources ViewRenderer::Resources() noexcept {
    if (frameEncoder_.has_value() && frameEncoder_->IsInitialized()) {
        return ::Aero::Render::RenderResources{
            &textResources_,
            &meshResources_,
            &imageResources_
        };
    }
    return {};
}

ViewRenderer::ViewRenderer(
    View& view,
    Base::IAllocator& allocator) noexcept
    : allocator_(&allocator), view_(&view) {}

ViewRenderer::~ViewRenderer() noexcept {
    Shutdown();
}

Base::Result<void> ViewRenderer::Init(
    Base::Ref<RenderDevice> device) noexcept {
    if (view_ == nullptr ||
        view_->state_ == nullptr ||
        !view_->state_->initialized) {
        return ViewNotInitialized(
            "Renderer requires an initialized View");
    }
    if (!device) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Renderer requires a RenderDevice");
    }
    if (initialized_) {
        return device_.Get() == device.Get()
            ? Base::Result<void>()
            : Base::Result<void>(Base::Status::Failure(
                  Base::ErrorCode::AlreadyExists,
                  "Renderer is already initialized"));
    }

    if (device->State() != RenderDeviceState::Ready) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Device is not ready");
    }

    auto& data = *view_->state_;
    Base::Ref<RenderDevice> previous =
        data.device;
    const bool changingDevice =
        previous.Get() != device.Get();
    if (changingDevice && previous) {
        Base::Result<void> idle =
            previous->WaitIdle();
        if (!idle) return idle.GetStatus();

        Aero::Render::ImageResources*
            previousImages = data.GetImageResources();
        if (data.images != nullptr) {
            data.images->ReleaseBackendResources(
                previousImages);
        }
        data.VisitTextElements(
            data.RootVisual(), nullptr);
        if (data.text != nullptr) {
            Base::Result<bool> detached =
                data.text->SynchronizeBackend(
                    *previous, nullptr, true);
            if (!detached) return detached.GetStatus();
        }
        data.VisitPaths(
            data.RootVisual(), nullptr);
        if (data.tree != nullptr) data.tree->SetMeshResources(nullptr);
        ShutdownRenderResources();
    }

    if (!frameEncoder_.has_value()) {
        Base::Result<void> prepared = InitializeRenderResources(
            *device,
            device->Generation());
        if (!prepared) {
            ShutdownRenderResources();
            return prepared.GetStatus();
        }
    }

    Base::Result<void> status;
    data.device = device;
    data.deviceGeneration =
        device->Generation();
    if (data.tree != nullptr) {
        data.tree->SetMeshResources(data.GetMeshResources());
    }
    data.VisitPaths(
        data.RootVisual(),
        data.GetMeshResources(),
        true);
    if (data.tree != nullptr) {
        data.tree->SetTextLayout(data.text != nullptr ? data.text->Layout() : nullptr);
    }
    data.VisitTextElements(
        data.RootVisual(),
        data.text != nullptr ? data.text->Layout() : nullptr,
        true);

    Aero::Media::Visual* rootVisual =
        data.RootVisual();
    if (rootVisual != nullptr) {
        status = data.RenderTree()->Invalidate(
            *rootVisual,
            Aero::Render::RenderInvalidation::All);
    }
    if (!status) {
        return status.GetStatus();
    }

    device_ = std::move(device);
    updatedVersion_ = 0U;
    renderedVersion_ = 0U;
    offscreenReady_ = false;
    initialized_ = true;
    return {};
}

void ViewRenderer::Shutdown() noexcept {
    if (device_) {
        static_cast<void>(device_->WaitIdle());
    }
    ShutdownRenderResources();
    device_.Reset();
    updatedVersion_ = 0U;
    renderedVersion_ = 0U;
    offscreenReady_ = false;
    initialized_ = false;
}

bool ViewRenderer::IsInitialized() const noexcept {
    return initialized_;
}

bool ViewRenderer::UpdateRenderTree() noexcept {
    if (!initialized_ || !device_ ||
        view_ == nullptr || view_->state_ == nullptr ||
        !view_->state_->initialized) {
        if (view_ != nullptr && view_->state_ != nullptr) {
            view_->state_->ReportRendererFailure(ViewNotInitialized(
                "Renderer must be initialized before UpdateRenderTree"));
        }
        return false;
    }

    if (device_->State() != RenderDeviceState::Ready) {
        view_->state_->ReportRendererFailure(ViewApiInvalidState(
            "Render device is not ready"));
        return false;
    }

    auto& data = *view_->state_;
    if (data.RenderTree() == nullptr) {
        data.ReportRendererFailure(ViewNotInitialized(
            "View render tree is unavailable"));
        return false;
    }
    const ::Aero::Render::RenderFrame& frame =
        data.RenderTree()->CurrentFrame();
    if (frame.Version() == 0U) {
        data.ClearRendererFailure();
        return false;
    }
    Base::Result<void> valid =
        ::Aero::Render::ValidateRenderFrame(frame);
    if (!valid) {
        data.ReportRendererFailure(valid.GetStatus());
        return false;
    }

    const bool changed =
        frame.Version() != updatedVersion_;
    if (changed) {
        updatedVersion_ = frame.Version();
        offscreenReady_ = false;
    }
    data.ClearRendererFailure();
    return changed;
}

bool ViewRenderer::RenderOffscreen() noexcept {
    if (!initialized_ || !device_ || !frameEncoder_.has_value() ||
        view_ == nullptr || view_->state_ == nullptr) {
        if (view_ != nullptr && view_->state_ != nullptr) {
            view_->state_->ReportRendererFailure(ViewNotInitialized(
                "Renderer must be initialized before RenderOffscreen"));
        }
        return false;
    }

    const ::Aero::Render::RenderFrame& frame =
        view_->state_->RenderTree()->CurrentFrame();
    if (frame.Version() == 0U) {
        offscreenReady_ = true;
        view_->state_->ClearRendererFailure();
        return true;
    }
    if (frame.PixelWidth() == 0U || frame.PixelHeight() == 0U) {
        offscreenReady_ = true;
        view_->state_->ClearRendererFailure();
        return true;
    }
    if (frame.Version() != updatedVersion_) {
        view_->state_->ReportRendererFailure(ViewApiInvalidState(
            "UpdateRenderTree must run before RenderOffscreen"));
        return false;
    }

    Base::Result<void> submitted =
        RenderOffscreenFrame(frame);
    if (!submitted) {
        view_->state_->ReportRendererFailure(submitted.GetStatus());
        return false;
    }
    offscreenReady_ = true;
    view_->state_->ClearRendererFailure();
    return true;
}

void ViewRenderer::Render(
    RenderTarget& target) noexcept {
    if (!initialized_ || !device_ || !frameEncoder_.has_value() ||
        view_ == nullptr || view_->state_ == nullptr) {
        if (view_ != nullptr && view_->state_ != nullptr) {
            view_->state_->ReportRendererFailure(ViewNotInitialized(
                "Renderer must be initialized before Render"));
        }
        return;
    }

    Base::Ref<RenderDevice> surfaceDevice = target.GetDevice();
    if (!surfaceDevice || surfaceDevice.Get() != device_.Get()) {
        view_->state_->ReportRendererFailure(ViewApiInvalidState(
            "RenderTarget must belong to the renderer RenderDevice"));
        return;
    }

    const ::Aero::Render::RenderFrame& frame =
        view_->state_->RenderTree()->CurrentFrame();
    if (frame.Version() == 0U) {
        view_->state_->ClearRendererFailure();
        return;
    }
    if (frame.Version() != updatedVersion_) {
        view_->state_->ReportRendererFailure(ViewApiInvalidState(
            "UpdateRenderTree must run before Render"));
        return;
    }
    if (!offscreenReady_) {
        view_->state_->ReportRendererFailure(ViewApiInvalidState(
            "RenderOffscreen must run before Render"));
        return;
    }
    if (frame.PixelWidth() == 0U || frame.PixelHeight() == 0U) {
        renderedVersion_ = frame.Version();
        offscreenReady_ = false;
        view_->state_->ClearRendererFailure();
        return;
    }

    Base::Result<void> submitted =
        RenderOnscreenFrame(frame, target);
    if (!submitted) {
        view_->state_->ReportRendererFailure(submitted.GetStatus());
        return;
    }

    renderedVersion_ = frame.Version();
    offscreenReady_ = false;
    view_->state_->ClearRendererFailure();
}


} // namespace Aero
