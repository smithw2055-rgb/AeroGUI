#pragma once

#include <Aero/Base/Assert.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Controls {
class Control;
class VisualStateManagerExecution;
struct FrameworkTemplateState;
}

namespace Aero {

class AERO_GUI_API VisualState : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(VisualState, Base::Object, "urn:aero", "VisualState")
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }

    StringView GetName() const noexcept { return name_.View(); }
    void SetName(StringView value) noexcept {
        Base::Result<void> assigned = name_.Assign(value);
        if (!assigned) { AERO_ASSERT(false); return; }
    }
    Span<const Ref<Base::Object>> GetSetters() const noexcept { return {setters_.Data(), setters_.Size()}; }
    void AddSetter(Ref<Base::Object> value) noexcept { setters_.PushBack(std::move(value)); }
    void ClearSetters() noexcept { setters_.Clear(); } const Ref<Media::Animation::Storyboard>&
    GetStoryboard() const noexcept { return storyboard_; }
    void SetStoryboard(Ref<Media::Animation::Storyboard> value) noexcept {
        if (storyboard_ && value) { AERO_ASSERT(false); return; }
        storyboard_ = std::move(value);
    }

private:
    String name_;
    Base::Vector<Ref<Base::Object>> setters_;
    Ref<Media::Animation::Storyboard> storyboard_;
};

} // namespace Aero

namespace Aero {

class AERO_GUI_API VisualTransition : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(VisualTransition, Base::Object, "urn:aero", "VisualTransition")
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }

    StringView GetFrom() const noexcept { return from_.View(); }
    StringView GetTo() const noexcept { return to_.View(); }
    StringView GetGeneratedDuration() const noexcept { return generatedDuration_.View(); }
    void SetFrom(StringView value) noexcept {
        Base::Result<void> assigned = from_.Assign(value);
        if (!assigned) { AERO_ASSERT(false); return; }
    }
    void SetTo(StringView value) noexcept {
        Base::Result<void> assigned = to_.Assign(value);
        if (!assigned) { AERO_ASSERT(false); return; }
    }
    void SetGeneratedDuration(StringView value) noexcept {
        Result<Media::Animation::Duration> valid = Media::Animation::Duration::TryParse(value);
        if (!valid) { AERO_ASSERT(false); return; }
        Base::Result<void> assigned = generatedDuration_.Assign(value);
        if (!assigned) { AERO_ASSERT(false); return; }
    }
    Ref<Media::Animation::EasingFunctionBase> GetGeneratedEasingFunction() const noexcept {
        return generatedEasingFunction_;
    }
    void SetGeneratedEasingFunction(Ref<Media::Animation::EasingFunctionBase> value) noexcept {
        generatedEasingFunction_ = std::move(value);
    } const Ref<Media::Animation::Storyboard>& GetStoryboard() const noexcept { return storyboard_; }
    void SetStoryboard(Ref<Media::Animation::Storyboard> value) noexcept {
        if (storyboard_ && value) { AERO_ASSERT(false); return; }
        storyboard_ = std::move(value);
    }

private:
    String from_;
    String to_;
    String generatedDuration_;
    Ref<Media::Animation::EasingFunctionBase> generatedEasingFunction_;
    Ref<Media::Animation::Storyboard> storyboard_;
};

} // namespace Aero

namespace Aero {

class AERO_GUI_API VisualStateGroup : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(VisualStateGroup, Base::Object, "urn:aero", "VisualStateGroup")
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }

    StringView GetName() const noexcept { return name_.View(); }
    void SetName(StringView value) noexcept {
        Base::Result<void> assigned = name_.Assign(value);
        if (!assigned) { AERO_ASSERT(false); return; }
    }
    Span<const Ref<VisualState>> GetStates() const noexcept { return {states_.Data(), states_.Size()}; }
    void AddState(Ref<VisualState> value) noexcept { states_.PushBack(std::move(value)); }
    void ClearStates() noexcept { states_.Clear(); }
    Span<const Ref<VisualTransition>> GetTransitions() const noexcept {
        return {transitions_.Data(), transitions_.Size()};
    }
    void AddTransition(Ref<VisualTransition> value) noexcept { transitions_.PushBack(std::move(value)); }
    void ClearTransitions() noexcept { transitions_.Clear(); }

private:
    String name_;
    Base::Vector<Ref<VisualState>> states_;
    Base::Vector<Ref<VisualTransition>> transitions_;
};

} // namespace Aero

namespace Aero {

class AERO_GUI_API VisualStateGroupCollection : public Base::Object {
    AERO_DECLARE_TYPE(VisualStateGroupCollection, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Span<const Ref<VisualStateGroup>> GetItems() const noexcept { return {items_.Data(), items_.Size()}; }
    void Add(Ref<VisualStateGroup> value) noexcept { items_.PushBack(std::move(value)); }
    void Clear() noexcept { items_.Clear(); }

private:
    Base::Vector<Ref<VisualStateGroup>> items_;
};

} // namespace Aero

namespace Aero {

// Public authoring uses the WPF static entry point. Runtime state and animation
// bookkeeping remain private and are accessed only by the controls runtime.
class AERO_GUI_API VisualStateManager : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(VisualStateManager, Base::Object, "urn:aero", "VisualStateManager")
public:

    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    static bool GoToState(Controls::Control& control, StringView stateName, bool useTransitions = true) noexcept;

    AERO_ATTACHED_PROPERTY(Ref<VisualStateGroupCollection>, VisualStateGroups);

    ~VisualStateManager() noexcept override;
    VisualStateManager(const VisualStateManager&) = delete;
    VisualStateManager& operator=(const VisualStateManager&) = delete;

private:
    friend class Controls::VisualStateManagerExecution;
    friend struct Controls::FrameworkTemplateState;
    VisualStateManager() noexcept = default;
    void* impl_ = nullptr;
};

} // namespace Aero
