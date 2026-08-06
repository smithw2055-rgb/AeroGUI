#include <Aero/Integration/D3D11.hpp>

namespace {

[[maybe_unused]]
Aero::Base::Result<
    Aero::Base::Ref<Aero::RenderDevice>>
CreateD3D11Device(
    Aero::Integration::NativeWindowHandle window) noexcept {
    Aero::Integration::D3D11WindowDeviceOptions options;
    options.window = window;
    options.width = 640U;
    options.height = 480U;
    return Aero::Integration::CreateD3D11WindowDevice(
        options);
}

} // namespace
