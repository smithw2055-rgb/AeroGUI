#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/IRenderer.hpp>
#include <AeroRender/RenderDevice.hpp>
#include <AeroRender/RenderTarget.hpp>
#include "render/FrameEncoder.hpp"
#include "render/RenderResources.hpp"
#include "render/TextRenderer.hpp"

#include <cstdint>
#include <optional>
#include <thread>

namespace Aero {

class View;

// The only concrete IRenderer. Its delayed render-data member is owned by the
// View and is created when a host supplies a RenderDevice.
class ViewRenderer final : public IRenderer {
public:
    ViewRenderer(View& view, Base::IAllocator& allocator) noexcept;
    ~ViewRenderer() noexcept override;

    ViewRenderer(const ViewRenderer&) = delete;
    ViewRenderer& operator=(const ViewRenderer&) = delete;

    Base::Result<void> Init(Base::Ref<RenderDevice> device) noexcept override;
    void Shutdown() noexcept override;
    bool IsInitialized() const noexcept override;
    bool UpdateRenderTree() noexcept override;
    bool RenderOffscreen() noexcept override;
    void Render(RenderTarget& target) noexcept override;

    ::Aero::Render::RenderResources Resources() noexcept;

    Base::Ref<RenderDevice> Device() noexcept { return device_; }
    ::Aero::Render::UiFrameEncoder* FrameEncoder() noexcept {
        return frameEncoder_.has_value() ? &*frameEncoder_ : nullptr;
    }

    Base::Result<void> RenderOnscreenFrame(
        const ::Aero::Render::RenderFrame& frame,
        RenderTarget& target) noexcept;
    ::Aero::Render::FrameStatistics LastStatistics() const noexcept;

private:
    Base::Result<void> InitializeRenderResources(
        RenderDevice& device,
        std::uint64_t generation) noexcept;
    void ShutdownRenderResources() noexcept;
    Base::Result<void> VerifyRenderResources() const noexcept;
    Base::Result<void> RenderOffscreenFrame(
        const ::Aero::Render::RenderFrame& frame) noexcept;

    Base::IAllocator* allocator_ = nullptr;
    View* view_ = nullptr;
    Base::Ref<RenderDevice> device_;
    std::optional<::Aero::Render::UiFrameEncoder> frameEncoder_;
    ::Aero::Render::TextResources textResources_{};
    ::Aero::Render::MeshResources meshResources_{};
    ::Aero::Render::ImageResources imageResources_{};
    std::thread::id renderThread_;
    std::uint64_t deviceGeneration_ = 0U;
    std::uint64_t updatedVersion_ = 0U;
    std::uint64_t renderedVersion_ = 0U;
    bool initialized_ = false;
    bool offscreenReady_ = false;
};

} // namespace Aero
