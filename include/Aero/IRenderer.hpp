#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <AeroRender/RenderTarget.hpp>

namespace Aero {

class RenderDevice;

// Render-thread interface owned by one View. UI state is committed through
// UpdateRenderTree(); true means a new immutable frame was published. Hosts may
// skip GPU work when it returns false unless native exposure/resize requires a
// re-present. Offscreen and onscreen passes remain explicit for host scheduling.
class AERO_GUI_API IRenderer {
public:
    virtual ~IRenderer() = default;

    IRenderer(const IRenderer&) = delete;
    IRenderer& operator=(const IRenderer&) = delete;

    virtual Result<void> Init(
        Ref<RenderDevice> device) noexcept = 0;
    virtual void Shutdown() noexcept = 0;
    virtual bool IsInitialized() const noexcept = 0;

    virtual bool UpdateRenderTree() noexcept = 0;
    virtual bool RenderOffscreen() noexcept = 0;
    virtual void Render(RenderTarget& target) noexcept = 0;

protected:
    IRenderer() noexcept = default;
};

} // namespace Aero
