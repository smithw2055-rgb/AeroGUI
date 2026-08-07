#include <Aero/Render/D3D11.hpp>
#include "render/private/BackendApi.hpp"

#include <utility>

namespace Aero::Render {

Base::Result<Base::Ref<Aero::RenderDevice>> CreateD3D11Device(
    const D3D11DeviceOptions& options,
    Base::IAllocator* allocator) noexcept {
    return ::Aero::Render::Detail::CreateD3D11Device(options, allocator);
}

Base::Result<Base::Ref<Aero::RenderTarget>> CreateD3D11RenderTarget(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11RenderTargetOptions& options,
    Base::IAllocator* allocator) noexcept {
    ::Aero::Render::Detail::D3D11EmbeddedTargetOptions native;
    native.acquireTarget = options.acquireTarget;
    native.callbackContext = options.callbackContext;
    return ::Aero::Render::Detail::CreateD3D11EmbeddedTarget(
        std::move(device), native, allocator);
}

} // namespace Aero::Render
