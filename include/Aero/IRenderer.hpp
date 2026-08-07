#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/RenderTarget.hpp>

namespace Aero {

class RenderDevice;

// Render-thread interface owned by one View. UI state is committed through
// UpdateRenderTree(); GPU work remains explicitly split into offscreen and
// onscreen passes so the host retains scheduling control.
class AERO_API IRenderer {
public:
    virtual ~IRenderer() = default;

    IRenderer(const IRenderer&) = delete;
    IRenderer& operator=(const IRenderer&) = delete;

    virtual Base::Result<void> Init(
        Base::Ref<RenderDevice> device) noexcept = 0;
    virtual void Shutdown() noexcept = 0;
    virtual bool IsInitialized() const noexcept = 0;

    virtual Base::Result<bool> UpdateRenderTree() noexcept = 0;
    virtual Base::Result<void> RenderOffscreen() noexcept = 0;
    virtual Base::Result<void> Render(
        RenderTarget& target) noexcept = 0;

protected:
    IRenderer() noexcept = default;
};

} // namespace Aero
