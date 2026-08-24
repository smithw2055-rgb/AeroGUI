// Auto-relocated base-class method definitions (WPF semantic kernel).
#include <Aero/FrameworkContentElement.hpp>
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

// from src/gui/core/ContentElement.cpp

void FrameworkContentElement::SetResources(
    Base::Ref<ResourceDictionary> value) noexcept {
    (void)Aero::AssignResourceDictionary(
        resources_,
        std::move(value),
        "FrameworkContentElement Resources is already assigned");
}

// from src/gui/core/ContentElement.cpp

void FrameworkContentElement::ClearAuthoredTriggers() noexcept {
    authoredTriggers_.Clear();
}

// from src/gui/core/ContentElement.cpp

Base::Result<void> FrameworkContentElement::AddAuthoredTrigger(
    Base::Ref<Base::Object> trigger) noexcept {
    if (!trigger) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "FrameworkContentElement trigger cannot be null");
    }
    return authoredTriggers_.PushBack(std::move(trigger));
}
} // namespace Aero {
