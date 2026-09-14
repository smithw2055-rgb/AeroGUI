#pragma once

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/Vector.hpp>
#include <cstdint>

namespace Aero {

class DependencyObject;
class Style;

using TriggerActionHandler = Base::Result<void>(*)(
    DependencyObject& owner,
    Base::Span<const Base::Ref<Base::Object>> actions,
    void* context) noexcept;

struct StyleApplication {
    DependencyObject* object = nullptr;
    const Style* style = nullptr;
    Base::Vector<std::uint8_t> triggerStates;
    Base::Vector<std::uint8_t> bindingTriggerStates;
    Base::Vector<std::uint8_t> bindingTriggerKnown;
};

} // namespace Aero
