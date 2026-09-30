#include "gui/ViewFrame.hpp"
#include <Aero/Shapes.hpp>
#include "gui/text/TextPipeline.hpp"
#include "render/RenderTree.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

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


void ViewFrame::AttachTextLayout(
        Aero::Media::Visual& node,
        ::Aero::Controls::TextBlockLayout* service,
        bool invalidate) noexcept {
        if (metadata == nullptr) return;
        const Meta::TypeId type = node.RuntimeType();
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::TextBlock::StaticTypeId())) {
            (*static_cast<Controls::TextBlock*>(&node)).AttachTextLayout(
                service,
                invalidate);
        }
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::TextBox::StaticTypeId())) {
            (*static_cast<Controls::TextBox*>(&node)).AttachTextLayout(
                service,
                invalidate);
        }
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::PasswordBox::
                    StaticTypeId())) {
            (*static_cast<Controls::PasswordBox*>(
                    &node)).AttachTextLayout(
                service,
                invalidate);
        }
    }

Aero::Render::MeshResources*
 ViewFrame::GetMeshResources() noexcept {
        return publicRenderer.Resources().meshes;
    }

Aero::Render::ImageResources*
 ViewFrame::GetImageResources() noexcept {
        return publicRenderer.Resources().images;
    }

void ViewFrame::AttachPathResources(
        Aero::Media::Visual& node,
        Aero::Render::MeshResources* service,
        bool invalidate) noexcept {
        if (metadata == nullptr) return;
        const Meta::TypeId type = node.RuntimeType();
        if (metadata->Types().IsDerivedFrom(
                type,
                Shapes::Path::StaticTypeId())) {
            (*static_cast<Shapes::Path*>(&node)).AttachMeshResources(
                service,
                invalidate);
        }
    }

void ViewFrame::VisitTextElements(
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
             (*rootVisual).RenderChildren()) {
            VisitTextElements(
                child,
                service,
                invalidate,
                effectivelyVisible);
        }
    }

void ViewFrame::VisitPaths(
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
             (*rootVisual).RenderChildren()) {
            VisitPaths(
                child,
                service,
                invalidate,
                effectivelyVisible);
        }
    }

void ViewFrame::TextLifecycleHook(
        const Aero::ElementTreeLifecycleEvent& event,
        void* context) noexcept {
        auto* runtime = static_cast<ViewFrame*>(context);
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

const ::Aero::Render::RenderFrame* ViewFrame::CurrentFrame(
    const View& view) noexcept
{
    return view.state_ != nullptr && view.state_->RenderTree() != nullptr
        ? &view.state_->RenderTree()->CurrentFrame()
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

const ::Aero::Render::RenderFrame* CurrentFrameForConformance(
    const View& view) noexcept {
    return ViewFrame::CurrentFrame(view);
}

double MaxAbsCommittedProjectiveM13(const View& view) noexcept {
    const ::Aero::Render::RenderFrame* frame =
        ViewFrame::CurrentFrame(view);
    if (frame == nullptr) return 0.0;
    double maxAbs = 0.0;
    for (const ::Aero::Render::RenderNodeSnapshot& node : frame->Nodes()) {
        maxAbs = std::max(maxAbs, std::abs(node.renderTransform.m13));
        maxAbs = std::max(maxAbs, std::abs(node.renderTransform.m23));
        maxAbs = std::max(maxAbs, std::abs(node.renderTransform.m33 - 1.0));
    }
    return maxAbs;
}


} // namespace Aero
