#include "../RenderContext.hpp"
#include "../Presentation.hpp"
#include "D3D11RenderDevice.hpp"

#include <AeroRender/D3D11.hpp>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#endif

#include <cstdint>
#include <new>
#include <utility>

namespace Aero::Render {
namespace {

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status Unsupported(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::Unsupported, message);
}

Base::Status NativeFailure(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InternalError, message);
}

#if defined(_WIN32)

template<class T>
void ReleaseCom(T*& value) noexcept {
    if (value != nullptr) {
        value->Release();
        value = nullptr;
    }
}

bool IsDeviceRemoval(HRESULT result) noexcept {
    return result == DXGI_ERROR_DEVICE_REMOVED ||
        result == DXGI_ERROR_DEVICE_RESET ||
        result == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}

class D3D11RenderContext final : public RenderContext {
public:
    ~D3D11RenderContext() noexcept override { Shutdown(); }

    Base::Result<void> Initialize(
        Platform::NativeWindowHandle window,
        std::uint32_t width,
        std::uint32_t height,
        Base::IAllocator* allocator) noexcept {
        Base::Result<void> validSize =
            ValidatePresentationSize({width, height});
        if (!window.IsValid() ||
            window.system != Platform::WindowSystem::Win32 ||
            !validSize) {
            return InvalidArgument("D3D11 application context requires a Win32 window and nonzero dimensions");
        }
        window_ = reinterpret_cast<HWND>(window.window);
        width_ = width;
        height_ = height;

        DXGI_SWAP_CHAIN_DESC swapChainDescriptor{};
        swapChainDescriptor.BufferDesc.Width = width;
        swapChainDescriptor.BufferDesc.Height = height;
        swapChainDescriptor.BufferDesc.RefreshRate.Numerator = 0U;
        swapChainDescriptor.BufferDesc.RefreshRate.Denominator = 1U;
        swapChainDescriptor.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        swapChainDescriptor.BufferDesc.ScanlineOrdering =
            DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        swapChainDescriptor.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        swapChainDescriptor.SampleDesc.Count = 1U;
        swapChainDescriptor.SampleDesc.Quality = 0U;
        swapChainDescriptor.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDescriptor.BufferCount = 2U;
        swapChainDescriptor.OutputWindow = window_;
        swapChainDescriptor.Windowed = TRUE;
        swapChainDescriptor.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        const D3D_FEATURE_LEVEL levelsWith11_1[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0};
        const D3D_FEATURE_LEVEL levelsWithout11_1[] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0};

        auto createNative = [&](D3D_DRIVER_TYPE driver) noexcept -> HRESULT {
            HRESULT result = D3D11CreateDeviceAndSwapChain(
                nullptr,
                driver,
                nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                levelsWith11_1,
                static_cast<UINT>(sizeof(levelsWith11_1) / sizeof(levelsWith11_1[0])),
                D3D11_SDK_VERSION,
                &swapChainDescriptor,
                &swapChain_,
                &device_,
                &featureLevel_,
                &immediateContext_);
            if (result == E_INVALIDARG) {
                result = D3D11CreateDeviceAndSwapChain(
                    nullptr,
                    driver,
                    nullptr,
                    D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                    levelsWithout11_1,
                    static_cast<UINT>(sizeof(levelsWithout11_1) / sizeof(levelsWithout11_1[0])),
                    D3D11_SDK_VERSION,
                    &swapChainDescriptor,
                    &swapChain_,
                    &device_,
                    &featureLevel_,
                    &immediateContext_);
            }
            return result;
        };

        HRESULT result = createNative(D3D_DRIVER_TYPE_HARDWARE);
        if (FAILED(result)) {
            ReleaseNative();
            result = createNative(D3D_DRIVER_TYPE_WARP);
        }
        if (FAILED(result) || device_ == nullptr ||
            immediateContext_ == nullptr || swapChain_ == nullptr) {
            ReleaseNative();
            return NativeFailure("Unable to create the D3D11 application device and swap chain");
        }

        IDXGIFactory* factory = nullptr;
        IDXGIDevice* dxgiDevice = nullptr;
        IDXGIAdapter* adapter = nullptr;
        if (SUCCEEDED(device_->QueryInterface(
                __uuidof(IDXGIDevice),
                reinterpret_cast<void**>(&dxgiDevice))) &&
            SUCCEEDED(dxgiDevice->GetAdapter(&adapter)) &&
            SUCCEEDED(adapter->GetParent(
                __uuidof(IDXGIFactory),
                reinterpret_cast<void**>(&factory)))) {
            static_cast<void>(factory->MakeWindowAssociation(
                window_, DXGI_MWA_NO_ALT_ENTER));
        }
        ReleaseCom(factory);
        ReleaseCom(adapter);
        ReleaseCom(dxgiDevice);

        D3D11_TEXTURE2D_DESC stencilDesc{};
        stencilDesc.Width = width;
        stencilDesc.Height = height;
        stencilDesc.MipLevels = 1U;
        stencilDesc.ArraySize = 1U;
        stencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        stencilDesc.SampleDesc.Count = 1U;
        stencilDesc.SampleDesc.Quality = 0U;
        stencilDesc.Usage = D3D11_USAGE_DEFAULT;
        stencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        device_->CreateTexture2D(&stencilDesc, nullptr, &depthStencil_);
        if (depthStencil_ != nullptr) {
            device_->CreateDepthStencilView(depthStencil_, nullptr, &depthStencilView_);
        }

        Render::D3D11::DeviceOptions deviceOptions;
        deviceOptions.device = reinterpret_cast<std::uintptr_t>(device_);
        deviceOptions.immediateContext =
            reinterpret_cast<std::uintptr_t>(immediateContext_);
        deviceOptions.statePolicy =
            Render::D3D11::StatePreservationPolicy::HostResetsState;
        Base::Result<Base::Ref<RenderDevice>> createdDevice =
            Render::D3D11::CreateDevice(deviceOptions, allocator);
        if (!createdDevice) {
            ReleaseNative();
            return createdDevice.GetStatus();
        }

        Render::D3D11::TargetOptions targetOptions;
        targetOptions.acquireTarget = &AcquireTarget;
        targetOptions.callbackContext = this;
        targetOptions.clearBeforeRender = true;
        targetOptions.width = width;
        targetOptions.height = height;
        Base::Result<Base::Ref<RenderTarget>> createdTarget =
            Render::D3D11::CreateTarget(
                std::move(createdDevice).Value(), targetOptions, allocator);
        if (!createdTarget) {
            ReleaseNative();
            return createdTarget.GetStatus();
        }
        Base::Result<void> adopted =
            AdoptTarget(std::move(createdTarget).Value());
        if (!adopted) {
            ReleaseNative();
            return adopted.GetStatus();
        }
        return {};
    }

protected:
    Base::Result<void> BeginPresentation() noexcept override {
        if (swapChain_ == nullptr || activeBackBuffer_ != nullptr) {
            return InvalidState("D3D11 presentation frame acquisition is invalid");
        }
        ID3D11Texture2D* backBuffer = nullptr;
        const HRESULT result = swapChain_->GetBuffer(
            0U,
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void**>(&backBuffer));
        if (FAILED(result) || backBuffer == nullptr) {
            if (IsDeviceRemoval(result)) NotifyDeviceLost();
            ReleaseCom(backBuffer);
            return NativeFailure("Unable to acquire the D3D11 swap-chain back buffer");
        }
        D3D11_TEXTURE2D_DESC descriptor{};
        backBuffer->GetDesc(&descriptor);
        if (descriptor.Width != width_ || descriptor.Height != height_ ||
            descriptor.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
            ReleaseCom(backBuffer);
            return InvalidState("D3D11 swap-chain back buffer does not match the application context");
        }
        activeBackBuffer_ = backBuffer;
        ID3D11RenderTargetView* rtv = nullptr;
        if (device_ != nullptr) {
            device_->CreateRenderTargetView(activeBackBuffer_, nullptr, &rtv);
        }
        if (Target() != nullptr) {
            auto* target = static_cast<D3D11RenderTarget*>(Target());
            target->SetRTV(rtv);
            target->SetDSV(depthStencilView_);
            target->SetSize(descriptor.Width, descriptor.Height);
        }
        ReleaseCom(rtv);
        return {};
    }

    Base::Result<void> ResizePresentation(
        std::uint32_t width,
        std::uint32_t height) noexcept override {
        Base::Result<void> validSize =
            ValidatePresentationSize({width, height});
        if (swapChain_ == nullptr || !validSize ||
            activeBackBuffer_ != nullptr) {
            return InvalidArgument("D3D11 presentation resize is invalid");
        }
        immediateContext_->ClearState();
        immediateContext_->Flush();
        ReleaseCom(depthStencilView_);
        ReleaseCom(depthStencil_);
        const HRESULT result = swapChain_->ResizeBuffers(
            0U, width, height, DXGI_FORMAT_UNKNOWN, 0U);
        if (FAILED(result)) {
            if (IsDeviceRemoval(result)) NotifyDeviceLost();
            return NativeFailure("Unable to resize the D3D11 swap chain");
        }
        D3D11_TEXTURE2D_DESC stencilDesc{};
        stencilDesc.Width = width;
        stencilDesc.Height = height;
        stencilDesc.MipLevels = 1U;
        stencilDesc.ArraySize = 1U;
        stencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        stencilDesc.SampleDesc.Count = 1U;
        stencilDesc.SampleDesc.Quality = 0U;
        stencilDesc.Usage = D3D11_USAGE_DEFAULT;
        stencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        device_->CreateTexture2D(&stencilDesc, nullptr, &depthStencil_);
        if (depthStencil_ != nullptr) {
            device_->CreateDepthStencilView(depthStencil_, nullptr, &depthStencilView_);
        }
        width_ = width;
        height_ = height;
        ++surfaceGeneration_;
        return {};
    }

    Base::Result<void> PresentFrame() noexcept override {
        if (swapChain_ == nullptr || activeBackBuffer_ == nullptr) {
            return InvalidState("D3D11 presentation has no acquired frame");
        }
        if (Target() != nullptr) {
            static_cast<D3D11RenderTarget*>(Target())->SetRTV(nullptr);
        }

        const HRESULT result = swapChain_->Present(1U, 0U);
        ReleaseCom(activeBackBuffer_);
        if (FAILED(result)) {
            if (IsDeviceRemoval(result)) NotifyDeviceLost();
            return NativeFailure("Unable to present the D3D11 swap chain");
        }
        return {};
    }

    void CancelFrame() noexcept override {
        if (Target() != nullptr) {
            static_cast<D3D11RenderTarget*>(Target())->SetRTV(nullptr);
        }
        ReleaseCom(activeBackBuffer_);
    }

    void ShutdownPresentation() noexcept override {
        ReleaseNative();
    }

private:
    static Base::Status AcquireTarget(
        void* context,
        Render::D3D11::EmbeddedTarget* target) noexcept {
        auto* owner = static_cast<D3D11RenderContext*>(context);
        if (owner == nullptr || target == nullptr ||
            owner->activeBackBuffer_ == nullptr) {
            return InvalidState("D3D11 application target was requested outside an active frame");
        }
        target->texture2D = reinterpret_cast<std::uintptr_t>(
            owner->activeBackBuffer_);
        target->depthStencilView = reinterpret_cast<std::uintptr_t>(
            owner->depthStencilView_);
        target->width = owner->width_;
        target->height = owner->height_;
        target->stableId =
            static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(owner->swapChain_)) ^
            owner->surfaceGeneration_;
        return Base::Status::Ok();
    }

    void NotifyDeviceLost() noexcept {
        Base::Ref<RenderDevice> device = Device();
        if (device) device->NotifyDeviceLost();
    }

    void ReleaseNative() noexcept {
        ReleaseCom(activeBackBuffer_);
        ReleaseCom(depthStencilView_);
        ReleaseCom(depthStencil_);
        if (immediateContext_ != nullptr) {
            immediateContext_->ClearState();
            immediateContext_->Flush();
        }
        ReleaseCom(swapChain_);
        ReleaseCom(immediateContext_);
        ReleaseCom(device_);
        window_ = nullptr;
        width_ = 0U;
        height_ = 0U;
        featureLevel_ = D3D_FEATURE_LEVEL_10_0;
        surfaceGeneration_ = 1U;
    }

    HWND window_ = nullptr;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* immediateContext_ = nullptr;
    IDXGISwapChain* swapChain_ = nullptr;
    ID3D11Texture2D* activeBackBuffer_ = nullptr;
    ID3D11Texture2D* depthStencil_ = nullptr;
    ID3D11DepthStencilView* depthStencilView_ = nullptr;
    D3D_FEATURE_LEVEL featureLevel_ = D3D_FEATURE_LEVEL_10_0;
    std::uint32_t width_ = 0U;
    std::uint32_t height_ = 0U;
    std::uint64_t surfaceGeneration_ = 1U;
};

#endif

} // namespace

AERO_RENDER_D3D11_API Base::Result<RenderContext*> CreateD3D11RenderContext(
    Platform::NativeWindowHandle window,
    std::uint32_t width,
    std::uint32_t height,
    Base::IAllocator* allocator) noexcept {
#if defined(_WIN32)
    auto* context = new (std::nothrow) D3D11RenderContext();
    if (context == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Unable to allocate the D3D11 application context");
    }
    Base::Result<void> initialized =
        context->Initialize(window, width, height, allocator);
    if (!initialized) {
        delete context;
        return initialized.GetStatus();
    }
    return static_cast<RenderContext*>(context);
#else
    static_cast<void>(window);
    static_cast<void>(width);
    static_cast<void>(height);
    static_cast<void>(allocator);
    return Unsupported("D3D11 application backend is not enabled");
#endif
}

} // namespace Aero::Render
