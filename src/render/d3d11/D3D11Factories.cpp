#include <AeroRender/D3D11.hpp>
#include "render/RenderDeviceState.hpp"

#include <utility>

namespace Aero::Render {

Base::Result<Base::Ref<Aero::RenderTarget>> D3D11::CreateTarget(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11::TargetOptions& options,
    Base::IAllocator* allocator) noexcept {
    ::Aero::Render::D3D11EmbeddedTargetOptions native;
    native.acquireTarget = options.acquireTarget;
    native.callbackContext = options.callbackContext;
    native.clearBeforeRender = options.clearBeforeRender;
    return ::Aero::Render::CreateD3D11EmbeddedTarget(
        std::move(device), native, allocator);
}

} // namespace Aero::Render
