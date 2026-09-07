// Auto-relocated base-class method definitions (WPF semantic kernel).
#include <Aero/FrameworkContentElement.hpp>
#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Events.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Controls.hpp>
#include <cstdio>
#include <new>
#include "gui/core/state/ElementTree.hpp"
#include "gui/core/state/LayoutEngine.hpp"
#include "gui/core/state/FreezableState.hpp"
#include "gui/core/state/PropertyEngine.hpp"
#include "gui/core/state/RoutedEvents.hpp"
#include "gui/core/state/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/input/InputState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/meta/MetadataState.hpp"

using namespace Aero;
using namespace Aero::Media;
using namespace Aero::Meta;
using namespace Aero::Threading;

namespace Aero {

struct FrameworkContentElement::FrameworkContentRare {
    Base::Vector<Base::Ref<Base::Object>> authoredTriggers;
};

FrameworkContentElement::FrameworkContentRare*
FrameworkContentElement::EnsureFrameworkContentRare() noexcept {
    if (frameworkRare_ == nullptr) {
        frameworkRare_ = new (std::nothrow) FrameworkContentRare();
    }
    return frameworkRare_;
}

FrameworkContentElement::~FrameworkContentElement() {
    delete resources_;
    resources_ = nullptr;
    delete frameworkRare_;
    frameworkRare_ = nullptr;
}

// from src/gui/core/ContentElement.cpp

ResourceDictionary& FrameworkContentElement::GetResources() noexcept {
    return EnsureOwnedResources(resources_);
}

const ResourceDictionary&
FrameworkContentElement::GetResources() const noexcept {
    return EnsureOwnedResources(resources_);
}

void FrameworkContentElement::SetResources(
    Base::Ref<ResourceDictionary> value) noexcept {
    (void)Aero::AssignResourceDictionary(
        EnsureOwnedResources(resources_),
        std::move(value),
        "FrameworkContentElement Resources is already assigned");
}

// from src/gui/core/ContentElement.cpp

void FrameworkContentElement::ClearAuthoredTriggers() noexcept {
    if (frameworkRare_ != nullptr) {
        frameworkRare_->authoredTriggers.Clear();
    }
}

Base::Span<const Base::Ref<Base::Object>>
FrameworkContentElement::AuthoredTriggers() const noexcept {
    return frameworkRare_ != nullptr
        ? frameworkRare_->authoredTriggers.AsSpan()
        : Base::Span<const Base::Ref<Base::Object>>{};
}

// from src/gui/core/ContentElement.cpp

void FrameworkContentElement::AddAuthoredTrigger(
    Base::Ref<Base::Object> trigger) noexcept {
    if (!trigger) { AERO_ASSERT(false); return; }
    FrameworkContentRare* rare = EnsureFrameworkContentRare();
    if (rare == nullptr) { AERO_ASSERT(false); return; }
    Base::Result<void> pushed = rare->authoredTriggers.PushBack(std::move(trigger));
    if (!pushed) { AERO_ASSERT(false); return; }
}
} // namespace Aero {
