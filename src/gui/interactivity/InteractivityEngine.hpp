#pragma once

// Source-only interactivity evaluation engine. Not installed under include/Aero.
// Included from ViewState.hpp after ViewState is defined.

namespace Aero {

class InteractivityEngine {
public:
    explicit InteractivityEngine(ViewState& owner) noexcept;
    void Bind() noexcept;
    void NotifyLayoutUpdated() noexcept;
    void RetryPendingInteractionTriggers() noexcept;
    void FlushPendingStyleDataTriggerEvaluations() noexcept;

    ViewState* view = nullptr;
    Base::IAllocator* allocator = nullptr;
    ::Aero::Meta::Registry* metadata = nullptr;
    Aero::AnimationEngine* animations = nullptr;
    Aero::EventRouter* events = nullptr;
    Aero::InputRouter* input = nullptr;
    Aero::ElementTree* tree = nullptr;
    Aero::StyleEngine* styles = nullptr;
    Meta::EffectiveValueEngine* values = nullptr;
    ::Aero::Threading::Dispatcher* dispatcher = nullptr;
    Aero::Controls::TemplateEngine* templates = nullptr;
    Aero::BindingEngine* bindings = nullptr;
    class StoryboardHost* storyboards = nullptr;

    static Base::Result<void> ExecuteStyleTriggerActions(
        ::Aero::DependencyObject& owner,
        Base::Span<const Base::Ref<Base::Object>> actions,
        void* context) noexcept;
    Base::Result<bool> ConditionBehaviorsAllowExecution(
        Base::Span<const Base::Ref<Base::Object>> behaviors,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept;

    struct StyleDataTriggerAggregate {
        Base::Vector<std::uint8_t> known;
        Base::Vector<std::uint8_t> active;
    };
    struct StyleDataTriggerHandlerState {
        InteractivityEngine* runtime = nullptr;
        Aero::FrameworkElement* target = nullptr;
        const Aero::Style* style = nullptr;
        std::uint32_t triggerIndex = 0U;
        std::uint32_t conditionIndex = 0U;
        StyleDataTriggerAggregate* aggregate = nullptr;
        bool ownsAggregate = false;
        ::Aero::DependencyObject* source = nullptr;
        Base::Object* metadataSource = nullptr;
        Meta::DependencyPropertyHandle property;
        Meta::MemberId metadataProperty = Meta::InvalidMemberId;
        Meta::PropertyValue expected;

        void Invoke(
            ::Aero::DependencyObject&,
            const Meta::DependencyPropertyChangedEventArgs&) noexcept;
        static void MetadataInvoke(
            Base::Object&,
            Meta::MemberId property,
            void* context) noexcept;
    };
    struct StyleDataTriggerSubscription {
        Aero::FrameworkElement* target = nullptr;
        ::Aero::DependencyObject* source = nullptr;
        Base::Object* metadataSource = nullptr;
        Meta::DependencyPropertyHandle property;
        std::uint64_t metadataSubscription = 0U;
        Meta::DependencyPropertyChangedEventHandler handler;
        StyleDataTriggerHandlerState* context = nullptr;
    };
    Base::Vector<StyleDataTriggerSubscription> styleDataTriggerSubscriptions;

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
        Meta::MemberId metadataProperty = Meta::InvalidMemberId;

        void Invoke(
            ::Aero::DependencyObject&,
            const Meta::DependencyPropertyChangedEventArgs&) noexcept;
        static void MetadataInvoke(
            Base::Object&,
            Meta::MemberId property,
            void* context) noexcept;
    };
    struct PropertyChangedTriggerSubscription {
        Aero::FrameworkElement* owner = nullptr;
        ::Aero::DependencyObject* source = nullptr;
        Base::Object* metadataSource = nullptr;
        Meta::DependencyPropertyHandle property;
        std::uint64_t metadataSubscription = 0U;
        Meta::DependencyPropertyChangedEventHandler handler;
        PropertyChangedTriggerState* context = nullptr;
    };
    Base::Vector<PropertyChangedTriggerSubscription>
        propertyChangedTriggerSubscriptions;

    struct InteractionDataTriggerState {
        InteractivityEngine* runtime = nullptr;
        Aero::DataTrigger* trigger = nullptr;
        Aero::FrameworkElement* owner = nullptr;
        const Aero::NameScope* names = nullptr;
        Meta::MemberId metadataProperty = Meta::InvalidMemberId;
        bool active = false;

        void Invoke(
            ::Aero::DependencyObject&,
            const Meta::DependencyPropertyChangedEventArgs&) noexcept;
        static void MetadataInvoke(
            Base::Object&,
            Meta::MemberId property,
            void* context) noexcept;
    };
    struct InteractionDataTriggerSubscription {
        Aero::FrameworkElement* owner = nullptr;
        ::Aero::DependencyObject* source = nullptr;
        Base::Object* metadataSource = nullptr;
        Meta::DependencyPropertyHandle property;
        std::uint64_t metadataSubscription = 0U;
        Meta::DependencyPropertyChangedEventHandler handler;
        InteractionDataTriggerState* context = nullptr;
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
    bool flushingPendingStyleDataTriggers_ = false;
    Base::Vector<StyleDataTriggerHandlerState*>
        pendingStyleDataTriggerEvaluations;

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

    struct DataTemplateTriggerHandlerState {
        InteractivityEngine* runtime = nullptr;
        Base::Ref<Aero::Controls::DataTemplateTriggerState> triggerContext;
        std::uint32_t triggerIndex = 0U;
        std::uint32_t conditionIndex = 0U;
        Meta::MemberId metadataProperty = Meta::InvalidMemberId;

        void Invoke(
            ::Aero::DependencyObject&,
            const Meta::DependencyPropertyChangedEventArgs&) noexcept;
        static void MetadataInvoke(
            Base::Object&,
            Meta::MemberId property,
            void* context) noexcept;
    };
    struct DataTemplateTriggerSubscription {
        ::Aero::DependencyObject* source = nullptr;
        Base::Object* metadataSource = nullptr;
        Meta::DependencyPropertyHandle property;
        std::uint64_t metadataSubscription = 0U;
        Meta::DependencyPropertyChangedEventHandler handler;
        DataTemplateTriggerHandlerState* context = nullptr;
    };
    Base::Vector<DataTemplateTriggerSubscription>
        dataTemplateTriggerSubscriptions;
    Base::Status animationEventStatus;

    Base::Result<bool> DataTemplateTriggerValuesMatch(
        const Meta::PropertyValue& actual,
        Meta::PropertyValue expected) noexcept;
    Base::Result<bool> EvaluateTriggerComparison(
        const Meta::PropertyValue& actual,
        Meta::PropertyValue expected,
        Base::StringView comparison) noexcept;
    Base::Object* ResolveDataTemplateConditionSource(
        Aero::Controls::DataTemplateTriggerState& context,
        Aero::Controls::DataTemplateTriggerCondition& condition,
        Base::StringView& path) noexcept;
    Base::Result<bool> EvaluateDataTemplateCondition(
        Aero::Controls::DataTemplateTriggerState& context,
        Aero::Controls::DataTemplateTriggerCondition& condition) noexcept;
    Base::Result<void> EnsureDataTemplateProviderTokens(
        Aero::Controls::DataTemplateTriggerState& context) noexcept;
    Base::Result<void> EvaluateDataTemplateTrigger(
        Aero::Controls::DataTemplateTriggerState& context,
        std::uint32_t triggerIndex) noexcept;
    Base::Result<bool> StyleDataTriggerValuesMatch(
        const Meta::PropertyValue& actual,
        Meta::PropertyValue expected) noexcept;
    Base::Result<void> EvaluateStyleDataTrigger(
        StyleDataTriggerHandlerState& state) noexcept;
    void ClearStyleDataTriggersFor(Aero::FrameworkElement& target) noexcept;
    Base::Result<std::uint32_t> StartStyleDataTriggers(
        Aero::FrameworkElement& target,
        const Aero::Style& style) noexcept;
    Base::Result<std::uint32_t> StartDataTemplateTriggers(
        Aero::Controls::DataTemplateTriggerState& context) noexcept;
    Base::Result<void> AttachDataTemplateClrSubscription(
        Aero::Controls::DataTemplateTriggerState& context,
        std::uint32_t triggerIndex) noexcept;
    Base::Result<Base::Ref<Interactivity::Behavior>> CloneBehaviorPrototype(
        const Interactivity::Behavior& prototype) noexcept;
    Base::Object* ResolveBehaviorBindingSource(
        const Data::Binding& binding,
        Interactivity::Behavior& behavior,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept;
    Base::Object* ResolveAuthoredBindingSource(
        const Data::Binding& binding,
        Aero::FrameworkElement& owner,
        Aero::Controls::DataTemplateTriggerState* dataTemplateContext,
        const Aero::NameScope* names,
        Base::Object* self) noexcept;
    Base::Result<Meta::PropertyValue> EvaluateAuthoredBinding(
        const Data::Binding& binding,
        Aero::FrameworkElement& owner,
        Aero::Controls::DataTemplateTriggerState* dataTemplateContext,
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

    struct InteractionTriggerProperty {
        Base::Object* source = nullptr;
        ::Aero::DependencyObject* dependencySource = nullptr;
        Meta::DependencyPropertyHandle dependencyProperty;
        Meta::MemberId metadataProperty = Meta::InvalidMemberId;
    };
    Base::Result<InteractionTriggerProperty> ResolveInteractionTriggerProperty(
        const Data::Binding& binding,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept;
    Base::Result<bool> EvaluateInteractionDataTrigger(
        InteractionDataTriggerState& state) noexcept;
    Base::Result<bool> StartPropertyChangedTrigger(
        Aero::Interactivity::PropertyChangedTrigger& trigger,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept;
    Base::Result<bool> StartInteractionDataTrigger(
        Aero::DataTrigger& trigger,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept;
    Base::Result<void> PendUntilDataContext(
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
        Aero::Controls::DataTemplateTriggerState& context) noexcept;
    void ClearDataTemplateTriggerProvidersInSubtree(
        Aero::Media::Visual& visual) noexcept;
    void DetachBehaviorsInSubtree(Aero::Media::Visual& visual) noexcept;
    void ClearAnimationSubscriptionsFor(
        Aero::Media::Visual& fragmentRoot) noexcept;
    void ClearAnimationEventSubscriptions() noexcept;
};

} // namespace Aero
