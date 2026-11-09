#include <Aero/Interactivity/Behavior.hpp>
#include <Aero/FrameworkElement.hpp>
#include "gui/ViewFrame.hpp"
#include "gui/core/FrameworkElementSeams.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/core/Describe.hpp"
#include "gui/core/ValueConversion.hpp"
#include <Aero/Interactivity/Conditions.hpp>
#include <Aero/Interactivity/Interaction.hpp>
#include <Aero/Interactivity/InteractionTriggers.hpp>
#include <Aero/Interactivity/TriggerAction.hpp>
#include <Aero/Triggers.hpp>
#include <Aero/Media/Animation.hpp>
#include <Aero/Media/Animation/StoryboardCompletedTrigger.hpp>
#include <Aero/Data/Binding.hpp>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>
#include "gui/core/DependencyObjectAccess.hpp"
#include "gui/core/PropertyStore.hpp"

namespace Aero::Interactivity {

namespace {

StoryboardHost* ResolveStoryboardHost(FrameworkElement& owner) noexcept {
    ElementTree* tree = ElementTree::Of(owner);
    ViewFrame* view = tree != nullptr ? tree->GetViewState() : nullptr;
    return view != nullptr ? view->Storyboards() : nullptr;
}

void RecordActionFailure(StoryboardHost* host, const Base::Status& status) noexcept {
    if (host != nullptr) {
        host->activeActionStatus = status;
    }
}

} // namespace

void TriggerAction::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    static_cast<void>(associatedObject);
    static_cast<void>(parameter);
}

void ChangePropertyAction::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    static_cast<void>(parameter);
    StoryboardHost* host = ResolveStoryboardHost(associatedObject);
    if (host == nullptr || host->Metadata() == nullptr) {
        return;
    }
    Base::Result<Base::Object*> resolvedTarget =
        host->ResolveTargetObject(
            GetTargetObject(),
            GetTargetName(),
            associatedObject,
            host->activeActionDataTemplateContext,
            host->activeActionNames,
            this);
    if (!resolvedTarget) {
        RecordActionFailure(host, resolvedTarget.GetStatus());
        return;
    }
    Base::Object* targetObject = resolvedTarget.Value();
    if (targetObject == nullptr ||
        !host->Metadata()->Types().IsDerivedFrom(
            targetObject->RuntimeType(),
            DependencyObject::StaticTypeId())) {
        RecordActionFailure(
            host,
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "ChangePropertyAction TargetName did not resolve to a DependencyObject"));
        return;
    }
    auto& target = static_cast<DependencyObject&>(*targetObject);
    Base::Result<StoryboardHost::ResolvedAnimationProperty> resolved =
        host->ResolveAnimationProperty(target, GetPropertyName());
    if (!resolved) {
        RecordActionFailure(host, resolved.GetStatus());
        return;
    }

    DependencyObject& propertyTarget = *resolved.Value().target;
    const Meta::DependencyPropertyHandle propertyHandle = resolved.Value().property;
    const Meta::DependencyProperty* property =
        (*host->Metadata()).DependencyProperties().Find(propertyHandle);
    if (property == nullptr) {
        RecordActionFailure(
            host,
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "ChangePropertyAction property metadata was not found"));
        return;
    }

    Meta::PropertyValue value = GetValue();
    Base::Ref<Data::Binding> valueBinding = GetValueBinding();
    if (valueBinding) {
        if (host->Interactivity() == nullptr) {
            RecordActionFailure(
                host,
                Base::Status::Failure(
                    Base::ErrorCode::NotInitialized,
                    "ChangePropertyAction ValueBinding requires an active InteractivityEngine"));
            return;
        }
        Base::Result<Meta::PropertyValue> evaluated =
            host->Interactivity()->EvaluateAuthoredBinding(
                *valueBinding,
                associatedObject,
                host->activeActionDataTemplateContext,
                host->activeActionNames,
                this);
        if (!evaluated) {
            RecordActionFailure(host, evaluated.GetStatus());
            return;
        }
        value = std::move(evaluated).Value();
    }
    Base::Result<Meta::PropertyValue> coerced =
        NormalizeValueForProperty(
            host->Metadata(),
            *property,
            std::move(value));
    if (!coerced) {
        RecordActionFailure(host, coerced.GetStatus());
        return;
    }
    propertyTarget.SetCurrentValue(
        propertyHandle,
        std::move(coerced).Value());
}

void InvokeCommandAction::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    StoryboardHost* host = ResolveStoryboardHost(associatedObject);
    if (host == nullptr || host->Metadata() == nullptr) {
        return;
    }
    Base::Ref<Input::ICommand> command = GetCommand();
    if (!command && GetCommandBinding()) {
        if (host->Interactivity() == nullptr) {
            RecordActionFailure(
                host,
                Base::Status::Failure(
                    Base::ErrorCode::NotInitialized,
                    "InvokeCommandAction CommandBinding requires an active InteractivityEngine"));
            return;
        }
        Base::Result<Meta::PropertyValue> evaluated =
            host->Interactivity()->EvaluateAuthoredBinding(
                *GetCommandBinding(),
                associatedObject,
                host->activeActionDataTemplateContext,
                host->activeActionNames,
                this);
        if (!evaluated) {
            RecordActionFailure(host, evaluated.GetStatus());
            return;
        }
        if (evaluated.Value().Kind() != Meta::ValueKind::Object ||
            evaluated.Value().IsNullObject() ||
            !evaluated.Value().AsObject() ||
            !host->Metadata()->Types().IsDerivedFrom(
                evaluated.Value().AsObject()->RuntimeType(),
                Input::ICommand::StaticTypeId())) {
            RecordActionFailure(
                host,
                Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "InvokeCommandAction Binding did not return ICommand"));
            return;
        }
        command = Base::Ref<Input::ICommand>::FromBorrowed(
            *static_cast<Input::ICommand*>(
                evaluated.Value().AsObject().Get()));
    }
    if (!command) {
        RecordActionFailure(
            host,
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "InvokeCommandAction Command is unavailable"));
        return;
    }

    Meta::PropertyValue resolvedParameter = GetCommandParameter();
    if (GetCommandParameterBinding()) {
        if (host->Interactivity() == nullptr) {
            RecordActionFailure(
                host,
                Base::Status::Failure(
                    Base::ErrorCode::NotInitialized,
                    "InvokeCommandAction CommandParameterBinding requires an active InteractivityEngine"));
            return;
        }
        Base::Result<Meta::PropertyValue> evaluated =
            host->Interactivity()->EvaluateAuthoredBinding(
                *GetCommandParameterBinding(),
                associatedObject,
                host->activeActionDataTemplateContext,
                host->activeActionNames,
                this);
        if (!evaluated) {
            RecordActionFailure(host, evaluated.GetStatus());
            return;
        }
        resolvedParameter = std::move(evaluated).Value();
    }
    if (resolvedParameter.IsUnset() && !parameter.IsUnset()) {
        resolvedParameter = parameter;
    }
    if (resolvedParameter.IsUnset()) {
        resolvedParameter = Meta::PropertyValue::NullObject(
            Meta::TypeOf<Base::Object>());
    }
    UIElement* target = TryCast<UIElement>(&associatedObject);
    if (target == nullptr) {
        RecordActionFailure(
            host,
            Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "InvokeCommandAction owner is not a UIElement"));
        return;
    }
    const bool canExecute = host->Input() != nullptr
        ? host->Input()->CanExecute(*command, resolvedParameter, *target)
        : command->CanExecute(resolvedParameter, target);
    if (!canExecute) return;
    if (host->Input() != nullptr) {
        Base::Result<bool> executed =
            host->Input()->Execute(*command, resolvedParameter, *target);
        if (!executed) {
            RecordActionFailure(host, executed.GetStatus());
        }
        return;
    }
    command->Execute(resolvedParameter, target);
}

void SetFocusAction::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    static_cast<void>(parameter);
    StoryboardHost* host = ResolveStoryboardHost(associatedObject);
    if (host == nullptr || host->Metadata() == nullptr) {
        return;
    }
    if (!GetEngage() || host->Input() == nullptr) return;
    Base::Result<Base::Object*> resolvedTarget =
        host->ResolveTargetObject(
            GetTargetObject(),
            GetTargetName(),
            associatedObject,
            host->activeActionDataTemplateContext,
            host->activeActionNames,
            this);
    if (!resolvedTarget) {
        RecordActionFailure(host, resolvedTarget.GetStatus());
        return;
    }
    Base::Object* targetObject = resolvedTarget.Value();
    UIElement* target =
        targetObject != nullptr && host->Metadata()->Types().IsDerivedFrom(
            targetObject->RuntimeType(), UIElement::StaticTypeId())
        ? static_cast<UIElement*>(targetObject)
        : nullptr;
    if (target == nullptr) {
        RecordActionFailure(
            host,
            Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "SetFocusAction target is unavailable"));
        return;
    }
    if (!target->GetIsLoaded()) {
        if (host->view == nullptr || host->view->Focus() == nullptr) {
            RecordActionFailure(
                host,
                Base::Status::Failure(
                    Base::ErrorCode::NotInitialized,
                    "View focus host is unavailable"));
            return;
        }
        host->view->Focus()->QueueFocus(*target);
        return;
    }
    if (!target->GetIsEnabled()) return;
    // Focus is best-effort. A failed SetFocus must not fail the
    // EventTrigger (QuestLog MouseEnter -> SelectAction).
    static_cast<void>(host->Input()->SetFocus(target));
}

void SelectAction::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    static_cast<void>(parameter);
    StoryboardHost* host = ResolveStoryboardHost(associatedObject);
    const Meta::TypeRegistry& types =
        host != nullptr && host->Metadata() != nullptr
        ? host->Metadata()->Types()
        : DependencyObjectAccess::PropertyRegistry(associatedObject).Types();
    if (types.IsDerivedFrom(
            associatedObject.RuntimeType(),
            Controls::ListBoxItem::StaticTypeId())) {
        static_cast<Controls::ListBoxItem&>(associatedObject).SetIsSelected(true);
        return;
    }
    if (types.IsDerivedFrom(
            associatedObject.RuntimeType(),
            Controls::TabItem::StaticTypeId())) {
        static_cast<Controls::TabItem&>(associatedObject).SetIsSelected(true);
        return;
    }
    RecordActionFailure(
        host,
        Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "SelectAction owner is not a selectable item container"));
}

void SelectAllAction::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    static_cast<void>(parameter);
    StoryboardHost* host = ResolveStoryboardHost(associatedObject);
    const Meta::TypeRegistry& types =
        host != nullptr && host->Metadata() != nullptr
        ? host->Metadata()->Types()
        : DependencyObjectAccess::PropertyRegistry(associatedObject).Types();
    if (types.IsDerivedFrom(
            associatedObject.RuntimeType(),
            Controls::Primitives::TextBoxBase::StaticTypeId())) {
        static_cast<Controls::Primitives::TextBoxBase&>(associatedObject).SelectAll();
        return;
    }
    RecordActionFailure(
        host,
        Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "SelectAllAction owner is not a text editor"));
}

void PlaySoundAction::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    static_cast<void>(parameter);
    if (!GetIsEnabled() || GetSource().Empty()) {
        return;
    }
    StoryboardHost* host = ResolveStoryboardHost(associatedObject);
    const double volume = GetVolume();
    if (!std::isfinite(volume) || volume < 0.0 || volume > 1.0) {
        RecordActionFailure(
            host,
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "PlaySoundAction Volume must be between zero and one"));
        return;
    }
    if (host == nullptr || host->view == nullptr) {
        return;
    }
    if (!host->view->audio.Initialize()) {
        // Audio is optional for headless and provider-free hosts.
        return;
    }
    host->view->audio.SetEffectsVolume(static_cast<float>(volume));
    host->view->audio.PlayEffect(GetSource());
}

void RemoveElementAction::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    static_cast<void>(parameter);
    StoryboardHost* host = ResolveStoryboardHost(associatedObject);
    if (host == nullptr || host->Metadata() == nullptr) {
        return;
    }
    Base::Result<Base::Object*> resolvedTarget =
        host->ResolveTargetObject(
            GetTargetObject(),
            {},
            associatedObject,
            host->activeActionDataTemplateContext,
            host->activeActionNames,
            this);
    if (!resolvedTarget) {
        RecordActionFailure(host, resolvedTarget.GetStatus());
        return;
    }
    Base::Object* targetObject = resolvedTarget.Value();
    if (targetObject == nullptr ||
        !host->Metadata()->Types().IsDerivedFrom(
            targetObject->RuntimeType(),
            UIElement::StaticTypeId())) {
        RecordActionFailure(
            host,
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "RemoveElementAction target is not a UIElement"));
        return;
    }
    auto& target = static_cast<UIElement&>(*targetObject);
    Media::Visual* current = TryCast<Media::Visual>(target.GetLogicalParent());
    if (current == nullptr) current = target.GetVisualParent();
    while (current != nullptr) {
        if (host->Metadata()->Types().IsDerivedFrom(
                current->RuntimeType(),
                Controls::ItemsControl::StaticTypeId())) {
            auto& items = static_cast<Controls::ItemsControl&>(*current);
            std::uint32_t index = UINT32_MAX;
            for (std::uint32_t candidate = 0U; candidate < items.GetCount(); ++candidate) {
                Base::Ref<Base::Object> item = items.GetItem(candidate);
                if (item.Get() == &target) {
                    index = candidate;
                    break;
                }
            }
            if (index != UINT32_MAX) {
                Base::Ref<Base::Object> removed = items.GetItems().RemoveAt(index);
                if (!removed) {
                    RecordActionFailure(
                        host,
                        Base::Status::Failure(
                            Base::ErrorCode::NotFound,
                            "Failed to remove item from ItemCollection"));
                }
                return;
            }
        }
        current = TryCast<Media::Visual>(current->GetLogicalParent()) != nullptr
            ? TryCast<Media::Visual>(current->GetLogicalParent())
            : current->GetVisualParent();
    }
    RecordActionFailure(
        host,
        Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "RemoveElementAction target is not owned by an ItemsControl"));
}

void LaunchUriOrFileAction::Invoke(
    ::Aero::FrameworkElement& associatedObject,
    const Meta::PropertyValue& parameter) noexcept {
    static_cast<void>(parameter);
    StoryboardHost* host = ResolveStoryboardHost(associatedObject);
    String path;
    if (GetPathBinding()) {
        if (host == nullptr || host->Interactivity() == nullptr) {
            RecordActionFailure(
                host,
                Base::Status::Failure(
                    Base::ErrorCode::NotInitialized,
                    "LaunchUriOrFileAction PathBinding requires an active InteractivityEngine"));
            return;
        }
        Base::Result<Meta::PropertyValue> evaluated =
            host->Interactivity()->EvaluateAuthoredBinding(
                *GetPathBinding(),
                associatedObject,
                host->activeActionDataTemplateContext,
                host->activeActionNames,
                this);
        if (!evaluated) {
            RecordActionFailure(host, evaluated.GetStatus());
            return;
        }
        if (evaluated.Value().Kind() == Meta::ValueKind::String) {
            static_cast<void>(path.Assign(evaluated.Value().AsString()));
        }
    } else {
        static_cast<void>(path.Assign(GetPath()));
    }
    if (path.View().Empty()) {
        RecordActionFailure(
            host,
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "LaunchUriOrFileAction Path is empty"));
    }
}

Behavior::~Behavior() {
    Detach();
}

bool Behavior::Attach(FrameworkElement& object) noexcept {
    if (associatedObject_ == &object) return true;
    if (associatedObject_ != nullptr) return false;
    associatedObject_ = &object;
    OnAttached();
    return true;
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

Base::Ref<Behavior> Behavior::ClonePrototype(
    const Behavior& prototype,
    Meta::Registry& metadata) noexcept {
    Base::Result<Base::Ref<Base::Object>> created =
        metadata.CreateObject(prototype.RuntimeType());
    if (!created || !created.Value() ||
        !metadata.Types().IsDerivedFrom(
            created->RuntimeType(),
            Behavior::StaticTypeId())) {
        return {};
    }
    Base::Ref<Behavior> clone =
        Base::Ref<Behavior>::FromBorrowed(
            *static_cast<Behavior*>(created.Value().Get()));
    const PropertyStore* store = DependencyObjectAccess::Store(prototype);
    if (store != nullptr) {
        for (const auto& record : store->entries) {
            const StoredValueEntry& entry = record.Value();
            if (!entry.HasLocal()) {
                continue;
            }
            const Meta::PropertyValue local = entry.LocalValueOrUnset();
            if (local.IsUnset()) {
                continue;
            }
            clone->SetValue(Meta::DependencyPropertyHandle{record.Key()}, local);
        }
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
            .Property<&ChangePropertyAction::GetTargetName, &ChangePropertyAction::SetTargetName>("TargetName")
            .Property<&ChangePropertyAction::GetTargetObject, &ChangePropertyAction::SetTargetObject>("TargetObject", PropertyFlags::Structural)
            .Property<&ChangePropertyAction::GetPropertyName, &ChangePropertyAction::SetPropertyName>("PropertyName")
            .Property<&ChangePropertyAction::GetValue, &ChangePropertyAction::SetValue>("Value")
            .Factory();
}

AERO_DESCRIBE(SetFocusAction) {
    using namespace Aero::Meta;
    Register<SetFocusAction>(context)
            .Property<&SetFocusAction::GetTargetName, &SetFocusAction::SetTargetName>("TargetName")
            .Property<&SetFocusAction::GetTargetObject, &SetFocusAction::SetTargetObject>("TargetObject", PropertyFlags::Structural)
            .Property<&SetFocusAction::GetEngage, &SetFocusAction::SetEngage>("Engage")
            .Factory();
}

AERO_DESCRIBE(LaunchUriOrFileAction) {
    using namespace Aero::Meta;
    Register<LaunchUriOrFileAction>(context)
            .Property<&LaunchUriOrFileAction::GetPath, &LaunchUriOrFileAction::SetPath>("Path")
            .Factory();
}

AERO_DESCRIBE(RemoveElementAction) {
    using namespace Aero::Meta;
    Register<RemoveElementAction>(context)
            .Property<&RemoveElementAction::GetTargetObject, &RemoveElementAction::SetTargetObject>("TargetObject", PropertyFlags::Structural)
            .Factory();
}

AERO_DESCRIBE(PropertyChangedTrigger) {
    using namespace Aero::Meta;
    Register<PropertyChangedTrigger>(context)
            .Property<&PropertyChangedTrigger::GetBinding, &PropertyChangedTrigger::SetBinding>("Binding")
            .Content<TriggerAction>("Actions", ContentKind::Collection, &::Aero::MetadataSupport::AddPropertyChangedTriggerAction, &::Aero::MetadataSupport::ClearPropertyChangedTriggerActions)
            .Factory();
}

AERO_DESCRIBE(KeyTrigger) {
    using namespace Aero::Meta;
    Register<KeyTrigger>(context)
            .Property<&KeyTrigger::GetKey, &KeyTrigger::SetKey>("Key")
            .Property<&KeyTrigger::GetActiveOnFocus, &KeyTrigger::SetActiveOnFocus>("ActiveOnFocus")
            .Content<TriggerAction>("Actions", ContentKind::Collection, &::Aero::MetadataSupport::AddKeyTriggerAction, &::Aero::MetadataSupport::ClearKeyTriggerActions)
            .Factory();
}

AERO_DESCRIBE(InvokeCommandAction) {
    using namespace Aero::Meta;
    Register<InvokeCommandAction>(context)
            .Property<&InvokeCommandAction::GetCommand, &InvokeCommandAction::SetCommand>("Command")
            .Property<&InvokeCommandAction::GetCommandParameter, &InvokeCommandAction::SetCommandParameter>("CommandParameter")
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
            .Property<&PlaySoundAction::GetSource, &PlaySoundAction::SetSource>("Source")
            .Property<&PlaySoundAction::GetVolume, &PlaySoundAction::SetVolume>("Volume")
            .Property(PlaySoundAction::IsEnabledProperty, true)
            .Factory();
}

AERO_DESCRIBE(ComparisonCondition) {
    using namespace Aero::Meta;
    Register<ComparisonCondition>(context)
            .Property<&ComparisonCondition::GetLeftOperand, &ComparisonCondition::SetLeftOperand>("LeftOperand")
            .Property<&ComparisonCondition::GetRightOperand, &ComparisonCondition::SetRightOperand>("RightOperand")
            .Property<&ComparisonCondition::GetComparisonOperator, &ComparisonCondition::SetComparisonOperator>("Operator")
            .Factory();
}

AERO_DESCRIBE(ConditionalExpression) {
    using namespace Aero::Meta;
    Register<ConditionalExpression>(context)
            .Property<&ConditionalExpression::GetChaining, &ConditionalExpression::SetChaining>("ForwardChaining")
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

#include "gui/core/EnumRegistration.hpp"

namespace Aero {

void PopulateInteractivityEnums(
    Meta::Registration& context) noexcept {
    using namespace Interactivity;

    AERO_AUTO_REGISTER_NAMED_ENUM(ComparisonCondition::Operator, "ComparisonConditionOperator");
    AERO_AUTO_REGISTER_NAMED_ENUM(ConditionalExpression::ForwardChaining, "ForwardChaining");
}

} // namespace Aero
