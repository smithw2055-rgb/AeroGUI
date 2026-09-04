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
            actual.Kind() == Meta::ValueKind::String) {
            return actual.AsString() == expected.AsString();
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
            state.style == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style DataTrigger subscription is invalid");
        }
        const bool hasDependency =
            state.source != nullptr && state.property.IsValid();
        const bool hasMetadata =
            state.metadataSource != nullptr &&
            state.metadataProperty != Meta::InvalidMemberId &&
            metadata != nullptr;
        if (!hasDependency && !hasMetadata) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Style DataTrigger subscription is invalid");
        }
        Base::Result<Meta::PropertyValue> actual = hasDependency
            ? state.source->GetValue(state.property)
            : metadata->GetProperty(
                  *state.metadataSource, state.metadataProperty);
        if (!actual) return actual.GetStatus();
        Base::Result<bool> matches = StyleDataTriggerValuesMatch(
            actual.Value(), state.expected);
        if (!matches) return matches.GetStatus();
        if (state.aggregate != nullptr) {
            if (state.conditionIndex >= state.aggregate->known.Size() ||
                state.conditionIndex >= state.aggregate->active.Size()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style MultiDataTrigger condition index is out of range");
            }
            state.aggregate->known[state.conditionIndex] = 1U;
            state.aggregate->active[state.conditionIndex] =
                matches.Value() ? 1U : 0U;
            bool allKnown = true;
            bool allActive = true;
            for (std::uint32_t index = 0U;
                 index < state.aggregate->known.Size();
                 ++index) {
                allKnown = allKnown &&
                    state.aggregate->known[index] != 0U;
                allActive = allActive &&
                    state.aggregate->active[index] != 0U;
            }
            if (!allKnown) {
                return {};
            }
            return styles->SetBindingTriggerState(
                *state.target,
                *state.style,
                state.triggerIndex,
                allActive);
        }
        return styles->SetBindingTriggerState(
            *state.target,
            *state.style,
            state.triggerIndex,
            matches.Value());
    }

void InteractivityEngine::FlushPendingStyleDataTriggerEvaluations() noexcept {
        if (flushingPendingStyleDataTriggers_ ||
            pendingStyleDataTriggerEvaluations.Empty()) {
            return;
        }
        flushingPendingStyleDataTriggers_ = true;
        Base::Vector<StyleDataTriggerHandlerState*> snapshot(allocator);
        for (StyleDataTriggerHandlerState* context :
             pendingStyleDataTriggerEvaluations) {
            Base::Result<void> retained = snapshot.PushBack(context);
            if (!retained) {
                flushingPendingStyleDataTriggers_ = false;
                return;
            }
        }
        pendingStyleDataTriggerEvaluations.Clear();
        for (StyleDataTriggerHandlerState* context : snapshot) {
            if (context == nullptr || context->target == nullptr) {
                continue;
            }
            bool live = false;
            for (const StyleDataTriggerSubscription& subscription :
                 styleDataTriggerSubscriptions) {
                live = live || subscription.context == context;
            }
            if (!live) continue;
            Base::Result<void> evaluated =
                EvaluateStyleDataTrigger(*context);
            if (!evaluated && view != nullptr) {
                view->ReportUpdateFailure(evaluated.GetStatus());
            }
        }
        flushingPendingStyleDataTriggers_ = false;
    }

void InteractivityEngine::ClearStyleDataTriggersFor(
        Aero::FrameworkElement& target) noexcept {
        for (std::uint32_t index = 0U;
             index < pendingStyleDataTriggerEvaluations.Size();) {
            StyleDataTriggerHandlerState* context =
                pendingStyleDataTriggerEvaluations[index];
            if (context == nullptr || context->target != &target) {
                ++index;
                continue;
            }
            if (index + 1U != pendingStyleDataTriggerEvaluations.Size()) {
                pendingStyleDataTriggerEvaluations[index] =
                    pendingStyleDataTriggerEvaluations.Back();
            }
            pendingStyleDataTriggerEvaluations.PopBack();
        }
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
            if (subscription.metadataSource != nullptr &&
                subscription.metadataSubscription != 0U &&
                metadata != nullptr) {
                static_cast<void>(metadata->UnsubscribePropertyChanged(
                    *subscription.metadataSource,
                    subscription.metadataSubscription));
            }
            if (subscription.context != nullptr) {
                if (subscription.context->ownsAggregate &&
                    subscription.context->aggregate != nullptr) {
                    FreeObject(
                        *allocator,
                        Base::MemoryTag::Ui,
                        subscription.context->aggregate);
                }
                FreeObject(
                    *allocator,
                    Base::MemoryTag::Ui,
                    subscription.context);
            }
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
            if (!trigger.binding) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Style DataTrigger Binding is incomplete");
            }

            const std::uint32_t conditionCount =
                1U + trigger.extraBindings.Size();
            StyleDataTriggerAggregate* aggregate = nullptr;
            if (conditionCount > 1U) {
                Base::Result<void> allocated = AllocateObject(
                    *allocator,
                    Base::MemoryTag::Ui,
                    aggregate);
                if (!allocated) return allocated.GetStatus();
                Base::Result<void> sized =
                    aggregate->known.Resize(conditionCount, 0U);
                if (sized) {
                    sized = aggregate->active.Resize(conditionCount, 0U);
                }
                if (!sized) {
                    FreeObject(
                        *allocator,
                        Base::MemoryTag::Ui,
                        aggregate);
                    return sized.GetStatus();
                }
            }

            // {Binding Path} DataTriggers use DataContext. Item containers often
            // receive Style before PrepareContainer assigns the item, so a
            // missing DataContext is a retry, not a hard error. Skip the
            // whole trigger so MultiDataTrigger extras are not attached alone.
            auto bindingWaitsForDataContext =
                [](const Base::Ref<Data::Binding>& binding) noexcept {
                    return binding &&
                        binding->GetElementName().Empty() &&
                        !binding->GetRelativeSource() &&
                        !binding->GetSource();
                };
            auto dataContextReady = [&target]() noexcept {
                const Base::Value dataContext = target.GetDataContext();
                return !dataContext.IsNullObject() &&
                    dataContext.AsObject().Get() != nullptr;
            };
            bool deferred = bindingWaitsForDataContext(trigger.binding) &&
                !dataContextReady();
            for (std::uint32_t extraIndex = 0U;
                 extraIndex < trigger.extraBindings.Size() && !deferred;
                 ++extraIndex) {
                deferred = bindingWaitsForDataContext(
                    trigger.extraBindings[extraIndex].binding) &&
                    !dataContextReady();
            }
            if (deferred) {
                if (aggregate != nullptr) {
                    FreeObject(
                        *allocator,
                        Base::MemoryTag::Ui,
                        aggregate);
                }
                continue;
            }

            auto attachCondition =
                [this, &target, &style, index, aggregate](
                    const Base::Ref<Data::Binding>& binding,
                    const Meta::PropertyValue& expected,
                    std::uint32_t conditionIndex,
                    bool ownsAggregate) -> Base::Result<void> {
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
                        Aero::Media::Visual* current =
                            ::Aero::TryCast<::Aero::Media::Visual>(
                                target.GetLogicalParent());
                        if (current == nullptr) {
                            current = target.GetVisualParent();
                        }
                        while (current != nullptr) {
                            const Meta::TypeInfo* type =
                                metadata->Types().FindType(
                                    current->RuntimeType());
                            const bool matchesType = ancestorName.Empty() ||
                                (type != nullptr &&
                                 type->Name() == ancestorName);
                            if (matchesType &&
                                ++matchedLevel == requestedLevel) {
                                sourceObject = current;
                                break;
                            }
                            Aero::Media::Visual* next =
                                ::Aero::TryCast<::Aero::Media::Visual>(
                                    current->GetLogicalParent());
                            if (next == nullptr) {
                                next = current->GetVisualParent();
                            }
                            current = next;
                        }
                    }
                } else {
                    // Default binding source is the element's DataContext
                    // (inherited from the logical tree), mirroring how a
                    // plain {Binding Path} resolves its source.
                    Base::Value dataContext = target.GetDataContext();
                    if (!dataContext.IsNullObject()) {
                        Base::Object* contextObject =
                            dataContext.AsObject().Get();
                        if (contextObject != nullptr) {
                            sourceObject = contextObject;
                        }
                    }
                }
                if (sourceObject == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Style DataTrigger Binding source was not found");
                }
                const Base::StringView path =
                    binding->GetPath().GetPath();
                ::Aero::DependencyObject* dependencySource = nullptr;
                const Meta::DependencyProperty* dependencyProperty = nullptr;
                Meta::MemberId metadataProperty = Meta::InvalidMemberId;
                if (metadata->Types().IsDerivedFrom(
                        sourceObject->RuntimeType(),
                        ::Aero::DependencyObject::StaticTypeId())) {
                    dependencySource =
                        static_cast<::Aero::DependencyObject*>(sourceObject);
                    dependencyProperty =
                        ::Aero::MetadataPrivate::
                            DependencyProperties(*metadata).Find(
                                sourceObject->RuntimeType(), path);
                }
                if (dependencyProperty == nullptr) {
                    Base::StringView rootPath = path;
                    for (std::uint32_t pathIndex = 0U;
                         pathIndex < path.SizeBytes(); ++pathIndex) {
                        if (path[pathIndex] == '.') {
                            rootPath = path.Substr(0U, pathIndex);
                            break;
                        }
                    }
                    const Meta::PropertyInfo* clrProperty =
                        metadata->Types().FindProperty(
                            sourceObject->RuntimeType(), rootPath, true);
                    if (clrProperty == nullptr ||
                        !metadata->CanReadProperty(clrProperty->Id())) {
                        return Base::Status::Failure(
                            Base::ErrorCode::NotFound,
                            "Style DataTrigger Binding path was not found");
                    }
                    metadataProperty = clrProperty->Id();
                    dependencySource = nullptr;
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
                context->conditionIndex = conditionIndex;
                context->aggregate = aggregate;
                context->ownsAggregate = ownsAggregate;
                context->source = dependencySource;
                context->metadataSource =
                    dependencyProperty == nullptr ? sourceObject : nullptr;
                context->property = dependencyProperty != nullptr
                    ? dependencyProperty->Handle()
                    : Meta::DependencyPropertyHandle{};
                context->metadataProperty = metadataProperty;
                context->expected = expected;
                auto callback = [context](
                    ::Aero::DependencyObject& object,
                    const Meta::DependencyPropertyChangedEventArgs& args)
                    noexcept {
                        context->Invoke(object, args);
                    };
                Meta::DependencyPropertyChangedEventHandler handler(callback);
                std::uint64_t metadataSubscription = 0U;
                Base::Result<void> subscribed;
                if (dependencyProperty != nullptr) {
                    dependencySource->AddValueChangedHandler(
                        dependencyProperty->Handle(), handler);
                } else {
                    Base::Result<std::uint64_t> notification =
                        metadata->SubscribePropertyChanged(
                            *sourceObject,
                            &StyleDataTriggerHandlerState::MetadataInvoke,
                            context);
                    if (notification) {
                        metadataSubscription = notification.Value();
                    } else {
                        subscribed = notification.GetStatus();
                    }
                }
                if (!subscribed) {
                    FreeObject(
                        *allocator,
                        Base::MemoryTag::Ui,
                        context);
                    return subscribed.GetStatus();
                }
                StyleDataTriggerSubscription subscription;
                subscription.target = &target;
                subscription.source = dependencySource;
                subscription.metadataSource =
                    dependencyProperty == nullptr ? sourceObject : nullptr;
                subscription.property = dependencyProperty != nullptr
                    ? dependencyProperty->Handle()
                    : Meta::DependencyPropertyHandle{};
                subscription.metadataSubscription = metadataSubscription;
                subscription.handler = handler;
                subscription.context = context;
                Base::Result<void> retained =
                    styleDataTriggerSubscriptions.PushBack(
                        std::move(subscription));
                if (!retained) {
                    if (dependencyProperty != nullptr) {
                        (void)dependencySource->RemoveValueChangedHandler(
                            dependencyProperty->Handle(), handler);
                    } else if (metadataSubscription != 0U) {
                        static_cast<void>(metadata->UnsubscribePropertyChanged(
                            *sourceObject, metadataSubscription));
                    }
                    FreeObject(
                        *allocator,
                        Base::MemoryTag::Ui,
                        context);
                    return retained.GetStatus();
                }
                // Defer the first evaluation until DataBind. Evaluating
                // here runs SetBindingTriggerState inside ApplyViewUi /
                // item generation, which can re-enter the property engine
                // and prevent the first frame from completing.
                return pendingStyleDataTriggerEvaluations.PushBack(context);
            };

            Base::Result<void> attached = attachCondition(
                trigger.binding,
                trigger.value,
                0U,
                aggregate != nullptr);
            if (!attached) {
                if (aggregate != nullptr) {
                    bool owned = false;
                    for (const StyleDataTriggerSubscription& existing :
                         styleDataTriggerSubscriptions) {
                        owned = owned ||
                            (existing.context != nullptr &&
                             existing.context->aggregate == aggregate);
                    }
                    if (!owned) {
                        FreeObject(
                            *allocator,
                            Base::MemoryTag::Ui,
                            aggregate);
                    }
                }
                return attached.GetStatus();
            }
            ++started;
            for (std::uint32_t extraIndex = 0U;
                 extraIndex < trigger.extraBindings.Size();
                 ++extraIndex) {
                attached = attachCondition(
                    trigger.extraBindings[extraIndex].binding,
                    trigger.extraBindings[extraIndex].value,
                    extraIndex + 1U,
                    false);
                if (!attached) return attached.GetStatus();
                ++started;
            }
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

void InteractivityEngine::StyleDataTriggerHandlerState::MetadataInvoke(
    Base::Object&,
    Meta::MemberId property,
    void* context) noexcept
{
    auto* state = static_cast<StyleDataTriggerHandlerState*>(context);
    if (state == nullptr ||
        (state->metadataProperty != Meta::InvalidMemberId &&
         property != Meta::InvalidMemberId &&
         property != state->metadataProperty)) {
        return;
    }
    if (state->runtime == nullptr ||
        !state->runtime->animationEventStatus.IsOk()) {
        return;
    }
    Base::Result<void> evaluated =
        state->runtime->EvaluateStyleDataTrigger(*state);
    if (!evaluated) {
        state->runtime->animationEventStatus =
            evaluated.GetStatus();
    }
}

} // namespace Aero
