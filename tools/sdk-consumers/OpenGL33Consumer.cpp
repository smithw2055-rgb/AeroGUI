#include <Aero/Render/OpenGL33.hpp>

namespace {

[[maybe_unused]]
Aero::Base::Result<
    Aero::Base::Ref<Aero::Integration::RenderSurface>>
CreateOpenGL33Surface(
    Aero::Integration::NativeWindowHandle window) noexcept {
    Aero::Integration::OpenGL33WindowSurfaceOptions options;
    options.window = window;
    options.width = 640U;
    options.height = 480U;
    return Aero::Integration::CreateOpenGL33WindowSurface(options);
}

} // namespace
