// Auto-relocated base-class method definitions (WPF semantic kernel).
#include <Aero/Visual.hpp>
#include <Aero/UIElement.hpp>
#include <Aero/FrameworkElement.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Allocator.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Events.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Controls.hpp>
#include <cstdio>
#include <cstdint>
#include "gui/core/State.hpp"
#include "gui/media/Transform3DMath.hpp" 
#include "gui/input/InputState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/meta/MetadataState.hpp"

using namespace Aero;
using namespace Aero::Media;
using namespace Aero::Meta;
using namespace Aero::Threading;

namespace Aero {
namespace Media {

// from src/gui/core/ElementTree.cpp

Base::Result<Base::Ref<Base::Object>>
Visual::AcquireLifetime() noexcept {
    if (!lifetime_) {
        Base::Result<Base::Ref<Aero::VisualLifetime>> created =
            Base::MakeRef<Aero::VisualLifetime>(*this);
        if (!created) return created.GetStatus();
        lifetime_ = std::move(created).Value();
    }
    return lifetime_;
}

bool Visual::IsAncestorOf(const Visual& descendant) const noexcept {
    if (this == &descendant) {
        return false;
    }
    const Visual* current = descendant.GetVisualParent();
    while (current != nullptr) {
        if (current == this) {
            return true;
        }
        current = current->GetVisualParent();
    }
    return false;
}

namespace {

Base::Transform2D Translation(double x, double y) noexcept {
    Base::Transform2D result;
    result.dx = x;
    result.dy = y;
    return result;
}

Base::ProjectiveTransform2D ToRootTransformProjective(
    const Visual& visual) noexcept {
    const Visual* chain[64];
    std::uint32_t count = 0U;
    for (const Visual* current = &visual;
         current != nullptr && count < 64U;
         current = current->GetVisualParent()) {
        chain[count++] = current;
    }

    Base::Size rootSize{};
    for (std::uint32_t index = count; index > 0U; --index) {
        if (const UIElement* rootElement =
                ::Aero::TryCast<UIElement>(chain[index - 1U])) {
            rootSize = rootElement->GetRenderSize();
            break;
        }
    }
    Media::Transform3DContext incoming =
        Media::MakeImplicitViewRootContext(rootSize);
    Base::ProjectiveTransform2D world = Base::IdentityProjective();

    for (std::uint32_t index = count; index > 0U; --index) {
        const Visual* current = chain[index - 1U];
        const UIElement* element = ::Aero::TryCast<UIElement>(current);
        const FrameworkElement* framework =
            ::Aero::TryCast<FrameworkElement>(current);
        if (element == nullptr) {
            continue;
        }

        const Base::ProjectiveTransform2D localVisual =
            framework != nullptr
            ? framework->GetLocalVisualTransform()
            : Base::IdentityProjective();
        const Base::Rect slot = element->GetLayoutSlot();
        Base::Transform2D viewboxMatrix{};
        const Base::Transform2D* viewbox =
            framework != nullptr &&
                    framework->TryGetViewboxTransform(viewboxMatrix)
                ? &viewboxMatrix
                : nullptr;
        Base::Transform3 innerVisual;
        Base::Transform3 outerVisual;
        Media::LiftLocalVisualWithViewbox(
            localVisual, viewbox, innerVisual, outerVisual);
        const Media::Transform3DContext childContext =
            Media::AdvanceTransform3DContext(
                incoming,
                element->GetTransform3D().Get(),
                innerVisual,
                slot,
                element->GetRenderSize(),
                false,
                outerVisual);
        Media::Transform3D* local3D = element->GetTransform3D().Get();
        const bool local3DActive =
            local3D != nullptr &&
            ::Aero::TryCast<Media::PerspectiveTransform3D>(local3D) ==
                nullptr &&
            !Base::LeavesZ0PlaneUnchanged(local3D->GetTransform3D());
        const bool parent3DActive =
            !Base::LeavesZ0PlaneUnchanged(incoming.accumulated);
        Base::ProjectiveTransform2D renderTransform = localVisual;
        if (local3DActive) {
            renderTransform = Media::CollapseLocalTransform3D(
                local3D,
                element->GetRenderSize(),
                innerVisual,
                outerVisual,
                childContext.depth);
        } else if (!parent3DActive) {
            renderTransform = Media::CollapseRelativeToParent(
                incoming,
                childContext,
                slot,
                localVisual,
                false);
        }
        if (::Aero::TryCast<Media::PerspectiveTransform3D>(
                element->GetTransform3D().Get()) != nullptr) {
            renderTransform = localVisual;
        }
        const Base::ProjectiveTransform2D local = Base::Compose(
            renderTransform,
            Base::ToProjective(Translation(slot.x, slot.y)));
        world = Base::Compose(local, world);
        incoming = childContext;
    }
    return world;
}

} // namespace

Base::Transform2D Visual::TransformToVisual(
    const Visual& visual) const noexcept {
    Base::ProjectiveTransform2D projective;
    if (!TryTransformToVisual(visual, projective)) {
        return {};
    }
    Base::Transform2D affine;
    if (!Base::TryToTransform2D(projective, affine)) {
        return {};
    }
    return affine;
}

bool Visual::TryTransformToVisual(
    const Visual& visual,
    Base::ProjectiveTransform2D& output) const noexcept {
    if (this == &visual) {
        output = Base::IdentityProjective();
        return true;
    }
    const Base::ProjectiveTransform2D fromRoot =
        ToRootTransformProjective(*this);
    const Base::ProjectiveTransform2D toRoot =
        ToRootTransformProjective(visual);
    Base::ProjectiveTransform2D inverse;
    if (!Base::Invert(toRoot, inverse)) {
        return false;
    }
    output = Base::Compose(fromRoot, inverse);
    return Base::IsFiniteTransform(output);
}

Base::Point Visual::PointToScreen(Base::Point point) const noexcept {
    Base::Point screen{};
    if (!TryPointToScreen(point, screen)) {
        return point;
    }
    return screen;
}

bool Visual::TryPointToScreen(
    Base::Point point,
    Base::Point& screen) const noexcept {
    return Base::TryTransformPoint(
        ToRootTransformProjective(*this), point, screen);
}

Base::Point Visual::PointFromScreen(Base::Point point) const noexcept {
    Base::Point local{};
    if (!TryPointFromScreen(point, local)) {
        return point;
    }
    return local;
}

bool Visual::TryPointFromScreen(
    Base::Point point,
    Base::Point& local) const noexcept {
    return Base::TryUnprojectPointToLocalPlane(
        ToRootTransformProjective(*this), point, local);
}
} // namespace Media {
} // namespace Aero {
