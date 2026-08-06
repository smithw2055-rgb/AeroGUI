#include <Aero/Render/D3D11.hpp>

namespace {

[[maybe_unused]]
Aero::Base::Result<
    Aero::Base::Ref<Aero::Integration::RenderSurface>>
CreateD3D11Surface(
    Aero::Integration::NativeWindowHandle window) noexcept {
    Aero::Integration::D3D11WindowSurfaceOptions options;
    options.window = window;
    options.width = 640U;
    options.height = 480U;
    return Aero::Integration::CreateD3D11WindowSurface(options);
}

} // namespace
