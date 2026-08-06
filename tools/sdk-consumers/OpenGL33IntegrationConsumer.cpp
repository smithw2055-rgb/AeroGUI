#include <Aero/Integration/OpenGL33.hpp>

namespace {

[[maybe_unused]]
Aero::Base::Result<
    Aero::Base::Ref<Aero::RenderDevice>>
CreateOpenGL33Device(
    Aero::Integration::NativeWindowHandle window) noexcept {
    Aero::Integration::OpenGL33WindowDeviceOptions options;
    options.window = window;
    options.width = 640U;
    options.height = 480U;
    return Aero::Integration::CreateOpenGL33WindowDevice(
        options);
}

} // namespace
