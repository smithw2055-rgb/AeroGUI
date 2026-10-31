#include "gui/ViewFrame.hpp"
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


Aero::Render::MeshResources* ViewFrame::GetMeshResources() noexcept {
    return publicRenderer.Resources().meshes;
}

Aero::Render::ImageResources* ViewFrame::GetImageResources() noexcept {
    return publicRenderer.Resources().images;
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
