#include "gui/ViewState.hpp"
#include "gui/internal/AeroGuiInternal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

using namespace ::Aero;
namespace MediaAnimation = ::Aero::Media::Animation;

Base::Result<void> InteractivityEngine::ExecuteStyleTriggerActions(
        ::Aero::DependencyObject& owner,
        Base::Span<const Base::Ref<Base::Object>>
            actions,
        void* context) noexcept {
        auto* runtime = static_cast<InteractivityEngine*>(context);
        if (runtime == nullptr ||
            !runtime->metadata->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Aero::FrameworkElement::
                    StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Style Trigger action owner is not a FrameworkElement");
        }
        auto& element =
            static_cast<Aero::FrameworkElement&>(
                owner);
        for (const Base::Ref<Base::Object>& authored :
             actions) {
            if (!authored ||
                !runtime->metadata->Types().IsDerivedFrom(
                    authored->RuntimeType(),
                    Aero::Interactivity::TriggerAction::
                        StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Style Trigger contains an invalid action");
            }
            Base::Result<void> executed =
                runtime->storyboards->ExecuteAnimationAction(
                    static_cast<Aero::Interactivity::TriggerAction&>(
                        *authored),
                    element);
            if (!executed) return executed.GetStatus();
        }
        return {};
    }

Base::Result<bool> InteractivityEngine::StyleDataTriggerValuesMatch(
        const Meta::PropertyValue& actual,
        Meta::PropertyValue expected) noexcept {
        if (actual.Kind() == Meta::ValueKind::Object &&
            !actual.IsNullObject() && actual.AsObject() &&
            actual.AsObject()->RuntimeType() ==
                ::Aero::Controls::BoxedItemValue::StaticTypeId()) {
            return StyleDataTriggerValuesMatch(
                static_cast<const ::Aero::Controls::BoxedItemValue&>(
                    *actual.AsObject()).Value(),
                std::move(expected));
        }
        if (expected.IsNullObject()) {
            return actual.IsNullObject();
        }
        if (expected.Kind() == Meta::ValueKind::String &&
            expected.Type() != actual.Type()) {
            Base::Result<Meta::PropertyValue> converted =
                metadata->TryConvertText(
                    actual.Type(), expected.AsString());
            if (!converted) return false;
            expected = std::move(converted).Value();
        }
        return actual == expected;
    }

Base::Result<void> InteractivityEngine::EvaluateStyleDataTrigger(
        StyleDataTriggerHandlerState& state) noexcept {
        if (styles == nullptr || state.target == nullptr ||
            state.style == nullptr || state.source == nullptr ||
            !state.property.IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style DataTrigger subscription is invalid");
        }
        Base::Result<Meta::PropertyValue> actual =
            state.source->GetValue(state.property);
        if (!actual) return actual.GetStatus();
        Base::Result<bool> matches = StyleDataTriggerValuesMatch(
            actual.Value(), state.expected);
        if (!matches) return matches.GetStatus();
        return styles->SetBindingTriggerState(
            *state.target,
            *state.style,
            state.triggerIndex,
            matches.Value());
    }

void InteractivityEngine::ClearStyleDataTriggersFor(
        Aero::FrameworkElement& target) noexcept {
        for (std::uint32_t index = 0U;
             index < styleDataTriggerSubscriptions.Size();) {
            StyleDataTriggerSubscription& subscription =
                styleDataTriggerSubscriptions[index];
            if (subscription.target != &target) {
                ++index;
                continue;
            }
            if (subscription.source != nullptr &&
                subscription.property.IsValid()) {
                (void)subscription.source->RemoveValueChangedHandler(
                    subscription.property,
                    subscription.handler);
            }
            FreeObject(
                *allocator,
                Base::MemoryTag::Ui,
                subscription.context);
            if (index + 1U !=
                styleDataTriggerSubscriptions.Size()) {
                styleDataTriggerSubscriptions[index] =
                    std::move(styleDataTriggerSubscriptions.Back());
            }
            styleDataTriggerSubscriptions.PopBack();
        }
    }

Base::Result<std::uint32_t> InteractivityEngine::StartStyleDataTriggers(
        Aero::FrameworkElement& target,
        const Aero::Style& style) noexcept {
        std::uint32_t started = 0U;
        const Base::Span<const Aero::TriggerPlan> triggers =
            Aero::StylePrivate::RuntimeTriggers(style);
        for (std::uint32_t index = 0U;
             index < triggers.Size(); ++index) {
            const Aero::TriggerPlan& trigger = triggers[index];
            if (!trigger.IsBindingTrigger()) continue;
            bool alreadyAttached = false;
            for (const StyleDataTriggerSubscription& existing :
                 styleDataTriggerSubscriptions) {
                alreadyAttached = alreadyAttached ||
                    (existing.target == &target &&
                     existing.context != nullptr &&
                     existing.context->style == &style &&
                     existing.context->triggerIndex == index);
            }
            if (alreadyAttached) continue;

            const Base::Ref<Data::Binding> binding = trigger.binding;
            Base::Object* sourceObject = nullptr;
            if (!binding->GetElementName().Empty()) {
                sourceObject = target.FindName(
                    binding->GetElementName());
            } else if (binding->GetRelativeSource()) {
                const Data::RelativeSourceMode mode =
                    binding->GetRelativeSource()->GetMode();
                if (mode == Data::RelativeSourceMode::Self) {
                    sourceObject = &target;
                } else if (mode ==
                           Data::RelativeSourceMode::TemplatedParent) {
                    sourceObject = target.GetTemplatedParent();
                } else if (mode ==
                           Data::RelativeSourceMode::FindAncestor) {
                    Base::StringView ancestorName =
                        binding->GetRelativeSource()->GetAncestorType();
                    for (std::uint32_t nameIndex = 0U;
                         nameIndex < ancestorName.SizeBytes(); ++nameIndex) {
                        if (ancestorName[nameIndex] == ':') {
                            ancestorName = ancestorName.Substr(
                                nameIndex + 1U,
                                ancestorName.SizeBytes() - nameIndex - 1U);
                            break;
                        }
                    }
                    const std::uint32_t requestedLevel =
                        binding->GetRelativeSource()->GetAncestorLevel();
                    std::uint32_t matchedLevel = 0U;
                    Aero::Media::Visual* current = ::Aero::TryCast<::Aero::Media::Visual>(target.GetLogicalParent());
                    if (current == nullptr) {
                        current = target.GetVisualParent();
                    }
                    while (current != nullptr) {
                        const Meta::TypeInfo* type =
                            metadata->Types().FindType(
                                current->RuntimeType());
                        const bool matchesType = ancestorName.Empty() ||
                            (type != nullptr && type->Name() == ancestorName);
                        if (matchesType && ++matchedLevel == requestedLevel) {
                            sourceObject = current;
                            break;
                        }
                        Aero::Media::Visual* next = ::Aero::TryCast<::Aero::Media::Visual>(current->GetLogicalParent());
                        if (next == nullptr) {
                            next = current->GetVisualParent();
                        }
                        current = next;
                    }
                }
            }
            if (sourceObject == nullptr ||
                !metadata->Types().IsDerivedFrom(
                    sourceObject->RuntimeType(),
                    ::Aero::DependencyObject::StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Style DataTrigger Binding source was not found");
            }
            auto* source = static_cast<::Aero::DependencyObject*>(
                sourceObject);
            const Base::StringView path =
                binding->GetPath().GetPath();
            const Meta::DependencyProperty* property =
                ::Aero::MetadataPrivate::
                    DependencyProperties(*metadata).Find(
                        source->RuntimeType(), path);
            if (property == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Style DataTrigger Binding path was not found");
            }

            StyleDataTriggerHandlerState* context = nullptr;
            Base::Result<void> allocated = AllocateObject(
                *allocator,
                Base::MemoryTag::Ui,
                context);
            if (!allocated) return allocated.GetStatus();
            context->runtime = this;
            context->target = &target;
            context->style = &style;
            context->triggerIndex = index;
            context->source = source;
            context->property = property->Handle();
            context->expected = trigger.value;
            auto callback = [context](
                ::Aero::DependencyObject& object,
                const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
                    context->Invoke(object, args);
                };
            Meta::DependencyPropertyChangedEventHandler handler(callback);
            Base::Result<void> subscribed =
                source->AddValueChangedHandlerChecked(
                    property->Handle(), handler);
            if (!subscribed) {
                FreeObject(
                    *allocator,
                    Base::MemoryTag::Ui,
                    context);
                return subscribed.GetStatus();
            }
            StyleDataTriggerSubscription subscription;
            subscription.target = &target;
            subscription.source = source;
            subscription.property = property->Handle();
            subscription.handler = handler;
            subscription.context = context;
            Base::Result<void> retained =
                styleDataTriggerSubscriptions.PushBack(
                    std::move(subscription));
            if (!retained) {
                (void)source->RemoveValueChangedHandler(
                    property->Handle(), handler);
                FreeObject(
                    *allocator,
                    Base::MemoryTag::Ui,
                    context);
                return retained.GetStatus();
            }
            Base::Result<void> evaluated =
                EvaluateStyleDataTrigger(*context);
            if (!evaluated) return evaluated.GetStatus();
            ++started;
        }
        return started;
    }

void InteractivityEngine::StyleDataTriggerHandlerState::Invoke(
    ::Aero::DependencyObject&,
    const Meta::DependencyPropertyChangedEventArgs&) noexcept
{
    if (runtime == nullptr ||
        !runtime->animationEventStatus.IsOk()) {
        return;
    }
    Base::Result<void> evaluated =
        runtime->EvaluateStyleDataTrigger(*this);
    if (!evaluated) {
        runtime->animationEventStatus =
            evaluated.GetStatus();
    }
}

} // namespace Aero
