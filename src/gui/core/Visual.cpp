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

Base::Transform2D ToRootTransform(const Visual& visual) noexcept {
    Base::Transform2D result;
    const Visual* current = &visual;
    while (current != nullptr) {
        const ::Aero::UIElement* element =
            ::Aero::TryCast<::Aero::UIElement>(current);
        const ::Aero::FrameworkElement* framework =
            ::Aero::TryCast<::Aero::FrameworkElement>(current);
        if (element != nullptr) {
            Base::Transform2D local = framework != nullptr
                ? framework->GetLocalVisualTransform()
                : Base::Transform2D{};
            const Base::Rect slot = element->GetLayoutSlot();
            local = ComposeTransforms(local, Translation(slot.x, slot.y));
            result = ComposeTransforms(result, local);
        }
        current = current->GetVisualParent();
    }
    return result;
}

} // namespace

Base::Transform2D Visual::TransformToVisual(
    const Visual& visual) const noexcept {
    if (this == &visual) {
        return {};
    }
    const Base::Transform2D fromRoot = ToRootTransform(*this);
    const Base::Transform2D toRoot = ToRootTransform(visual);
    Base::Transform2D inverse;
    if (!InvertTransform(toRoot, inverse)) {
        return {};
    }
    return ComposeTransforms(fromRoot, inverse);
}

Base::Point Visual::PointToScreen(Base::Point point) const noexcept {
    // Root-visual space is the screen space for this tree. Host window origin
    // and pixel conversion are not part of the Visual contract.
    return TransformPoint(ToRootTransform(*this), point);
}

Base::Point Visual::PointFromScreen(Base::Point point) const noexcept {
    Base::Transform2D inverse;
    if (!InvertTransform(ToRootTransform(*this), inverse)) {
        return point;
    }
    return TransformPoint(inverse, point);
}
} // namespace Media {
} // namespace Aero {
