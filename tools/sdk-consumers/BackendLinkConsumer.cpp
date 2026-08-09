#if defined(AERO_LINK_D3D11)
#include <AeroRender/D3D11.hpp>

auto volatile g_backendFactory = &Aero::Render::D3D11::CreateDevice;
#elif defined(AERO_LINK_OPENGL33)
#include <AeroRender/OpenGL33.hpp>

auto volatile g_backendFactory = &Aero::Render::OpenGL33::CreateDevice;
#else
#error "Select exactly one Aero render backend"
#endif

int main() {
    return g_backendFactory == nullptr ? 1 : 0;
}
