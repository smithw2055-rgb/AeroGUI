#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Allocator.hpp>
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
    bool IsInitialized() const noexcept;

    Base::Result<bool> UpdateRenderTree() noexcept;
    Base::Result<void> RenderOffscreen() noexcept;
    Base::Result<void> Render() noexcept;

private:
    friend class View;

    Renderer(View& view, Base::IAllocator& allocator) noexcept;

    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace Aero
