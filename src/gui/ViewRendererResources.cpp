#include "gui/ViewRenderer.hpp"
#include "render/RenderDeviceState.hpp"

#include <thread>

namespace Aero {
namespace {

Base::Status NotInitialized(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotInitialized, message);
}

Base::Status WrongThread(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::WrongThread, message);
}

Base::Status DeviceUnavailable(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

} // namespace

Base::Result<void> ViewRenderer::InitializeRenderResources(
    Render::RenderDeviceBase& device,
    std::uint64_t generation) noexcept {
    if (frameEncoder_.has_value() && frameEncoder_->IsInitialized()) {
        return renderThread_ == std::this_thread::get_id()
            ? Base::Result<void>{}
            : Base::Result<void>(WrongThread(
                  "ViewRenderer resources must stay on their owning render thread"));
    }
    if (!device.AreResourcesReady() || generation == 0U ||
        allocator_ == nullptr) {
        return NotInitialized(
            "ViewRenderer requires a ready graphics device and generation");
    }

    frameEncoder_.emplace(device, allocator_);
    Base::Result<void> initialized = frameEncoder_->Initialize();
    if (!initialized) {
        ShutdownRenderResources();
        return initialized;
    }
    textResources_.emplace(
        device, *frameEncoder_, generation, *allocator_);
    meshResources_.emplace(
        device, *frameEncoder_, generation, *allocator_);
    imageResources_.emplace(
        device, *frameEncoder_, generation, *allocator_);
    renderThread_ = std::this_thread::get_id();
    deviceGeneration_ = generation;
    return {};
}

void ViewRenderer::ShutdownRenderResources() noexcept {
    if (imageResources_.has_value()) {
        imageResources_->Shutdown();
        imageResources_.reset();
    }
    if (meshResources_.has_value()) {
        meshResources_->Shutdown();
        meshResources_.reset();
    }
    if (textResources_.has_value()) {
        textResources_->Shutdown();
        textResources_.reset();
    }
    if (frameEncoder_.has_value()) {
        frameEncoder_->Shutdown();
        frameEncoder_.reset();
    }
    renderThread_ = {};
    deviceGeneration_ = 0U;
}

Base::Result<void> ViewRenderer::VerifyRenderResources() const noexcept {
    if (!frameEncoder_.has_value() ||
        !frameEncoder_->IsInitialized() ||
        !textResources_.has_value() ||
        !meshResources_.has_value() ||
        !imageResources_.has_value()) {
        return NotInitialized("ViewRenderer resources are not initialized");
    }
    if (renderThread_ != std::this_thread::get_id()) {
        return WrongThread(
            "ViewRenderer must render from its owning render thread");
    }
    Render::RenderDeviceBase* backend = device_
        ? Render::RenderDeviceBase::From(*device_)
        : nullptr;
    if (backend == nullptr || !backend->AreResourcesReady() ||
        backend->BackendGeneration() != deviceGeneration_) {
        return DeviceUnavailable(
            "ViewRenderer graphics device is unavailable");
    }
    return {};
}

Base::Result<Graphics::FenceValue> ViewRenderer::RenderOffscreenFrame(
    const ::Aero::Render::RenderFrame& frame) noexcept {
    Base::Result<void> ready = VerifyRenderResources();
    if (!ready) return ready.GetStatus();
    Render::RenderDeviceBase* backend =
        Render::RenderDeviceBase::From(*device_);
    Base::Result<std::uint32_t> collected = backend->CollectGarbage();
    if (!collected) return collected.GetStatus();
    Base::Result<Graphics::FenceValue> recorded =
        frameEncoder_->RecordOffscreen(frame);
    if (!recorded) return recorded.GetStatus();
    return std::move(recorded).Value();
}

Base::Result<Graphics::FenceValue> ViewRenderer::RenderOnscreenFrame(
    const ::Aero::Render::RenderFrame& frame,
    const ::Aero::Render::FrameTarget& target) noexcept {
    Base::Result<void> ready = VerifyRenderResources();
    if (!ready) return ready.GetStatus();
    Render::RenderDeviceBase* backend =
        Render::RenderDeviceBase::From(*device_);
    Base::Result<std::uint32_t> collected = backend->CollectGarbage();
    if (!collected) return collected.GetStatus();
    Base::Result<Graphics::FenceValue> recorded =
        frameEncoder_->RecordOnscreen(frame, target);
    if (!recorded) return recorded.GetStatus();
    return std::move(recorded).Value();
}

::Aero::Render::FrameEncoderStatistics
ViewRenderer::LastStatistics() const noexcept {
    return frameEncoder_.has_value() && frameEncoder_->IsInitialized()
        ? frameEncoder_->LastStatistics()
        : ::Aero::Render::FrameEncoderStatistics{};
}

::Aero::Render::RenderResources ViewRenderer::Resources() noexcept {
    return frameEncoder_.has_value() && frameEncoder_->IsInitialized() &&
           textResources_.has_value() && meshResources_.has_value() &&
           imageResources_.has_value()
        ? ::Aero::Render::RenderResources{
              &textResources_->Table(),
              &meshResources_->Table(),
              &imageResources_->Table()}
        : ::Aero::Render::RenderResources{};
}

} // namespace Aero
