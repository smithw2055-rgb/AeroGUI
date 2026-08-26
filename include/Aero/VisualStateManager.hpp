#pragma once

#include <Aero/Media/Animation.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyProperty.hpp>

#include <utility>

namespace Aero::Controls { class Control; }

namespace Aero {

class AERO_GUI_API VisualState : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        VisualState, Base::Object, "urn:aero", "VisualState")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    StringView GetName() const noexcept { return name_.View(); }
    Result<void> SetName(StringView value) noexcept {
        return name_.Assign(value);
    }
    Span<const Ref<Base::Object>> GetSetters() const noexcept {
        return {setters_.Data(), setters_.Size()};
    }
    Result<void> AddSetter(
        Ref<Base::Object> value) noexcept {
        return setters_.PushBack(std::move(value));
    }
    void ClearSetters() noexcept { setters_.Clear(); }
    const Ref<Media::Animation::Storyboard>&
    GetStoryboard() const noexcept {
        return storyboard_;
    }
    Result<void> SetStoryboard(
        Ref<Media::Animation::Storyboard> value) noexcept {
        if (storyboard_ && value) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "VisualState accepts only one Storyboard");
        }
        storyboard_ = std::move(value);
        return {};
    }

private:
    String name_;
    Base::Vector<Ref<Base::Object>> setters_;
    Ref<Media::Animation::Storyboard> storyboard_;
};

class AERO_GUI_API VisualTransition : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        VisualTransition, Base::Object, "urn:aero", "VisualTransition")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    StringView GetFrom() const noexcept { return from_.View(); }
    StringView GetTo() const noexcept { return to_.View(); }
    StringView GetGeneratedDuration() const noexcept {
        return generatedDuration_.View();
    }
    Result<void> SetFrom(StringView value) noexcept {
        return from_.Assign(value);
    }
    Result<void> SetTo(StringView value) noexcept {
        return to_.Assign(value);
    }
    Result<void> SetGeneratedDuration(
        StringView value) noexcept {
        Media::Animation::Storyboard validator;
        Result<void> valid =
            validator.SetDurationChecked(value);
        if (!valid) return valid.GetStatus();
        return generatedDuration_.Assign(value);
    }
    Ref<Media::Animation::EasingFunctionBase>
    GetGeneratedEasingFunction() const noexcept {
        return generatedEasingFunction_;
    }
    Result<void> SetGeneratedEasingFunction(
        Ref<Media::Animation::EasingFunctionBase> value) noexcept {
        generatedEasingFunction_ = std::move(value);
        return {};
    }
    const Ref<Media::Animation::Storyboard>&
    GetStoryboard() const noexcept {
        return storyboard_;
    }
    Result<void> SetStoryboard(
        Ref<Media::Animation::Storyboard> value) noexcept {
        if (storyboard_ && value) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "VisualTransition accepts only one Storyboard");
        }
        storyboard_ = std::move(value);
        return {};
    }

private:
    String from_;
    String to_;
    String generatedDuration_;
    Ref<Media::Animation::EasingFunctionBase>
        generatedEasingFunction_;
    Ref<Media::Animation::Storyboard> storyboard_;
};

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

class AERO_GUI_API VisualStateGroupCollection : public Base::Object {
    AERO_DECLARE_TYPE(VisualStateGroupCollection, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Span<const Ref<VisualStateGroup>> GetItems() const noexcept {
        return {items_.Data(), items_.Size()};
    }
    Result<void> Add(Ref<VisualStateGroup> value) noexcept {
        return items_.PushBack(std::move(value));
    }
    void Clear() noexcept { items_.Clear(); }

private:
    Base::Vector<Ref<VisualStateGroup>> items_;
};

struct VisualStateManagerRuntime;

// Public authoring uses the WPF static entry point. Runtime state and animation
// bookkeeping remain private and are accessed only by the controls runtime.
class AERO_GUI_API VisualStateManager : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        VisualStateManager, Base::Object, "urn:aero", "VisualStateManager")
public:

    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    static bool GoToState(
        Controls::Control& control,
        StringView stateName,
        bool useTransitions = true) noexcept;

    inline static constexpr AttachedProperty<Ref<VisualStateGroupCollection>> VisualStateGroupsProperty{"VisualStateGroups"};

    ~VisualStateManager() noexcept override;
    VisualStateManager(const VisualStateManager&) = delete;
    VisualStateManager& operator=(const VisualStateManager&) = delete;

private:
    friend struct VisualStateManagerRuntime;
    VisualStateManager() noexcept = default;
    void* impl_ = nullptr;
};

} // namespace Aero
