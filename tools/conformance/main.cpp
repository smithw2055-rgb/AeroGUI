#include <Aero/Controls.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/Gui.hpp>
#include <Aero/ViewOptions.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Markup/XamlProvider.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/View.hpp>

#include "render/FrameEncoder.hpp"
#include "gui/core/state/PropertyEngine.hpp"
#include "gui/ViewRenderer.hpp"
#include "render/opengl33/OpenGL33RenderDevice.hpp"

namespace Aero {
const ::Aero::Render::RenderFrame* CurrentFrameForConformance(
    const View& view) noexcept;
}

#if defined(_WIN32)
#include "render/platform/win32/OpenGLWindow.hpp"
#include "render/d3d11/D3D11RenderDevice.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef DeviceCapabilities
#undef DeviceCapabilities
#endif
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace {

int failures = 0;
std::uint32_t changedCount = 0U;

void Check(bool condition, const char* message) noexcept {
    if (condition) return;
    ++failures;
    std::fprintf(stderr, "conformance failure: %s\n", message);
}

void CountChanged(Aero::Freezable&) noexcept {
    ++changedCount;
}

class GraphFreezable final : public Aero::Freezable {
public:
    explicit GraphFreezable(bool rejectsFreeze = false) noexcept
        : Freezable(Aero::Freezable::StaticTypeId()),
          rejectsFreeze_(rejectsFreeze) {}

    void SetChild(GraphFreezable* child) noexcept { child_ = child; }

protected:
    bool FreezeCore(bool) noexcept override {
        return !rejectsFreeze_ &&
            (child_ == nullptr || child_->CanFreeze());
    }

private:
    GraphFreezable* child_ = nullptr;
    bool rejectsFreeze_ = false;
};

void VerifyFreezeGraph() noexcept {
    auto acceptedRef = Aero::Base::MakeRef<GraphFreezable>();
    auto rejectedRef = Aero::Base::MakeRef<GraphFreezable>(true);
    if (!acceptedRef || !rejectedRef) return;
    GraphFreezable* accepted = acceptedRef.Value().Get();
    GraphFreezable* rejected = rejectedRef.Value().Get();
    accepted->SetChild(rejected);
    Check(!accepted->CanFreeze(),
        "an unfreezable child must reject the whole graph");
    Check(!accepted->Freeze(),
        "Freeze must report an unfreezable child");
    Check(!accepted->IsFrozen() && !rejected->IsFrozen(),
        "failed Freeze must not partially freeze the graph");

    auto firstRef = Aero::Base::MakeRef<GraphFreezable>();
    auto secondRef = Aero::Base::MakeRef<GraphFreezable>();
    if (!firstRef || !secondRef) return;
    GraphFreezable* first = firstRef.Value().Get();
    GraphFreezable* second = secondRef.Value().Get();
    first->SetChild(second);
    second->SetChild(first);
    Check(!first->CanFreeze(),
        "CanFreeze must reject an object graph cycle");
    Check(!first->Freeze(),
        "Freeze must reject an object graph cycle");
}

Aero::Base::Result<Aero::Meta::PropertyValue> EvaluateFreezeExpression(
    void*,
    Aero::DependencyObject&,
    Aero::Meta::DependencyPropertyHandle) noexcept {
    return Aero::Meta::PropertyValue::FromDouble(
        Aero::Meta::TypeOf<double>(), 0.5);
}

void VerifyExpressionFreezeRejection() noexcept {
    Aero::Base::Result<Aero::Base::Ref<Aero::Media::GradientStop>> made =
        Aero::Base::MakeRef<Aero::Media::GradientStop>();
    Check(made.HasValue(), "expression freeze probe allocation failed");
    if (!made) return;
    Aero::Base::Ref<Aero::Media::GradientStop> stop =
        std::move(made).Value();

    Aero::Meta::EffectiveValueEngine values(
        stop->GetDispatcher(), stop->PropertyRegistry());
    Aero::Base::Result<void> initialized = values.Initialize();
    Check(initialized.HasValue(),
        "expression freeze value engine initialization failed");
    if (!initialized) return;

    Aero::Meta::PropertyExpression expression;
    expression.evaluate = &EvaluateFreezeExpression;
    for (Aero::Meta::PropertyExpressionKind kind : {
             Aero::Meta::PropertyExpressionKind::Binding,
             Aero::Meta::PropertyExpressionKind::DynamicResource}) {
        expression.kind = kind;
        Aero::Base::Result<void> attached = values.SetLocalExpression(
            *stop, Aero::Media::GradientStop::OffsetProperty.Handle(),
            expression);
        Check(attached.HasValue(),
            "expression freeze probe attachment failed");
        Check(!stop->CanFreeze() && !stop->Freeze() &&
                !stop->IsFrozen(),
            "Binding or DynamicResource expression was accepted by Freeze");
        Check(values.ClearLocalExpression(
                *stop,
                Aero::Media::GradientStop::OffsetProperty.Handle()).HasValue(),
            "expression freeze probe cleanup failed");
    }

    const Aero::Meta::PropertyValue animated =
        Aero::Meta::PropertyValue::FromDouble(
            Aero::Meta::TypeOf<double>(), 0.75);
    Check(values.SetAnimationValue(
            *stop,
            Aero::Media::GradientStop::OffsetProperty.Handle(),
            animated).HasValue(),
        "animation freeze probe attachment failed");
    Check(!stop->CanFreeze() && !stop->Freeze() && !stop->IsFrozen(),
        "animated value was accepted by Freeze");
    Check(values.ClearAnimationValue(
            *stop,
            Aero::Media::GradientStop::OffsetProperty.Handle()).HasValue(),
        "animation freeze probe cleanup failed");
    Check(stop->CanFreeze(),
        "Freezable remained blocked after expression cleanup");
    Check(values.DetachObject(*stop).HasValue(),
        "expression freeze probe detach failed");
}

void VerifySharedConsumers() noexcept {
    Aero::Base::Result<Aero::Base::Ref<Aero::Media::SolidColorBrush>> made =
        Aero::Base::MakeRef<Aero::Media::SolidColorBrush>();
    Check(made.HasValue(), "shared brush allocation failed");
    if (!made) return;

    Aero::Base::Ref<Aero::Media::SolidColorBrush> brush =
        std::move(made).Value();
    Aero::Base::Ref<Aero::Media::Brush> shared(brush);
    auto firstRef = Aero::Base::MakeRef<Aero::Controls::Border>();
    auto secondRef = Aero::Base::MakeRef<Aero::Controls::Border>();
    if (!firstRef || !secondRef) return;
    Aero::Controls::Border& first = *firstRef.Value();
    Aero::Controls::Border& second = *secondRef.Value();
    first.SetBackground(shared);
    second.SetBackground(shared);
    static_cast<void>(first.TakeInvalidations());
    static_cast<void>(second.TakeInvalidations());

    brush->SetColor({1.0F, 0.0F, 0.0F, 1.0F});
    Check(first.PendingInvalidations() !=
            Aero::Meta::PropertyInvalidationFlags::None,
        "first shared-brush consumer was not invalidated");
    Check(second.PendingInvalidations() !=
            Aero::Meta::PropertyInvalidationFlags::None,
        "second shared-brush consumer was not invalidated");

    first.SetBackground({});
    static_cast<void>(first.TakeInvalidations());
    static_cast<void>(second.TakeInvalidations());
    brush->SetColor({0.0F, 1.0F, 0.0F, 1.0F});
    Check(first.PendingInvalidations() ==
            Aero::Meta::PropertyInvalidationFlags::None,
        "detached shared-brush consumer was still invalidated");
    Check(second.PendingInvalidations() !=
            Aero::Meta::PropertyInvalidationFlags::None,
        "remaining shared-brush consumer lost invalidation");
}

void VerifyGradientFreeze() noexcept {
    Aero::Base::Result<Aero::Base::Ref<Aero::Media::GradientStop>> stopMade =
        Aero::Base::MakeRef<Aero::Media::GradientStop>();
    Check(stopMade.HasValue(), "gradient stop allocation failed");
    if (!stopMade) return;
    Aero::Base::Ref<Aero::Media::GradientStop> stop =
        std::move(stopMade).Value();
    Check(Aero::Media::GradientStop::OffsetProperty.Handle().IsValid(),
        "GradientStop.Offset dependency property is not bound");

    auto brushRef = Aero::Base::MakeRef<Aero::Media::LinearGradientBrush>();
    if (!brushRef) return;
    Aero::Media::LinearGradientBrush& brush = *brushRef.Value();
    changedCount = 0U;
    Aero::FreezableChangedHandler handler(&CountChanged);
    Check(brush.AddChangedHandlerChecked(handler).HasValue(),
        "Freezable Changed subscription failed");
    Check(brush.AddGradientStop(stop).HasValue(),
        "gradient stop attachment failed");
    const std::uint32_t beforeChildChange = changedCount;
    Aero::Base::Result<void> changed = stop->SetValueChecked(
        Aero::Media::GradientStop::OffsetProperty, 0.25);
    if (!changed) {
        std::fprintf(stderr, "GradientStop.Offset status before freeze: %u\n",
            static_cast<unsigned>(changed.GetStatus().code));
    }
    Check(changed.HasValue(),
        "checked GradientStop mutation failed before Freeze");
    Check(changedCount > beforeChildChange,
        "GradientStop change did not propagate through GradientBrush");

    const std::uint32_t beforeFreeze = changedCount;
    Check(brush.CanFreeze(), "valid gradient graph must be freezable");
    Check(brush.Freeze().HasValue(), "valid gradient graph failed to freeze");
    Check(brush.IsFrozen() && stop->IsFrozen(),
        "Freeze did not recursively commit the gradient graph");
    Check(changedCount == beforeFreeze + 1U,
        "Freeze must publish exactly one final Changed notification");

    const double frozenOffset = stop->GetOffset();
    Aero::Base::Result<void> rejected = stop->SetValueChecked(
        Aero::Media::GradientStop::OffsetProperty, 0.75);
    if (rejected || rejected.GetStatus().code !=
            Aero::Base::ErrorCode::ReadOnly) {
        std::fprintf(stderr, "GradientStop.Offset status after freeze: %u\n",
            static_cast<unsigned>(rejected.GetStatus().code));
    }
    Check(!rejected &&
            rejected.GetStatus().code == Aero::Base::ErrorCode::ReadOnly,
        "checked mutation of a frozen object must return ReadOnly");
    stop->SetOffset(0.75);
    Check(std::fabs(stop->GetOffset() - frozenOffset) < 0.000001,
        "void mutation changed a frozen object");
    Check(brush.Freeze().HasValue(), "Freeze must be idempotent");
}

void VerifyViewport(Aero::View& view) noexcept {
    const Aero::ViewViewport expected{
        {800.0, 450.0}, 1600U, 900U, 2.0};
    Check(view.SetViewport(expected).HasValue(),
        "View rejected a valid atomic viewport");
    const Aero::ViewViewport invalid{
        {0.0, 450.0}, 1U, 900U, 2.0};
    Check(!view.SetViewport(invalid),
        "View accepted a partially inconsistent viewport");
    view.Update(0.0);
    const ::Aero::Render::RenderFrame* actual =
        Aero::CurrentFrameForConformance(view);
    Check(actual != nullptr &&
            actual->LogicalSize().width == 800.0 &&
            actual->LogicalSize().height == 450.0 &&
            actual->PixelWidth() == 1600U &&
            actual->PixelHeight() == 900U &&
            actual->DpiScale() == 2.0,
        "View viewport did not preserve logical, pixel, and DPI values");
}

constexpr char GuiFacadeXaml[] = R"XAML(
<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      x:Name="FacadeRoot">
  <Border x:Name="FacadeChild"/>
</Grid>
)XAML";

class StaticXamlStream final : public Aero::Base::Stream {
public:
    StaticXamlStream() noexcept = default;

    bool CanRead() const noexcept override { return true; }
    Aero::Base::Result<std::uint32_t> Read(
        Aero::Base::Span<std::uint8_t> destination) noexcept override {
        const std::uint32_t length =
            static_cast<std::uint32_t>(sizeof(GuiFacadeXaml) - 1U);
        const std::uint32_t remaining = length - position_;
        const std::uint32_t count =
            destination.Size() < remaining
            ? destination.Size() : remaining;
        if (count != 0U) {
            std::memcpy(
                destination.Data(), GuiFacadeXaml + position_, count);
            position_ += count;
        }
        return count;
    }

private:
    std::uint32_t position_ = 0U;
};

Aero::Base::Result<Aero::Markup::StreamResourceInfo> OpenGuiFacadeXaml(
    const Aero::Base::ResourceUri& uri,
    void*) noexcept {
    Aero::Base::Result<Aero::Base::Ref<StaticXamlStream>> made =
        Aero::Base::MakeRef<StaticXamlStream>();
    if (!made) return made.GetStatus();
    Aero::Markup::StreamResourceInfo resource;
    resource.uri = uri;
    resource.stream = Aero::Base::Ref<Aero::Base::Stream>(
        std::move(made).Value());
    resource.revision = 1U;
    return resource;
}

void VerifyGuiLoadFacade(Aero::Gui& gui) noexcept {
    Aero::Base::Result<Aero::Base::Ref<Aero::Controls::Grid>> abandoned =
        gui.LoadXaml<Aero::Controls::Grid>("memory:///Abandoned.xaml");
    Check(abandoned.HasValue(), "Gui abandoned-document load failed");
    if (!abandoned) return;
    Aero::Base::WeakRef<Aero::Controls::Grid> abandonedRoot(
        abandoned.Value());
    abandoned.Value().Reset();

    Aero::Base::Result<Aero::Base::Ref<Aero::Controls::Grid>> loaded =
        gui.LoadXaml<Aero::Controls::Grid>("memory:///Facade.xaml");
    Check(abandonedRoot.Expired(),
        "Gui retained an unclaimed XAML document after its root was released");
    Check(loaded.HasValue(), "Gui::LoadXaml facade failed");
    if (!loaded) return;
    Aero::Controls::Grid* root = loaded.Value().Get();
    Aero::Base::Result<Aero::Base::Ref<Aero::View>> view =
        gui.CreateView(std::move(loaded).Value());
    Check(view.HasValue(), "Gui::CreateView(root) facade failed");
    if (!view) return;
    Check(view.Value()->GetContent() == root,
        "Gui facade did not preserve the loaded root identity");
    Check(root->FindName("FacadeChild") != nullptr,
        "Gui facade discarded the XAML NameScope during View creation");

    Aero::Base::Result<Aero::Base::Ref<Aero::Controls::Grid>> component =
        Aero::Base::MakeRef<Aero::Controls::Grid>();
    Check(component.HasValue(), "Gui component root allocation failed");
    if (!component) return;
    Aero::Base::Result<void> populated = gui.LoadComponent(
        *component.Value(), "memory:///Component.xaml");
    Check(populated.HasValue(), "Gui::LoadComponent facade failed");
    if (!populated) return;
    Aero::Base::Result<Aero::Base::Ref<Aero::Controls::Grid>> interleaved =
        gui.LoadXaml<Aero::Controls::Grid>("memory:///Interleaved.xaml");
    Check(interleaved.HasValue(), "Gui interleaved document load failed");
    if (!interleaved) return;
    Aero::Base::Result<Aero::Base::Ref<Aero::View>> componentView =
        gui.CreateView(std::move(component).Value());
    Check(componentView.HasValue(),
        "Gui::CreateView(component) facade failed");
    if (!componentView) return;
    Check(componentView.Value()->GetContent()->FindName(
              "FacadeChild") != nullptr,
        "Gui::LoadComponent discarded the XAML NameScope");
    Aero::Base::Result<Aero::Base::Ref<Aero::View>> interleavedView =
        gui.CreateView(std::move(interleaved).Value());
    Check(interleavedView.HasValue(),
        "Gui discarded a pending interleaved XAML document");
}

#if defined(_WIN32)
LRESULT CALLBACK ConformanceWindowProcedure(
    HWND window,
    UINT message,
    WPARAM word,
    LPARAM value) noexcept {
    return DefWindowProcW(window, message, word, value);
}

bool RenderD3D11Readback(
    const ::Aero::Render::RenderFrame& frame,
    std::uint32_t width,
    std::uint32_t height,
    Aero::Base::Vector<std::uint8_t>& pixels) noexcept {
    Aero::Render::D3D11RenderDeviceOptions options;
    options.useWarp = true;
    options.allowWarpFallback = true;
    Aero::Render::D3D11RenderDevice backend(options);
    Aero::Base::Result<void> initialized = backend.Initialize();
    if (!initialized) {
        std::fprintf(stderr, "D3D11 WARP initialize: %u %s\n",
            static_cast<unsigned>(initialized.GetStatus().code),
            initialized.GetStatus().message);
        Check(false, "D3D11 pixel backend initialization failed");
        return false;
    }

    auto target = backend.CreateRenderTarget("conformance", width, height, 1, false);
    if (!target) {
        Check(false, "D3D11 target creation failed");
        backend.Shutdown();
        return false;
    }

    Aero::Render::UiFrameEncoder renderer(backend, &Aero::Base::GetDefaultAllocator());
    Aero::Base::Result<void> encInit = renderer.Initialize();
    if (!encInit) {
        Check(false, "D3D11 renderer initialize failed");
        backend.Shutdown();
        return false;
    }

    Aero::Base::Result<void> rec = renderer.RecordOnscreen(frame, *target);
    renderer.Shutdown();
    if (!rec) {
        Check(false, "D3D11 record failed");
        backend.Shutdown();
        return false;
    }

    ID3D11Device* dev = backend.NativeDevice();
    ID3D11DeviceContext* ctx = backend.NativeContext();
    auto* d3dTex = static_cast<Aero::Render::D3D11Texture*>(target->GetTexture());
    ID3D11Texture2D* nativeTex = d3dTex != nullptr ? d3dTex->GetNativeTexture() : nullptr;
    if (dev == nullptr || ctx == nullptr || nativeTex == nullptr) {
        Check(false, "D3D11 device/context/texture is null");
        backend.Shutdown();
        return false;
    }

    D3D11_TEXTURE2D_DESC stagingDesc{};
    stagingDesc.Width = width;
    stagingDesc.Height = height;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* stagingTex = nullptr;
    HRESULT hr = dev->CreateTexture2D(&stagingDesc, nullptr, &stagingTex);
    if (FAILED(hr) || stagingTex == nullptr) {
        Check(false, "D3D11 staging texture creation failed");
        backend.Shutdown();
        return false;
    }

    ctx->CopyResource(stagingTex, nativeTex);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = ctx->Map(stagingTex, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        stagingTex->Release();
        Check(false, "D3D11 map staging texture failed");
        backend.Shutdown();
        return false;
    }

    static_cast<void>(pixels.Resize(width * height * 4U));
    for (std::uint32_t y = 0; y < height; ++y) {
        std::memcpy(
            pixels.Data() + y * width * 4U,
            static_cast<const std::uint8_t*>(mapped.pData) + y * mapped.RowPitch,
            width * 4U);
    }
    ctx->Unmap(stagingTex, 0);
    stagingTex->Release();
    backend.Shutdown();
    return true;
}

bool RenderOpenGL33Readback(
    const ::Aero::Render::RenderFrame& frame,
    std::uint32_t width,
    std::uint32_t height,
    Aero::Base::Vector<std::uint8_t>& pixels) noexcept {
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    constexpr wchar_t WindowClass[] =
        L"AeroConformanceOpenGL33Window";
    WNDCLASSW windowClass{};
    windowClass.style = CS_OWNDC;
    windowClass.lpfnWndProc = &ConformanceWindowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = WindowClass;
    const ATOM atom = RegisterClassW(&windowClass);
    if (atom == 0U && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Check(false, "OpenGL conformance window class registration failed");
        return false;
    }
    HWND window = CreateWindowExW(
        0U,
        WindowClass,
        L"Aero Conformance",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        static_cast<int>(width),
        static_cast<int>(height),
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (window == nullptr) {
        Check(false, "OpenGL conformance window creation failed");
        if (atom != 0U) {
            static_cast<void>(UnregisterClassW(WindowClass, instance));
        }
        return false;
    }

    Aero::Render::Win32::OpenGLWindow surface;
    Aero::Platform::NativeWindowHandle nativeWindow;
    nativeWindow.system = Aero::Platform::WindowSystem::Win32;
    nativeWindow.window = reinterpret_cast<std::uintptr_t>(window);
    Aero::Base::Result<void> created = surface.Initialize(
        nativeWindow, {width, height});
    bool rendered = false;
    if (!created) {
        std::fprintf(stderr, "WGL surface initialize: %u %s\n",
            static_cast<unsigned>(created.GetStatus().code),
            created.GetStatus().message);
        Check(false, "OpenGL conformance surface initialization failed");
    } else {
        Aero::Render::OpenGL33::DeviceOptions deviceOptions;
        deviceOptions.resolve =
            &Aero::Render::Win32::OpenGLWindow::ResolveCallback;
        deviceOptions.makeCurrent =
            &Aero::Render::Win32::OpenGLWindow::MakeCurrentCallback;
        deviceOptions.isCurrent =
            &Aero::Render::Win32::OpenGLWindow::IsCurrentCallback;
        deviceOptions.contextGeneration =
            &Aero::Render::Win32::OpenGLWindow::GenerationCallback;
        deviceOptions.callbackContext = &surface;
        deviceOptions.checkErrors = true;
        Aero::Render::OpenGL33RenderDevice backend(deviceOptions);
        Aero::Base::Result<void> initialized = backend.Initialize();
        if (!initialized) {
            std::fprintf(stderr, "OpenGL backend initialize: %u %s\n",
                static_cast<unsigned>(initialized.GetStatus().code),
                initialized.GetStatus().message);
            Check(false, "OpenGL pixel backend initialization failed");
        } else {
            auto target = backend.CreateRenderTarget("conformance", width, height, 1, false);
            if (!target) {
                Check(false, "OpenGL target creation failed");
            } else {
                Aero::Render::UiFrameEncoder renderer(backend, &Aero::Base::GetDefaultAllocator());
                Aero::Base::Result<void> encInit = renderer.Initialize();
                if (!encInit) {
                    Check(false, "OpenGL renderer initialize failed");
                } else {
                    Aero::Base::Result<void> rec = renderer.RecordOnscreen(frame, *target);
                    renderer.Shutdown();
                    if (!rec) {
                        Check(false, "OpenGL record failed");
                    } else {
                        using PFNGLBINDFRAMEBUFFER = void (__stdcall*)(unsigned int, unsigned int);
                        using PFNGLREADPIXELS = void (__stdcall*)(int, int, int, int, unsigned int, unsigned int, void*);
                        auto glBindFB = reinterpret_cast<PFNGLBINDFRAMEBUFFER>(
                            Aero::Render::Win32::OpenGLWindow::ResolveCallback(&surface, "glBindFramebuffer"));
                        auto glRead = reinterpret_cast<PFNGLREADPIXELS>(
                            Aero::Render::Win32::OpenGLWindow::ResolveCallback(&surface, "glReadPixels"));
                        if (glBindFB != nullptr && glRead != nullptr) {
                            auto* glTarget = static_cast<Aero::Render::OpenGL33RenderTarget*>(target.Get());
                            unsigned int fbo = glTarget->GetFBO();
                            glBindFB(0x8CA9 /* GL_READ_FRAMEBUFFER */, fbo);
                            Aero::Base::Vector<std::uint8_t> raw(&Aero::Base::GetDefaultAllocator());
                            static_cast<void>(raw.Resize(width * height * 4U));
                            glRead(
                                0, 0, width, height, 0x1908 /* GL_RGBA */, 0x1401 /* GL_UNSIGNED_BYTE */, raw.Data());
                            glBindFB(0x8CA9 /* GL_READ_FRAMEBUFFER */, 0);

                            static_cast<void>(pixels.Resize(width * height * 4U));
                            for (std::uint32_t y = 0; y < height; ++y) {
                                std::memcpy(
                                    pixels.Data() + y * width * 4U,
                                    raw.Data() + (height - 1U - y) * width * 4U,
                                    width * 4U);
                            }
                            rendered = true;
                        } else {
                            Check(false, "OpenGL resolve for readback failed");
                        }
                    }
                }
            }
            backend.Shutdown();
        }
        surface.Shutdown();
    }
    static_cast<void>(DestroyWindow(window));
    if (atom != 0U) {
        static_cast<void>(UnregisterClassW(WindowClass, instance));
    }
    return rendered;
}

void VerifyNativePixelReadback(
    const ::Aero::Render::RenderFrame& frame,
    std::uint32_t width,
    std::uint32_t height) noexcept {
    Aero::Base::Vector<std::uint8_t> d3dPixels;
    Aero::Base::Vector<std::uint8_t> glPixels;
    if (!RenderD3D11Readback(frame, width, height, d3dPixels) ||
        !RenderOpenGL33Readback(frame, width, height, glPixels)) {
        return;
    }
    Check(d3dPixels.Size() == glPixels.Size() &&
            d3dPixels.Size() == width * height * 4U,
        "native pixel readback dimensions differ");
    if (d3dPixels.Size() != glPixels.Size() || d3dPixels.Empty()) return;

    std::uint64_t totalDifference = 0U;
    std::uint32_t outlierCount = 0U;
    std::uint32_t nonTransparentD3D = 0U;
    std::uint32_t nonTransparentGl = 0U;
    std::uint32_t d3dMinX = width;
    std::uint32_t d3dMinY = height;
    std::uint32_t d3dMaxX = 0U;
    std::uint32_t d3dMaxY = 0U;
    std::uint32_t glMinX = width;
    std::uint32_t glMinY = height;
    std::uint32_t glMaxX = 0U;
    std::uint32_t glMaxY = 0U;
    for (std::uint32_t index = 0U;
         index < d3dPixels.Size(); ++index) {
        const int difference = std::abs(
            static_cast<int>(d3dPixels[index]) -
            static_cast<int>(glPixels[index]));
        totalDifference += static_cast<std::uint32_t>(difference);
        if (difference > 16) ++outlierCount;
        if ((index & 3U) == 3U) {
            const std::uint32_t pixel = index / 4U;
            const std::uint32_t x = pixel % width;
            const std::uint32_t y = pixel / width;
            if (d3dPixels[index] != 0U) {
                ++nonTransparentD3D;
                d3dMinX = (std::min)(d3dMinX, x);
                d3dMinY = (std::min)(d3dMinY, y);
                d3dMaxX = (std::max)(d3dMaxX, x);
                d3dMaxY = (std::max)(d3dMaxY, y);
            }
            if (glPixels[index] != 0U) {
                ++nonTransparentGl;
                glMinX = (std::min)(glMinX, x);
                glMinY = (std::min)(glMinY, y);
                glMaxX = (std::max)(glMaxX, x);
                glMaxY = (std::max)(glMaxY, y);
            }
        }
    }
    const double meanDifference =
        static_cast<double>(totalDifference) /
        static_cast<double>(d3dPixels.Size());
    const std::uint32_t pixelCount = width * height;
    if (nonTransparentD3D <= pixelCount / 8U ||
        nonTransparentGl <= pixelCount / 8U ||
        meanDifference > 4.0 ||
        outlierCount > d3dPixels.Size() / 100U) {
        std::fprintf(stderr,
            "pixel diagnostics: d3d-alpha=%u gl-alpha=%u pixels=%u "
            "mean-diff=%.3f outliers=%u channels=%u "
            "d3d-box=%u,%u-%u,%u gl-box=%u,%u-%u,%u\n",
            nonTransparentD3D,
            nonTransparentGl,
            pixelCount,
            meanDifference,
            outlierCount,
            d3dPixels.Size(),
            d3dMinX,
            d3dMinY,
            d3dMaxX,
            d3dMaxY,
            glMinX,
            glMinY,
            glMaxX,
            glMaxY);
    }
    Check(nonTransparentD3D > pixelCount / 8U &&
            nonTransparentGl > pixelCount / 8U,
        "native backends produced an empty effect frame");
    Check(meanDifference <= 4.0 &&
            outlierCount <= d3dPixels.Size() / 100U,
        "D3D11 and OpenGL effect pixels exceed tolerance");
}
#endif

void VerifyMaskAndEffectRendering(Aero::View& view) noexcept {
    constexpr char xaml[] = R"XAML(
<Grid xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation">
  <Border/>
</Grid>
)XAML";
    Aero::Markup::XamlReader reader(view.GetGui());
    auto document = reader.Parse(Aero::Base::StringView(xaml));
    Check(document.HasValue(), "nested effect XAML parse failed");
    if (!document) return;
    const Aero::Base::Ref<Aero::Base::Object>& rootObject =
        document.Value().Root();
    Check(rootObject &&
            rootObject->RuntimeType() == Aero::Controls::Grid::StaticTypeId(),
        "nested effect XAML root was not resolved");
    if (!rootObject ||
        rootObject->RuntimeType() != Aero::Controls::Grid::StaticTypeId()) {
        return;
    }
    auto* root = static_cast<Aero::Controls::Grid*>(rootObject.Get());
    Aero::UIElement* childElement = root->GetChildren().GetItem(0U);
    Check(childElement != nullptr &&
            childElement->RuntimeType() ==
                Aero::Controls::Border::StaticTypeId(),
        "nested effect XAML child was not resolved");
    if (childElement == nullptr ||
        childElement->RuntimeType() !=
            Aero::Controls::Border::StaticTypeId()) {
        return;
    }
    auto* child = static_cast<Aero::Controls::Border*>(childElement);
    auto backgroundMade =
        Aero::Base::MakeRef<Aero::Media::SolidColorBrush>();
    auto rootMaskMade =
        Aero::Base::MakeRef<Aero::Media::SolidColorBrush>();
    auto rootEffectMade =
        Aero::Base::MakeRef<Aero::Media::BlurEffect>();
    auto childEffectMade =
        Aero::Base::MakeRef<Aero::Media::DropShadowEffect>();
    auto gradientMade =
        Aero::Base::MakeRef<Aero::Media::LinearGradientBrush>();
    auto firstStopMade =
        Aero::Base::MakeRef<Aero::Media::GradientStop>();
    auto secondStopMade =
        Aero::Base::MakeRef<Aero::Media::GradientStop>();
    const bool allocated =
        backgroundMade && rootMaskMade && rootEffectMade &&
        childEffectMade && gradientMade && firstStopMade &&
        secondStopMade;
    Check(allocated, "render conformance object allocation failed");
    if (!allocated) return;
    Aero::Base::Ref<Aero::Media::SolidColorBrush> background =
        std::move(backgroundMade).Value();
    Aero::Base::Ref<Aero::Media::SolidColorBrush> rootMask =
        std::move(rootMaskMade).Value();
    Aero::Base::Ref<Aero::Media::BlurEffect> rootEffect =
        std::move(rootEffectMade).Value();
    Aero::Base::Ref<Aero::Media::DropShadowEffect> childEffect =
        std::move(childEffectMade).Value();
    Aero::Base::Ref<Aero::Media::LinearGradientBrush> gradient =
        std::move(gradientMade).Value();
    Aero::Base::Ref<Aero::Media::GradientStop> firstStop =
        std::move(firstStopMade).Value();
    Aero::Base::Ref<Aero::Media::GradientStop> secondStop =
        std::move(secondStopMade).Value();
    firstStop->SetOffset(0.0);
    firstStop->SetColor({1.0F, 1.0F, 1.0F, 0.1F});
    secondStop->SetOffset(1.0);
    secondStop->SetColor({1.0F, 1.0F, 1.0F, 1.0F});
    Check(gradient->AddGradientStop(firstStop).HasValue() &&
            gradient->AddGradientStop(secondStop).HasValue(),
        "gradient mask stop attachment failed");
    background->SetColor({0.2F, 0.4F, 0.8F, 1.0F});
    rootMask->SetColor({1.0F, 1.0F, 1.0F, 0.85F});
    rootEffect->SetRadius(6.0);
    childEffect->SetBlurRadius(5.0);
    childEffect->SetShadowDepth(4.0);
    childEffect->SetDirection(315.0);
    childEffect->SetOpacity(0.7);
    childEffect->SetColor({0.0F, 0.0F, 0.0F, 1.0F});
    gradient->SetStartPoint({0.0, 0.0});
    gradient->SetEndPoint({1.0, 1.0});
    root->SetBackground(
        Aero::Base::Ref<Aero::Media::Brush>(background));
    child->SetBackground(
        Aero::Base::Ref<Aero::Media::Brush>(background));
    root->SetOpacityMask(
        Aero::Base::Ref<Aero::Media::Brush>(rootMask));
    root->SetEffect(
        Aero::Base::Ref<Aero::Media::Effect>(rootEffect));
    child->SetOpacityMask(
        Aero::Base::Ref<Aero::Media::Brush>(gradient));
    child->SetEffect(
        Aero::Base::Ref<Aero::Media::Effect>(childEffect));

    const Aero::Size logicalSize{128.0, 96.0};
    constexpr std::uint32_t pixelWidth = 256U;
    constexpr std::uint32_t pixelHeight = 192U;
    Aero::Base::Result<void> mounted =
        view.SetContent(std::move(document).Value(), logicalSize);
    if (!mounted) {
        std::fprintf(stderr, "View mount failed: %u %s\n",
            static_cast<unsigned>(mounted.GetStatus().code),
            mounted.GetStatus().message);
    }
    Check(mounted.HasValue(), "nested effect View mount failed");
    if (!mounted) return;
    view.SetSize(logicalSize);
    view.SetScale(2.0);
    const bool updated = view.Update(0.0);
    Check(updated, "first mounted View update did not commit a frame");
    const ::Aero::Render::RenderFrame* frame =
        Aero::CurrentFrameForConformance(view);
    if (frame != nullptr && frame->GradientRamps().Empty()) {
        std::fprintf(stderr,
            "render frame diagnostics: version=%llu nodes=%u commands=%u ramps=%u\n",
            static_cast<unsigned long long>(frame->Version()),
            frame->Nodes().Size(),
            frame->Commands().Size(),
            frame->GradientRamps().Size());
        for (const Aero::Render::RenderNodeSnapshot& node : frame->Nodes()) {
            std::fprintf(stderr,
                "  node=%llu parent=%llu mask=%u effect=%u size=%.1fx%.1f\n",
                static_cast<unsigned long long>(node.id),
                static_cast<unsigned long long>(node.parentId),
                static_cast<unsigned>(node.mask.kind),
                static_cast<unsigned>(node.effect.kind),
                node.renderSize.width,
                node.renderSize.height);
        }
    }
    Check(frame != nullptr && !frame->GradientRamps().Empty(),
        "gradient mask ramp was not captured in RenderFrame");
    if (frame == nullptr) return;

#if defined(_WIN32)
    if (frame != nullptr) {
        VerifyNativePixelReadback(
            *frame, pixelWidth, pixelHeight);
    }
#endif
}

void VerifyAuthoringPropertySynchronization() noexcept {
    auto itemsRef = Aero::Base::MakeRef<Aero::Controls::ItemsControl>();
    if (!itemsRef) return;
    Aero::Controls::ItemsControl& items = *itemsRef.Value();

    auto itemTemplate = Aero::Base::MakeRef<Aero::DataTemplate>();
    Check(itemTemplate.HasValue(),
        "DataTemplate allocation failed");
    if (itemTemplate) {
        items.SetItemTemplate(itemTemplate.Value());
        const Aero::Value local = items.ReadLocalValue(
            Aero::Controls::ItemsControl::ItemTemplateProperty);
        Check(items.GetItemTemplate() == itemTemplate.Value().Get() &&
                local.Kind() == Aero::ValueKind::Object &&
                local.AsObject().Get() == itemTemplate.Value().Get(),
            "ItemsControl ItemTemplate setter bypassed dependency-property state");
    }

    auto itemsPanel =
        Aero::Base::MakeRef<Aero::Controls::ItemsPanelTemplate>();
    Check(itemsPanel.HasValue(),
        "ItemsPanelTemplate allocation failed");
    if (itemsPanel) {
        items.SetItemsPanel(itemsPanel.Value());
        const Aero::Value local = items.ReadLocalValue(
            Aero::Controls::ItemsControl::ItemsPanelProperty);
        Check(items.GetItemsPanel() == itemsPanel.Value().Get() &&
                local.Kind() == Aero::ValueKind::Object &&
                local.AsObject().Get() == itemsPanel.Value().Get(),
            "ItemsControl ItemsPanel setter bypassed dependency-property state");
    }

    auto containerStyle = Aero::Base::MakeRef<Aero::Style>();
    Check(containerStyle.HasValue(), "Style allocation failed");
    if (containerStyle) {
        items.SetItemContainerStyle(containerStyle.Value());
        const Aero::Value local = items.ReadLocalValue(
            Aero::Controls::ItemsControl::ItemContainerStyleProperty);
        Check(items.GetItemContainerStyle() ==
                    containerStyle.Value().Get() &&
                local.Kind() == Aero::ValueKind::Object &&
                local.AsObject().Get() == containerStyle.Value().Get(),
            "ItemsControl ItemContainerStyle setter bypassed dependency-property state");
    }

    auto transitionRef = Aero::Base::MakeRef<Aero::VisualTransition>();
    if (transitionRef) {
        Aero::VisualTransition& transition = *transitionRef.Value();
        Check(!transition.SetGeneratedDuration("not-a-duration"),
            "VisualTransition accepted an invalid GeneratedDuration");
        Check(transition.SetGeneratedDuration("250ms").HasValue(),
            "VisualTransition rejected a valid GeneratedDuration");
    }
    Check(
        Aero::VisualStateManager::VisualStateGroupsProperty.Name() ==
            Aero::Base::StringView("VisualStateGroups"),
        "VisualStateManager exposes an internal attached-property name");
}

} // namespace

int main() {
    static_assert(std::is_base_of<Aero::DependencyObject,
        Aero::Freezable>::value, "Freezable must derive from DependencyObject");
    static_assert(std::is_base_of<Aero::Freezable,
        Aero::Media::Brush>::value, "Brush must derive from Freezable");
    static_assert(std::is_base_of<Aero::Freezable,
        Aero::Media::Transform>::value, "Transform must derive from Freezable");
    static_assert(std::is_base_of<Aero::Freezable,
        Aero::Media::Effect>::value, "Effect must derive from Freezable");
    static_assert(std::is_base_of<Aero::Freezable,
        Aero::Media::Geometry>::value, "Geometry must derive from Freezable");

    Aero::Gui gui;
    Aero::Base::Result<Aero::Base::Ref<Aero::Markup::XamlProviderAdapter>>
        facadeProvider =
            Aero::Base::MakeRef<Aero::Markup::XamlProviderAdapter>(
                &OpenGuiFacadeXaml, nullptr, nullptr);
    Check(facadeProvider.HasValue(), "Gui facade provider allocation failed");
    const Aero::Base::Result<void> providerAdded = facadeProvider
        ? gui.SetXamlProvider(
            std::move(facadeProvider).Value(), "memory")
        : Aero::Base::Result<void>(facadeProvider.GetStatus());
    Check(providerAdded.HasValue(), "Gui facade provider registration failed");
    const Aero::Base::Result<void> initialized = gui.Initialize();
    Check(initialized.HasValue(), "Gui initialization failed");
    if (initialized) {
        VerifyGuiLoadFacade(gui);
        Aero::ViewOptions viewOptions;
        viewOptions.loadBuiltInTheme = true;
        viewOptions.builtInTheme = Aero::BuiltInTheme::Light;
        Aero::Base::Result<Aero::Base::Ref<Aero::View>> view =
            gui.CreateView(viewOptions);
        Check(view.HasValue(), "View creation failed");
        if (view) {
            VerifyMaskAndEffectRendering(*view.Value());
            VerifyFreezeGraph();
            VerifyExpressionFreezeRejection();
            VerifySharedConsumers();
            VerifyGradientFreeze();
            VerifyViewport(*view.Value());
            VerifyAuthoringPropertySynchronization();
        }
    }
    if (failures != 0) return 1;
    std::puts("aero-conformance: passed");
    return 0;
}
