#include <Aero/Integration/D3D11.hpp>

namespace {

[[maybe_unused]]
Aero::Base::Result<
    Aero::Base::Ref<Aero::Integration::RenderEndpoint>>
CreateD3D11Endpoint(
    Aero::Platform::NativeWindowHandle window) noexcept {
    Aero::Integration::D3D11WindowEndpointOptions options;
    options.window = window;
    options.width = 640U;
    options.height = 480U;
    return Aero::Integration::CreateD3D11WindowEndpoint(
        options);
}

} // namespace
