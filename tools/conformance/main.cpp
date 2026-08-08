#include <Aero/Controls.hpp>
#include <Aero/Freezable.hpp>
#include <Aero/Gui.hpp>
#include <Aero/ViewOptions.hpp>
#include <Aero/Gui/XamlReader.hpp>
#include <Aero/Gui/Brush.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Gui/Geometry.hpp>
#include <Aero/Gui/Transform.hpp>
#include <Aero/Gui/View.hpp>

#include "render/FrameEncoder.hpp"
#include "gui/PropertyRuntime.hpp"
#include "gui/ViewRenderer.hpp"
#include "render/RenderDeviceState.hpp"
#include "render/RenderTargetState.hpp"
#include "render/opengl33/OpenGL33RenderDevice.hpp"

namespace Aero {
const ::Aero::Render::RenderFrame* CurrentFrameForConformance(
    const View& view) noexcept;
}

#if defined(_WIN32)
#include "app/platform/win32/OpenGLWindow.hpp"
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
    GraphFreezable accepted;
    GraphFreezable rejected(true);
    accepted.SetChild(&rejected);
    Check(!accepted.CanFreeze(),
        "an unfreezable child must reject the whole graph");
    Check(!accepted.Freeze(),
        "Freeze must report an unfreezable child");
    Check(!accepted.IsFrozen() && !rejected.IsFrozen(),
        "failed Freeze must not partially freeze the graph");

    GraphFreezable first;
    GraphFreezable second;
    first.SetChild(&second);
    second.SetChild(&first);
    Check(!first.CanFreeze(),
        "CanFreeze must reject an object graph cycle");
    Check(!first.Freeze(),
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
    Aero::Controls::Border first;
    Aero::Controls::Border second;
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

    Aero::Media::LinearGradientBrush brush;
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
    view.SetSize({800.0, 450.0});
    view.SetScale(2.0);
    Check(view.Update().HasValue(), "View size/scale update failed");
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

class ProbeRenderTargetState;

class ProbeRenderDeviceState final : public Aero::RenderDevice::Access {
public:
    explicit ProbeRenderDeviceState(Aero::Base::IAllocator& allocator) noexcept
        : Aero::RenderDevice::Access(allocator) {}

    Aero::Render::RenderBackendKind Backend() const noexcept override {
        return Aero::Render::RenderBackendKind::Headless;
    }
    Aero::Base::Result<Aero::Graphics::FenceValue> DrawBatch(
        Aero::Render::RenderBatch&&) noexcept override {
        return Aero::Graphics::FenceValue{0U};
    }
    void NotifyDeviceLost() noexcept override;
    Aero::Base::Result<void> RestoreDevice() noexcept override;
    Aero::Base::Result<void> WaitIdle(std::uint32_t) noexcept override {
        return {};
    }
    Aero::Render::BackendHealth GetDeviceHealth() const noexcept override {
        return deviceHealth;
    }
    Aero::Graphics::DeviceCapabilities
    QueryNativeDeviceCapabilities() const noexcept override { return {}; }
    Aero::Graphics::NativeRenderBackendKind
    NativeBackendKind() const noexcept override {
        return Aero::Graphics::NativeRenderBackendKind::Invalid;
    }
    Aero::Graphics::GraphicsCapabilities
    QueryNativeGraphicsCapabilities() const noexcept override { return {}; }
    Aero::Base::Result<void> CreateNativeResource(
        Aero::Graphics::ResourceHandle,
        const Aero::Graphics::ResourceDescriptor&) noexcept override {
        return {};
    }
    void DestroyNativeResource(
        Aero::Graphics::ResourceHandle) noexcept override {}
    Aero::Base::Result<void> ConfigureNativeTexture(
        Aero::Graphics::ResourceHandle,
        const Aero::Graphics::TextureResourceDescriptor&) noexcept override {
        return {};
    }
    Aero::Base::Result<void> ConfigureNativeSampler(
        Aero::Graphics::ResourceHandle,
        const Aero::Graphics::SamplerDescriptor&) noexcept override {
        return {};
    }
    Aero::Base::Result<void> ConfigureNativePipeline(
        Aero::Graphics::ResourceHandle,
        Aero::Render::UiPipelineKey) noexcept override {
        return {};
    }
    Aero::Base::Result<void> SubmitNativeBatch(
        const Aero::Render::RenderBatch&,
        Aero::Graphics::ResourceHandle,
        Aero::Graphics::FenceValue) noexcept override {
        return {};
    }
    Aero::Base::Result<void> UpdateNativeBuffer(
        Aero::Graphics::ResourceHandle,
        std::uint64_t,
        Aero::Base::Span<const std::uint8_t>) noexcept override {
        return {};
    }
    Aero::Base::Result<void> UpdateNativeTexture(
        Aero::Graphics::ResourceHandle,
        const Aero::Graphics::TextureRegion&,
        Aero::Base::Span<const std::uint8_t>) noexcept override {
        return {};
    }
    Aero::Graphics::FenceValue
    NativeLastSubmittedFence() const noexcept override { return 0U; }
    Aero::Graphics::FenceValue
    NativeCompletedFence() const noexcept override { return 0U; }
    bool NativeDeviceLost() const noexcept override {
        return deviceHealth == Aero::Render::BackendHealth::DeviceLost;
    }
    Aero::Base::Result<void> RenderFrame() noexcept {
        if (!failNext) return {};
        failNext = false;
        return Aero::Base::Status::Failure(
            Aero::Base::ErrorCode::Unsupported,
            "probe frame is intentionally unsupported");
    }

    ProbeRenderTargetState* target = nullptr;
    bool failNext = false;
    Aero::Render::BackendHealth deviceHealth =
        Aero::Render::BackendHealth::Ready;
};

class ProbeRenderTargetState final : public Aero::RenderTarget::Access {
public:
    explicit ProbeRenderTargetState(ProbeRenderDeviceState& device) noexcept
        : Aero::RenderTarget::Access(Aero::RenderTargetKind::Embedded),
          device_(&device) {
        device.target = this;
    }
    ~ProbeRenderTargetState() noexcept override {
        if (device_ != nullptr && device_->target == this) device_->target = nullptr;
    }

    Aero::Base::Result<void> Render(
        Aero::ViewRenderer&,
        const ::Aero::Render::RenderFrame&) noexcept override {
        return device_ != nullptr
            ? device_->RenderFrame()
            : Aero::Base::Result<void>(Aero::Base::Status::Failure(
                  Aero::Base::ErrorCode::InvalidState,
                  "probe target has no device"));
    }
    Aero::Base::Result<void> Resize(std::uint32_t, std::uint32_t) noexcept override {
        return {};
    }
    void NotifySurfaceLost() noexcept override {
        surfaceHealth = Aero::Render::SurfaceHealth::Lost;
    }
    Aero::Base::Result<void> RestoreSurface() noexcept override {
        surfaceHealth = Aero::Render::SurfaceHealth::Ready;
        return {};
    }
    Aero::Render::SurfaceHealth GetSurfaceHealth() const noexcept override {
        return surfaceHealth;
    }
    void RestoreAfterDeviceLoss() noexcept {
        surfaceHealth = Aero::Render::SurfaceHealth::Ready;
    }

    Aero::Render::SurfaceHealth surfaceHealth =
        Aero::Render::SurfaceHealth::Ready;

private:
    ProbeRenderDeviceState* device_ = nullptr;
};

void ProbeRenderDeviceState::NotifyDeviceLost() noexcept {
    deviceHealth = Aero::Render::BackendHealth::DeviceLost;
    if (target != nullptr) target->NotifySurfaceLost();
}

Aero::Base::Result<void> ProbeRenderDeviceState::RestoreDevice() noexcept {
    deviceHealth = Aero::Render::BackendHealth::Ready;
    if (target != nullptr) target->RestoreAfterDeviceLoss();
    return {};
}

void VerifyRenderDeviceState(
    Aero::View& view,
    const ::Aero::Render::RenderFrame& frame) noexcept {
    Aero::Base::IAllocator& allocator = Aero::Base::GetDefaultAllocator();
    auto* state = new (std::nothrow) ProbeRenderDeviceState(allocator);
    Check(state != nullptr, "render device probe allocation failed");
    if (state == nullptr) return;
    auto made = Aero::Render::AdoptRenderDevice(state, &allocator);
    Check(made.HasValue(), "render device probe adoption failed");
    if (!made) return;
    Aero::Base::Ref<Aero::RenderDevice> device = std::move(made).Value();

    auto* targetState = new (std::nothrow) ProbeRenderTargetState(*state);
    Check(targetState != nullptr, "render target probe allocation failed");
    if (targetState == nullptr) return;
    auto targetMade = Aero::Render::AdoptRenderTarget(
        device, targetState, Aero::RenderTargetKind::Embedded, &allocator);
    Check(targetMade.HasValue(), "render target probe adoption failed");
    if (!targetMade) return;
    Aero::Base::Ref<Aero::RenderTarget> target = std::move(targetMade).Value();
    auto& renderer = static_cast<Aero::ViewRenderer&>(view.GetRenderer());

    state->failNext = true;
    Aero::Base::Result<void> unsupported = Aero::RenderTarget::Access::Render(
        *target, renderer, frame);
    Check(!unsupported &&
            unsupported.GetStatus().code == Aero::Base::ErrorCode::Unsupported,
        "probe Unsupported frame did not fail as requested");
    Check(device->State() == Aero::RenderDeviceState::Ready &&
            target->State() == Aero::RenderTargetState::Ready &&
            state->statistics.failedFrameCount == 1U,
        "ordinary frame failure poisoned a ready render target");

    Check(Aero::RenderTarget::Access::Render(
              *target, renderer, frame).HasValue(),
        "normal frame after Unsupported failure did not render");
    Check(state->statistics.acceptedFrameCount == 1U &&
            state->statistics.completedFrameCount == 1U,
        "successful recovery frame statistics are incorrect");

    target->NotifyLost();
    Check(device->State() == Aero::RenderDeviceState::Ready &&
            target->State() == Aero::RenderTargetState::Lost,
        "target loss incorrectly poisoned the render device");
    Check(target->Restore().HasValue() &&
            target->State() == Aero::RenderTargetState::Ready,
        "target loss restore failed");

    device->NotifyDeviceLost();
    Check(device->State() == Aero::RenderDeviceState::DeviceLost &&
            target->State() == Aero::RenderTargetState::DeviceLost,
        "device loss did not propagate to the render target");
    Check(device->Restore().HasValue() &&
            device->State() == Aero::RenderDeviceState::Ready &&
            target->State() == Aero::RenderTargetState::Ready,
        "device loss restore failed");

    state->deviceHealth = Aero::Render::BackendHealth::Failed;
    state->failNext = true;
    Check(!Aero::RenderTarget::Access::Render(
               *target, renderer, frame) &&
            device->State() == Aero::RenderDeviceState::Failed &&
            target->State() == Aero::RenderTargetState::Failed,
        "fatal backend health did not fail the render target");
}

template<class TBackend>
bool RenderAndReadback(
    TBackend& backend,
    const ::Aero::Render::RenderFrame& frame,
    std::uint32_t width,
    std::uint32_t height,
    Aero::Base::Vector<std::uint8_t>& pixels,
    const char* failureMessage) noexcept {
    Aero::Render::UiFrameEncoder renderer(
        backend, &Aero::Base::GetDefaultAllocator());
    Aero::Base::Result<void> initialized = renderer.Initialize();
    if (!initialized) {
        std::fprintf(stderr, "%s: renderer initialize: %u %s\n",
            failureMessage,
            static_cast<unsigned>(initialized.GetStatus().code),
            initialized.GetStatus().message);
        Check(false, failureMessage);
        return false;
    }

    Aero::Graphics::TextureResourceDescriptor descriptor;
    descriptor.width = width;
    descriptor.height = height;
    descriptor.format = Aero::Graphics::GraphicsTextureFormat::Bgra8Unorm;
    descriptor.usage =
        Aero::Graphics::TextureUsageBit(
            Aero::Graphics::TextureUsage::Sampled) |
        Aero::Graphics::TextureUsageBit(
            Aero::Graphics::TextureUsage::RenderTarget) |
        Aero::Graphics::TextureUsageBit(
            Aero::Graphics::TextureUsage::CopySource);
    auto target = backend.CreateRenderTarget(descriptor);
    if (!target) {
        std::fprintf(stderr, "%s: target creation: %u %s\n",
            failureMessage,
            static_cast<unsigned>(target.GetStatus().code),
            target.GetStatus().message);
        Check(false, failureMessage);
        renderer.Shutdown();
        return false;
    }
    auto submitted = renderer.Record(
        frame,
        {target.Value(), width, height,
         Aero::Graphics::LoadOperation::Clear});
    if (!submitted) {
        std::fprintf(stderr, "%s: frame encoding: %u %s\n",
            failureMessage,
            static_cast<unsigned>(submitted.GetStatus().code),
            submitted.GetStatus().message);
        Check(false, failureMessage);
        renderer.Shutdown();
        return false;
    }
    Aero::Base::Result<void> waited =
        backend.WaitForFence(submitted.Value());
    if (!waited) {
        std::fprintf(stderr, "%s: fence wait: %u %s\n",
            failureMessage,
            static_cast<unsigned>(waited.GetStatus().code),
            waited.GetStatus().message);
        Check(false, failureMessage);
        renderer.Shutdown();
        return false;
    }
    const std::uint32_t rowPitch = width * 4U;
    Aero::Base::Result<void> resized = pixels.Resize(rowPitch * height);
    if (!resized) {
        Check(false, "native pixel readback allocation failed");
        renderer.Shutdown();
        return false;
    }
    Aero::Base::Result<void> readback = backend.ReadbackTexture(
        target.Value(), pixels.AsSpan(), rowPitch);
    if (!readback) {
        std::fprintf(stderr, "%s: readback: %u %s\n",
            failureMessage,
            static_cast<unsigned>(readback.GetStatus().code),
            readback.GetStatus().message);
        Check(false, failureMessage);
        renderer.Shutdown();
        return false;
    }
    renderer.Shutdown();
    auto& device = static_cast<Aero::RenderDevice::Access&>(backend);
    static_cast<void>(device.DestroyResource(
        target.Value(), device.LastSubmittedFence()));
    static_cast<void>(device.CollectGarbage());
    return true;
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
    Aero::Graphics::D3D11RenderDeviceOptions options;
    options.deviceMode = Aero::Graphics::D3D11DeviceMode::Warp;
    options.allowWarpFallback = true;
    Aero::Graphics::D3D11RenderDevice backend(options);
    Aero::Base::Result<void> initialized = backend.Initialize();
    if (!initialized) {
        std::fprintf(stderr, "D3D11 WARP initialize: %u %s\n",
            static_cast<unsigned>(initialized.GetStatus().code),
            initialized.GetStatus().message);
        Check(false, "D3D11 pixel backend initialization failed");
        return false;
    }
    const bool rendered = RenderAndReadback(
        backend,
        frame,
        width,
        height,
        pixels,
        "D3D11 pixel readback failed");
    backend.Shutdown();
    return rendered;
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

    Aero::App::Win32::OpenGLWindow surface;
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
        Aero::Render::OpenGL33DeviceOptions deviceOptions;
        deviceOptions.resolve =
            &Aero::App::Win32::OpenGLWindow::ResolveCallback;
        deviceOptions.makeCurrent =
            &Aero::App::Win32::OpenGLWindow::MakeCurrentCallback;
        deviceOptions.isCurrent =
            &Aero::App::Win32::OpenGLWindow::IsCurrentCallback;
        deviceOptions.contextGeneration =
            &Aero::App::Win32::OpenGLWindow::GenerationCallback;
        deviceOptions.callbackContext = &surface;
        deviceOptions.checkErrors = true;
        Aero::Graphics::OpenGL33RenderDevice backend(deviceOptions);
        Aero::Base::Result<void> initialized = backend.Initialize();
        if (!initialized) {
            std::fprintf(stderr, "OpenGL backend initialize: %u %s\n",
                static_cast<unsigned>(initialized.GetStatus().code),
                initialized.GetStatus().message);
            Check(false, "OpenGL pixel backend initialization failed");
        } else {
            rendered = RenderAndReadback(
                backend,
                frame,
                width,
                height,
                pixels,
                "OpenGL pixel readback failed");
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
    view.SetContent(std::move(document).Value(), logicalSize);
    view.SetSize(logicalSize);
    view.SetScale(2.0);
    Check(view.Update().HasValue(), "nested effect View update failed");
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

    VerifyRenderDeviceState(view, *frame);
#if defined(_WIN32)
    if (frame != nullptr) {
        VerifyNativePixelReadback(
            *frame, pixelWidth, pixelHeight);
    }
#endif
}

void VerifyAuthoringPropertySynchronization() noexcept {
    Aero::Controls::ItemsControl items;

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

    Aero::VisualTransition transition;
    Check(!transition.SetGeneratedDuration("not-a-duration"),
        "VisualTransition accepted an invalid GeneratedDuration");
    Check(transition.SetGeneratedDuration("250ms").HasValue(),
        "VisualTransition rejected a valid GeneratedDuration");
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
    const Aero::Base::Result<void> initialized = gui.Initialize();
    Check(initialized.HasValue(), "Gui initialization failed");
    if (initialized) {
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
