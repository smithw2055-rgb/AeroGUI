#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Integration/RenderDevice.hpp>

#include <cstdint>

namespace Aero {

class View;

// Per-View rendering facade. View::Update() advances UI state while this
// object synchronizes and renders the retained frame through a shared device.
class AERO_API Renderer final {
public:
    ~Renderer() noexcept;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    Base::Result<void> Init(
        Base::Ref<Integration::RenderDevice> device) noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() const noexcept { return initialized_; }

    Base::Result<bool> UpdateRenderTree() noexcept;
    Base::Result<void> RenderOffscreen() noexcept;
    Base::Result<void> Render() noexcept;

private:
    friend class View;

    explicit Renderer(View& view) noexcept
        : view_(&view) {}

    View* view_ = nullptr;
    Base::Ref<Integration::RenderDevice> device_;
    std::uint64_t updatedVersion_ = 0U;
    std::uint64_t renderedVersion_ = 0U;
    bool initialized_ = false;
    bool offscreenReady_ = false;
};

} // namespace Aero
