#include <Aero/Render/OpenGL33.hpp>

#include <utility>

namespace {

[[maybe_unused]]
Aero::Base::Result<Aero::Base::Ref<Aero::RenderTarget>>
CreateOpenGL33Target(
    Aero::Base::Ref<Aero::RenderDevice> device) noexcept {
    Aero::Render::OpenGL33RenderTargetOptions options;
    return Aero::Render::CreateOpenGL33RenderTarget(
        std::move(device), options);
}

} // namespace
