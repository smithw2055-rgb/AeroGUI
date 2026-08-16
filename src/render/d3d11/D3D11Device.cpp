#include "D3D11RenderDevice.hpp"
#include <AeroRender/D3D11.hpp>

namespace Aero::Render {

Base::Result<Base::Ref<Aero::RenderDevice>> D3D11::CreateDevice(
    const ::Aero::Render::D3D11::DeviceOptions& options,
    Base::IAllocator* allocator) noexcept {
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : Base::GetDefaultAllocator();

    D3D11RenderDeviceOptions nativeOptions;
    nativeOptions.borrowedDevice = reinterpret_cast<ID3D11Device*>(options.device);
    nativeOptions.borrowedContext = reinterpret_cast<ID3D11DeviceContext*>(options.immediateContext);
    nativeOptions.useWarp = options.useWarp;
    nativeOptions.allowWarpFallback = options.allowWarpFallback;
    nativeOptions.enableDebugLayer = options.enableDebugLayer;

    Base::Result<Base::Ref<D3D11RenderDevice>> made =
        Base::MakeRefWithAllocator<D3D11RenderDevice>(
            selected, nativeOptions, &selected);
    if (!made) return made.GetStatus();

    Base::Ref<D3D11RenderDevice> device = std::move(made).Value();
    Base::Result<void> init = device->Initialize();
    if (!init) return init.GetStatus();

    return Base::Ref<Aero::RenderDevice>(std::move(device));
}

Base::Result<Base::Ref<Aero::RenderTarget>> D3D11::CreateTarget(
    Base::Ref<Aero::RenderDevice> device,
    const D3D11::TargetOptions& options,
    Base::IAllocator* allocator) noexcept {
    static_cast<void>(allocator);
    if (!device) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument, "Device is null");
    }

    if (options.acquireTarget != nullptr) {
        D3D11::EmbeddedTarget targetInfo{};
        Base::Status st = options.acquireTarget(options.callbackContext, &targetInfo);
        if (st.IsOk() && targetInfo.width > 0 && targetInfo.height > 0) {
            auto* d3dDev = static_cast<D3D11RenderDevice*>(device.Get());
            ID3D11RenderTargetView* rtv = reinterpret_cast<ID3D11RenderTargetView*>(targetInfo.renderTargetView);
            ID3D11DepthStencilView* dsv = reinterpret_cast<ID3D11DepthStencilView*>(targetInfo.depthStencilView);
            ID3D11Texture2D* tex = reinterpret_cast<ID3D11Texture2D*>(targetInfo.texture2D);

            Ref<D3D11Texture> d3dTex = Base::MakeRef<D3D11Texture>(
                tex, nullptr, targetInfo.width, targetInfo.height, false, true).Value();

            Ref<RenderDevice> devCopy = device;
            return Base::Ref<Aero::RenderTarget>(
                Base::MakeRef<D3D11RenderTarget>(
                    std::move(devCopy), std::move(d3dTex), rtv, dsv,
                    targetInfo.width, targetInfo.height).Value());
        }
    }

    return device->CreateRenderTarget("DefaultTarget", 800, 600, 1, false);
}

} // namespace Aero::Render
