#include <Aero/Render/D3D11.hpp>

#include <utility>

namespace {

[[maybe_unused]]
Aero::Base::Result<Aero::Base::Ref<Aero::RenderTarget>>
CreateD3D11Target(
    Aero::Base::Ref<Aero::RenderDevice> device) noexcept {
    Aero::Render::D3D11RenderTargetOptions options;
    return Aero::Render::CreateD3D11RenderTarget(
        std::move(device), options);
}

} // namespace
