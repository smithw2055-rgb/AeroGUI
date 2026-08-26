#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/VisualState.hpp>
#include <Aero/VisualTransition.hpp>

namespace Aero {

class AERO_GUI_API VisualStateGroup : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        VisualStateGroup, Base::Object, "urn:aero", "VisualStateGroup")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    StringView GetName() const noexcept { return name_.View(); }
    Result<void> SetName(StringView value) noexcept {
        return name_.Assign(value);
    }
    Span<const Ref<VisualState>> GetStates() const noexcept {
        return {states_.Data(), states_.Size()};
    }
    Result<void> AddState(Ref<VisualState> value) noexcept {
        return states_.PushBack(std::move(value));
    }
    void ClearStates() noexcept { states_.Clear(); }
    Span<const Ref<VisualTransition>>
    GetTransitions() const noexcept {
        return {transitions_.Data(), transitions_.Size()};
    }
    Result<void> AddTransition(
        Ref<VisualTransition> value) noexcept {
        return transitions_.PushBack(std::move(value));
    }
    void ClearTransitions() noexcept { transitions_.Clear(); }

private:
    String name_;
    Base::Vector<Ref<VisualState>> states_;
    Base::Vector<Ref<VisualTransition>> transitions_;
};

} // namespace Aero
