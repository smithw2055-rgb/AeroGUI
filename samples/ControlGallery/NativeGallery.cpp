#include "GalleryRuntime.hpp"

#include <Aero/Base/Result.hpp>
#include <Aero/Rhi/Graphics.hpp>
#include <Aero/Rhi/Surface.hpp>

namespace Aero::Samples::ControlGallery {
namespace {

Base::Status UnsupportedBackend(
    const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        message);
}

Base::Result<void> ValidatePlan(
    const Presentation::RenderPlan& plan) noexcept {
    if (plan.Nodes().Empty() ||
        plan.Commands().Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ControlGallery render plan is empty");
    }
    return {};
}

} // namespace

Base::Result<void> RunNativeGallery(
    GalleryRuntime& runtime,
    Base::StringView backend,
    bool simulateContextLoss,
    bool interactive) noexcept {
    Base::Result<void> valid =
        ValidatePlan(runtime.Plan());
    if (!valid) {
        return valid.GetStatus();
    }
    if (backend == Base::StringView("null")) {
        return {};
    }
#if defined(AERO_CONTROL_GALLERY_WINDOWS_NATIVE)
    extern Base::Result<void>
    RunControlGalleryD3D11(
        GalleryRuntime&,
        bool,
        bool) noexcept;
    extern Base::Result<void>
    RunControlGalleryWgl(
        GalleryRuntime&,
        bool,
        bool) noexcept;
    if (backend ==
        Base::StringView("d3d11")) {
        return RunControlGalleryD3D11(
            runtime,
            simulateContextLoss,
            interactive);
    }
    if (backend ==
        Base::StringView("opengl")) {
        return RunControlGalleryWgl(
            runtime,
            simulateContextLoss,
            interactive);
    }
#elif defined(AERO_CONTROL_GALLERY_GLX_NATIVE)
    extern Base::Result<void>
    RunControlGalleryGlx(
        GalleryRuntime&,
        bool,
        bool) noexcept;
    if (backend ==
        Base::StringView("opengl")) {
        return RunControlGalleryGlx(
            runtime,
            simulateContextLoss,
            interactive);
    }
#else
    static_cast<void>(simulateContextLoss);
    static_cast<void>(interactive);
#endif
    return UnsupportedBackend(
        "Requested ControlGallery native backend "
        "is unavailable in this build");
}

} // namespace Aero::Samples::ControlGallery
