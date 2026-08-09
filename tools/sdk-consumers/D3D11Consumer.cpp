#include <AeroRender/D3D11.hpp>

#include <utility>

namespace {

[[maybe_unused]]
Aero::Result<Aero::Ref<Aero::RenderTarget>>
CreateD3D11Target(
    Aero::Ref<Aero::RenderDevice> device) noexcept {
    Aero::Render::D3D11::TargetOptions options;
    return Aero::Render::D3D11::CreateTarget(
        std::move(device), options);
}

} // namespace
