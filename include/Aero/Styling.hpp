#pragma once

#include <Aero/Animation.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Base.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Style.hpp>

namespace Aero::Controls::Detail {
class FrameworkTemplateAccess;
class VisualStateManagerAccess;
}

namespace Aero::Controls {

struct VisualStateSetter final {
    Base::String targetName;
    DependencyPropertyHandle property;
    Core::PropertyValue value;
};

struct VisualState final {
    Base::String name;
    Base::Vector<VisualStateSetter> setters;
    Base::Ref<Media::Animation::Storyboard> storyboard;
};

struct VisualTransition final {
    Base::String from;
    Base::String to;
    Media::Animation::AnimationTime generatedDurationMicroseconds = 0U;
    Base::Ref<Media::Animation::EasingFunctionBase> generatedEasingFunction;
    Base::Ref<Media::Animation::Storyboard> storyboard;
};

struct VisualStateGroup final {
    Base::String name;
    Base::Vector<VisualState> states;
    Base::Vector<VisualTransition> transitions;
};

// WPF-shaped template object. XAML compilation, factory callbacks, bindings,
// triggers, namescopes and the immutable runtime program are implementation
// details owned by the markup and controls runtime.
class AERO_API FrameworkTemplate : public Base::Object {
    AERO_DECLARE_TYPE(FrameworkTemplate, Base::Object)
public:
    FrameworkTemplate() noexcept;
    ~FrameworkTemplate() noexcept override;

    FrameworkTemplate(const FrameworkTemplate&) = delete;
    FrameworkTemplate& operator=(const FrameworkTemplate&) = delete;

    Core::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Core::TypeId GetTargetType() const noexcept;
    bool GetIsSealed() const noexcept;
    ResourceDictionary& GetResources() noexcept;
    const ResourceDictionary& GetResources() const noexcept;
    Base::Result<void> SetResources(Base::Ref<ResourceDictionary> value) noexcept;

private:
    friend class Detail::FrameworkTemplateAccess;
    void* state_ = nullptr;
};

class AERO_API ControlTemplate final : public FrameworkTemplate {
    AERO_DECLARE_TYPE(ControlTemplate, FrameworkTemplate)
public:
    ControlTemplate() noexcept = default;
    ~ControlTemplate() noexcept override = default;
    Core::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
};

// Public authoring uses the WPF static entry point. Runtime state and animation
// bookkeeping remain private and are accessed only by the controls runtime.
class AERO_API VisualStateManager final {
public:
    static bool GoToState(Control& control, Base::StringView stateName, bool useTransitions = true) noexcept;

    ~VisualStateManager() noexcept;
    VisualStateManager(const VisualStateManager&) = delete;
    VisualStateManager& operator=(const VisualStateManager&) = delete;

private:
    friend class Detail::VisualStateManagerAccess;
    VisualStateManager() noexcept = default;
    void* impl_ = nullptr;
};

} // namespace Aero::Controls
