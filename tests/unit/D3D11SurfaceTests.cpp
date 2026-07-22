#include <Aero/Rhi/D3D11Backend.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <cstdint>
#include <cstdio>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Rhi;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

class HiddenWindow final {
public:
    HiddenWindow() noexcept = default;

    ~HiddenWindow() noexcept {
        Reset();
    }

    HiddenWindow(const HiddenWindow&) = delete;
    HiddenWindow& operator=(const HiddenWindow&) = delete;

    bool Initialize() noexcept {
        if (window_ != nullptr) {
            return true;
        }

        instance_ = GetModuleHandleW(nullptr);
        if (instance_ == nullptr) {
            return false;
        }

        WNDCLASSEXW windowClass{};
        windowClass.cbSize = static_cast<UINT>(sizeof(windowClass));
        windowClass.lpfnWndProc = &HiddenWindow::WindowProcedure;
        windowClass.hInstance = instance_;
        windowClass.lpszClassName = ClassName;
        atom_ = RegisterClassExW(&windowClass);
        if (atom_ == 0U) {
            return false;
        }

        window_ = CreateWindowExW(
            0U,
            ClassName,
            L"AeroGUI D3D11 Surface Test",
            static_cast<DWORD>(WS_OVERLAPPEDWINDOW),
            0,
            0,
            128,
            128,
            nullptr,
            nullptr,
            instance_,
            nullptr);
        if (window_ == nullptr) {
            Reset();
            return false;
        }
        return true;
    }

    HWND Handle() const noexcept {
        return window_;
    }

private:
    static constexpr wchar_t ClassName[] =
        L"AeroGuiD3D11SurfaceTestWindow";

    static LRESULT CALLBACK WindowProcedure(
        HWND window,
        UINT message,
        WPARAM wordParameter,
        LPARAM longParameter) noexcept {
        return DefWindowProcW(
            window, message, wordParameter, longParameter);
    }

    void Reset() noexcept {
        if (window_ != nullptr) {
            static_cast<void>(DestroyWindow(window_));
            window_ = nullptr;
        }
        if (atom_ != 0U && instance_ != nullptr) {
            static_cast<void>(UnregisterClassW(ClassName, instance_));
            atom_ = 0U;
        }
        instance_ = nullptr;
    }

    HINSTANCE instance_ = nullptr;
    ATOM atom_ = 0U;
    HWND window_ = nullptr;
};

NativeSurfaceDescriptor MakeSurfaceDescriptor(
    D3D11GraphicsBackend& backend,
    HWND window,
    std::uint32_t width,
    std::uint32_t height) noexcept {
    NativeSurfaceDescriptor descriptor;
    descriptor.kind = SurfaceKind::D3D11Window;
    descriptor.ownership = SurfaceOwnership::Owned;
    descriptor.presentMode = PresentMode::Immediate;
    descriptor.width = width;
    descriptor.height = height;
    descriptor.colorFormat = GraphicsTextureFormat::Bgra8Unorm;
    descriptor.depthStencilFormat = GraphicsTextureFormat::Depth24Stencil8;
    descriptor.sampleCount = 1U;
    descriptor.stableId = UINT64_C(0xD3115AFC);
    descriptor.d3d11.window = reinterpret_cast<std::uintptr_t>(window);
    descriptor.d3d11.device = backend.NativeDevice();
    descriptor.d3d11.immediateContext = backend.NativeImmediateContext();
    return descriptor;
}

D3D11ExternalRenderTargetDescriptor MakeImportDescriptor(
    const SurfaceFrame& frame) noexcept {
    D3D11ExternalRenderTargetDescriptor descriptor;
    descriptor.texture2D = frame.target.colorTarget;
    descriptor.depthStencilView = frame.target.depthStencilTarget;
    descriptor.texture.width = frame.target.width;
    descriptor.texture.height = frame.target.height;
    descriptor.texture.sampleCount = frame.target.sampleCount;
    descriptor.texture.format = frame.target.colorFormat;
    descriptor.texture.usage = TextureUsageBit(TextureUsage::RenderTarget) |
        TextureUsageBit(TextureUsage::CopySource);
    descriptor.stableId = frame.target.stableId;
    return descriptor;
}

Result<GraphicsCommandBuffer> MakeClearCommands(
    ResourceHandle target,
    std::uint32_t width,
    std::uint32_t height,
    Color color) noexcept {
    GraphicsCommandEncoder encoder;
    RenderPassDescriptor pass;
    pass.renderArea = {
        0.0,
        0.0,
        static_cast<double>(width),
        static_cast<double>(height)};
    pass.colorAttachmentCount = 1U;
    pass.colorAttachments[0].target = target;
    pass.colorAttachments[0].load = LoadOperation::Clear;
    pass.colorAttachments[0].store = StoreOperation::Store;
    pass.colorAttachments[0].clearColor = color;

    Result<void> begin = encoder.BeginRenderPass(pass);
    if (!begin) {
        return begin.GetStatus();
    }
    Result<void> end = encoder.EndRenderPass();
    if (!end) {
        return end.GetStatus();
    }
    return encoder.Finish();
}

bool VerifySolidGreen(
    D3D11GraphicsBackend& backend,
    ResourceHandle target,
    std::uint32_t width,
    std::uint32_t height) noexcept {
    if (width == 0U || height == 0U ||
        width > UINT32_MAX / 4U ||
        height > UINT32_MAX / (width * 4U)) {
        return false;
    }

    constexpr std::uint32_t MaximumPixels = 64U * 64U;
    if (width * height > MaximumPixels) {
        return false;
    }
    std::uint8_t pixels[MaximumPixels * 4U]{};
    const std::uint32_t rowPitch = width * 4U;
    const std::uint32_t byteCount = height * rowPitch;
    Result<void> readback = backend.ReadbackTexture(
        target,
        Span<std::uint8_t>(pixels, byteCount),
        rowPitch);
    if (!readback) {
        return false;
    }

    for (std::uint32_t pixel = 0U; pixel < width * height; ++pixel) {
        const std::uint32_t offset = pixel * 4U;
        if (pixels[offset + 0U] != 0U ||
            pixels[offset + 1U] != 255U ||
            pixels[offset + 2U] != 0U ||
            pixels[offset + 3U] != 255U) {
            return false;
        }
    }
    return true;
}

bool TestOwnedBorrowedResizeAndPresentation() {
    HiddenWindow window;
    CHECK(window.Initialize());

    D3D11BackendOptions backendOptions;
    backendOptions.deviceMode = D3D11DeviceMode::Warp;
    backendOptions.allowWarpFallback = false;
    D3D11GraphicsBackend backend(backendOptions);
    CHECK(backend.Initialize());

    RhiDevice device(backend);
    CHECK(device.Initialize());

    D3D11SwapChainSurface surfaceBackend(backend);
    SurfaceSession surface(surfaceBackend);
    NativeSurfaceDescriptor descriptor = MakeSurfaceDescriptor(
        backend, window.Handle(), 64U, 64U);
    CHECK(surface.Initialize(descriptor));
    CHECK(surface.State() == SurfaceState::Ready);
    CHECK(surfaceBackend.NativeSwapChain() != 0U);
    CHECK(surfaceBackend.OwnsSwapChain());

    Result<SurfaceFrame> acquired = surface.AcquireFrame();
    CHECK(acquired);
    SurfaceFrame frame = acquired.Value();
    CHECK(frame.target.width == 64U);
    CHECK(frame.target.height == 64U);
    CHECK(frame.target.colorTarget != 0U);

    Result<ResourceHandle> imported = ImportD3D11ExternalRenderTarget(
        device,
        backend,
        MakeImportDescriptor(frame));
    CHECK(imported);

    Result<GraphicsCommandBuffer> clearCommands = MakeClearCommands(
        imported.Value(),
        64U,
        64U,
        {0.0F, 1.0F, 0.0F, 1.0F});
    CHECK(clearCommands);
    CHECK(clearCommands.Value().CommandCount() == 2U);

    GraphicsQueue queue(backend);
    CHECK(queue.Initialize());
    Result<FenceValue> submitted = queue.Submit(clearCommands.Value());
    CHECK(submitted);
    CHECK(submitted.Value() == 1U);
    CHECK(backend.WaitForFence(submitted.Value()));
    CHECK(VerifySolidGreen(backend, imported.Value(), 64U, 64U));

    Result<std::uint64_t> checksum = backend.ReadbackTextureChecksum(
        imported.Value());
    CHECK(checksum);
    CHECK(checksum.Value() == UINT64_C(0x7090CCF69CA18383));

    CHECK(surface.Present(frame, submitted.Value()));
    CHECK(device.DestroyResource(imported.Value(), submitted.Value()));
    Result<std::uint32_t> collected = device.CollectGarbage();
    CHECK(collected);
    CHECK(collected.Value() == 1U);

    CHECK(surface.Resize(80U, 48U));
    Result<SurfaceFrame> resized = surface.AcquireFrame();
    CHECK(resized);
    CHECK(resized.Value().target.width == 80U);
    CHECK(resized.Value().target.height == 48U);
    SurfaceFrame resizedFrame = resized.Value();
    CHECK(surface.DiscardFrame(resizedFrame));

    NativeSurfaceDescriptor borrowedDescriptor = descriptor;
    borrowedDescriptor.ownership = SurfaceOwnership::Borrowed;
    borrowedDescriptor.width = 80U;
    borrowedDescriptor.height = 48U;
    borrowedDescriptor.d3d11.swapChain = surfaceBackend.NativeSwapChain();

    D3D11SwapChainSurface borrowedBackend(backend);
    SurfaceSession borrowedSurface(borrowedBackend);
    CHECK(borrowedSurface.Initialize(borrowedDescriptor));
    CHECK(!borrowedBackend.OwnsSwapChain());
    CHECK(borrowedBackend.NativeSwapChain() ==
        surfaceBackend.NativeSwapChain());
    Result<SurfaceFrame> borrowedFrameResult = borrowedSurface.AcquireFrame();
    CHECK(borrowedFrameResult);
    SurfaceFrame borrowedFrame = borrowedFrameResult.Value();
    CHECK(borrowedSurface.DiscardFrame(borrowedFrame));
    borrowedSurface.Shutdown();

    D3D11SurfacePresenter presenter(device, backend, surface);
    CHECK(presenter.Initialize());
    Result<D3D11SurfaceFrame> presentedFrameResult = presenter.AcquireFrame();
    CHECK(presentedFrameResult);
    D3D11SurfaceFrame presentedFrame = presentedFrameResult.Value();
    CHECK(presentedFrame.IsValid());

    Result<GraphicsCommandBuffer> blueCommands = MakeClearCommands(
        presentedFrame.renderTarget,
        80U,
        48U,
        {0.0F, 0.0F, 1.0F, 1.0F});
    CHECK(blueCommands);
    Result<FenceValue> presented = presenter.SubmitAndPresent(
        presentedFrame,
        blueCommands.Value());
    CHECK(presented);
    CHECK(presented.Value() == 2U);
    CHECK(!presentedFrame.IsValid());
    CHECK(!presenter.HasFrameInFlight());
    CHECK(backend.WaitForFence(presented.Value()));
    CHECK(presenter.CollectGarbage());

    CHECK(presenter.Resize(96U, 64U));
    Result<D3D11SurfaceFrame> discardResult = presenter.AcquireFrame();
    CHECK(discardResult);
    D3D11SurfaceFrame discardFrame = discardResult.Value();
    CHECK(discardFrame.surface.target.width == 96U);
    CHECK(discardFrame.surface.target.height == 64U);
    CHECK(presenter.DiscardFrame(discardFrame));
    CHECK(!discardFrame.IsValid());
    CHECK(presenter.CollectGarbage());

    presenter.Shutdown();
    CHECK(surface.NotifyContextLost());
    CHECK(surface.State() == SurfaceState::Lost);
    NativeSurfaceDescriptor restoreDescriptor = descriptor;
    restoreDescriptor.width = 96U;
    restoreDescriptor.height = 64U;
    CHECK(surface.Restore(restoreDescriptor));
    CHECK(surface.State() == SurfaceState::Ready);
    Result<SurfaceFrame> restoredResult = surface.AcquireFrame();
    CHECK(restoredResult);
    SurfaceFrame restoredFrame = restoredResult.Value();
    CHECK(restoredFrame.target.width == 96U);
    CHECK(restoredFrame.target.height == 64U);
    CHECK(surface.DiscardFrame(restoredFrame));

    surface.Shutdown();
    CHECK(device.LiveResourceCount() == 0U);
    CHECK(backend.LiveResourceCount() == 0U);
    return true;
}

} // namespace

int main() {
    if (!TestOwnedBorrowedResizeAndPresentation()) return 1;
    std::puts("Aero D3D11 swap-chain surface tests passed");
    return 0;
}
