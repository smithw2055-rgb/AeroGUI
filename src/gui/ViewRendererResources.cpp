#include "gui/ViewRenderer.hpp"
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
    static Render::RenderImageId nextId = 1000U;
    const Render::RenderImageId id = ++nextId;
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
    void*,
    Base::Span<const Aero::Point>,
    Base::Span<const std::uint32_t>) noexcept {
    static Render::RenderMeshId nextMeshId = 1000U;
    return ++nextMeshId;
}

void ReleaseMeshResource(void*, Render::RenderMeshId) noexcept {}

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

} // namespace Aero
