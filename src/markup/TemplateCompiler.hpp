#pragma once

#include "../runtime/RuntimeFwd.hpp"

// Private template compiler used by ObjectWriter finalization.

#include "DeferredContent.hpp"

#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Styling.hpp>
#include "../controls/TemplateAccess.hpp"
#include <Aero/Controls/Panels.hpp>
#include <Aero/Meta/MetadataRuntime.hpp>
#include <Aero/Markup/Schema.hpp>
#include <Aero/Animation.hpp>

#include <cstdint>


namespace Aero::Markup::Detail {

class XamlVisualStateObject final : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        XamlVisualStateObject,
        Base::Object,
        "urn:aero",
        "VisualState")
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::Result<void> SetName(Base::StringView value) noexcept {
        return name_.TryAssign(value);
    }
    Base::StringView Name() const noexcept {
        return name_.View();
    }
    Base::Result<void> AddSetter(
        const Base::Ref<Base::Object>& value) noexcept {
        return setters_.TryPushBack(value);
    }
    void ClearSetters() noexcept {
        setters_.Clear();
    }
    Base::Span<const Base::Ref<Base::Object>>
    Setters() const noexcept {
        return {setters_.Data(), setters_.Size()};
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
    const Base::Ref<Media::Animation::Storyboard>&
    StoryboardValue() const noexcept {
        return storyboard_;
    }

private:
    Base::String name_;
    Base::Vector<Base::Ref<Base::Object>> setters_;
    Base::Ref<Media::Animation::Storyboard> storyboard_;
};

class XamlVisualTransitionObject final
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        XamlVisualTransitionObject,
        Base::Object,
        "urn:aero",
        "VisualTransition")
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::StringView From() const noexcept {
        return from_.View();
    }
    Base::StringView To() const noexcept {
        return to_.View();
    }
    Base::StringView GeneratedDuration() const noexcept {
        return generatedDuration_.View();
    }
    Base::Result<void> SetFrom(
        Base::StringView value) noexcept {
        return from_.TryAssign(value);
    }
    Base::Result<void> SetTo(
        Base::StringView value) noexcept {
        return to_.TryAssign(value);
    }
    Base::Result<void> SetGeneratedDuration(
        Base::StringView value) noexcept {
        Media::Animation::Storyboard validator;
        Base::Result<void> valid =
            validator.SetDuration(value);
        if (!valid) return valid.GetStatus();
        return generatedDuration_.TryAssign(value);
    }
    Base::Ref<Media::Animation::EasingFunctionBase>
    GeneratedEasingFunction() const noexcept {
        return generatedEasingFunction_;
    }
    Base::Result<void> SetGeneratedEasingFunction(
        Base::Ref<Media::Animation::EasingFunctionBase> value) noexcept {
        generatedEasingFunction_ = std::move(value);
        return {};
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
    const Base::Ref<Media::Animation::Storyboard>&
    StoryboardValue() const noexcept {
        return storyboard_;
    }

private:
    Base::String from_;
    Base::String to_;
    Base::String generatedDuration_;
    Base::Ref<Media::Animation::EasingFunctionBase>
        generatedEasingFunction_;
    Base::Ref<Media::Animation::Storyboard> storyboard_;
};

class XamlVisualStateGroupObject final
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        XamlVisualStateGroupObject,
        Base::Object,
        "urn:aero",
        "VisualStateGroup")
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::Result<void> SetName(Base::StringView value) noexcept {
        return name_.TryAssign(value);
    }
    Base::StringView Name() const noexcept {
        return name_.View();
    }
    Base::Result<void> AddState(
        const Base::Ref<Base::Object>& value) noexcept {
        return states_.TryPushBack(value);
    }
    void ClearStates() noexcept {
        states_.Clear();
    }
    Base::Span<const Base::Ref<Base::Object>>
    States() const noexcept {
        return {states_.Data(), states_.Size()};
    }
    Base::Result<void> AddTransition(
        const Base::Ref<Base::Object>& value) noexcept {
        return transitions_.TryPushBack(value);
    }
    void ClearTransitions() noexcept {
        transitions_.Clear();
    }
    Base::Span<const Base::Ref<Base::Object>>
    Transitions() const noexcept {
        return {
            transitions_.Data(),
            transitions_.Size()};
    }

private:
    Base::String name_;
    Base::Vector<Base::Ref<Base::Object>> states_;
    Base::Vector<Base::Ref<Base::Object>> transitions_;
};

// WPF permits VisualStateManager.VisualStateGroups on the root element of a
// ControlTemplate rather than only as a ControlTemplate member. Keep that
// authored collection on the root dependency object until the template is
// compiled into its immutable state program.
class XamlVisualStateGroupStore final
    : public Base::Object {
    AERO_DECLARE_TYPE(XamlVisualStateGroupStore, Base::Object)
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Result<void> Add(
        const Base::Ref<Base::Object>& value) noexcept {
        return groups_.TryPushBack(value);
    }
    Base::Span<const Base::Ref<Base::Object>> Groups() const noexcept {
        return {groups_.Data(), groups_.Size()};
    }

private:
    Base::Vector<Base::Ref<Base::Object>> groups_;
};

class XamlVisualStateManagerObject final
    : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        XamlVisualStateManagerObject,
        Base::Object,
        "urn:aero",
        "VisualStateManager")
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    inline static constexpr Members::AttachedProperty<
        Base::Ref<Base::Object>>
        VisualStateGroupStoreProperty{
            "_VisualStateGroupStore"};
};

struct TemplatePrototypeProperty final {
    Core::DependencyPropertyHandle property;
    Core::Value value;
    // A dependency-object property participating in a template Binding is
    // cloned as part of the instance graph rather than shared with the
    // authored prototype (for example SolidColorBrush.Color in a DataTemplate).
    std::uint32_t objectNode = UINT32_MAX;
};

struct TemplatePrototypeNode final {
    Core::TypeId type = Core::InvalidTypeId;
    Base::String name;
    std::uint32_t parent = UINT32_MAX;
    Core::MemberId contentMember = Core::InvalidMemberId;
    Base::Vector<TemplatePrototypeProperty> properties;
    Base::Vector<Controls::GridLength> gridColumns;
    Base::Vector<Controls::GridLength> gridRows;
};

struct TemplatePrototypeBinding final {
    std::uint32_t target = UINT32_MAX;
    std::uint32_t source = UINT32_MAX;
    Aero::Detail::BindingManager* manager = nullptr;
    Core::MetadataRuntime* metadata = nullptr;
    Core::DependencyPropertyHandle targetProperty;
    Core::DependencyPropertyHandle dataContextProperty;
    Base::String path;
    Base::String stringFormat;
    Data::BindingMode mode =
        Data::BindingMode::OneWay;
    Core::UpdateSourceTrigger updateSourceTrigger =
        Core::UpdateSourceTrigger::PropertyChanged;
};

struct CompiledTemplateBlueprint final {
    Core::MetadataRuntime* runtime = nullptr;
    Core::DependencyPropertyRegistry* properties = nullptr;
    Base::Vector<TemplatePrototypeNode> nodes;
    Base::Vector<TemplatePrototypeBinding> bindings;
    Base::Vector<Base::Ref<Aero::TriggerBase>>
        dataTemplateTriggers;
    // Non-property triggers are retained on the compiled blueprint so every
    // control-template instance can materialize its own sources, name scope,
    // subscriptions, and animation actions.
    Base::Vector<Base::Ref<Aero::TriggerBase>>
        controlTemplateDataTriggers;
    Base::Vector<Base::Ref<Media::Animation::EventTrigger>>
        controlTemplateEventTriggers;
    std::uint32_t contentPresenter = UINT32_MAX;
};

struct CompiledTemplateDefinition final {
    Core::TypeId targetType = Core::InvalidTypeId;
    CompiledTemplateBlueprint blueprint;
    Base::Vector<Controls::TemplatePropertyTrigger>
        propertyTriggers;
    Base::Vector<Controls::TemplateBindingPlan>
        contentSourceBindings;
    Base::Vector<Controls::VisualStateGroup>
        visualStateGroups;
};

Base::Result<void> BuildCompiledTemplate(
    Controls::TemplateBuildContext& context,
    void* factoryContext) noexcept;

Base::Result<Base::Ref<Base::Object>>
BuildCompiledDeferredTemplate(
    const Base::Ref<Base::Object>& payload,
    void* factoryContext) noexcept;

Base::Result<CompiledTemplateBlueprint>
CompileDeferredTemplateBlueprint(
    const Base::Ref<Base::Object>& visualTree,
    const Aero::NameScope* names,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    Core::MetadataRuntime& runtime,
    Core::DependencyPropertyRegistry& properties) noexcept;

Base::Result<CompiledTemplateDefinition>
CompileControlTemplateDefinition(
    Controls::ControlTemplate& controlTemplate,
    Base::Span<const DeferredContentEdge> edges,
    Base::Span<const DeferredBindingEdge> bindings,
    Core::MetadataRuntime& runtime,
    Core::DependencyPropertyRegistry& properties) noexcept;

} // namespace Aero::Markup::Detail
