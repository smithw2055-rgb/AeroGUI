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
#include "gui/core/State.hpp" 
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
    Base::ProjectiveTransform2D result = Base::IdentityProjective();
    const Visual* current = &visual;
    while (current != nullptr) {
        const ::Aero::UIElement* element =
            ::Aero::TryCast<::Aero::UIElement>(current);
        const ::Aero::FrameworkElement* framework =
            ::Aero::TryCast<::Aero::FrameworkElement>(current);
        if (element != nullptr) {
            Base::ProjectiveTransform2D local = framework != nullptr
                ? framework->GetLocalVisualTransform()
                : Base::IdentityProjective();
            const Base::Rect slot = element->GetLayoutSlot();
            local = Base::Compose(
                local, Base::ToProjective(Translation(slot.x, slot.y)));
            result = Base::Compose(result, local);
        }
        current = current->GetVisualParent();
    }
    return result;
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
