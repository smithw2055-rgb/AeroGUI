#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/FrameworkElement.hpp>
#include "gui/core/FrameworkElementSeams.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/RenderStateCallbacks.hpp"
#include "gui/core/ValueConversion.hpp"
#include "gui/data/BindingEngine.hpp"
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/BlendBehaviors.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Style.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/MediaActions.hpp>
#include <Aero/Media/Animation/StoryboardActions.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Media/Animation/TimerTrigger.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Images.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Pen.hpp>
#include <Aero/Media/Fonts.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Collections.hpp>
#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/EventSetter.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/ApplicationCommands.hpp>
#include <Aero/InputGesture.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Data/MultiBinding.hpp>
#include <Aero/Data/BooleanToVisibilityConverter.hpp>
#include <Aero/Data/IMultiValueConverter.hpp>
#include <Aero/Data/IValueConverter.hpp>
#include <Aero/DataObject.hpp>
#include <Aero/DragDrop.hpp>
#include <Aero/Input/Cursor.hpp>
#include <Aero/Input/Mouse.hpp>
#include <Aero/Input/Keyboard.hpp>
#include <Aero/Animatable.hpp>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero::Interactivity {

Behavior::~Behavior() {
    Detach();
}

Base::Result<void> Behavior::Attach(FrameworkElement& object) noexcept {
    if (associatedObject_ == &object) return {};
    if (associatedObject_ != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Behavior is already attached to another object");
    }
    associatedObject_ = &object;
    OnAttached();
    return {};
}

void Behavior::Detach() noexcept {
    if (associatedObject_ == nullptr) return;
    OnDetaching();
    associatedObject_ = nullptr;
}

void Behavior::AddAuthoredBinding(
    Meta::DependencyPropertyHandle property,
    Base::Ref<Aero::Data::Binding> binding) noexcept {
    if (!property.IsValid() || !binding) { AERO_ASSERT(false); return; }
    for (AuthoredBinding& existing : authoredBindings_) {
        if (existing.property == property) {
            existing.binding = std::move(binding);
            return;
        }
    }
    authoredBindings_.PushBack(
        {property, std::move(binding)});
}

void Behavior::CopyAuthoredBindingsTo(
    Behavior& destination) const noexcept {
    for (const AuthoredBinding& binding : authoredBindings_) {
        destination.AddAuthoredBinding(
            binding.property, binding.binding);
    }
}

Base::Result<Base::Ref<Behavior>> Behavior::ClonePrototype(
    const Behavior& prototype,
    Meta::Registry& metadata) noexcept {
    Base::Result<Base::Ref<Base::Object>> created =
        metadata.CreateObject(prototype.RuntimeType());
    if (!created) return created.GetStatus();
    if (!created.Value() ||
        !metadata.Types().IsDerivedFrom(
            created.Value()->RuntimeType(),
            Behavior::StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Behavior factory returned an incompatible object");
    }
    Base::Ref<Behavior> clone =
        Base::Ref<Behavior>::FromBorrowed(
            *static_cast<Behavior*>(created.Value().Get()));
    for (const Meta::DependencyProperty& property :
         DependencyObjectAccess::PropertyRegistry((prototype)).Properties()) {
        if (property.MetadataFor(prototype.RuntimeType()) == nullptr ||
            property.MetadataFor(clone->RuntimeType()) == nullptr) {
            continue;
        }
        Meta::PropertyValue local =
            prototype.ReadLocalValue(property.Handle());
        if (local.IsUnset()) continue;
        clone->SetValue(property.Handle(), local);
    }
    prototype.CopyAuthoredBindingsTo(*clone);
    return clone;
}

void StyleBehaviorCollection::Add(
    Base::Ref<Base::Object> value) noexcept {
    if (!value) { AERO_ASSERT(false); return; }
    items_.PushBack(std::move(value));
}

void StyleTriggerCollection::Add(
    Base::Ref<Base::Object> value) noexcept {
    if (!value) { AERO_ASSERT(false); return; }
    items_.PushBack(std::move(value));
}

void StyleInteraction::OnBehaviorsChanged(
    DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
    if (!DependencyObjectAccess::PropertyRegistry((object)).Types().IsDerivedFrom(
            object.RuntimeType(), FrameworkElement::StaticTypeId())) {
        return;
    }
    auto& element = static_cast<FrameworkElement&>(object);
    static_cast<void>(
        (element).ClearStyleBehaviorPrototypes());
    const Meta::Value& value = args.GetNewValue();
    if (value.Kind() != Meta::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject() ||
        value.AsObject()->RuntimeType() !=
            StyleBehaviorCollection::StaticTypeId()) {
        return;
    }
    for (const Base::Ref<Base::Object>& behavior :
         static_cast<StyleBehaviorCollection&>(*value.AsObject()).GetItems()) {
        (element).AddStyleBehaviorPrototype( behavior);
    }
}

void StyleInteraction::OnTriggersChanged(
    DependencyObject& object,
    const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
    if (!DependencyObjectAccess::PropertyRegistry((object)).Types().IsDerivedFrom(
            object.RuntimeType(), FrameworkElement::StaticTypeId())) {
        return;
    }
    auto& element = static_cast<FrameworkElement&>(object);
    static_cast<void>(
        (element).ClearStyleTriggerPrototypes());
    const Meta::Value& value = args.GetNewValue();
    if (value.Kind() != Meta::ValueKind::Object ||
        value.IsNullObject() || !value.AsObject() ||
        value.AsObject()->RuntimeType() !=
            StyleTriggerCollection::StaticTypeId()) {
        return;
    }
    for (const Base::Ref<Base::Object>& trigger :
         static_cast<StyleTriggerCollection&>(*value.AsObject()).GetItems()) {
        (element).AddStyleTriggerPrototype( trigger);
    }
}

} // namespace Aero::Interactivity

// Metadata registration for the types implemented in this file.
namespace Aero::MetadataSupport {
using namespace ::Aero::Interactivity;
using Media::Animation::EventTrigger;
using Media::Animation::StoryboardCompletedTrigger;
namespace {

void AddPropertyChangedTriggerAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<TriggerAction> retained =
        Base::Ref<TriggerAction>::TryFromBorrowed(
            static_cast<TriggerAction&>(*value));
    if (!retained) return;
    static_cast<PropertyChangedTrigger&>(owner)
        .AddAction(std::move(retained));
}

void ClearPropertyChangedTriggerActions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<PropertyChangedTrigger&>(owner)
        .ClearActions();
}

void AddKeyTriggerAction(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    Base::Ref<TriggerAction> retained =
        Base::Ref<TriggerAction>::TryFromBorrowed(
            static_cast<TriggerAction&>(*value));
    if (!retained) return;
    static_cast<KeyTrigger&>(owner)
        .AddAction(std::move(retained));
}

void ClearKeyTriggerActions(
    Base::Object& owner,
    void*) noexcept {
    static_cast<KeyTrigger&>(owner).ClearActions();
}

void AddInteractionBehavior(
    Base::Object& owner, const Base::Ref<Base::Object>& value, void*) noexcept {
    if (!value) return;
    if (owner.RuntimeType() == DataTrigger::StaticTypeId()) {
        static_cast<DataTrigger&>(owner).AddBehavior(value);
    } else if (owner.RuntimeType() ==
               StoryboardCompletedTrigger::StaticTypeId()) {
        static_cast<StoryboardCompletedTrigger&>(owner)
            .AddConditionBehavior(value);
    } else if (owner.RuntimeType() ==
               EventTrigger::StaticTypeId()) {
        static_cast<EventTrigger&>(owner)
            .AddConditionBehavior(value);
    } else {
        static_cast<void>(FrameworkElementSeams::AddAuthoredBehavior(
            static_cast<FrameworkElement&>(owner), value));
    }
}

void ClearInteractionBehaviors(Base::Object& owner, void*) noexcept {
    if (owner.RuntimeType() == DataTrigger::StaticTypeId()) {
        static_cast<DataTrigger&>(owner).ClearBehaviors();
    } else if (owner.RuntimeType() ==
               StoryboardCompletedTrigger::StaticTypeId()) {
        static_cast<StoryboardCompletedTrigger&>(owner)
            .ClearConditionBehaviors();
    } else if (owner.RuntimeType() ==
               EventTrigger::StaticTypeId()) {
        static_cast<EventTrigger&>(owner)
            .ClearConditionBehaviors();
    } else {
        static_cast<void>(FrameworkElementSeams::ClearAuthoredBehaviors(
            static_cast<FrameworkElement&>(owner)));
    }
}

void AddStyleBehaviorItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    static_cast<void>(
        static_cast<StyleBehaviorCollection&>(owner)
            .Add(value));
}

void ClearStyleBehaviorItems(Base::Object& owner, void*) noexcept {
    static_cast<StyleBehaviorCollection&>(owner).Clear();
}

void AddStyleTriggerItem(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    static_cast<void>(
        static_cast<StyleTriggerCollection&>(owner)
            .Add(value));
}

void ClearStyleTriggerItems(Base::Object& owner, void*) noexcept {
    static_cast<StyleTriggerCollection&>(owner).Clear();
}

void AddConditionalComparison(
    Base::Object& owner, const Base::Ref<Base::Object>& value, void*) noexcept {
    if (!value) return;
    Base::Ref<ComparisonCondition> retained =
        Base::Ref<ComparisonCondition>::TryFromBorrowed(
            static_cast<ComparisonCondition&>(*value));
    if (!retained) return;
    static_cast<ConditionalExpression&>(owner)
        .AddCondition(std::move(retained));
}

void ClearConditionalComparisons(Base::Object& owner, void*) noexcept {
    static_cast<ConditionalExpression&>(owner).ClearConditions(); return;
}

void SetConditionBehaviorExpression(
    Base::Object& owner, const Base::Ref<Base::Object>& value, void*) noexcept {
    static_cast<ConditionBehavior&>(owner).SetExpression(
        Base::Ref<ConditionalExpression>::FromBorrowed(
            static_cast<ConditionalExpression&>(*value)));
    return;
}

void ClearConditionBehaviorExpression(Base::Object& owner, void*) noexcept {
    static_cast<ConditionBehavior&>(owner).SetExpression({});
    return;
}

void AddInteractionTrigger(
    Base::Object& owner,
    const Base::Ref<Base::Object>& value,
    void*) noexcept {
    if (!value) return;
    auto& dependencyObject =
        static_cast<DependencyObject&>(owner);
    const Meta::TypeRegistry& types =
        DependencyObjectAccess::PropertyRegistry((dependencyObject)).Types();
    if (types.IsDerivedFrom(
            owner.RuntimeType(), FrameworkElement::StaticTypeId())) {
        static_cast<void>(
            FrameworkElementSeams::AddAuthoredTrigger(
                static_cast<FrameworkElement&>(owner), value));
    } else if (types.IsDerivedFrom(
                   owner.RuntimeType(),
                   FrameworkContentElement::StaticTypeId())) {
        static_cast<void>(
            FrameworkContentElementSeams::AddAuthoredTrigger(
                static_cast<FrameworkContentElement&>(owner), value));
    }
}

void ClearInteractionTriggers(
    Base::Object& owner,
    void*) noexcept {
    auto& dependencyObject =
        static_cast<DependencyObject&>(owner);
    const Meta::TypeRegistry& types =
        DependencyObjectAccess::PropertyRegistry((dependencyObject)).Types();
    if (types.IsDerivedFrom(
            owner.RuntimeType(), FrameworkElement::StaticTypeId())) {
        static_cast<void>(
            FrameworkElementSeams::ClearAuthoredTriggers(
                static_cast<FrameworkElement&>(owner)));
    } else if (types.IsDerivedFrom(
                   owner.RuntimeType(),
                   FrameworkContentElement::StaticTypeId())) {
        static_cast<void>(
            FrameworkContentElementSeams::ClearAuthoredTriggers(
                static_cast<FrameworkContentElement&>(owner)));
    }
}

} // namespace
} // namespace Aero::MetadataSupport

namespace Aero::Interactivity {

AERO_DESCRIBE(TriggerAction) {
    using namespace Aero::Meta;
    Register<TriggerAction>(context, TypeFlags::Abstract);
}

AERO_DESCRIBE(ChangePropertyAction) {
    using namespace Aero::Meta;
    Register<ChangePropertyAction>(context)
            .Property("TargetName", &ChangePropertyAction::GetTargetName, &ChangePropertyAction::SetTargetName)
            .Property<Base::Ref<Data::Binding>, &ChangePropertyAction::GetTargetObject, &ChangePropertyAction::SetTargetObject>("TargetObject", PropertyFlags::Structural)
            .Property("PropertyName", &ChangePropertyAction::GetPropertyName, &ChangePropertyAction::SetPropertyName)
            .Property<Meta::PropertyValue, &ChangePropertyAction::GetValue, &ChangePropertyAction::SetValue>("Value", PropertyFlags::AnyValue)
            .Factory();
}

AERO_DESCRIBE(SetFocusAction) {
    using namespace Aero::Meta;
    Register<SetFocusAction>(context)
            .Property("TargetName", &SetFocusAction::GetTargetName, &SetFocusAction::SetTargetName)
            .Property<Base::Ref<Data::Binding>, &SetFocusAction::GetTargetObject, &SetFocusAction::SetTargetObject>("TargetObject", PropertyFlags::Structural)
            .Property("Engage", &SetFocusAction::GetEngage, &SetFocusAction::SetEngage)
            .Factory();
}

AERO_DESCRIBE(LaunchUriOrFileAction) {
    using namespace Aero::Meta;
    Register<LaunchUriOrFileAction>(context)
            .Property("Path", &LaunchUriOrFileAction::GetPath, &LaunchUriOrFileAction::SetPath)
            .Factory();
}

AERO_DESCRIBE(RemoveElementAction) {
    using namespace Aero::Meta;
    Register<RemoveElementAction>(context)
            .Property<Base::Ref<Data::Binding>, &RemoveElementAction::GetTargetObject, &RemoveElementAction::SetTargetObject>("TargetObject", PropertyFlags::Structural)
            .Factory();
}

AERO_DESCRIBE(PropertyChangedTrigger) {
    using namespace Aero::Meta;
    Register<PropertyChangedTrigger>(context)
            .Property<Base::Ref<Data::Binding>, &PropertyChangedTrigger::GetBinding, &PropertyChangedTrigger::SetBinding>("Binding")
            .Content<TriggerAction>("Actions", ContentKind::Collection, &::Aero::MetadataSupport::AddPropertyChangedTriggerAction, &::Aero::MetadataSupport::ClearPropertyChangedTriggerActions)
            .Factory();
}

AERO_DESCRIBE(KeyTrigger) {
    using namespace Aero::Meta;
    Register<KeyTrigger>(context)
            .Property("Key", &KeyTrigger::GetKey, &KeyTrigger::SetKey)
            .Property("ActiveOnFocus", &KeyTrigger::GetActiveOnFocus, &KeyTrigger::SetActiveOnFocus)
            .Content<TriggerAction>("Actions", ContentKind::Collection, &::Aero::MetadataSupport::AddKeyTriggerAction, &::Aero::MetadataSupport::ClearKeyTriggerActions)
            .Factory();
}

AERO_DESCRIBE(InvokeCommandAction) {
    using namespace Aero::Meta;
    Register<InvokeCommandAction>(context)
            .Property("Command", &InvokeCommandAction::GetCommand, &InvokeCommandAction::SetCommand)
            .Property<Meta::PropertyValue, &InvokeCommandAction::GetCommandParameter, &InvokeCommandAction::SetCommandParameter>("CommandParameter", PropertyFlags::AnyValue)
            .Factory();
}

AERO_DESCRIBE(SelectAction) {
    using namespace Aero::Meta;
    Register<SelectAction>(context)
            .Factory();
}

AERO_DESCRIBE(SelectAllAction) {
    using namespace Aero::Meta;
    Register<SelectAllAction>(context)
            .Factory();
}

AERO_DESCRIBE(PlaySoundAction) {
    using namespace Aero::Meta;
    Register<PlaySoundAction>(context)
            .Property("Source", &PlaySoundAction::GetSource, &PlaySoundAction::SetSource)
            .Property("Volume", &PlaySoundAction::GetVolume, &PlaySoundAction::SetVolume)
            .Property(PlaySoundAction::IsEnabledProperty, true)
            .Factory();
}

AERO_DESCRIBE(ComparisonCondition) {
    using namespace Aero::Meta;
    Register<ComparisonCondition>(context)
            .Property<Base::Ref<Data::Binding>, &ComparisonCondition::GetLeftOperand, &ComparisonCondition::SetLeftOperand>("LeftOperand")
            .Property<Meta::PropertyValue, &ComparisonCondition::GetRightOperand, &ComparisonCondition::SetRightOperand>("RightOperand", PropertyFlags::AnyValue)
            .Property("Operator", &ComparisonCondition::GetComparisonOperator, &ComparisonCondition::SetComparisonOperator)
            .Factory();
}

AERO_DESCRIBE(ConditionalExpression) {
    using namespace Aero::Meta;
    Register<ConditionalExpression>(context)
            .Property("ForwardChaining", &ConditionalExpression::GetChaining, &ConditionalExpression::SetChaining)
            .Content<ComparisonCondition>("Conditions", ContentKind::Collection, &::Aero::MetadataSupport::AddConditionalComparison, &::Aero::MetadataSupport::ClearConditionalComparisons)
            .Factory();
}

AERO_DESCRIBE(ConditionBehavior) {
    using namespace Aero::Meta;
    Register<ConditionBehavior>(context)
            .Content<ConditionalExpression>("Expression", ContentKind::Single, &::Aero::MetadataSupport::SetConditionBehaviorExpression, &::Aero::MetadataSupport::ClearConditionBehaviorExpression)
            .Factory();
}

AERO_DESCRIBE(Behavior) {
    using namespace Aero::Meta;
    Register<Behavior>(context, TypeFlags::Abstract);
}

AERO_DESCRIBE(StyleBehaviorCollection) {
    using namespace Aero::Meta;
    Register<StyleBehaviorCollection>(context)
            .Content<Base::Object>("Items", ContentKind::Collection, &::Aero::MetadataSupport::AddStyleBehaviorItem, &::Aero::MetadataSupport::ClearStyleBehaviorItems)
            .Factory();
}

AERO_DESCRIBE(StyleTriggerCollection) {
    using namespace Aero::Meta;
    Register<StyleTriggerCollection>(context)
            .Content<Base::Object>("Items", ContentKind::Collection, &::Aero::MetadataSupport::AddStyleTriggerItem, &::Aero::MetadataSupport::ClearStyleTriggerItems)
            .Factory();
}

AERO_DESCRIBE(StyleInteraction) {
    using namespace Aero::Meta;
    Register<StyleInteraction>(context, TypeFlags::Abstract)
            .Property(StyleInteraction::BehaviorsProperty, FrameworkPropertyMetadata(Base::Ref<StyleBehaviorCollection>{}) .Changed(&StyleInteraction::OnBehaviorsChanged))
            .Property(StyleInteraction::TriggersProperty, FrameworkPropertyMetadata(Base::Ref<StyleTriggerCollection>{}) .Changed(&StyleInteraction::OnTriggersChanged));
}

AERO_DESCRIBE(Interaction) {
    using namespace Aero::Meta;
    Register<Interaction>(context, TypeFlags::Abstract)
            .Collection<Base::Object>("Triggers", &::Aero::MetadataSupport::AddInteractionTrigger, &::Aero::MetadataSupport::ClearInteractionTriggers, PropertyFlags::Attached | PropertyFlags::Structural)
            .Collection<Base::Object>("Behaviors", &::Aero::MetadataSupport::AddInteractionBehavior, &::Aero::MetadataSupport::ClearInteractionBehaviors, PropertyFlags::Attached | PropertyFlags::Structural);
}

} // namespace Aero::Interactivity

