#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Triggers/Setter.hpp>
#include <Aero/Triggers/SetterBase.hpp>

namespace Aero {

using Meta::TypeId;

class AERO_GUI_API TriggerBase : public Base::Object {
    AERO_DECLARE_TYPE(TriggerBase, Base::Object)
public:
    TypeId RuntimeType() const noexcept override { return runtimeType_; }
    Result<void> AddEnterAction(Ref<Base::Object> action) noexcept;
    Result<void> AddExitAction(Ref<Base::Object> action) noexcept;
    void ClearEnterActions() noexcept { enterActions_.Clear(); }
    void ClearExitActions() noexcept { exitActions_.Clear(); }
    Span<const Ref<Base::Object>> GetEnterActions() const noexcept {
        return {enterActions_.Data(), enterActions_.Size()};
    }
    Span<const Ref<Base::Object>> GetExitActions() const noexcept {
        return {exitActions_.Data(), exitActions_.Size()};
    }
    Result<void> AddBehavior(Ref<Base::Object> behavior) noexcept {
        return behavior ? behaviors_.PushBack(std::move(behavior))
                        : Result<void>(Base::Status::Failure(
                              Base::ErrorCode::InvalidArgument,
                              "Trigger behavior cannot be null"));
    }
    void ClearBehaviors() noexcept { behaviors_.Clear(); }
    Span<const Ref<Base::Object>> GetBehaviors() const noexcept {
        return {behaviors_.Data(), behaviors_.Size()};
    }

protected:
    explicit TriggerBase(TypeId runtimeType) noexcept : runtimeType_(runtimeType) {}
    ~TriggerBase() override = default;

private:
    TypeId runtimeType_ = StaticTypeId();
    Base::Vector<Ref<Base::Object>> enterActions_;
    Base::Vector<Ref<Base::Object>> exitActions_;
    Base::Vector<Ref<Base::Object>> behaviors_;
};

} // namespace Aero
