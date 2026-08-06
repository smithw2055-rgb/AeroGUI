#pragma once

#include <Aero/IRenderer.hpp>
#include <Aero/Base/Allocator.hpp>

namespace Aero {
class View;

namespace Runtime::Detail {

// Source-private implementation of the public per-View rendering contract.
// It owns synchronization state only; shared GPU resources remain on
// RenderDevice and retained UI state remains on View.
class ViewRenderer final : public IRenderer {
public:
    ViewRenderer(
        View& view,
        Base::IAllocator& allocator) noexcept;
    ~ViewRenderer() noexcept override;

    ViewRenderer(const ViewRenderer&) = delete;
    ViewRenderer& operator=(const ViewRenderer&) = delete;

    Base::Result<void> Init(
        Base::Ref<RenderDevice> device) noexcept override;
    void Shutdown() noexcept override;
    bool IsInitialized() const noexcept override;

    Base::Result<bool> UpdateRenderTree() noexcept override;
    Base::Result<void> RenderOffscreen() noexcept override;
    Base::Result<void> Render(
        Integration::RenderSurface& surface) noexcept override;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace Runtime::Detail
} // namespace Aero
