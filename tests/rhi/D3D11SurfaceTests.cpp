#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Rhi/D3D11Backend.hpp>
#include <Aero/Render/D3D11RendererBackend.hpp>

#if defined(AERO_D3D11_TEXT_RENDER_TESTS)
#include <Aero/Render/D3D11TextBlockRenderService.hpp>
#include <Aero/Text/FreeTypeAdapter.hpp>
#include <Aero/Text/HarfBuzzAdapter.hpp>
#endif

#include <Aero/Base/Ref.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Controls/RuntimeMetadata.hpp>
#include <Aero/Presentation/Metadata.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlDependencyProperty.hpp>
#include <Aero/Markup/XamlNodeReader.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Markup/XamlVisualTree.hpp>
#include <Aero/Markup/XmlTokenizer.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Presentation;
using namespace Aero::Controls;
using namespace Aero::Markup;
using namespace Aero::Rhi;
using namespace Aero::Render;
#if defined(AERO_D3D11_TEXT_RENDER_TESTS)
using namespace Aero::Text;
#endif

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

class PlanPanel final : public FrameworkElement {
public:
    PlanPanel(
        TypeId type,
        bool requestInstancedStroke = false) noexcept
        : FrameworkElement(type),
          requestInstancedStroke_(requestInstancedStroke) {}

protected:
    Result<Size> MeasureOverride(Size available) noexcept override {
        for (UIElement* child : LayoutChildren()) {
            Result<void> measured = MeasureChild(*child, available);
            if (!measured) return measured.GetStatus();
        }
        return available;
    }

    Result<Size> ArrangeOverride(Size finalSize) noexcept override {
        for (UIElement* child : LayoutChildren()) {
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

class PlanElement final : public FrameworkElement {
public:
    PlanElement(
        TypeId type,
        bool requestNonAxisAlignedClip = false,
        bool requestFillBatch = false,
        bool requestSplitFillBatch = false,
        bool requestRoundedFill = false,
        RenderImageId image = InvalidRenderImageId,
        std::uint32_t imageCount = 1U,
        RenderMeshId mesh = InvalidRenderMeshId,
        std::uint32_t meshCount = 1U,
        RenderGlyphRunId glyphRun = InvalidRenderGlyphRunId,
        std::uint32_t glyphRunCount = 1U) noexcept
        : FrameworkElement(type),
          requestNonAxisAlignedClip_(requestNonAxisAlignedClip),
          requestFillBatch_(requestFillBatch),
          requestSplitFillBatch_(requestSplitFillBatch),
          requestRoundedFill_(requestRoundedFill),
          image_(image), imageCount_(imageCount), mesh_(mesh), meshCount_(meshCount),
          glyphRun_(glyphRun), glyphRunCount_(glyphRunCount) {}

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
        if (requestRoundedFill_) {
            result = builder.FillRoundedRect(
                {0.0, 0.0, 12.0, 8.0}, {1.0F, 0.0F, 0.0F, 1.0F}, 4.0);
            if (!result) return result;
        }
        if (image_ != InvalidRenderImageId) {
            for (std::uint32_t imageIndex = 0U;
                 imageIndex < imageCount_;
                 ++imageIndex) {
                const bool secondImage = imageIndex == 1U;
                const bool thirdImage = imageIndex == 2U;
                result = builder.DrawImage(image_,
                    thirdImage ? Rect{5.0, 1.0, 4.0, 4.0} :
                        Rect{1.0, 1.0, 4.0, 4.0},
                    secondImage ? Rect{0.5, 0.0, 0.5, 0.5} :
                        (thirdImage ? Rect{0.0, 0.5, 0.5, 0.5} :
                            Rect{0.0, 0.0, 0.5, 0.5}));
                if (!result) return result;
            }
        }
        if (mesh_ != InvalidRenderMeshId) {
            for (std::uint32_t meshIndex = 0U;
                 meshIndex < meshCount_;
                 ++meshIndex) {
                result = builder.DrawMesh(mesh_);
                if (!result) return result;
            }
        }
        if (glyphRun_ != InvalidRenderGlyphRunId) {
            for (std::uint32_t glyphIndex = 0U;
                 glyphIndex < glyphRunCount_;
                 ++glyphIndex) {
                result = builder.DrawGlyphRun(
                    glyphRun_, {1.0F, 0.0F, 0.0F, 1.0F});
                if (!result) return result;
            }
        }
        const std::uint32_t fillBatchRectCount = requestSplitFillBatch_
            ? 65U
            : (requestFillBatch_ ? 3U : 1U);
        for (std::uint32_t fillIndex = 1U;
             fillIndex < fillBatchRectCount;
             ++fillIndex) {
            const bool secondRect = fillIndex == 1U;
            const bool overlappingBlend = !requestSplitFillBatch_ &&
                !secondRect;
            result = builder.FillRect(
                secondRect ? Rect{12.0, 0.0, 6.0, 8.0} :
                    (overlappingBlend ? Rect{0.0, 0.0, 12.0, 8.0} :
                        Rect{0.0, 8.0, 6.0, 4.0}),
                secondRect ? Color{1.0F, 0.0F, 0.0F, 1.0F} :
                    (overlappingBlend ? Color{1.0F, 0.0F, 0.0F, 0.5F} :
                        Color{0.0F, 0.0F, 1.0F, 1.0F}));
            if (!result) return result;
        }
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
    bool requestFillBatch_ = false;
    bool requestSplitFillBatch_ = false;
    bool requestRoundedFill_ = false;
    RenderImageId image_ = InvalidRenderImageId;
    std::uint32_t imageCount_ = 1U;
    RenderMeshId mesh_ = InvalidRenderMeshId;
    std::uint32_t meshCount_ = 1U;
    RenderGlyphRunId glyphRun_ = InvalidRenderGlyphRunId;
    std::uint32_t glyphRunCount_ = 1U;
};

// This fixture deliberately uses the production controls, rather than the
// PlanPanel/PlanElement test renderables below. Its renderer is the live
// D3D11 RenderPlan backend, making the test a single XAML -> layout -> GPU
// frame path instead of joining independent unit-test results.
struct XamlControlFixture final {
    explicit XamlControlFixture(
        D3D11RenderPlanBackend& renderBackend) noexcept
        : renderBackend_(&renderBackend) {}

    Dispatcher dispatcher;
    MetadataDomain metadata;
    std::unique_ptr<MetadataRuntime> runtime;
    std::unique_ptr<ObjectServicesScope> services;
    std::unique_ptr<EffectiveValueEngine> values;
    std::unique_ptr<ObjectTree> tree;
    std::unique_ptr<LayoutManager> layout;
    std::unique_ptr<RenderManager> renderer;
    std::unique_ptr<XamlSchemaContext> schema;
    std::unique_ptr<XamlActivationProviderRegistry> activation;
    std::unique_ptr<XamlDependencyPropertyBridge> dependencyProperties;
    std::unique_ptr<XamlVisualTreeHost> visual;
    TypeId objectType = InvalidTypeId;
    TypeId doubleType = InvalidTypeId;
    TypeId stringType = InvalidTypeId;
    TypeId layoutType = InvalidTypeId;
    TypeId renderType = InvalidTypeId;
    TypeId stackPanelType = InvalidTypeId;
    TypeId borderType = InvalidTypeId;
    TypeId textBlockType = InvalidTypeId;

    bool Initialize() {
        CHECK(renderBackend_ != nullptr);
        CHECK(Aero::Controls::TryRegisterBuiltInUiMetadata(metadata));
        CHECK(metadata.Seal());
        objectType = BuiltinTypes::Object;
        doubleType = BuiltinTypes::Double;
        stringType = BuiltinTypes::String;
        layoutType = BuiltinTypes::UIElement;
        renderType = BuiltinTypes::FrameworkElement;
        stackPanelType = BuiltinTypes::StackPanel;
        borderType = BuiltinTypes::Border;
        textBlockType = BuiltinTypes::TextBlock;

        runtime = std::make_unique<MetadataRuntime>(metadata);
        services = std::make_unique<ObjectServicesScope>(
            dispatcher, metadata.DependencyProperties(), *runtime);
        values = std::make_unique<EffectiveValueEngine>(
            dispatcher, metadata.DependencyProperties());
        tree = std::make_unique<ObjectTree>(dispatcher, *values);
        layout = std::make_unique<LayoutManager>(dispatcher);
        renderer = std::make_unique<RenderManager>(
            dispatcher, *renderBackend_);
        schema = std::make_unique<XamlSchemaContext>(metadata, *runtime);
        activation = std::make_unique<XamlActivationProviderRegistry>(*schema);
        dependencyProperties = std::make_unique<XamlDependencyPropertyBridge>(
            *schema, metadata.DependencyProperties());
        visual = std::make_unique<XamlVisualTreeHost>(
            *tree, *layout, *values, renderer.get());

        CHECK(values->Initialize());
        CHECK(tree->Initialize());
        CHECK(layout->Initialize());
        CHECK(renderer->Initialize());
        CHECK(TryRegisterAeroPresentationXaml(
            *dependencyProperties, *activation, visual.get()));
        CHECK(runtime->Freeze());
        CHECK(schema->Freeze());
        CHECK(activation->Freeze());
        return true;
    }

    XamlActivationContext Activation() noexcept {
        XamlActivationContext context = XamlActivationContext::Create();
        context.dispatcher = &dispatcher;
        context.dependencyProperties = &metadata.DependencyProperties();
        return context;
    }

private:
    D3D11RenderPlanBackend* renderBackend_ = nullptr;
};

bool BuildPlan(
    RenderPlan& output,
    std::uint32_t width,
    std::uint32_t height,
    bool requestNonAxisAlignedClip = false,
    bool requestInstancedStroke = false,
    bool requestFillBatch = false,
    bool requestSplitFillBatch = false,
    bool requestRoundedFill = false,
    RenderImageId image = InvalidRenderImageId,
    std::uint32_t imageCount = 1U,
    RenderMeshId mesh = InvalidRenderMeshId,
    std::uint32_t meshCount = 1U,
    RenderGlyphRunId glyphRun = InvalidRenderGlyphRunId,
    std::uint32_t glyphRunCount = 1U) noexcept {
    TypeRegistry types;
    MetadataBehaviorRegistrationStore typesBehaviors{types};
    MetadataRegistrationTypes typesRegistration{types, typesBehaviors};
    DependencyPropertyRegistry properties(types, typesBehaviors);
    Dispatcher dispatcher;
    ObjectServicesScope presentation(dispatcher, properties);
    const StringView ns("urn:aero-d3d11-test");
    const TypeId objectType = MakeTypeId(ns, StringView("Object"));
    const TypeId panelType = MakeTypeId(ns, StringView("PlanPanel"));
    const TypeId elementType = MakeTypeId(ns, StringView("PlanElement"));
    CHECK(typesRegistration.TryRegisterType(TypeRegistration::Object(ns, StringView("Object"), InvalidTypeId, TypeFlags::None, nullptr)));
    CHECK(typesRegistration.TryRegisterType(TypeRegistration::Object(ns, StringView("PlanPanel"), objectType, TypeFlags::None, nullptr)));
    CHECK(typesRegistration.TryRegisterType(TypeRegistration::Object(ns, StringView("PlanElement"), objectType, TypeFlags::None, nullptr)));
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
    PlanPanel root(panelType, requestInstancedStroke);
    PlanElement child(
        elementType, requestNonAxisAlignedClip,
        requestFillBatch, requestSplitFillBatch, requestRoundedFill, image,
        imageCount, mesh, meshCount, glyphRun, glyphRunCount);
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
    std::uint32_t expectedCommandCount =
        (requestInstancedStroke ? 6U :
            (requestNonAxisAlignedClip ? 13U :
                (requestSplitFillBatch ? 73U :
                    (requestFillBatch ? 11U :
                        (requestRoundedFill ? 10U : 9U)))));
    if (image != InvalidRenderImageId) {
        expectedCommandCount += imageCount;
    }
    if (mesh != InvalidRenderMeshId) {
        expectedCommandCount += meshCount;
    }
    if (glyphRun != InvalidRenderGlyphRunId) {
        expectedCommandCount += glyphRunCount;
    }
    CHECK(output.Commands().Size() == expectedCommandCount);
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

Result<FenceValue> UploadTestImage(
    RhiDevice& device,
    ResourceHandle texture) noexcept {
    static constexpr std::uint8_t Pixels[] = {
        255U, 0U, 0U, 255U,
        0U, 255U, 0U, 255U,
        0U, 0U, 255U, 255U,
        255U, 255U, 255U, 255U};
    GraphicsCommandEncoder encoder;
    Result<void> uploaded = encoder.UploadTexture(
        texture, {0U, 0U, 2U, 2U, 0U, 0U, 8U},
        Span<const std::uint8_t>(Pixels, static_cast<std::uint32_t>(sizeof(Pixels))));
    if (!uploaded) {
        return uploaded.GetStatus();
    }
    Result<GraphicsCommandBuffer> commands = encoder.Finish();
    if (!commands) {
        return commands.GetStatus();
    }
    return device.Submit(commands.Value());
}

struct TestMeshVertex final {
    float x;
    float y;
    float red;
    float green;
    float blue;
    float alpha;
};

struct TestGlyphVertex final {
    float x;
    float y;
    float u;
    float v;
};

Result<FenceValue> UploadTestMesh(
    RhiDevice& device,
    ResourceHandle vertexBuffer,
    ResourceHandle indexBuffer) noexcept {
    static constexpr TestMeshVertex Vertices[] = {
        {1.0F, 1.0F, 1.0F, 0.0F, 0.0F, 1.0F},
        {6.0F, 1.0F, 1.0F, 0.0F, 0.0F, 1.0F},
        {1.0F, 6.0F, 1.0F, 0.0F, 0.0F, 1.0F}};
    static constexpr std::uint32_t Indices[] = {0U, 1U, 2U};
    GraphicsCommandEncoder encoder;
    const auto* vertexBytes = reinterpret_cast<const std::uint8_t*>(Vertices);
    Result<void> uploaded = encoder.UploadBuffer(vertexBuffer, 0U,
        {vertexBytes, static_cast<std::uint32_t>(sizeof(Vertices))});
    if (uploaded) {
        const auto* indexBytes = reinterpret_cast<const std::uint8_t*>(Indices);
        uploaded = encoder.UploadBuffer(indexBuffer, 0U,
            {indexBytes, static_cast<std::uint32_t>(sizeof(Indices))});
    }
    if (!uploaded) return uploaded.GetStatus();
    Result<GraphicsCommandBuffer> commands = encoder.Finish();
    if (!commands) return commands.GetStatus();
    return device.Submit(commands.Value());
}

Result<FenceValue> UploadTestGlyph(
    RhiDevice& device,
    ResourceHandle vertexBuffer,
    ResourceHandle indexBuffer,
    ResourceHandle atlasTexture) noexcept {
    static constexpr TestGlyphVertex Vertices[] = {
        {1.0F, 1.0F, 0.5F, 0.5F},
        {6.0F, 1.0F, 0.5F, 0.5F},
        {1.0F, 6.0F, 0.5F, 0.5F}};
    static constexpr std::uint16_t Indices[] = {0U, 1U, 2U};
    static constexpr std::uint8_t Atlas[] = {128U};
    GraphicsCommandEncoder encoder;
    const auto* vertexBytes = reinterpret_cast<const std::uint8_t*>(Vertices);
    Result<void> uploaded = encoder.UploadBuffer(vertexBuffer, 0U,
        {vertexBytes, static_cast<std::uint32_t>(sizeof(Vertices))});
    if (uploaded) {
        const auto* indexBytes = reinterpret_cast<const std::uint8_t*>(Indices);
        uploaded = encoder.UploadBuffer(indexBuffer, 0U,
            {indexBytes, static_cast<std::uint32_t>(sizeof(Indices))});
    }
    if (uploaded) {
        uploaded = encoder.UploadTexture(atlasTexture, {0U, 0U, 1U, 1U, 0U, 0U, 1U},
            Span<const std::uint8_t>(Atlas, static_cast<std::uint32_t>(sizeof(Atlas))));
    }
    if (!uploaded) return uploaded.GetStatus();
    Result<GraphicsCommandBuffer> commands = encoder.Finish();
    if (!commands) return commands.GetStatus();
    return device.Submit(commands.Value());
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

bool TestXamlStackPanelBorderD3D11Presentation(
    RhiDevice& device,
    D3D11GraphicsBackend& backend,
    SurfaceSession& surface,
    D3D11RenderPlanBackend& renderBackend,
    RenderGlyphRunId glyphRun) {
    XamlControlFixture fixture(renderBackend);
    CHECK(fixture.Initialize());

    DiagnosticBag diagnostics;
    Utf8XmlTokenizer tokenizer;
    CHECK(tokenizer.Reset(StringView(
        "<StackPanel xmlns=\"urn:aero\"><Border Width=\"24\" Height=\"16\" "
        "Background=\"#FF0000\"/><TextBlock Text=\"A\"/>"
        "</StackPanel>"),
        &diagnostics));
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(*fixture.schema, &diagnostics);
    Result<Ref<Object>> loaded = LoadXamlVisualTreeWithActivation(
        *fixture.visual,
        writer,
        reader,
        *fixture.activation,
        fixture.Activation());
    CHECK(loaded && diagnostics.Size() == 0U);
    StackPanel* root = static_cast<StackPanel*>(loaded.Value().Get());
    CHECK(root != nullptr);
    CHECK(fixture.visual->Mount(*root, fixture.stackPanelType, {80.0, 48.0}));
    const Span<Visual* const> children = root->VisualChildren();
    CHECK(children.Size() == 2U);
    Border* border = static_cast<Border*>(children[0]);
    TextBlock* text = static_cast<TextBlock*>(children[1]);
    CHECK(border != nullptr && text != nullptr);
    CHECK(border->HasWidth() && border->Width() == 24.0);
    CHECK(border->HasHeight() && border->Height() == 16.0);
    CHECK(border->Background().red == 1.0F && border->Background().green == 0.0F &&
        border->Background().blue == 0.0F && border->Background().alpha == 1.0F);
    CHECK(text->Text() == StringView("A"));
    CHECK(text->SetGlyphRun(glyphRun, {6.0, 6.0}));
    CHECK(text->SetForeground({0.0F, 0.0F, 1.0F, 1.0F}));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::Layout));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::RenderCommit));
    CHECK(fixture.renderer->CurrentPlan().Nodes().Size() == 3U);
    const D3D11RenderPlanSubmitStatistics statistics =
        renderBackend.LastSubmitStatistics();
    CHECK(statistics.renderPassCount == 1U);
    CHECK(statistics.drawCallCount == 2U);
    CHECK(statistics.rectangleInstanceCount == 1U);
    CHECK(statistics.glyphDrawCallCount == 1U);
    CHECK(statistics.glyphInstanceCount == 1U);
    CHECK(backend.WaitForFence(renderBackend.LastSubmittedFence()));

    Result<SurfaceFrame> frameResult = surface.AcquireFrame();
    CHECK(frameResult);
    SurfaceFrame frame = frameResult.Value();
    Result<ResourceHandle> target = ImportD3D11ExternalRenderTarget(
        device, backend, MakeImportDescriptor(frame));
    CHECK(target);
    std::uint8_t pixel[4]{};
    CHECK(ReadBackPixel(backend, target.Value(), 80U, 48U, 4U, 4U, pixel));
    CHECK(pixel[0] == 0U && pixel[1] == 0U &&
        pixel[2] == 255U && pixel[3] == 255U);
    CHECK(surface.DiscardFrame(frame));
    CHECK(device.DestroyResource(
        target.Value(), renderBackend.LastSubmittedFence()));
    CHECK(fixture.visual->Unmount());
    return true;
}

#if defined(AERO_D3D11_TEXT_RENDER_TESTS)
bool TestAutomaticTextBlockD3D11Presentation(
    RhiDevice& device,
    D3D11GraphicsBackend& backend,
    SurfaceSession& surface,
    D3D11RenderPlanBackend& renderBackend) {
    FreeTypeAdapter fontProvider;
    CHECK(fontProvider.Initialize());
    HarfBuzzAdapter shaper(fontProvider);
    FontManager fonts;
    CHECK(fonts.Initialize());
    CHECK(fonts.RegisterProvider(
        {&fontProvider, &shaper, &fontProvider}));

    Typeface latinTypeface;
    CHECK(latinTypeface.TrySetFamily("Roboto"));
    CHECK(latinTypeface.TrySetLanguage("en"));
    FontSource latinSource;
    latinSource.kind = FontSourceKind::File;
    latinSource.identifier = AERO_TEXT_TEST_FONT;
    FontFace latinFace;
    CHECK(fonts.LoadFace(
        fontProvider.Identity().id,
        latinSource, latinTypeface, latinFace));

    Typeface cjkTypeface;
    CHECK(cjkTypeface.TrySetFamily("Mplus"));
    CHECK(cjkTypeface.TrySetLanguage("zh-CN"));
    FontSource cjkSource;
    cjkSource.kind = FontSourceKind::File;
    cjkSource.identifier = AERO_TEXT_TEST_CJK_FONT;
    FontFace cjkFace;
    CHECK(fonts.LoadFace(
        fontProvider.Identity().id,
        cjkSource, cjkTypeface, cjkFace));

    D3D11GlyphRunResourceRegistry registry(renderBackend);
    TextBlockRenderService textService(
        fonts, device, backend, registry);
    TextBlockRenderServiceConfig config;
    config.face = latinFace;
    config.fallbackFaces = {&cjkFace, 1U};
    config.pixelSize = 20.0F;
    config.atlas.pageWidth = 256U;
    config.atlas.pageHeight = 256U;
    config.atlas.maxPages = 2U;
    CHECK(textService.Initialize(config));

    {
        TextBlockLayoutServiceScope textScope(textService);
        XamlControlFixture fixture(renderBackend);
        CHECK(fixture.Initialize());
        DiagnosticBag diagnostics;
        Utf8XmlTokenizer tokenizer;
        CHECK(tokenizer.Reset(StringView(
            "<TextBlock xmlns=\"urn:aero\" "
            "Text=\"A1&#x4E2D;&#x6587;\"/>"),
            &diagnostics));
        XamlNodeReader reader(tokenizer, &diagnostics);
        XamlObjectWriter writer(*fixture.schema, &diagnostics);
        Result<Ref<Object>> loaded =
            LoadXamlVisualTreeWithActivation(
                *fixture.visual,
                writer,
                reader,
                *fixture.activation,
                fixture.Activation());
        CHECK(loaded && diagnostics.Size() == 0U);
        TextBlock* text =
            static_cast<TextBlock*>(loaded.Value().Get());
        CHECK(text != nullptr);
        CHECK(text->LayoutService() == &textService);
        CHECK(text->SetForeground(
            {0.0F, 0.0F, 1.0F, 1.0F}));
        CHECK(fixture.visual->Mount(
            *text, fixture.textBlockType,
            {80.0, 48.0}));
        CHECK(fixture.dispatcher.RunFramePhase(
            DispatcherFramePhase::Layout));
        CHECK(text->DesiredSize().width > 0.0);
        CHECK(text->DesiredSize().height > 0.0);
        CHECK(!text->GlyphRuns().Empty());
        CHECK(fixture.dispatcher.RunFramePhase(
            DispatcherFramePhase::RenderCommit));
        const D3D11RenderPlanSubmitStatistics statistics =
            renderBackend.LastSubmitStatistics();
        CHECK(statistics.glyphDrawCallCount >= 1U);
        CHECK(statistics.glyphInstanceCount >= 1U);
        CHECK(backend.WaitForFence(
            renderBackend.LastSubmittedFence()));

        Result<SurfaceFrame> frameResult =
            surface.AcquireFrame();
        CHECK(frameResult);
        SurfaceFrame frame = frameResult.Value();
        Result<ResourceHandle> target =
            ImportD3D11ExternalRenderTarget(
                device, backend,
                MakeImportDescriptor(frame));
        CHECK(target);
        constexpr std::uint32_t Width = 80U;
        constexpr std::uint32_t Height = 48U;
        std::uint8_t pixels[Width * Height * 4U]{};
        CHECK(backend.ReadbackTexture(
            target.Value(),
            Span<std::uint8_t>(
                pixels,
                static_cast<std::uint32_t>(
                    sizeof(pixels))),
            Width * 4U));
        bool foundBlueCoverage = false;
        for (std::uint32_t offset = 0U;
             offset < sizeof(pixels);
             offset += 4U) {
            if (pixels[offset] > 20U &&
                pixels[offset] > pixels[offset + 1U] &&
                pixels[offset] > pixels[offset + 2U]) {
                foundBlueCoverage = true;
                break;
            }
        }
        CHECK(foundBlueCoverage);
        CHECK(surface.DiscardFrame(frame));
        CHECK(device.DestroyResource(
            target.Value(),
            renderBackend.LastSubmittedFence()));
        CHECK(fixture.visual->Unmount());
    }

    CHECK(backend.WaitForFence(
        renderBackend.LastSubmittedFence()));
    CHECK(textService.CollectGarbage());
    textService.Shutdown();
    CHECK(device.CollectGarbage());
    CHECK(fonts.ReleaseFace(latinFace.handle));
    CHECK(fonts.ReleaseFace(cjkFace.handle));
    fonts.Shutdown();
    fontProvider.Shutdown();
    return true;
}
#endif

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

    Result<FenceValue> submitted = device.Submit(clearCommands.Value());
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
    D3D11RenderPlanBackend renderBackend(device, presenter);
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

    RenderPlan fillBatchPlan;
    CHECK(BuildPlan(fillBatchPlan, 80U, 48U, false, false, true));
    CHECK(renderBackend.Submit(fillBatchPlan));
    const D3D11RenderPlanSubmitStatistics fillBatchStatistics =
        renderBackend.LastSubmitStatistics();
    CHECK(fillBatchStatistics.renderPassCount == 1U);
    CHECK(fillBatchStatistics.drawCallCount == 3U);
    CHECK(fillBatchStatistics.rectangleInstanceCount == 5U);
    CHECK(fillBatchStatistics.uniformBufferUploadCount == 3U);
    CHECK(fillBatchStatistics.pipelineBindingCount == 1U);
    CHECK(fillBatchStatistics.vertexBufferBindingCount == 1U);
    CHECK(fillBatchStatistics.uniformBufferBindingCount == 1U);
    CHECK(renderBackend.LastSubmittedFence() == 5U);
    CHECK(backend.WaitForFence(renderBackend.LastSubmittedFence()));
    Result<SurfaceFrame> fillBatchFrameResult = surface.AcquireFrame();
    CHECK(fillBatchFrameResult);
    SurfaceFrame fillBatchFrame = fillBatchFrameResult.Value();
    Result<ResourceHandle> fillBatchTarget = ImportD3D11ExternalRenderTarget(
        device, backend, MakeImportDescriptor(fillBatchFrame));
    CHECK(fillBatchTarget);
    std::uint8_t fillBatchPixel[4]{};
    CHECK(ReadBackPixel(
        backend, fillBatchTarget.Value(), 80U, 48U, 14U, 9U,
        fillBatchPixel));
    CHECK(fillBatchPixel[0] == 0U &&
        fillBatchPixel[1] >= 126U && fillBatchPixel[1] <= 129U &&
        fillBatchPixel[2] >= 126U && fillBatchPixel[2] <= 129U &&
        fillBatchPixel[3] == 255U);
    CHECK(surface.DiscardFrame(fillBatchFrame));
    CHECK(device.DestroyResource(
        fillBatchTarget.Value(), renderBackend.LastSubmittedFence()));

    RenderPlan splitFillBatchPlan;
    CHECK(BuildPlan(splitFillBatchPlan, 80U, 48U, false, false, false, true));
    CHECK(renderBackend.Submit(splitFillBatchPlan));
    const D3D11RenderPlanSubmitStatistics splitFillBatchStatistics =
        renderBackend.LastSubmitStatistics();
    CHECK(splitFillBatchStatistics.renderPassCount == 1U);
    CHECK(splitFillBatchStatistics.drawCallCount == 4U);
    CHECK(splitFillBatchStatistics.rectangleInstanceCount == 67U);
    CHECK(splitFillBatchStatistics.uniformBufferUploadCount == 4U);
    CHECK(splitFillBatchStatistics.pipelineBindingCount == 1U);
    CHECK(splitFillBatchStatistics.vertexBufferBindingCount == 1U);
    CHECK(splitFillBatchStatistics.uniformBufferBindingCount == 1U);
    CHECK(renderBackend.LastSubmittedFence() == 6U);
    CHECK(backend.WaitForFence(renderBackend.LastSubmittedFence()));
    Result<SurfaceFrame> splitFillBatchFrameResult = surface.AcquireFrame();
    CHECK(splitFillBatchFrameResult);
    SurfaceFrame splitFillBatchFrame = splitFillBatchFrameResult.Value();
    Result<ResourceHandle> splitFillBatchTarget = ImportD3D11ExternalRenderTarget(
        device, backend, MakeImportDescriptor(splitFillBatchFrame));
    CHECK(splitFillBatchTarget);
    std::uint8_t splitFillBatchPixel[4]{};
    CHECK(ReadBackPixel(
        backend, splitFillBatchTarget.Value(), 80U, 48U, 14U, 9U,
        splitFillBatchPixel));
    CHECK(splitFillBatchPixel[0] == 0U && splitFillBatchPixel[1] == 255U &&
        splitFillBatchPixel[2] == 0U && splitFillBatchPixel[3] == 255U);
    CHECK(surface.DiscardFrame(splitFillBatchFrame));
    CHECK(device.DestroyResource(
        splitFillBatchTarget.Value(), renderBackend.LastSubmittedFence()));

    RenderPlan roundedFillPlan;
    CHECK(BuildPlan(
        roundedFillPlan, 80U, 48U, false, false, false, false, true));
    CHECK(renderBackend.Submit(roundedFillPlan));
    const D3D11RenderPlanSubmitStatistics roundedFillStatistics =
        renderBackend.LastSubmitStatistics();
    CHECK(roundedFillStatistics.renderPassCount == 1U);
    CHECK(roundedFillStatistics.drawCallCount == 3U);
    CHECK(roundedFillStatistics.rectangleInstanceCount == 4U);
    CHECK(roundedFillStatistics.uniformBufferUploadCount == 3U);
    CHECK(roundedFillStatistics.pipelineBindingCount == 1U);
    CHECK(roundedFillStatistics.vertexBufferBindingCount == 1U);
    CHECK(roundedFillStatistics.uniformBufferBindingCount == 1U);
    CHECK(renderBackend.LastSubmittedFence() == 7U);
    CHECK(backend.WaitForFence(renderBackend.LastSubmittedFence()));
    Result<SurfaceFrame> roundedFillFrameResult = surface.AcquireFrame();
    CHECK(roundedFillFrameResult);
    SurfaceFrame roundedFillFrame = roundedFillFrameResult.Value();
    Result<ResourceHandle> roundedFillTarget = ImportD3D11ExternalRenderTarget(
        device, backend, MakeImportDescriptor(roundedFillFrame));
    CHECK(roundedFillTarget);
    std::uint8_t roundedCornerPixel[4]{};
    std::uint8_t roundedInteriorPixel[4]{};
    CHECK(ReadBackPixel(
        backend, roundedFillTarget.Value(), 80U, 48U, 12U, 8U,
        roundedCornerPixel));
    CHECK(ReadBackPixel(
        backend, roundedFillTarget.Value(), 80U, 48U, 22U, 12U,
        roundedInteriorPixel));
    CHECK(roundedCornerPixel[0] == 0U && roundedCornerPixel[1] == 255U &&
        roundedCornerPixel[2] == 0U && roundedCornerPixel[3] == 255U);
    CHECK(roundedInteriorPixel[0] == 0U && roundedInteriorPixel[1] == 0U &&
        roundedInteriorPixel[2] == 255U && roundedInteriorPixel[3] == 255U);
    CHECK(surface.DiscardFrame(roundedFillFrame));
    CHECK(device.DestroyResource(
        roundedFillTarget.Value(), renderBackend.LastSubmittedFence()));

    TextureResourceDescriptor imageDescriptor;
    imageDescriptor.width = 2U;
    imageDescriptor.height = 2U;
    imageDescriptor.format = GraphicsTextureFormat::Rgba8Unorm;
    imageDescriptor.usage = TextureUsageBit(TextureUsage::Sampled) |
        TextureUsageBit(TextureUsage::CopyDestination);
    Result<ResourceHandle> imageTexture = device.CreateTexture(imageDescriptor);
    CHECK(imageTexture);
    SamplerDescriptor imageSamplerDescriptor;
    imageSamplerDescriptor.minFilter = FilterMode::Nearest;
    imageSamplerDescriptor.magFilter = FilterMode::Nearest;
    imageSamplerDescriptor.mipFilter = FilterMode::Nearest;
    Result<ResourceHandle> imageSampler =
        device.CreateSampler(imageSamplerDescriptor);
    CHECK(imageSampler);
    Result<FenceValue> imageUpload = UploadTestImage(
        device, imageTexture.Value());
    CHECK(imageUpload);
    CHECK(imageUpload.Value() == 8U);
    CHECK(backend.WaitForFence(imageUpload.Value()));
    CHECK(renderBackend.RegisterImage(
        1U, imageTexture.Value(), imageSampler.Value()));
    CHECK(!renderBackend.RegisterImage(
        1U, imageTexture.Value(), imageSampler.Value()));

    RenderPlan imagePlan;
    CHECK(BuildPlan(
        imagePlan, 80U, 48U, false, false, false, false, false, 1U, 3U));
    CHECK(renderBackend.Submit(imagePlan));
    const D3D11RenderPlanSubmitStatistics imageStatistics =
        renderBackend.LastSubmitStatistics();
    CHECK(imageStatistics.renderPassCount == 1U);
    CHECK(imageStatistics.drawCallCount == 4U);
    CHECK(imageStatistics.rectangleInstanceCount == 3U);
    CHECK(imageStatistics.imageInstanceCount == 3U);
    CHECK(imageStatistics.uniformBufferUploadCount == 4U);
    CHECK(imageStatistics.pipelineBindingCount == 2U);
    CHECK(imageStatistics.vertexBufferBindingCount == 2U);
    CHECK(imageStatistics.uniformBufferBindingCount == 2U);
    CHECK(imageStatistics.textureSamplerBindingCount == 1U);
    CHECK(renderBackend.LastSubmittedFence() == 9U);
    CHECK(backend.WaitForFence(renderBackend.LastSubmittedFence()));
    Result<SurfaceFrame> imageFrameResult = surface.AcquireFrame();
    CHECK(imageFrameResult);
    SurfaceFrame imageFrame = imageFrameResult.Value();
    Result<ResourceHandle> imageTarget = ImportD3D11ExternalRenderTarget(
        device, backend, MakeImportDescriptor(imageFrame));
    CHECK(imageTarget);
    std::uint8_t imagePixel[4]{};
    std::uint8_t secondImagePixel[4]{};
    CHECK(ReadBackPixel(
        backend, imageTarget.Value(), 80U, 48U, 16U, 11U, imagePixel));
    CHECK(ReadBackPixel(
        backend, imageTarget.Value(), 80U, 48U, 22U, 11U, secondImagePixel));
    CHECK(imagePixel[0] == 0U && imagePixel[1] == 255U &&
        imagePixel[2] == 0U && imagePixel[3] == 255U);
    CHECK(secondImagePixel[0] == 255U && secondImagePixel[1] == 0U &&
        secondImagePixel[2] == 0U && secondImagePixel[3] == 255U);
    CHECK(surface.DiscardFrame(imageFrame));
    CHECK(device.DestroyResource(
        imageTarget.Value(), renderBackend.LastSubmittedFence()));

    RenderPlan splitImageBatchPlan;
    CHECK(BuildPlan(splitImageBatchPlan, 80U, 48U, false, false, false,
        false, false, 1U, 65U));
    CHECK(renderBackend.Submit(splitImageBatchPlan));
    const D3D11RenderPlanSubmitStatistics splitImageStatistics =
        renderBackend.LastSubmitStatistics();
    CHECK(splitImageStatistics.renderPassCount == 1U);
    CHECK(splitImageStatistics.drawCallCount == 5U);
    CHECK(splitImageStatistics.rectangleInstanceCount == 3U);
    CHECK(splitImageStatistics.imageInstanceCount == 65U);
    CHECK(splitImageStatistics.uniformBufferUploadCount == 5U);
    CHECK(splitImageStatistics.pipelineBindingCount == 2U);
    CHECK(splitImageStatistics.vertexBufferBindingCount == 2U);
    CHECK(splitImageStatistics.uniformBufferBindingCount == 2U);
    CHECK(splitImageStatistics.textureSamplerBindingCount == 2U);
    CHECK(renderBackend.LastSubmittedFence() == 10U);
    CHECK(backend.WaitForFence(renderBackend.LastSubmittedFence()));

    BufferDescriptor meshVertexDescriptor;
    meshVertexDescriptor.sizeBytes = sizeof(TestMeshVertex) * 3U;
    meshVertexDescriptor.usage = BufferUsage::Vertex;
    Result<ResourceHandle> meshVertex = device.CreateBuffer(meshVertexDescriptor);
    CHECK(meshVertex);
    BufferDescriptor meshIndexDescriptor;
    meshIndexDescriptor.sizeBytes = sizeof(std::uint32_t) * 3U;
    meshIndexDescriptor.usage = BufferUsage::Index;
    Result<ResourceHandle> meshIndex = device.CreateBuffer(meshIndexDescriptor);
    CHECK(meshIndex);
    Result<FenceValue> meshUpload = UploadTestMesh(
        device, meshVertex.Value(), meshIndex.Value());
    CHECK(meshUpload && meshUpload.Value() == 11U);
    CHECK(backend.WaitForFence(meshUpload.Value()));
    CHECK(renderBackend.RegisterMesh(
        1U, meshVertex.Value(), meshIndex.Value(), 3U, IndexType::UInt32));
    RenderPlan meshPlan;
    CHECK(BuildPlan(meshPlan, 80U, 48U, false, false, false, false,
        false, InvalidRenderImageId, 1U, 1U, 3U));
    CHECK(renderBackend.Submit(meshPlan));
    const D3D11RenderPlanSubmitStatistics meshStatistics =
        renderBackend.LastSubmitStatistics();
    CHECK(meshStatistics.drawCallCount == 4U);
    CHECK(meshStatistics.rectangleInstanceCount == 3U);
    CHECK(meshStatistics.meshDrawCallCount == 1U);
    CHECK(meshStatistics.meshInstanceCount == 3U);
    CHECK(meshStatistics.uniformBufferUploadCount == 4U);
    CHECK(meshStatistics.pipelineBindingCount == 2U);
    CHECK(meshStatistics.vertexBufferBindingCount == 2U);
    CHECK(meshStatistics.indexBufferBindingCount == 1U);
    CHECK(renderBackend.LastSubmittedFence() == 12U);
    CHECK(backend.WaitForFence(renderBackend.LastSubmittedFence()));
    Result<SurfaceFrame> meshFrameResult = surface.AcquireFrame();
    CHECK(meshFrameResult);
    SurfaceFrame meshFrame = meshFrameResult.Value();
    Result<ResourceHandle> meshTarget = ImportD3D11ExternalRenderTarget(
        device, backend, MakeImportDescriptor(meshFrame));
    CHECK(meshTarget);
    std::uint8_t meshPixel[4]{};
    CHECK(ReadBackPixel(
        backend, meshTarget.Value(), 80U, 48U, 16U, 11U, meshPixel));
    CHECK(meshPixel[0] == 0U && meshPixel[1] == 0U &&
        meshPixel[2] == 255U && meshPixel[3] == 255U);
    CHECK(surface.DiscardFrame(meshFrame));
    CHECK(device.DestroyResource(
        meshTarget.Value(), renderBackend.LastSubmittedFence()));
    RenderPlan splitMeshBatchPlan;
    CHECK(BuildPlan(splitMeshBatchPlan, 80U, 48U, false, false, false, false,
        false, InvalidRenderImageId, 1U, 1U, 65U));
    CHECK(renderBackend.Submit(splitMeshBatchPlan));
    const D3D11RenderPlanSubmitStatistics splitMeshStatistics =
        renderBackend.LastSubmitStatistics();
    CHECK(splitMeshStatistics.drawCallCount == 5U);
    CHECK(splitMeshStatistics.rectangleInstanceCount == 3U);
    CHECK(splitMeshStatistics.meshDrawCallCount == 2U);
    CHECK(splitMeshStatistics.meshInstanceCount == 65U);
    CHECK(splitMeshStatistics.uniformBufferUploadCount == 5U);
    CHECK(splitMeshStatistics.pipelineBindingCount == 2U);
    CHECK(splitMeshStatistics.vertexBufferBindingCount == 3U);
    CHECK(splitMeshStatistics.indexBufferBindingCount == 2U);
    CHECK(renderBackend.LastSubmittedFence() == 13U);
    CHECK(backend.WaitForFence(renderBackend.LastSubmittedFence()));
    BufferDescriptor glyphVertexDescriptor;
    glyphVertexDescriptor.sizeBytes = sizeof(TestGlyphVertex) * 3U;
    glyphVertexDescriptor.usage = BufferUsage::Vertex;
    Result<ResourceHandle> glyphVertex = device.CreateBuffer(glyphVertexDescriptor);
    CHECK(glyphVertex);
    BufferDescriptor glyphIndexDescriptor;
    glyphIndexDescriptor.sizeBytes = sizeof(std::uint16_t) * 3U;
    glyphIndexDescriptor.usage = BufferUsage::Index;
    Result<ResourceHandle> glyphIndex = device.CreateBuffer(glyphIndexDescriptor);
    CHECK(glyphIndex);
    TextureResourceDescriptor glyphAtlasDescriptor;
    glyphAtlasDescriptor.width = 1U;
    glyphAtlasDescriptor.height = 1U;
    glyphAtlasDescriptor.format = GraphicsTextureFormat::R8Unorm;
    glyphAtlasDescriptor.usage = TextureUsageBit(TextureUsage::Sampled) |
        TextureUsageBit(TextureUsage::CopyDestination);
    Result<ResourceHandle> glyphAtlas = device.CreateTexture(glyphAtlasDescriptor);
    CHECK(glyphAtlas);
    Result<FenceValue> glyphUpload = UploadTestGlyph(
        device, glyphVertex.Value(), glyphIndex.Value(), glyphAtlas.Value());
    CHECK(glyphUpload && glyphUpload.Value() == 14U);
    CHECK(backend.WaitForFence(glyphUpload.Value()));
    CHECK(renderBackend.RegisterGlyphRun(1U, glyphVertex.Value(), glyphIndex.Value(),
        3U, glyphAtlas.Value(), imageSampler.Value()));
    CHECK(!renderBackend.RegisterGlyphRun(1U, glyphVertex.Value(), glyphIndex.Value(),
        3U, glyphAtlas.Value(), imageSampler.Value()));
    RenderPlan glyphPlan;
    CHECK(BuildPlan(glyphPlan, 80U, 48U, false, false, false, false, false,
        InvalidRenderImageId, 1U, InvalidRenderMeshId, 1U, 1U, 3U));
    CHECK(renderBackend.Submit(glyphPlan));
    const D3D11RenderPlanSubmitStatistics glyphStatistics =
        renderBackend.LastSubmitStatistics();
    CHECK(glyphStatistics.drawCallCount == 4U);
    CHECK(glyphStatistics.rectangleInstanceCount == 3U);
    CHECK(glyphStatistics.glyphDrawCallCount == 1U);
    CHECK(glyphStatistics.glyphInstanceCount == 3U);
    CHECK(glyphStatistics.uniformBufferUploadCount == 4U);
    CHECK(glyphStatistics.pipelineBindingCount == 2U);
    CHECK(glyphStatistics.vertexBufferBindingCount == 2U);
    CHECK(glyphStatistics.indexBufferBindingCount == 1U);
    CHECK(glyphStatistics.textureSamplerBindingCount == 1U);
    CHECK(renderBackend.LastSubmittedFence() == 15U);
    CHECK(backend.WaitForFence(renderBackend.LastSubmittedFence()));
    Result<SurfaceFrame> glyphFrameResult = surface.AcquireFrame();
    CHECK(glyphFrameResult);
    SurfaceFrame glyphFrame = glyphFrameResult.Value();
    Result<ResourceHandle> glyphTarget = ImportD3D11ExternalRenderTarget(
        device, backend, MakeImportDescriptor(glyphFrame));
    CHECK(glyphTarget);
    std::uint8_t glyphPixel[4]{};
    CHECK(ReadBackPixel(
        backend, glyphTarget.Value(), 80U, 48U, 16U, 11U, glyphPixel));
    // Three same-run instances are batched into one indexed draw. Each uses
    // 128/255 atlas coverage, so red is 1 - (1 - 128/255)^3 and green is
    // the complementary contribution from the opaque green rectangle below.
    CHECK(glyphPixel[0] == 0U && glyphPixel[1] >= 26U &&
        glyphPixel[1] <= 37U && glyphPixel[2] >= 220U &&
        glyphPixel[2] <= 230U && glyphPixel[3] == 255U);
    CHECK(surface.DiscardFrame(glyphFrame));
    CHECK(device.DestroyResource(
        glyphTarget.Value(), renderBackend.LastSubmittedFence()));
    CHECK(TestXamlStackPanelBorderD3D11Presentation(
        device, backend, surface, renderBackend, 1U));
#if defined(AERO_D3D11_TEXT_RENDER_TESTS)
    CHECK(TestAutomaticTextBlockD3D11Presentation(
        device, backend, surface, renderBackend));
#endif
    CHECK(renderBackend.UnregisterGlyphRun(1U));
    CHECK(device.DestroyResource(
        glyphVertex.Value(), renderBackend.LastSubmittedFence()));
    CHECK(device.DestroyResource(
        glyphIndex.Value(), renderBackend.LastSubmittedFence()));
    CHECK(device.DestroyResource(
        glyphAtlas.Value(), renderBackend.LastSubmittedFence()));
    CHECK(renderBackend.UnregisterMesh(1U));
    CHECK(device.DestroyResource(
        meshVertex.Value(), renderBackend.LastSubmittedFence()));
    CHECK(device.DestroyResource(
        meshIndex.Value(), renderBackend.LastSubmittedFence()));
    CHECK(renderBackend.UnregisterImage(1U));
    CHECK(device.DestroyResource(
        imageTexture.Value(), renderBackend.LastSubmittedFence()));
    CHECK(device.DestroyResource(
        imageSampler.Value(), renderBackend.LastSubmittedFence()));

    const FenceValue expectedPresentedFence =
        renderBackend.LastSubmittedFence() + 1U;
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
    CHECK(presented.Value() == expectedPresentedFence);
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

bool TestFl10RenderPlanSurfaceSubmission() {
    HiddenWindow window;
    CHECK(window.Initialize());

    const D3D_FEATURE_LEVEL requestedLevel = D3D_FEATURE_LEVEL_10_0;
    ID3D11Device* nativeDevice = nullptr;
    ID3D11DeviceContext* nativeContext = nullptr;
    D3D_FEATURE_LEVEL createdLevel = D3D_FEATURE_LEVEL_9_1;
    const HRESULT created = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_WARP,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        &requestedLevel,
        1U,
        D3D11_SDK_VERSION,
        &nativeDevice,
        &createdLevel,
        &nativeContext);
    CHECK(SUCCEEDED(created));
    CHECK(nativeDevice != nullptr && nativeContext != nullptr);
    CHECK(createdLevel == D3D_FEATURE_LEVEL_10_0);

    D3D11BackendOptions backendOptions;
    backendOptions.deviceMode = D3D11DeviceMode::Borrowed;
    backendOptions.borrowedDevice = reinterpret_cast<std::uintptr_t>(nativeDevice);
    backendOptions.borrowedImmediateContext =
        reinterpret_cast<std::uintptr_t>(nativeContext);
    D3D11GraphicsBackend backend(backendOptions);
    CHECK(backend.Initialize());
    CHECK(backend.NativeFeatureLevel() ==
        static_cast<std::uint32_t>(D3D_FEATURE_LEVEL_10_0));

    RhiDevice device(backend);
    CHECK(device.Initialize());
    D3D11SwapChainSurface surfaceBackend(backend);
    SurfaceSession surface(surfaceBackend);
    CHECK(surface.Initialize(MakeSurfaceDescriptor(
        backend, window.Handle(), 64U, 48U)));
    D3D11SurfacePresenter presenter(device, backend, surface);
    CHECK(presenter.Initialize());
    D3D11RenderPlanBackend renderBackend(device, presenter);
    CHECK(renderBackend.Initialize());

    // Initialization creates every packaged SM4 RenderPlan pipeline, and the
    // submission exercises the rectangle path through a FL10_0 swap chain.
    RenderPlan renderPlan;
    CHECK(BuildPlan(renderPlan, 64U, 48U));
    CHECK(renderBackend.Submit(renderPlan));
    const D3D11RenderPlanSubmitStatistics statistics =
        renderBackend.LastSubmitStatistics();
    CHECK(statistics.renderPassCount == 1U);
    CHECK(statistics.drawCallCount == 3U);
    CHECK(statistics.rectangleInstanceCount == 3U);
    CHECK(backend.WaitForFence(renderBackend.LastSubmittedFence()));

    renderBackend.Shutdown();
    CHECK(device.CollectGarbage());
    presenter.Shutdown();
    surface.Shutdown();
    CHECK(device.LiveResourceCount() == 0U);
    CHECK(backend.LiveResourceCount() == 0U);
    backend.Shutdown();
    nativeContext->Release();
    nativeDevice->Release();
    return true;
}

} // namespace

int main() {
    if (!TestOwnedBorrowedResizeAndPresentation()) return 1;
    if (!TestFl10RenderPlanSurfaceSubmission()) return 1;
    std::puts("Aero D3D11 swap-chain surface tests passed");
    return 0;
}
