#include "OpenGL33RenderDevice.hpp"
#include <AeroRender/OpenGL33.hpp>

namespace Aero::Render {

Base::Result<Base::Ref<Aero::RenderDevice>> OpenGL33::CreateDevice(
    const ::Aero::Render::OpenGL33::DeviceOptions& options,
    Base::IAllocator* allocator) noexcept {
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();

    Base::Result<Base::Ref<OpenGL33RenderDevice>> made =
        Base::MakeRefWithAllocator<OpenGL33RenderDevice>(
            selected, options, &selected);
    if (!made) return made.GetStatus();

    Base::Ref<OpenGL33RenderDevice> device = std::move(made).Value();
    Base::Result<void> init = device->Initialize();
    if (!init) return init.GetStatus();

    return Base::Ref<Aero::RenderDevice>(std::move(device));
}

Base::Result<Base::Ref<Aero::RenderTarget>> OpenGL33::CreateTarget(
    Base::Ref<Aero::RenderDevice> device,
    const OpenGL33::TargetOptions& options,
    Base::IAllocator* allocator) noexcept {
    static_cast<void>(allocator);
    if (!device) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Device is null");
    }

    if (options.acquireTarget != nullptr) {
        OpenGL33::EmbeddedTarget targetInfo{};
        void* ctx = options.targetContext != nullptr ? options.targetContext : options.callbackContext;
        Base::Status st = options.acquireTarget(ctx, &targetInfo);
        if (st.IsOk() && targetInfo.width > 0 && targetInfo.height > 0) {
            Ref<OpenGL33Texture> glTex = Base::MakeRef<OpenGL33Texture>(
                targetInfo.framebuffer, targetInfo.width, targetInfo.height, false, true).Value();

            Ref<RenderDevice> devCopy = device;
            return Base::Ref<Aero::RenderTarget>(
                Base::MakeRef<OpenGL33RenderTarget>(
                    std::move(devCopy), std::move(glTex),
                    static_cast<unsigned int>(targetInfo.framebuffer),
                    static_cast<unsigned int>(targetInfo.depthStencilTexture),
                    targetInfo.width, targetInfo.height, targetInfo.defaultFramebuffer).Value());
        }
    }

    return device->CreateRenderTarget("DefaultGLTarget", 800, 600, 1, false);
}

} // namespace Aero::Render
