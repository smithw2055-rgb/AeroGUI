#pragma once

// Source-only interactivity evaluation engine. Not installed under include/Aero.
// Included from ViewFrame.hpp after ViewFrame is defined.
// All Style, ControlTemplate, and DataTemplate triggers as well as Interaction
// DataTriggers and PropertyChangedTriggers delegate trigger evaluation and
// binding subscriptions to TriggerRuntime (TriggerEngine) + BindingEngine.

#include "gui/templates/DataTemplateTriggerInstance.hpp"
#include "gui/triggers/TriggerEngine.hpp"
#include "gui/data/BindingCommon.hpp"

namespace Aero {

class InteractivityEngine {
public:
    explicit InteractivityEngine(ViewFrame& owner) noexcept;
    void Bind() noexcept;
    void NotifyLayoutUpdated() noexcept;
    void NotifyPreRender() noexcept;
    void RetryPendingInteractionTriggers() noexcept;
    void FlushPendingStyleDataTriggerEvaluations() noexcept;

    ViewFrame* view = nullptr;

    // Service pointers are not cached after Bind; read the ElementTree hub /
    // ViewFrame owners through these accessors.
    Base::IAllocator* Allocator() const noexcept;
    ::Aero::Meta::Registry* Metadata() const noexcept;
    Aero::AnimationEngine* Animations() const noexcept;
    Aero::InputRouter* Input() const noexcept;
    Aero::ElementTree* Tree() const noexcept;
    Aero::StyleEngine* Styles() const noexcept;
    Meta::EffectiveValueEngine* Values() const noexcept;
    Aero::BindingEngine* Bindings() const noexcept;
    class StoryboardHost* Storyboards() const noexcept;

    static Base::Result<void> ExecuteStyleTriggerActions(
        ::Aero::DependencyObject& owner,
        Base::Span<const Base::Ref<Base::Object>> actions,
        void* context) noexcept;
    static Base::Result<void> ExecuteDataTemplateTriggerActions(
        Aero::Controls::DataTemplateTriggerInstance& instance,
        Base::Span<const Base::Ref<Base::Object>> actions,
        void* context) noexcept;
    static Base::Result<std::uint32_t> StartDataTemplateEventTriggers(
        Aero::Controls::DataTemplateTriggerInstance& instance,
        void* context) noexcept;
    static void ReportTriggerFailure(
        const Base::Status& status,
        void* context) noexcept;

    Base::Result<bool> ConditionBehaviorsAllowExecution(
        Base::Span<const Base::Ref<Base::Object>> behaviors,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept;

    struct AttachedBehaviorInstance {
        Aero::FrameworkElement* target = nullptr;
        const Interactivity::Behavior* prototype = nullptr;
        Base::Ref<Interactivity::Behavior> instance;
        Base::Vector<Data::BindingHandle> bindings;
    };
    Base::Vector<AttachedBehaviorInstance> attachedBehaviorInstances;

    struct PropertyChangedTriggerState {
        InteractivityEngine* runtime = nullptr;
        Aero::Interactivity::PropertyChangedTrigger* trigger = nullptr;
        Aero::FrameworkElement* owner = nullptr;
        const Aero::NameScope* names = nullptr;

        void Invoke(
            ::Aero::DependencyObject&,
            const Meta::DependencyPropertyChangedEventArgs& args) noexcept;
    };
    struct PropertyChangedTriggerSubscription {
        Aero::FrameworkElement* owner = nullptr;
        TriggerRuntime::TriggerBindingSubscription binding;
        PropertyChangedTriggerState* context = nullptr;

        void Unbind(InteractivityEngine& engine) noexcept;
    };
    Base::Vector<PropertyChangedTriggerSubscription>
        propertyChangedTriggerSubscriptions;

    struct InteractionDataTriggerState {
        InteractivityEngine* runtime = nullptr;
        Aero::DataTrigger* trigger = nullptr;
        Aero::FrameworkElement* owner = nullptr;
        const Aero::NameScope* names = nullptr;
        bool active = false;

        void Invoke(
            ::Aero::DependencyObject&,
            const Meta::DependencyPropertyChangedEventArgs& args) noexcept;
    };
    struct InteractionDataTriggerSubscription {
        Aero::FrameworkElement* owner = nullptr;
        TriggerRuntime::TriggerBindingSubscription binding;
        InteractionDataTriggerState* context = nullptr;

        void Unbind(InteractivityEngine& engine) noexcept;
    };
    Base::Vector<InteractionDataTriggerSubscription>
        interactionDataTriggerSubscriptions;

    struct PendingInteractionTrigger {
        Aero::FrameworkElement* owner = nullptr;
        const Aero::NameScope* names = nullptr;
        Aero::DataTrigger* dataTrigger = nullptr;
        Aero::Interactivity::PropertyChangedTrigger* propertyTrigger =
            nullptr;
    };
    Base::Vector<PendingInteractionTrigger> pendingInteractionTriggers;
    bool retryingPendingInteractionTriggers_ = false;

    struct KeyTriggerState {
        InteractivityEngine* runtime = nullptr;
        Aero::Interactivity::KeyTrigger* trigger = nullptr;
        Aero::FrameworkElement* owner = nullptr;
        const Aero::NameScope* names = nullptr;

        void Invoke(Base::Object*, Aero::KeyEventArgs& args) noexcept;
    };
    struct KeyTriggerSubscription {
        Aero::FrameworkElement* owner = nullptr;
        Aero::UIElement* source = nullptr;
        Aero::KeyEventHandler handler;
        KeyTriggerState* context = nullptr;
    };
    Base::Vector<KeyTriggerSubscription> keyTriggerSubscriptions;
    Base::Status animationEventStatus;

    Base::Result<const Meta::BindingPathPlan*> FindOrCompileBindingPath(
        Meta::TypeId sourceType,
        Base::StringView path) noexcept;

    bool DataTemplateTriggerValuesMatch(
        const Meta::PropertyValue& actual,
        const Meta::PropertyValue& expected) noexcept;
    bool EvaluateTriggerComparison(
        const Meta::PropertyValue& actual,
        const Meta::PropertyValue& expected,
        Base::StringView comparison) noexcept;
    Base::Result<bool> EvaluateDataTemplateCondition(
        Aero::Controls::DataTemplateTriggerInstance& context,
        Aero::Controls::DataTemplateTriggerCondition& condition) noexcept;
    Base::Result<void> EnsureDataTemplateProviderTokens(
        Aero::Controls::DataTemplateTriggerInstance& context) noexcept;
    Base::Result<void> EvaluateDataTemplateTrigger(
        Aero::Controls::DataTemplateTriggerInstance& context,
        std::uint32_t triggerIndex) noexcept;
    void ClearStyleDataTriggersFor(Aero::FrameworkElement& target) noexcept;
    Base::Result<std::uint32_t> StartStyleDataTriggers(
        Aero::FrameworkElement& target,
        const Aero::Style& style) noexcept;
    Base::Result<std::uint32_t> StartDataTemplateTriggers(
        Aero::Controls::DataTemplateTriggerInstance& context) noexcept;
    Base::Ref<Interactivity::Behavior> CloneBehaviorPrototype(
        const Interactivity::Behavior& prototype) noexcept;
    Base::Object* ResolveBehaviorBindingSource(
        const Data::Binding& binding,
        Interactivity::Behavior& behavior,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept;
    Base::Object* ResolveAuthoredBindingSource(
        const Data::Binding& binding,
        Aero::FrameworkElement& owner,
        Aero::Controls::DataTemplateTriggerInstance* dataTemplateContext,
        const Aero::NameScope* names,
        Base::Object* self) noexcept;
    Base::Result<Meta::PropertyValue> EvaluateAuthoredBinding(
        const Data::Binding& binding,
        Aero::FrameworkElement& owner,
        Aero::Controls::DataTemplateTriggerInstance* dataTemplateContext,
        const Aero::NameScope* names,
        Base::Object* self) noexcept;
    Base::Result<void> ExecuteTriggerActions(
        Base::Span<const Base::Ref<Base::Object>> actions,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept;
    Base::Result<void> ExecuteTriggerActions(
        Base::Span<const Base::Ref<Aero::Interactivity::TriggerAction>> actions,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept;

    Base::Result<bool> EvaluateInteractionDataTrigger(
        InteractionDataTriggerState& state,
        const Meta::PropertyValue& current) noexcept;
    Base::Result<bool> StartPropertyChangedTrigger(
        Aero::Interactivity::PropertyChangedTrigger& trigger,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept;
    Base::Result<bool> StartInteractionDataTrigger(
        Aero::DataTrigger& trigger,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept;
    void PendUntilDataContext(
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names,
        Aero::DataTrigger* dataTrigger,
        Aero::Interactivity::PropertyChangedTrigger* propertyTrigger)
        noexcept;
    void ClearPendingInteractionTriggers() noexcept;
    static std::uint32_t KeyCodeFromName(Base::StringView key) noexcept;
    Base::Result<bool> StartKeyTrigger(
        Aero::Interactivity::KeyTrigger& trigger,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept;
    Base::Result<void> AttachBehavior(
        const Interactivity::Behavior& prototype,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names,
        bool clonePrototype) noexcept;
    bool IsInVisualSubtree(
        Aero::Media::Visual* node,
        const Aero::Media::Visual& fragmentRoot) const noexcept;
    void ClearDataTemplateTriggerProviders(
        Aero::Controls::DataTemplateTriggerInstance& context) noexcept;
    void ClearDataTemplateTriggerProvidersInSubtree(
        Aero::Media::Visual& visual) noexcept;
    void DetachBehaviorsInSubtree(Aero::Media::Visual& visual) noexcept;
    void ClearAnimationSubscriptionsFor(
        Aero::Media::Visual& fragmentRoot) noexcept;
    void ClearAnimationEventSubscriptions() noexcept;

private:
    TriggerRuntime* EnsureTriggerRuntime() noexcept;
};

} // namespace Aero
