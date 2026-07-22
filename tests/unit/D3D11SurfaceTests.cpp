#include <Aero/Rhi/D3D11Backend.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <dxgi.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

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

class PlanPanel final : public RenderElement {
public:
    PlanPanel(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& registry,
        TypeId type,
        bool requestInstancedStroke = false) noexcept
        : RenderElement(dispatcher, registry, type),
          requestInstancedStroke_(requestInstancedStroke) {}

protected:
    Result<Size> MeasureOverride(Size available) noexcept override {
        for (LayoutElement* child : LayoutChildren()) {
            Result<void> measured = MeasureChild(*child, available);
            if (!measured) return measured.GetStatus();
        }
        return available;
    }

    Result<Size> ArrangeOverride(Size finalSize) noexcept override {
        for (LayoutElement* child : LayoutChildren()) {
            Result<void> arranged = ArrangeChild(*child, {8.0, 6.0, 24.0, 16.0});
            if (!arranged) return arranged.GetStatus();
        }
        return finalSize;
    }

    Result<void> BuildDisplayList(DisplayListBuilder& builder) noexcept override {
        Result<void> result = builder.PushClip(
            {0.0, 0.0, RenderSize().width, RenderSize().height});
        if (!result) return result;
        if (requestInstancedStroke_) {
            result = builder.StrokeRect(
                {4.0, 3.0, 12.0, 10.0}, {1.0F, 0.0F, 0.0F, 1.0F}, 2.0);
            if (!result) return result;
            return builder.PopClip();
        }
        result = builder.FillRect(
            {0.0, 0.0, RenderSize().width, RenderSize().height},
            {0.0F, 0.0F, 1.0F, 1.0F});
        if (!result) return result;
        result = builder.PushOpacity(0.5);
        if (!result) return result;
        result = builder.FillRect(
            {16.0, 12.0, 24.0, 20.0}, {1.0F, 0.0F, 0.0F, 1.0F});
        if (!result) return result;
        result = builder.PopOpacity();
        if (!result) return result;
        return builder.PopClip();
    }

private:
    bool requestInstancedStroke_ = false;
};

class PlanElement final : public RenderElement {
public:
    PlanElement(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& registry,
        TypeId type,
        bool requestNonAxisAlignedClip = false) noexcept
        : RenderElement(dispatcher, registry, type),
          requestNonAxisAlignedClip_(requestNonAxisAlignedClip) {}

protected:
    Result<Size> MeasureOverride(Size available) noexcept override {
        return Size{
            std::fmin(24.0, available.width),
            std::fmin(16.0, available.height)};
    }

    Result<Size> ArrangeOverride(Size finalSize) noexcept override {
        return finalSize;
    }

    Result<void> BuildDisplayList(DisplayListBuilder& builder) noexcept override {
        Result<void> result = builder.PushTransform(
            {1.5, 0.0, 0.0, 1.0, 4.0, 2.0});
        if (!result) return result;
        if (requestNonAxisAlignedClip_) {
            result = builder.PushTransform(
                {0.7071067811865476, 0.7071067811865476,
                 -0.7071067811865476, 0.7071067811865476, 0.0, 0.0});
            if (!result) return result;
            result = builder.PushClip({0.0, 0.0, 6.0, 8.0});
            if (!result) return result;
        }
        result = builder.FillRect(
            {0.0, 0.0, 12.0, 8.0}, {0.0F, 1.0F, 0.0F, 1.0F});
        if (!result) return result;
        if (requestNonAxisAlignedClip_) {
            result = builder.PopClip();
            if (!result) return result;
            result = builder.PopTransform();
            if (!result) return result;
        }
        return builder.PopTransform();
    }

private:
    bool requestNonAxisAlignedClip_ = false;
};

bool BuildPlan(
    RenderPlan& output,
    std::uint32_t width,
    std::uint32_t height,
    bool requestNonAxisAlignedClip = false,
    bool requestInstancedStroke = false) noexcept {
    TypeRegistry types;
    DependencyPropertyRegistry properties(types);
    Dispatcher dispatcher;
    const StringView ns("urn:aero-d3d11-test");
    const TypeId objectType = MakeTypeId(ns, StringView("Object"));
    const TypeId panelType = MakeTypeId(ns, StringView("PlanPanel"));
    const TypeId elementType = MakeTypeId(ns, StringView("PlanElement"));
    CHECK(types.TryRegisterType({ns, StringView("Object"), InvalidTypeId,
        TypeFlags::None, nullptr}));
    CHECK(types.TryRegisterType({ns, StringView("PlanPanel"), objectType,
        TypeFlags::None, nullptr}));
    CHECK(types.TryRegisterType({ns, StringView("PlanElement"), objectType,
        TypeFlags::None, nullptr}));
    CHECK(types.Freeze());
    CHECK(properties.Freeze());

    EffectiveValueEngine values(dispatcher, properties);
    CHECK(values.Initialize());
    ObjectTree tree(dispatcher, values);
    CHECK(tree.Initialize());
    LayoutManager layout(dispatcher);
    CHECK(layout.Initialize());
    NullRenderBackend verifier;
    RenderManager renderer(dispatcher, verifier);
    CHECK(renderer.Initialize());
    PlanPanel root(
        dispatcher, properties, panelType, requestInstancedStroke);
    PlanElement child(
        dispatcher, properties, elementType, requestNonAxisAlignedClip);
    CHECK(tree.SetRoot(&root));
    CHECK(tree.AttachLogical(root, child));
    CHECK(tree.AttachVisual(root, child));
    CHECK(layout.Attach(root, child));
    CHECK(renderer.SetRoot(&root));
    CHECK(renderer.Attach(root, child));
    CHECK(layout.SetRoot(&root, {
        static_cast<double>(width), static_cast<double>(height)}));
    CHECK(dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(dispatcher.RunFramePhase(DispatcherFramePhase::RenderCommit));
    output = renderer.CurrentPlan();
    CHECK(output.Nodes().Size() == 2U);
    CHECK(output.Commands().Size() ==
        (requestInstancedStroke ? 6U :
            (requestNonAxisAlignedClip ? 13U : 9U)));
    CHECK(renderer.SetRoot(nullptr));
    CHECK(layout.SetRoot(nullptr, {0.0, 0.0}));
    CHECK(layout.Detach(root, child));
    CHECK(tree.DetachVisual(root, child));
    CHECK(tree.DetachLogical(root, child));
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(child));
    CHECK(values.DetachObject(root));
    return true;
}

bool ReadBackPixel(
    D3D11GraphicsBackend& backend,
    ResourceHandle target,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t x,
    std::uint32_t y,
    std::uint8_t (&pixel)[4]) noexcept {
    if (x >= width || y >= height || width == 0U || height == 0U ||
        width > 128U || height > 128U) {
        return false;
    }
    std::uint8_t pixels[128U * 128U * 4U]{};
    const std::uint32_t rowPitch = width * 4U;
    Result<void> readback = backend.ReadbackTexture(
        target, Span<std::uint8_t>(pixels, rowPitch * height), rowPitch);
    if (!readback) {
        return false;
    }
    std::memcpy(pixel, pixels + (y * rowPitch) + (x * 4U), sizeof(pixel));
    return true;
}

// Delegates surface setup to a real swap chain, then makes Present report a
// deterministic device-removal result. This exercises the terminal-loss path
// without destabilizing the WARP device used by the rest of the test.
class DeviceRemovedSwapChain final : public IDXGISwapChain {
public:
    explicit DeviceRemovedSwapChain(IDXGISwapChain& target) noexcept
        : target_(&target) {
        target_->AddRef();
    }

    ~DeviceRemovedSwapChain() noexcept {
        if (target_ != nullptr) {
            target_->Release();
        }
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(
        REFIID interfaceId,
        void** object) noexcept override {
        if (object == nullptr) {
            return E_POINTER;
        }
        *object = nullptr;
        if (interfaceId == __uuidof(IUnknown) ||
            interfaceId == __uuidof(IDXGIObject) ||
            interfaceId == __uuidof(IDXGIDeviceSubObject) ||
            interfaceId == __uuidof(IDXGISwapChain)) {
            *object = static_cast<IDXGISwapChain*>(this);
            AddRef();
            return S_OK;
        }
        return target_->QueryInterface(interfaceId, object);
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override {
        return static_cast<ULONG>(InterlockedIncrement(&references_));
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override {
        return static_cast<ULONG>(InterlockedDecrement(&references_));
    }

    HRESULT STDMETHODCALLTYPE SetPrivateData(
        REFGUID name,
        UINT dataSize,
        const void* data) noexcept override {
        return target_->SetPrivateData(name, dataSize, data);
    }

    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
        REFGUID name,
        const IUnknown* object) noexcept override {
        return target_->SetPrivateDataInterface(name, object);
    }

    HRESULT STDMETHODCALLTYPE GetPrivateData(
        REFGUID name,
        UINT* dataSize,
        void* data) noexcept override {
        return target_->GetPrivateData(name, dataSize, data);
    }

    HRESULT STDMETHODCALLTYPE GetParent(
        REFIID interfaceId,
        void** parent) noexcept override {
        return target_->GetParent(interfaceId, parent);
    }

    HRESULT STDMETHODCALLTYPE GetDevice(
        REFIID interfaceId,
        void** device) noexcept override {
        return target_->GetDevice(interfaceId, device);
    }

    HRESULT STDMETHODCALLTYPE Present(UINT, UINT) noexcept override {
        return DXGI_ERROR_DEVICE_REMOVED;
    }

    HRESULT STDMETHODCALLTYPE GetBuffer(
        UINT index,
        REFIID interfaceId,
        void** surface) noexcept override {
        return target_->GetBuffer(index, interfaceId, surface);
    }

    HRESULT STDMETHODCALLTYPE SetFullscreenState(
        BOOL fullscreen,
        IDXGIOutput* target) noexcept override {
        return target_->SetFullscreenState(fullscreen, target);
    }

    HRESULT STDMETHODCALLTYPE GetFullscreenState(
        BOOL* fullscreen,
        IDXGIOutput** target) noexcept override {
        return target_->GetFullscreenState(fullscreen, target);
    }

    HRESULT STDMETHODCALLTYPE GetDesc(
        DXGI_SWAP_CHAIN_DESC* descriptor) noexcept override {
        return target_->GetDesc(descriptor);
    }

    HRESULT STDMETHODCALLTYPE ResizeBuffers(
        UINT count,
        UINT width,
        UINT height,
        DXGI_FORMAT format,
        UINT flags) noexcept override {
        return target_->ResizeBuffers(count, width, height, format, flags);
    }

    HRESULT STDMETHODCALLTYPE ResizeTarget(
        const DXGI_MODE_DESC* descriptor) noexcept override {
        return target_->ResizeTarget(descriptor);
    }

    HRESULT STDMETHODCALLTYPE GetContainingOutput(
        IDXGIOutput** output) noexcept override {
        return target_->GetContainingOutput(output);
    }

    HRESULT STDMETHODCALLTYPE GetFrameStatistics(
        DXGI_FRAME_STATISTICS* statistics) noexcept override {
        return target_->GetFrameStatistics(statistics);
    }

    HRESULT STDMETHODCALLTYPE GetLastPresentCount(
        UINT* count) noexcept override {
        return target_->GetLastPresentCount(count);
    }

private:
    IDXGISwapChain* target_ = nullptr;
    LONG references_ = 1L;
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

    RenderPlan renderPlan;
    CHECK(BuildPlan(renderPlan, 80U, 48U));
    D3D11RenderPlanBackend renderBackend(device, backend, presenter);
    CHECK(renderBackend.Initialize());
    CHECK(renderBackend.Submit(renderPlan));
    const D3D11RenderPlanSubmitStatistics firstPlanStatistics =
        renderBackend.LastSubmitStatistics();
    CHECK(firstPlanStatistics.renderPassCount == 1U);
    CHECK(firstPlanStatistics.drawCallCount == 3U);
    CHECK(firstPlanStatistics.rectangleInstanceCount == 3U);
    CHECK(firstPlanStatistics.uniformBufferUploadCount == 3U);
    CHECK(firstPlanStatistics.pipelineBindingCount == 1U);
    CHECK(firstPlanStatistics.vertexBufferBindingCount == 1U);
    CHECK(firstPlanStatistics.uniformBufferBindingCount == 1U);
    CHECK(renderBackend.LastSubmittedFence() == 2U);
    CHECK(backend.WaitForFence(renderBackend.LastSubmittedFence()));
    Result<SurfaceFrame> verificationFrameResult = surface.AcquireFrame();
    CHECK(verificationFrameResult);
    SurfaceFrame verificationFrame = verificationFrameResult.Value();
    Result<ResourceHandle> verificationTarget = ImportD3D11ExternalRenderTarget(
        device, backend, MakeImportDescriptor(verificationFrame));
    CHECK(verificationTarget);
    std::uint8_t outerPixel[4]{};
    std::uint8_t innerPixel[4]{};
    CHECK(ReadBackPixel(
        backend, verificationTarget.Value(), 80U, 48U, 4U, 4U, outerPixel));
    CHECK(ReadBackPixel(
        backend, verificationTarget.Value(), 80U, 48U, 36U, 16U, innerPixel));
    CHECK(outerPixel[0] == 255U && outerPixel[1] == 0U &&
        outerPixel[2] == 0U && outerPixel[3] == 255U);
    CHECK(innerPixel[0] >= 126U && innerPixel[0] <= 129U &&
        innerPixel[1] == 0U && innerPixel[2] >= 126U &&
        innerPixel[2] <= 129U && innerPixel[3] == 255U);
    std::uint8_t childPixel[4]{};
    CHECK(ReadBackPixel(
        backend, verificationTarget.Value(), 80U, 48U, 14U, 9U, childPixel));
    CHECK(childPixel[0] == 0U && childPixel[1] == 255U &&
        childPixel[2] == 0U && childPixel[3] == 255U);
    CHECK(surface.DiscardFrame(verificationFrame));
    CHECK(device.DestroyResource(
        verificationTarget.Value(), renderBackend.LastSubmittedFence()));
    RenderPlan rotatedClipPlan;
    CHECK(BuildPlan(rotatedClipPlan, 80U, 48U, true));
    CHECK(renderBackend.Submit(rotatedClipPlan));
    const D3D11RenderPlanSubmitStatistics rotatedPlanStatistics =
        renderBackend.LastSubmitStatistics();
    CHECK(rotatedPlanStatistics.renderPassCount == 1U);
    CHECK(rotatedPlanStatistics.drawCallCount == 3U);
    CHECK(rotatedPlanStatistics.rectangleInstanceCount == 3U);
    CHECK(rotatedPlanStatistics.uniformBufferUploadCount == 3U);
    CHECK(rotatedPlanStatistics.pipelineBindingCount == 1U);
    CHECK(rotatedPlanStatistics.vertexBufferBindingCount == 1U);
    CHECK(rotatedPlanStatistics.uniformBufferBindingCount == 1U);
    CHECK(renderBackend.LastSubmittedFence() == 3U);
    CHECK(backend.WaitForFence(renderBackend.LastSubmittedFence()));
    Result<SurfaceFrame> rotatedFrameResult = surface.AcquireFrame();
    CHECK(rotatedFrameResult);
    SurfaceFrame rotatedFrame = rotatedFrameResult.Value();
    Result<ResourceHandle> rotatedTarget = ImportD3D11ExternalRenderTarget(
        device, backend, MakeImportDescriptor(rotatedFrame));
    CHECK(rotatedTarget);
    std::uint8_t insideRotatedClip[4]{};
    std::uint8_t outsideRotatedClip[4]{};
    CHECK(ReadBackPixel(
        backend, rotatedTarget.Value(), 80U, 48U, 11U, 13U, insideRotatedClip));
    CHECK(ReadBackPixel(
        backend, rotatedTarget.Value(), 80U, 48U, 17U, 17U, outsideRotatedClip));
    CHECK(insideRotatedClip[0] == 0U && insideRotatedClip[1] == 255U &&
        insideRotatedClip[2] == 0U && insideRotatedClip[3] == 255U);
    CHECK(outsideRotatedClip[0] >= 126U && outsideRotatedClip[0] <= 129U &&
        outsideRotatedClip[1] == 0U &&
        outsideRotatedClip[2] >= 126U && outsideRotatedClip[2] <= 129U &&
        outsideRotatedClip[3] == 255U);
    CHECK(surface.DiscardFrame(rotatedFrame));
    CHECK(device.DestroyResource(
        rotatedTarget.Value(), renderBackend.LastSubmittedFence()));

    RenderPlan instancedStrokePlan;
    CHECK(BuildPlan(instancedStrokePlan, 80U, 48U, false, true));
    CHECK(renderBackend.Submit(instancedStrokePlan));
    const D3D11RenderPlanSubmitStatistics strokePlanStatistics =
        renderBackend.LastSubmitStatistics();
    CHECK(strokePlanStatistics.renderPassCount == 1U);
    CHECK(strokePlanStatistics.drawCallCount == 2U);
    CHECK(strokePlanStatistics.rectangleInstanceCount == 5U);
    CHECK(strokePlanStatistics.uniformBufferUploadCount == 2U);
    CHECK(strokePlanStatistics.pipelineBindingCount == 1U);
    CHECK(strokePlanStatistics.vertexBufferBindingCount == 1U);
    CHECK(strokePlanStatistics.uniformBufferBindingCount == 1U);
    CHECK(renderBackend.LastSubmittedFence() == 4U);
    CHECK(backend.WaitForFence(renderBackend.LastSubmittedFence()));
    Result<SurfaceFrame> strokeFrameResult = surface.AcquireFrame();
    CHECK(strokeFrameResult);
    SurfaceFrame strokeFrame = strokeFrameResult.Value();
    Result<ResourceHandle> strokeTarget = ImportD3D11ExternalRenderTarget(
        device, backend, MakeImportDescriptor(strokeFrame));
    CHECK(strokeTarget);
    std::uint8_t strokePixel[4]{};
    std::uint8_t strokeInteriorPixel[4]{};
    CHECK(ReadBackPixel(
        backend, strokeTarget.Value(), 80U, 48U, 4U, 3U, strokePixel));
    CHECK(ReadBackPixel(
        backend, strokeTarget.Value(), 80U, 48U, 8U, 7U,
        strokeInteriorPixel));
    CHECK(strokePixel[0] == 0U && strokePixel[1] == 0U &&
        strokePixel[2] == 255U && strokePixel[3] == 255U);
    CHECK(strokeInteriorPixel[0] == 0U && strokeInteriorPixel[1] == 0U &&
        strokeInteriorPixel[2] == 0U && strokeInteriorPixel[3] == 0U);
    CHECK(surface.DiscardFrame(strokeFrame));
    CHECK(device.DestroyResource(
        strokeTarget.Value(), renderBackend.LastSubmittedFence()));

    renderBackend.Shutdown();
    CHECK(device.CollectGarbage());

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
    CHECK(presented.Value() == 5U);
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

    DeviceRemovedSwapChain removedSwapChain(*reinterpret_cast<IDXGISwapChain*>(
        surfaceBackend.NativeSwapChain()));
    NativeSurfaceDescriptor terminalDescriptor = restoreDescriptor;
    terminalDescriptor.ownership = SurfaceOwnership::Borrowed;
    terminalDescriptor.d3d11.swapChain = reinterpret_cast<std::uintptr_t>(
        static_cast<IDXGISwapChain*>(&removedSwapChain));
    D3D11SwapChainSurface terminalBackend(backend);
    SurfaceSession terminalSurface(terminalBackend);
    CHECK(terminalSurface.Initialize(terminalDescriptor));
    Result<SurfaceFrame> terminalFrameResult = terminalSurface.AcquireFrame();
    CHECK(terminalFrameResult);
    SurfaceFrame terminalFrame = terminalFrameResult.Value();
    Result<void> terminalPresent = terminalSurface.Present(
        terminalFrame, backend.LastSubmittedFence());
    CHECK(!terminalPresent);
    CHECK(terminalPresent.GetStatus().code == ErrorCode::InvalidState);
    CHECK(backend.IsDeviceLost());

    ResourceDescriptor postLossDescriptor;
    postLossDescriptor.type = ResourceType::Buffer;
    postLossDescriptor.buffer.sizeBytes = 16U;
    postLossDescriptor.buffer.usage = BufferUsage::Upload;
    CHECK(!device.CreateResource(postLossDescriptor));
    terminalSurface.Shutdown();

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
