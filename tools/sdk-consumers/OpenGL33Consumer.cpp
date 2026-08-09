#include <AeroRender/OpenGL33.hpp>

#include <utility>

namespace {

[[maybe_unused]]
Aero::Result<Aero::Ref<Aero::RenderTarget>>
CreateOpenGL33Target(
    Aero::Ref<Aero::RenderDevice> device) noexcept {
    Aero::Render::OpenGL33::TargetOptions options;
    return Aero::Render::OpenGL33::CreateTarget(
        std::move(device), options);
}

} // namespace
