#pragma once

#include <Aero/Gui/Storyboard.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Gui/Control.hpp>
#include <Aero/Gui/ResourceDictionary.hpp>
#include <Aero/Gui/Style.hpp>

namespace Aero {

class AERO_GUI_API VisualState : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        VisualState, Base::Object, "urn:aero", "VisualState")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::StringView GetName() const noexcept { return name_.View(); }
    Base::Result<void> SetName(Base::StringView value) noexcept {
        return name_.Assign(value);
    }
    Base::Span<const Base::Ref<Base::Object>> GetSetters() const noexcept {
        return {setters_.Data(), setters_.Size()};
    }
    Base::Result<void> AddSetter(
        Base::Ref<Base::Object> value) noexcept {
        return setters_.PushBack(std::move(value));
    }
    void ClearSetters() noexcept { setters_.Clear(); }
    const Base::Ref<Media::Animation::Storyboard>&
    GetStoryboard() const noexcept {
        return storyboard_;
    }
    Base::Result<void> SetStoryboard(
        Base::Ref<Media::Animation::Storyboard> value) noexcept {
        if (storyboard_ && value) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "VisualState accepts only one Storyboard");
        }
        storyboard_ = std::move(value);
        return {};
    }

private:
    Base::String name_;
    Base::Vector<Base::Ref<Base::Object>> setters_;
    Base::Ref<Media::Animation::Storyboard> storyboard_;
};

class AERO_GUI_API VisualTransition : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        VisualTransition, Base::Object, "urn:aero", "VisualTransition")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::StringView GetFrom() const noexcept { return from_.View(); }
    Base::StringView GetTo() const noexcept { return to_.View(); }
    Base::StringView GetGeneratedDuration() const noexcept {
        return generatedDuration_.View();
    }
    Base::Result<void> SetFrom(Base::StringView value) noexcept {
        return from_.Assign(value);
    }
    Base::Result<void> SetTo(Base::StringView value) noexcept {
        return to_.Assign(value);
    }
    Base::Result<void> SetGeneratedDuration(
        Base::StringView value) noexcept {
        Media::Animation::Storyboard validator;
        Base::Result<void> valid =
            validator.SetDurationChecked(value);
        if (!valid) return valid.GetStatus();
        return generatedDuration_.Assign(value);
    }
    Base::Ref<Media::Animation::EasingFunctionBase>
    GetGeneratedEasingFunction() const noexcept {
        return generatedEasingFunction_;
    }
    Base::Result<void> SetGeneratedEasingFunction(
        Base::Ref<Media::Animation::EasingFunctionBase> value) noexcept {
        generatedEasingFunction_ = std::move(value);
        return {};
    }
    const Base::Ref<Media::Animation::Storyboard>&
    GetStoryboard() const noexcept {
        return storyboard_;
    }
    Base::Result<void> SetStoryboard(
        Base::Ref<Media::Animation::Storyboard> value) noexcept {
        if (storyboard_ && value) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "VisualTransition accepts only one Storyboard");
        }
        storyboard_ = std::move(value);
        return {};
    }

private:
    Base::String from_;
    Base::String to_;
    Base::String generatedDuration_;
    Base::Ref<Media::Animation::EasingFunctionBase>
        generatedEasingFunction_;
    Base::Ref<Media::Animation::Storyboard> storyboard_;
};

class AERO_GUI_API VisualStateGroup : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        VisualStateGroup, Base::Object, "urn:aero", "VisualStateGroup")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::StringView GetName() const noexcept { return name_.View(); }
    Base::Result<void> SetName(Base::StringView value) noexcept {
        return name_.Assign(value);
    }
    Base::Span<const Base::Ref<VisualState>> GetStates() const noexcept {
        return {states_.Data(), states_.Size()};
    }
    Base::Result<void> AddState(Base::Ref<VisualState> value) noexcept {
        return states_.PushBack(std::move(value));
    }
    void ClearStates() noexcept { states_.Clear(); }
    Base::Span<const Base::Ref<VisualTransition>>
    GetTransitions() const noexcept {
        return {transitions_.Data(), transitions_.Size()};
    }
    Base::Result<void> AddTransition(
        Base::Ref<VisualTransition> value) noexcept {
        return transitions_.PushBack(std::move(value));
    }
    void ClearTransitions() noexcept { transitions_.Clear(); }

private:
    Base::String name_;
    Base::Vector<Base::Ref<VisualState>> states_;
    Base::Vector<Base::Ref<VisualTransition>> transitions_;
};

class AERO_GUI_API VisualStateGroupCollection : public Base::Object {
    AERO_DECLARE_TYPE(VisualStateGroupCollection, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Span<const Base::Ref<VisualStateGroup>> GetItems() const noexcept {
        return {items_.Data(), items_.Size()};
    }
    Base::Result<void> Add(Base::Ref<VisualStateGroup> value) noexcept {
        return items_.PushBack(std::move(value));
    }
    void Clear() noexcept { items_.Clear(); }

private:
    Base::Vector<Base::Ref<VisualStateGroup>> items_;
};

// WPF-shaped template object. XAML compilation, factory callbacks, bindings,
// triggers, namescopes and the immutable runtime program are implementation
// details owned by the markup and controls runtime.
class AERO_GUI_API FrameworkTemplate : public Base::Object {
    AERO_DECLARE_TYPE(FrameworkTemplate, Base::Object)
public:
    struct Access;

    FrameworkTemplate() noexcept;
    ~FrameworkTemplate() noexcept override;

    FrameworkTemplate(const FrameworkTemplate&) = delete;
    FrameworkTemplate& operator=(const FrameworkTemplate&) = delete;

    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Meta::TypeId GetTargetType() const noexcept;
    bool GetIsSealed() const noexcept;
    ResourceDictionary& GetResources() noexcept;
    const ResourceDictionary& GetResources() const noexcept;
    void SetResources(Base::Ref<ResourceDictionary> value) noexcept;

private:
    friend struct Access;
    void* state_ = nullptr;
};

} // namespace Aero

namespace Aero::Controls {

class AERO_GUI_API ControlTemplate : public Aero::FrameworkTemplate {
    AERO_DECLARE_TYPE(ControlTemplate, FrameworkTemplate)
public:
    ControlTemplate() noexcept = default;
    ~ControlTemplate() noexcept override = default;
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
};

} // namespace Aero::Controls

namespace Aero {

// Public authoring uses the WPF static entry point. Runtime state and animation
// bookkeeping remain private and are accessed only by the controls runtime.
class AERO_GUI_API VisualStateManager : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        VisualStateManager, Base::Object, "urn:aero", "VisualStateManager")
public:
    struct Access;

    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    static bool GoToState(
        Controls::Control& control,
        Base::StringView stateName,
        bool useTransitions = true) noexcept;

    inline static constexpr AttachedProperty<Base::Ref<VisualStateGroupCollection>> VisualStateGroupsProperty{"VisualStateGroups"};

    ~VisualStateManager() noexcept override;
    VisualStateManager(const VisualStateManager&) = delete;
    VisualStateManager& operator=(const VisualStateManager&) = delete;

private:
    friend struct Access;
    VisualStateManager() noexcept = default;
    void* impl_ = nullptr;
};

} // namespace Aero
