#include <Aero/Render/OpenGL33.hpp>

namespace {

[[maybe_unused]]
Aero::Base::Result<
    Aero::Base::Ref<Aero::RenderSurface>>
CreateOpenGL33Surface(
    Aero::Platform::NativeWindowHandle window) noexcept {
    Aero::Render::OpenGL33WindowSurfaceOptions options;
    options.window = window;
    options.width = 640U;
    options.height = 480U;
    return Aero::Render::CreateOpenGL33WindowSurface(options);
}

} // namespace
