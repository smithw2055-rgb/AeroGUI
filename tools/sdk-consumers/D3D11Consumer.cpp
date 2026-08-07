#include <Aero/Render/D3D11.hpp>

namespace {

[[maybe_unused]]
Aero::Base::Result<
    Aero::Base::Ref<Aero::RenderTarget>>
CreateD3D11Target(
    Aero::Platform::NativeWindowHandle window) noexcept {
    Aero::Render::D3D11WindowSurfaceOptions options;
    options.window = window;
    options.width = 640U;
    options.height = 480U;
    return Aero::Render::CreateD3D11WindowSurface(options);
}

} // namespace
