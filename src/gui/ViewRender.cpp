#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

using namespace ::Aero;
namespace MediaAnimation = ::Aero::Media::Animation;


namespace {

RenderingEventHandler& LegacyCompositionRenderingHandlers() noexcept {
    thread_local RenderingEventHandler handlers;
    return handlers;
}

} // namespace

void Media::CompositionTarget::AddRendering(
    const RenderingEventHandler& handler) noexcept {
    if (!handler.Empty()) {
        LegacyCompositionRenderingHandlers().Add(handler);
    }
}

bool Media::CompositionTarget::RemoveRendering(
    const RenderingEventHandler& handler) noexcept {
    return LegacyCompositionRenderingHandlers().Remove(handler);
}


void ViewState::AttachTextLayout(
        Aero::Media::Visual& node,
        ::Aero::Controls::TextBlockLayout* service,
        bool invalidate) noexcept {
        if (metadata == nullptr) return;
        const Meta::TypeId type = node.RuntimeType();
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::TextBlock::StaticTypeId())) {
            AeroGuiInternal::AttachTextLayout(
                *static_cast<Controls::TextBlock*>(&node),
                service,
                invalidate);
        }
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::TextBox::StaticTypeId())) {
            AeroGuiInternal::AttachTextLayout(
                *static_cast<Controls::TextBox*>(&node),
                service,
                invalidate);
        }
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::PasswordBox::
                    StaticTypeId())) {
            AeroGuiInternal::AttachTextLayout(
                *static_cast<Controls::PasswordBox*>(
                    &node),
                service,
                invalidate);
        }
    }

Aero::Render::MeshResources*
 ViewState::GetMeshResources() noexcept {
        return publicRenderer.Resources().meshes;
    }

Aero::Render::ImageResources*
 ViewState::GetImageResources() noexcept {
        return publicRenderer.Resources().images;
    }

void ViewState::AttachPathResources(
        Aero::Media::Visual& node,
        Aero::Render::MeshResources* service,
        bool invalidate) noexcept {
        if (metadata == nullptr) return;
        const Meta::TypeId type = node.RuntimeType();
        if (metadata->Types().IsDerivedFrom(
                type,
                Shapes::Path::StaticTypeId())) {
            AeroGuiInternal::PathAttachMeshResources(
                *static_cast<Shapes::Path*>(&node),
                service,
                invalidate);
        }
    }

void ViewState::VisitTextElements(
        Aero::Media::Visual* rootVisual,
        ::Aero::Controls::TextBlockLayout* service,
        bool invalidate,
        bool ancestorsVisible) noexcept {
        if (rootVisual == nullptr) return;
        bool effectivelyVisible = ancestorsVisible;
        if (Aero::UIElement* element =
                ::Aero::TryCast<::Aero::UIElement>(rootVisual);
            element != nullptr) {
            effectivelyVisible =
                ancestorsVisible &&
                element->GetVisibility() ==
                    Aero::Visibility::Visible;
        }
        AttachTextLayout(
            *rootVisual,
            service,
            invalidate && effectivelyVisible);
        for (Aero::Media::Visual* child :
             AeroGuiInternal::RenderChildren(*rootVisual)) {
            VisitTextElements(
                child,
                service,
                invalidate,
                effectivelyVisible);
        }
    }

void ViewState::VisitPaths(
        Aero::Media::Visual* rootVisual,
        Aero::Render::MeshResources* service,
        bool invalidate,
        bool ancestorsVisible) noexcept {
        if (rootVisual == nullptr) return;
        bool effectivelyVisible = ancestorsVisible;
        if (Aero::UIElement* element =
                ::Aero::TryCast<::Aero::UIElement>(rootVisual);
            element != nullptr) {
            effectivelyVisible =
                ancestorsVisible &&
                element->GetVisibility() ==
                    Aero::Visibility::Visible;
        }
        AttachPathResources(
            *rootVisual,
            service,
            invalidate && effectivelyVisible);
        for (Aero::Media::Visual* child :
             AeroGuiInternal::RenderChildren(*rootVisual)) {
            VisitPaths(
                child,
                service,
                invalidate,
                effectivelyVisible);
        }
    }

void ViewState::TextLifecycleHook(
        const Aero::ElementTreeLifecycleEvent& event,
        void* context) noexcept {
        auto* runtime = static_cast<ViewState*>(context);
        if (runtime == nullptr || event.node == nullptr) {
            return;
        }
        runtime->AttachTextLayout(
            *event.node,
            event.loaded && runtime->text != nullptr
                ? runtime->text->Layout()
                : nullptr);
        runtime->AttachPathResources(
            *event.node,
            event.loaded
                ? runtime->GetMeshResources()
                : nullptr);
    }

const ::Aero::Render::RenderFrame* ViewState::CurrentFrame(
    const View& view) noexcept
{
    return view.state_ != nullptr && view.state_->renderer != nullptr
        ? &view.state_->renderer->CurrentFrame()
        : nullptr;
}

void Media::CompositionTarget::AddRendering(
    View& view,
    const RenderingEventHandler& handler) noexcept {
    if (view.state_ != nullptr && !handler.Empty()) {
        view.state_->renderingHandlers.Add(handler);
    }
}

bool Media::CompositionTarget::RemoveRendering(
    View& view,
    const RenderingEventHandler& handler) noexcept {
    return view.state_ != nullptr &&
        view.state_->renderingHandlers.Remove(handler);
}

void Media::CompositionTarget::RaiseRendering(View& view) noexcept {
    if (view.state_ != nullptr &&
        !view.state_->renderingHandlers.Empty()) {
        view.state_->renderingHandlers.Invoke();
    }
    RenderingEventHandler& legacy =
        LegacyCompositionRenderingHandlers();
    if (!legacy.Empty()) legacy.Invoke();
}

ViewRenderer::ViewRenderer(
    View& view,
    Base::IAllocator& allocator) noexcept
    : allocator_(&allocator), view_(&view) {}

ViewRenderer::~ViewRenderer() noexcept {
    Shutdown();
}

Base::Result<void> ViewRenderer::Init(
    Base::Ref<RenderDevice> device) noexcept {
    if (view_ == nullptr ||
        view_->state_ == nullptr ||
        !view_->state_->initialized) {
        return ViewNotInitialized(
            "Renderer requires an initialized View");
    }
    if (!device) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Renderer requires a RenderDevice");
    }
    if (initialized_) {
        return device_.Get() == device.Get()
            ? Base::Result<void>()
            : Base::Result<void>(Base::Status::Failure(
                  Base::ErrorCode::AlreadyExists,
                  "Renderer is already initialized"));
    }

    if (device->State() != RenderDeviceState::Ready) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Device is not ready");
    }

    auto& data = *view_->state_;
    Base::Ref<RenderDevice> previous =
        data.device;
    const bool changingDevice =
        previous.Get() != device.Get();
    if (changingDevice && previous) {
        Base::Result<void> idle =
            previous->WaitIdle();
        if (!idle) return idle.GetStatus();

        Aero::Render::ImageResources*
            previousImages = data.GetImageResources();
        if (data.images != nullptr) {
            data.images->ReleaseBackendResources(
                previousImages);
        }
        data.VisitTextElements(
            data.RootVisual(), nullptr);
        if (data.text != nullptr) {
            Base::Result<bool> detached =
                data.text->SynchronizeBackend(
                    *previous, nullptr, true);
            if (!detached) return detached.GetStatus();
        }
        data.VisitPaths(
            data.RootVisual(), nullptr);
        if (data.tree != nullptr) data.tree->SetMeshResources(nullptr);
        ShutdownRenderResources();
    }

    if (!frameEncoder_.has_value()) {
        Base::Result<void> prepared = InitializeRenderResources(
            *device,
            device->Generation());
        if (!prepared) {
            ShutdownRenderResources();
            return prepared.GetStatus();
        }
    }

    Base::Result<void> status;
    data.device = device;
    data.deviceGeneration =
        device->Generation();
    if (data.tree != nullptr) {
        data.tree->SetMeshResources(data.GetMeshResources());
    }
    data.VisitPaths(
        data.RootVisual(),
        data.GetMeshResources(),
        true);
    if (data.tree != nullptr) {
        data.tree->SetTextLayout(data.text != nullptr ? data.text->Layout() : nullptr);
    }
    data.VisitTextElements(
        data.RootVisual(),
        data.text != nullptr ? data.text->Layout() : nullptr,
        true);

    Aero::Media::Visual* rootVisual =
        data.RootVisual();
    if (rootVisual != nullptr) {
        status = data.renderer->Invalidate(
            *rootVisual,
            Aero::Render::RenderInvalidation::All);
    }
    if (!status) {
        return status.GetStatus();
    }

    device_ = std::move(device);
    updatedVersion_ = 0U;
    renderedVersion_ = 0U;
    offscreenReady_ = false;
    initialized_ = true;
    return {};
}

void ViewRenderer::Shutdown() noexcept {
    if (device_) {
        static_cast<void>(device_->WaitIdle());
    }
    ShutdownRenderResources();
    device_.Reset();
    updatedVersion_ = 0U;
    renderedVersion_ = 0U;
    offscreenReady_ = false;
    initialized_ = false;
}

bool ViewRenderer::IsInitialized() const noexcept {
    return initialized_;
}

bool ViewRenderer::UpdateRenderTree() noexcept {
    if (!initialized_ || !device_ ||
        view_ == nullptr || view_->state_ == nullptr ||
        !view_->state_->initialized) {
        if (view_ != nullptr && view_->state_ != nullptr) {
            view_->state_->ReportRendererFailure(ViewNotInitialized(
                "Renderer must be initialized before UpdateRenderTree"));
        }
        return false;
    }

    if (device_->State() != RenderDeviceState::Ready) {
        view_->state_->ReportRendererFailure(ViewApiInvalidState(
            "Render device is not ready"));
        return false;
    }

    auto& data = *view_->state_;
    if (data.renderer == nullptr) {
        data.ReportRendererFailure(ViewNotInitialized(
            "View render tree is unavailable"));
        return false;
    }
    const ::Aero::Render::RenderFrame& frame =
        data.renderer->CurrentFrame();
    if (frame.Version() == 0U) {
        data.ClearRendererFailure();
        return false;
    }
    Base::Result<void> valid =
        ::Aero::Render::ValidateRenderFrame(frame);
    if (!valid) {
        data.ReportRendererFailure(valid.GetStatus());
        return false;
    }

    const bool changed =
        frame.Version() != updatedVersion_;
    if (changed) {
        updatedVersion_ = frame.Version();
        offscreenReady_ = false;
    }
    data.ClearRendererFailure();
    return changed;
}

bool ViewRenderer::RenderOffscreen() noexcept {
    if (!initialized_ || !device_ || !frameEncoder_.has_value() ||
        view_ == nullptr || view_->state_ == nullptr) {
        if (view_ != nullptr && view_->state_ != nullptr) {
            view_->state_->ReportRendererFailure(ViewNotInitialized(
                "Renderer must be initialized before RenderOffscreen"));
        }
        return false;
    }

    const ::Aero::Render::RenderFrame& frame =
        view_->state_->renderer->CurrentFrame();
    if (frame.Version() == 0U) {
        offscreenReady_ = true;
        view_->state_->ClearRendererFailure();
        return true;
    }
    if (frame.PixelWidth() == 0U || frame.PixelHeight() == 0U) {
        offscreenReady_ = true;
        view_->state_->ClearRendererFailure();
        return true;
    }
    if (frame.Version() != updatedVersion_) {
        view_->state_->ReportRendererFailure(ViewApiInvalidState(
            "UpdateRenderTree must run before RenderOffscreen"));
        return false;
    }

    Base::Result<void> submitted =
        RenderOffscreenFrame(frame);
    if (!submitted) {
        view_->state_->ReportRendererFailure(submitted.GetStatus());
        return false;
    }
    offscreenReady_ = true;
    view_->state_->ClearRendererFailure();
    return true;
}

void ViewRenderer::Render(
    RenderTarget& target) noexcept {
    if (!initialized_ || !device_ || !frameEncoder_.has_value() ||
        view_ == nullptr || view_->state_ == nullptr) {
        if (view_ != nullptr && view_->state_ != nullptr) {
            view_->state_->ReportRendererFailure(ViewNotInitialized(
                "Renderer must be initialized before Render"));
        }
        return;
    }

    Base::Ref<RenderDevice> surfaceDevice = target.GetDevice();
    if (!surfaceDevice || surfaceDevice.Get() != device_.Get()) {
        view_->state_->ReportRendererFailure(ViewApiInvalidState(
            "RenderTarget must belong to the renderer RenderDevice"));
        return;
    }

    const ::Aero::Render::RenderFrame& frame =
        view_->state_->renderer->CurrentFrame();
    if (frame.Version() == 0U) {
        view_->state_->ClearRendererFailure();
        return;
    }
    if (frame.Version() != updatedVersion_) {
        view_->state_->ReportRendererFailure(ViewApiInvalidState(
            "UpdateRenderTree must run before Render"));
        return;
    }
    if (!offscreenReady_) {
        view_->state_->ReportRendererFailure(ViewApiInvalidState(
            "RenderOffscreen must run before Render"));
        return;
    }
    if (frame.PixelWidth() == 0U || frame.PixelHeight() == 0U) {
        renderedVersion_ = frame.Version();
        offscreenReady_ = false;
        view_->state_->ClearRendererFailure();
        return;
    }

    Base::Result<void> submitted =
        RenderOnscreenFrame(frame, target);
    if (!submitted) {
        view_->state_->ReportRendererFailure(submitted.GetStatus());
        return;
    }

    renderedVersion_ = frame.Version();
    offscreenReady_ = false;
    view_->state_->ClearRendererFailure();
}

const ::Aero::Render::RenderFrame* CurrentFrameForConformance(
    const View& view) noexcept {
    return ViewState::CurrentFrame(view);
}


} // namespace Aero
