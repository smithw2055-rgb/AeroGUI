#pragma once

#include <Aero/Animation.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Core.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Style.hpp>

namespace Aero::Internal {
class TemplatePrivate;
}

namespace Aero::Controls {

struct VisualStateSetter {
    Base::String targetName;
    DependencyPropertyHandle property;
    Meta::PropertyValue value;
};

struct VisualState {
    Base::String name;
    Base::Vector<VisualStateSetter> setters;
    Base::Ref<Media::Animation::Storyboard> storyboard;
};

struct VisualTransition {
    Base::String from;
    Base::String to;
    Media::Animation::AnimationTime generatedDurationMicroseconds = 0U;
    Base::Ref<Media::Animation::EasingFunctionBase> generatedEasingFunction;
    Base::Ref<Media::Animation::Storyboard> storyboard;
};

struct VisualStateGroup {
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

    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Meta::TypeId GetTargetType() const noexcept;
    bool GetIsSealed() const noexcept;
    ResourceDictionary& GetResources() noexcept;
    const ResourceDictionary& GetResources() const noexcept;
    void SetResources(Base::Ref<ResourceDictionary> value) noexcept;

private:
    friend class ::Aero::Internal::TemplatePrivate;
    void* state_ = nullptr;
};

class AERO_API ControlTemplate : public FrameworkTemplate {
    AERO_DECLARE_TYPE(ControlTemplate, FrameworkTemplate)
public:
    ControlTemplate() noexcept = default;
    ~ControlTemplate() noexcept override = default;
    Meta::TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
};

// Public authoring uses the WPF static entry point. Runtime state and animation
// bookkeeping remain private and are accessed only by the controls runtime.
class AERO_API VisualStateManager {
public:
    static bool GoToState(Control& control, Base::StringView stateName, bool useTransitions = true) noexcept;

    ~VisualStateManager() noexcept;
    VisualStateManager(const VisualStateManager&) = delete;
    VisualStateManager& operator=(const VisualStateManager&) = delete;

private:
    friend class ::Aero::Internal::TemplatePrivate;
    VisualStateManager() noexcept = default;
    void* impl_ = nullptr;
};

} // namespace Aero::Controls
