#include <Aero/Integration/OpenGL33.hpp>

namespace {

[[maybe_unused]]
Aero::Base::Result<
    Aero::Base::Ref<Aero::Integration::RenderEndpoint>>
CreateOpenGL33Endpoint(
    Aero::Platform::NativeWindowHandle window) noexcept {
    Aero::Integration::OpenGL33WindowEndpointOptions options;
    options.window = window;
    options.width = 640U;
    options.height = 480U;
    return Aero::Integration::CreateOpenGL33WindowEndpoint(
        options);
}

} // namespace
