#include "runtime/ViewRuntime.hpp"
#include "runtime/ImageRuntime.hpp"
#include "runtime/TextRuntime.hpp"
#include "SchemaBundle.hpp"

#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Bars.hpp>
#include <Aero/Controls/ControlPrimitives.hpp>
#include <Aero/Controls/ContentControls.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Metadata.hpp>
#include <Aero/Controls/Menus.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/Trees.hpp>
#include <Aero/Controls/Virtualization.hpp>
#include <Aero/Core/Metadata/MetadataDomain.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include "core/metadata/MetadataDomainAccess.hpp"
#include "markup/Loader.hpp"
#include "markup/LoadOptionsAccess.hpp"
#include "markup/LoaderResult.hpp"
#include "UiDocumentAccess.hpp"
#include <Aero/Markup/Schema.hpp>
#include <Aero/Platform/Clipboard.hpp>
#include <Aero/Platform/Ime.hpp>
#include <Aero/Presentation/Binding.hpp>
#include <Aero/Presentation/Animation.hpp>
#include <Aero/Presentation/AnimationXaml.hpp>
#include <Aero/Presentation/Commands.hpp>
#include <Aero/Presentation/ObjectTree.hpp>
#include <Aero/Presentation/Brushes.hpp>
#include <Aero/Presentation/Resources.hpp>
#include <Aero/Presentation/Style.hpp>
#include <Aero/Presentation/Transforms.hpp>
#include <Aero/Presentation/VisualTreeMount.hpp>
#include <Aero/BuiltinThemes.generated.hpp>

#include "runtime/RuntimePresentationServices.hpp"
#include "runtime/DataTemplateTriggerContext.hpp"
#include "runtime/ControlRuntimeAccess.hpp"
#include "controls/TextServicesAccess.hpp"
#include "controls/PathServicesAccess.hpp"
#include "integration/RenderEndpointInternal.hpp"
#include "presentation/RenderingInternal.hpp"
#include "render/TextBackendAccess.hpp"

#include <chrono>
#include <cstdio>
#include <new>
#include <utility>

namespace Aero {
namespace {

Base::Status RuntimeInvalidState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

Base::Status RuntimeNotInitialized(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotInitialized, message);
}

Base::Result<Base::ResourceUri> BuiltInThemeUri(
    Base::StringView name) noexcept {
    Base::String text;
    Base::Result<void> assigned = text.TryAssign(
        Base::StringView(
            "pack://application:,,,/Aero.Themes;component/"));
    if (!assigned) return assigned.GetStatus();
    Base::Result<void> appended = text.TryAppend(name);
    if (!appended) return appended.GetStatus();
    return Base::ResourceUri::Parse(text.View());
}

template<class T, class... TArgs>
Base::Result<void> CreateRuntimeObject(
    Base::IAllocator& allocator,
    Base::MemoryTag tag,
    T*& output,
    TArgs&&... arguments) noexcept {
    if (output != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Runtime service is already allocated");
    }
    void* memory = allocator.Allocate({
        sizeof(T), alignof(T), tag});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Runtime service allocation failed");
    }
    output = new (memory) T(
        std::forward<TArgs>(arguments)...);
    return {};
}

template<class T>
void DestroyRuntimeObject(
    Base::IAllocator& allocator,
    Base::MemoryTag tag,
    T*& object) noexcept {
    if (object == nullptr) return;
    object->~T();
    allocator.Deallocate(
        object, sizeof(T), alignof(T), tag);
    object = nullptr;
}

class EndpointRenderBackend final
    : public Presentation::IRenderBackend {
public:
    void SetEndpoint(
        Base::Ref<Integration::RenderEndpoint> endpoint) noexcept {
        endpoint_ = std::move(endpoint);
    }

    void Reset() noexcept {
        endpoint_.Reset();
    }

    Base::Result<void> Submit(
        const Presentation::RenderPlan& plan) noexcept override {
        if (!endpoint_) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "View has no render endpoint");
        }
        return Integration::Detail::RenderEndpointAccess::Submit(
            *endpoint_, plan);
    }

private:
    void* QueryInternalService(
        std::uint64_t service) noexcept override {
        return endpoint_
            ? Integration::Detail::RenderEndpointAccess::
                  QueryInternalService(*endpoint_, service)
            : nullptr;
    }

    Base::Ref<Integration::RenderEndpoint> endpoint_;
};

} // namespace

struct ViewRuntime::Impl final {
    struct FragmentMount final {
        Controls::ContentControl* host = nullptr;
        Markup::LoaderResult document;
        Presentation::MountEdgeState rootEdge;
    };

    Impl(
        Base::IAllocator& value,
        SchemaBundle* sharedSchema = nullptr,
        Markup::DocumentCache* sharedDocumentCache = nullptr) noexcept
        : allocator(&value),
          ownedSchemaBundle(&value),
          schemaBundle(sharedSchema != nullptr
              ? sharedSchema
              : &ownedSchemaBundle),
          usesSharedSchema(sharedSchema != nullptr),
          ownedDocumentCache(&value),
          documentCache(sharedDocumentCache != nullptr
              ? sharedDocumentCache
              : &ownedDocumentCache),
          fragmentMounts(&value),
          storyboardSessions(&value),
          storyboardCompletionSessions(&value),
          storyboardCompletedSubscriptions(&value),
          itemGenerators(&value) {}

    Base::IAllocator* allocator = nullptr;
    Core::Dispatcher dispatcher;
    SchemaBundle ownedSchemaBundle;
    SchemaBundle* schemaBundle = nullptr;
    bool usesSharedSchema = false;
    Markup::DocumentCache ownedDocumentCache;
    Markup::DocumentCache* documentCache = nullptr;
    Core::MetadataDomain* metadata = nullptr;
    ModuleCatalog modules;
    ViewRuntimeOptions options;
    Base::Ref<Integration::RenderEndpoint> endpoint;
    EndpointRenderBackend endpointBackend;
    bool endpointBound = false;
    std::uint64_t endpointGeneration = 0U;

    Core::MetadataRuntime* metadataRuntime = nullptr;
    Core::ObjectServicesScope* objectServices = nullptr;
    Core::EffectiveValueEngine* values = nullptr;
    Presentation::AnimationManager* animations = nullptr;
    Presentation::ObjectTree* tree = nullptr;
    Presentation::LayoutManager* layout = nullptr;
    Presentation::RenderManager* renderer = nullptr;
    Detail::ImageRuntime* imageRuntime = nullptr;
    Detail::TextRuntime* textRuntime = nullptr;
    Presentation::BindingManager* bindings = nullptr;
    Presentation::RoutedEventManager* events = nullptr;
    Presentation::CommandManager* commands = nullptr;
    Controls::TemplateManager* templates = nullptr;
    Controls::VisualStateManager* visualStates = nullptr;
    Presentation::StyleManager* styles = nullptr;
    Detail::RuntimePresentationServices presentationServices;

    Markup::Schema* schema = nullptr;
    Presentation::VisualTreeMount* visualMount = nullptr;
    Markup::SourceProviderRegistry xamlSources;
    Markup::EmbeddedSourceProvider embeddedXaml;
    Markup::FileSourceProvider fileXaml;
    Presentation::ResourceDictionary applicationResources;
    Presentation::ResourceDictionary themeResources;
    Presentation::ResourceDictionary systemResources;
    Presentation::ResourceDictionary dynamicResourceEnvironment;

    Presentation::HitTestManager hitTests;
    Presentation::FocusManager* focus = nullptr;
    Presentation::PointerInputManager* pointer = nullptr;
    Presentation::KeyboardInputManager* keyboard = nullptr;
    Presentation::TextInputManager* textInput = nullptr;
    Controls::ControlInteractionManager* controlInteractions = nullptr;
    Controls::TextBoxInteractionManager* textBoxInteractions = nullptr;
    struct StoryboardSession final {
        explicit StoryboardSession(
            Base::IAllocator* allocator) noexcept
            : handles(allocator) {}

        Presentation::FrameworkElement* owner = nullptr;
        Base::String name;
        Base::Vector<
            Presentation::AnimationHandle>
            handles;
    };
    Base::Vector<StoryboardSession>
        storyboardSessions;
    struct StoryboardCompletionSession final {
        explicit StoryboardCompletionSession(
            Base::IAllocator* allocator) noexcept
            : handles(allocator) {}

        Base::Ref<Animation::Storyboard> storyboard;
        Presentation::FrameworkElement* owner = nullptr;
        Base::Vector<
            Presentation::AnimationHandle>
            handles;
    };
    struct StoryboardCompletedSubscription final {
        Animation::StoryboardCompletedTrigger* trigger =
            nullptr;
        Presentation::FrameworkElement* owner = nullptr;
        const Presentation::NameScope* names = nullptr;
    };
    Base::Vector<StoryboardCompletionSession>
        storyboardCompletionSessions;
    Base::Vector<StoryboardCompletedSubscription>
        storyboardCompletedSubscriptions;
    Base::Result<void> ExecuteAnimationAction(
        Animation::TriggerAction& action,
        Presentation::FrameworkElement& owner,
        Detail::DataTemplateTriggerContext*
            dataTemplateContext = nullptr,
        const Presentation::NameScope* names = nullptr) noexcept;
    void CancelStoryboardCompletionSessions(
        Base::Span<const Presentation::AnimationHandle>
            handles) noexcept;
    Base::Result<std::uint32_t>
    ProcessStoryboardCompletions() noexcept;
    static Base::Result<void> ExecuteStyleTriggerActions(
        Core::DependencyObject& owner,
        Base::Span<const Base::Ref<Base::Object>>
            actions,
        void* context) noexcept {
        auto* runtime = static_cast<Impl*>(context);
        if (runtime == nullptr ||
            !runtime->metadata->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Presentation::FrameworkElement::
                    StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Style Trigger action owner is not a FrameworkElement");
        }
        auto& element =
            static_cast<Presentation::FrameworkElement&>(
                owner);
        for (const Base::Ref<Base::Object>& authored :
             actions) {
            if (!authored ||
                !runtime->metadata->Types().IsDerivedFrom(
                    authored->RuntimeType(),
                    Animation::TriggerAction::
                        StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Style Trigger contains an invalid action");
            }
            Base::Result<void> executed =
                runtime->ExecuteAnimationAction(
                    static_cast<Animation::TriggerAction&>(
                        *authored),
                    element);
            if (!executed) return executed.GetStatus();
        }
        return {};
    }
    struct AnimationEventContext final {
        Impl* runtime = nullptr;
        Animation::EventTrigger* trigger = nullptr;
        Presentation::FrameworkElement* owner = nullptr;
        const Presentation::NameScope* names = nullptr;

        Base::Result<bool> EvaluateComparison(
            const Animation::ComparisonCondition& condition) noexcept {
            const Base::Ref<Presentation::BindingSpec> binding =
                condition.LeftOperand();
            if (!binding || runtime == nullptr ||
                runtime->metadataRuntime == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "ConditionBehavior requires a bound left operand");
            }
            if (binding->ElementName().Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "ConditionBehavior currently requires Binding ElementName");
            }
            Base::Object* source = names != nullptr
                ? names->Find(binding->ElementName())
                : runtime->loadedDocument.names.Find(
                      binding->ElementName());
            if (source == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "ConditionBehavior Binding ElementName was not found");
            }
            Base::Result<Core::BindingPathPlan> plan =
                Core::BindingPathPlan::Compile(
                    *runtime->metadataRuntime,
                    source->RuntimeType(), binding->Path());
            if (!plan) return plan.GetStatus();
            Base::Result<Core::PropertyValue> current =
                plan.Value().Get(*runtime->metadataRuntime, *source);
            if (!current) return current.GetStatus();
            Core::PropertyValue expected = condition.RightOperand();
            if (expected.IsNullObject()) {
                return current.Value().IsNullObject();
            }
            if (expected.Kind() == Core::ValueKind::String &&
                expected.Type() != current.Value().Type()) {
                Base::Result<Core::PropertyValue> converted =
                    Core::PropertyValue::TryFromString(
                        current.Value().Type(), expected.AsString());
                // WPF-style conditions simply do not match when their two
                // operands cannot be converted to a comparable type.
                if (!converted) return false;
                expected = std::move(converted).Value();
            }
            const auto comparison = condition.ComparisonOperator();
            if (comparison ==
                Animation::ComparisonCondition::Operator::Equal) {
                return current.Value().Equals(expected);
            }
            if (comparison ==
                Animation::ComparisonCondition::Operator::NotEqual) {
                return !current.Value().Equals(expected);
            }

            const auto isNumeric = [](Core::ValueKind kind) noexcept {
                return kind == Core::ValueKind::SignedInteger ||
                    kind == Core::ValueKind::UnsignedInteger ||
                    kind == Core::ValueKind::Double;
            };
            const auto numericValue = [](const Core::PropertyValue& value) noexcept {
                switch (value.Kind()) {
                case Core::ValueKind::SignedInteger:
                    return static_cast<long double>(value.AsSignedInteger());
                case Core::ValueKind::UnsignedInteger:
                    return static_cast<long double>(value.AsUnsignedInteger());
                case Core::ValueKind::Double:
                    return static_cast<long double>(value.AsDouble());
                default:
                    return 0.0L;
                }
            };
            if (isNumeric(current.Value().Kind()) && isNumeric(expected.Kind())) {
                const long double left = numericValue(current.Value());
                const long double right = numericValue(expected);
                switch (comparison) {
                case Animation::ComparisonCondition::Operator::LessThan:
                    return left < right;
                case Animation::ComparisonCondition::Operator::LessThanOrEqual:
                    return left <= right;
                case Animation::ComparisonCondition::Operator::GreaterThan:
                    return left > right;
                case Animation::ComparisonCondition::Operator::GreaterThanOrEqual:
                    return left >= right;
                default:
                    break;
                }
            }
            if (current.Value().Kind() == Core::ValueKind::String &&
                expected.Kind() == Core::ValueKind::String) {
                const int result = current.Value().AsString().Compare(
                    expected.AsString());
                switch (comparison) {
                case Animation::ComparisonCondition::Operator::LessThan:
                    return result < 0;
                case Animation::ComparisonCondition::Operator::LessThanOrEqual:
                    return result <= 0;
                case Animation::ComparisonCondition::Operator::GreaterThan:
                    return result > 0;
                case Animation::ComparisonCondition::Operator::GreaterThanOrEqual:
                    return result >= 0;
                default:
                    break;
                }
            }
            return false;
        }

        Base::Result<bool> BehaviorsAllowExecution() noexcept {
            for (const Base::Ref<Base::Object>& behavior :
                 trigger->Behaviors()) {
                if (!behavior) continue;
                if (behavior->RuntimeType() !=
                    Animation::ConditionBehavior::StaticTypeId()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::Unsupported,
                        "EventTrigger contains an unsupported behavior");
                }
                const Base::Ref<Animation::ConditionalExpression> expression =
                    static_cast<Animation::ConditionBehavior&>(*behavior).Expression();
                if (!expression) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidState,
                        "ConditionBehavior has no expression");
                }
                bool expressionResult = false;
                for (const Base::Ref<Animation::ComparisonCondition>& condition :
                     expression->Conditions()) {
                    if (!condition) continue;
                    Base::Result<bool> matches = EvaluateComparison(*condition);
                    if (!matches) return matches.GetStatus();
                    expressionResult = matches.Value();
                    if (!expressionResult && expression->Chaining() ==
                        Animation::ConditionalExpression::ForwardChaining::And) {
                        return false;
                    }
                    if (expressionResult && expression->Chaining() ==
                        Animation::ConditionalExpression::ForwardChaining::Or) {
                        break;
                    }
                }
                if (!expressionResult) return false;
            }
            return true;
        }

        void Invoke(
            Base::Object*,
            const Presentation::RoutedEventArgs&) noexcept {
            if (runtime == nullptr || trigger == nullptr ||
                owner == nullptr ||
                !runtime->animationEventStatus.IsOk()) {
                return;
            }
            Base::Result<bool> allowed = BehaviorsAllowExecution();
            if (!allowed) {
                runtime->animationEventStatus = allowed.GetStatus();
                return;
            }
            if (!allowed.Value()) return;
            for (const Base::Ref<Animation::TriggerAction>& action :
                 trigger->Actions()) {
                if (!action) continue;
                Base::Result<void> executed =
                    runtime->ExecuteAnimationAction(
                        *action, *owner, nullptr, names);
                if (!executed) {
                    runtime->animationEventStatus =
                        executed.GetStatus();
                    return;
                }
            }
        }
    };
    struct AnimationEventSubscription final {
        Presentation::UIElement* owner = nullptr;
        Core::RoutedEventHandle event;
        Presentation::RoutedEventHandler handler;
        AnimationEventContext* context = nullptr;
    };
    Base::Vector<AnimationEventSubscription>
        animationEventSubscriptions;
    struct DataTemplateTriggerHandlerContext final {
        Impl* runtime = nullptr;
        Base::Ref<
            Detail::DataTemplateTriggerContext>
            triggerContext;
        std::uint32_t triggerIndex = 0U;
        std::uint32_t conditionIndex = 0U;

        void Invoke(
            Core::DependencyObject&,
            const Core::
                DependencyPropertyChangedEventArgs&)
            noexcept;
    };
    struct DataTemplateTriggerSubscription final {
        Core::DependencyObject* source = nullptr;
        Core::DependencyPropertyHandle property;
        Core::DependencyPropertyChangedEventHandler
            handler;
        DataTemplateTriggerHandlerContext* context =
            nullptr;
    };
    Base::Vector<DataTemplateTriggerSubscription>
        dataTemplateTriggerSubscriptions;
    Base::Status animationEventStatus;
    Controls::ScrollInteractionManager* scrollInteractions = nullptr;
    Controls::SliderInteractionManager* sliderInteractions = nullptr;
    Controls::ListBoxInteractionManager* listBoxInteractions = nullptr;
    Controls::ComboBoxInteractionManager* comboBoxInteractions = nullptr;
    Controls::TreeViewInteractionManager* treeViewInteractions = nullptr;
    Controls::MenuInteractionManager* menuInteractions = nullptr;
    Base::Vector<Controls::ItemContainerGenerator*>
        itemGenerators;
    Base::Vector<Presentation::VisualHandle>
        pendingGeneratedPresentation;
    Base::Vector<Presentation::FrameworkElement*>
        renderOverlays;
    Base::Vector<Presentation::UIElement*>
        inputOverlays;
    Base::Vector<Presentation::Point>
        overlayOrigins;
    Base::Ref<Controls::ToolTip>
        pendingToolTip;
    Base::Ref<Controls::ToolTip>
        activeToolTip;
    Base::Ref<Presentation::UIElement>
        toolTipTarget;
    Base::Ref<Presentation::UIElement>
        overlayFocusReturn;
    std::uint32_t toolTipElapsed = 0U;
    std::uint32_t toolTipVisibleElapsed = 0U;

    Markup::LoaderResult loadedDocument;
    Base::Vector<FragmentMount> fragmentMounts;
    Markup::LoadContext loadContext;
    Base::Ref<Markup::EffectLifetime> effectLifetime;
    Base::Ref<Base::Object> root;
    std::uint64_t frameNumber = 0U;
    bool traceEndpointFrame = false;
    bool initialized = false;
    bool mounted = false;
    bool terminal = false;

    Presentation::IRenderBackend& SelectedBackend() noexcept {
        return endpointBackend;
    }

    Base::Result<void> EnsureDefaultXamlProviders() noexcept {
        Base::Result<Base::ResourceUri> light =
            BuiltInThemeUri(Base::StringView("Light.xaml"));
        if (!light) return light.GetStatus();
        Base::Result<void> status = embeddedXaml.TryAdd(
            light.Value(),
            {Detail::AeroThemeLightSource,
             static_cast<std::uint32_t>(
                 sizeof(Detail::AeroThemeLightSource))});
        if (!status) return status.GetStatus();
        Base::Result<Base::ResourceUri> dark =
            BuiltInThemeUri(Base::StringView("Dark.xaml"));
        if (!dark) return dark.GetStatus();
        status = embeddedXaml.TryAdd(
            dark.Value(),
            {Detail::AeroThemeDarkSource,
             static_cast<std::uint32_t>(
                 sizeof(Detail::AeroThemeDarkSource))});
        if (!status) return status.GetStatus();
        Base::Result<Base::ResourceUri> generic =
            BuiltInThemeUri(Base::StringView("Generic.xaml"));
        if (!generic) return generic.GetStatus();
        status = embeddedXaml.TryAdd(
            generic.Value(),
            {Detail::AeroThemeGenericSource,
             static_cast<std::uint32_t>(
                 sizeof(Detail::AeroThemeGenericSource))});
        if (!status) return status.GetStatus();

        status = xamlSources.TryRegister(
                embeddedXaml, Base::StringView("pack"));
        if (!status &&
            status.GetStatus().code !=
                Base::ErrorCode::AlreadyExists) {
            return status.GetStatus();
        }
        status = xamlSources.TryRegister(
            fileXaml, Base::StringView("file"));
        if (!status &&
            status.GetStatus().code !=
                Base::ErrorCode::AlreadyExists) {
            return status.GetStatus();
        }
        status = xamlSources.TryRegister(fileXaml);
        if (!status &&
            status.GetStatus().code !=
                Base::ErrorCode::AlreadyExists) {
            return status.GetStatus();
        }
        return {};
    }

    Base::Result<void> BeginDocumentLoad() noexcept {
        if (!initialized) {
            return RuntimeNotInitialized(
                "ViewRuntime must be initialized before XAML loading");
        }
        if (mounted || root || loadedDocument.root) {
            return RuntimeInvalidState(
                "ViewRuntime already owns a loaded document");
        }
        return {};
    }

    Base::Result<Markup::LoadOptions> LoadOptions(
        bool deferredEffects = false) noexcept {
        Markup::LoadOptions result;
        loadContext.resources = &dynamicResourceEnvironment;
        loadContext.effectiveValues = values;
        loadContext.bindings = bindings;
        loadContext.fallbackResources =
            &dynamicResourceEnvironment;
        loadContext.documentCache = documentCache;
        loadContext.dispatcher = &dispatcher;
        loadContext.dependencyProperties =
            &Core::Detail::MetadataDomainAccess::
                DependencyProperties(*metadata);
        loadContext.effectLifetime = effectLifetime;
        loadContext.effectCommitMode = deferredEffects
            ? Markup::EffectCommitMode::Deferred
            : Markup::EffectCommitMode::Immediate;
        Markup::Detail::LoadOptionsAccess::SetContext(
            result, &loadContext);
        return result;
    }

    void AttachTextService(
        Presentation::Visual& node,
        Controls::Detail::TextLayoutService* service,
        bool invalidate = false) noexcept {
        if (metadata == nullptr) return;
        const Core::TypeId type = node.RuntimeType();
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::TextBlock::StaticTypeId())) {
            Controls::Detail::TextServicesAccess::Attach(
                *static_cast<Controls::TextBlock*>(&node),
                service,
                invalidate);
        }
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::TextBox::StaticTypeId())) {
            Controls::Detail::TextServicesAccess::Attach(
                *static_cast<Controls::TextBox*>(&node),
                service,
                invalidate);
        }
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::PasswordBox::
                    StaticTypeId())) {
            Controls::Detail::TextServicesAccess::Attach(
                *static_cast<Controls::PasswordBox*>(
                    &node),
                service,
                invalidate);
        }
    }

    Aero::Detail::MeshBackendServices*
    MeshServices() noexcept {
        return Render::Detail::RenderBackendAccess::
            MeshServices(SelectedBackend());
    }

    Aero::Detail::ImageBackendServices*
    ImageServices() noexcept {
        return Render::Detail::RenderBackendAccess::
            ImageServices(SelectedBackend());
    }

    void AttachPathService(
        Presentation::Visual& node,
        Aero::Detail::MeshBackendServices* service,
        bool invalidate = false) noexcept {
        if (metadata == nullptr) return;
        const Core::TypeId type = node.RuntimeType();
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::Path::StaticTypeId())) {
            Controls::Detail::PathServicesAccess::Attach(
                *static_cast<Controls::Path*>(&node),
                service,
                invalidate);
        }
    }

    void VisitTextServices(
        Presentation::Visual* rootVisual,
        Controls::Detail::TextLayoutService* service,
        bool invalidate = false,
        bool ancestorsVisible = true) noexcept {
        if (rootVisual == nullptr) return;
        bool effectivelyVisible = ancestorsVisible;
        if (Presentation::UIElement* element =
                rootVisual->AsUIElement();
            element != nullptr) {
            effectivelyVisible =
                ancestorsVisible &&
                element->GetVisibility() ==
                    Presentation::Visibility::Visible;
        }
        AttachTextService(
            *rootVisual,
            service,
            invalidate && effectivelyVisible);
        for (Presentation::Visual* child :
             rootVisual->VisualChildren()) {
            VisitTextServices(
                child,
                service,
                invalidate,
                effectivelyVisible);
        }
    }

    void VisitPathServices(
        Presentation::Visual* rootVisual,
        Aero::Detail::MeshBackendServices* service,
        bool invalidate = false,
        bool ancestorsVisible = true) noexcept {
        if (rootVisual == nullptr) return;
        bool effectivelyVisible = ancestorsVisible;
        if (Presentation::UIElement* element =
                rootVisual->AsUIElement();
            element != nullptr) {
            effectivelyVisible =
                ancestorsVisible &&
                element->GetVisibility() ==
                    Presentation::Visibility::Visible;
        }
        AttachPathService(
            *rootVisual,
            service,
            invalidate && effectivelyVisible);
        for (Presentation::Visual* child :
             rootVisual->VisualChildren()) {
            VisitPathServices(
                child,
                service,
                invalidate,
                effectivelyVisible);
        }
    }

    static void TextLifecycleHook(
        const Presentation::ObjectTreeLifecycleEvent& event,
        void* context) noexcept {
        auto* runtime = static_cast<Impl*>(context);
        if (runtime == nullptr || event.node == nullptr) {
            return;
        }
        runtime->AttachTextService(
            *event.node,
            event.loaded && runtime->textRuntime != nullptr
                ? runtime->textRuntime->Service()
                : nullptr);
        runtime->AttachPathService(
            *event.node,
            event.loaded
                ? runtime->MeshServices()
                : nullptr);
    }

    void ClearLoadedDocument() noexcept {
        loadedDocument.Clear();
    }

    Presentation::ResourceEnvironment ResourceEnvironment() const noexcept {
        return {
            &applicationResources,
            &themeResources,
            &systemResources};
    }

    Base::Result<Presentation::ResourceDictionary*>
    ResolveResourceLayer(
        RuntimeResourceLayer layer) noexcept {
        switch (layer) {
        case RuntimeResourceLayer::Application:
            return &applicationResources;
        case RuntimeResourceLayer::Theme:
            return &themeResources;
        case RuntimeResourceLayer::System:
            return &systemResources;
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ViewRuntime resource layer is invalid");
    }

    Base::Result<void> RebuildDynamicResourceEnvironment() noexcept {
        Base::Result<void> rebuilt =
            dynamicResourceEnvironment.Clear();
        if (rebuilt) {
            rebuilt =
                dynamicResourceEnvironment.TryAddMerged(
                    systemResources);
        }
        if (rebuilt) {
            rebuilt =
                dynamicResourceEnvironment.TryAddMerged(
                    themeResources);
        }
        if (rebuilt) {
            rebuilt =
                dynamicResourceEnvironment.TryAddMerged(
                    applicationResources);
        }
        return rebuilt;
    }

    void DetachRuntimePresentation() noexcept {
        presentationServices.Detach(
            RootVisual(),
            {loadedDocument.visualContent.nodes.Data(),
             loadedDocument.visualContent.nodes.Size()});
    }

    Presentation::Visual* RootVisual() noexcept {
        if (!root) return nullptr;
        if (!metadata->Types().IsDerivedFrom(
                root->RuntimeType(),
                Presentation::Visual::StaticTypeId())) {
            return nullptr;
        }
        return static_cast<Presentation::Visual*>(root.Get());
    }

    Base::Result<void> SynchronizeOverlays() noexcept {
        renderOverlays.Clear();
        inputOverlays.Clear();
        overlayOrigins.Clear();
        Presentation::Visual* rootVisual =
            RootVisual();
        if (rootVisual == nullptr ||
            renderer == nullptr) {
            hitTests.ClearOverlays();
            return {};
        }
        Base::Vector<Presentation::Visual*> stack(
            allocator);
        Base::Result<void> appended =
            stack.TryPushBack(rootVisual);
        if (!appended) return appended.GetStatus();
        while (!stack.Empty()) {
            Presentation::Visual* node =
                stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;
            const Core::TypeId type =
                node->RuntimeType();
            bool open = false;
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::Popup::
                        StaticTypeId())) {
                open =
                    static_cast<Controls::Popup*>(
                        node)->IsOpen();
            } else if (
                metadata->Types().IsDerivedFrom(
                    type,
                    Controls::ContextMenu::
                        StaticTypeId())) {
                open =
                    static_cast<
                        Controls::ContextMenu*>(
                        node)->IsOpen();
            }
            if (open) {
                Presentation::Visual* ancestor =
                    node;
                while (ancestor != nullptr) {
                    Presentation::UIElement*
                        element =
                            ancestor->AsUIElement();
                    if (element != nullptr &&
                        !element->IsVisible()) {
                        open = false;
                        break;
                    }
                    ancestor =
                        ancestor->VisualParent();
                }
            }
            if (open) {
                Presentation::FrameworkElement*
                    framework =
                        node->AsFrameworkElement();
                Presentation::UIElement* input =
                    node->AsUIElement();
                if (framework != nullptr &&
                    input != nullptr) {
                    auto rootOrigin = [](
                        Presentation::UIElement&
                            element) noexcept {
                        Presentation::Point
                            result{};
                        Presentation::Visual*
                            current = &element;
                        while (current != nullptr) {
                            Presentation::UIElement*
                                currentElement =
                                    current->
                                        AsUIElement();
                            if (currentElement !=
                                nullptr) {
                                Presentation::
                                    FrameworkElement*
                                    currentFramework =
                                        currentElement->
                                            AsFrameworkElement();
                                if (currentFramework !=
                                    nullptr) {
                                    result =
                                        Presentation::
                                            TransformPoint(
                                                currentFramework->
                                                    LocalVisualTransform(),
                                                result);
                                }
                                const Presentation::
                                    Rect slot =
                                        currentElement->
                                            LayoutSlot();
                                result.x += slot.x;
                                result.y += slot.y;
                            }
                            current =
                                current->
                                    VisualParent();
                        }
                        return result;
                    };
                    Presentation::Point origin =
                        rootOrigin(*input);
                    if (metadata->Types().
                            IsDerivedFrom(
                                type,
                                Controls::
                                    ContextMenu::
                                        StaticTypeId())) {
                        Base::Ref<
                            Presentation::UIElement>
                            target =
                                static_cast<
                                    Controls::
                                        ContextMenu*>(
                                    node)->
                                    PlacementTarget();
                        if (target &&
                            target->
                                IsArrangeValid()) {
                            origin =
                                rootOrigin(*target);
                            origin.y +=
                                target->
                                    RenderSize().
                                        height;
                        }
                    }
                    appended =
                        renderOverlays.TryPushBack(
                            framework);
                    if (!appended) {
                        return appended.GetStatus();
                    }
                    appended =
                        overlayOrigins.TryPushBack(
                            origin);
                    if (!appended) {
                        return appended.GetStatus();
                    }
                    appended =
                        inputOverlays.TryPushBack(
                            input);
                    if (!appended) {
                        return appended.GetStatus();
                    }
                }
            }
            const Base::Span<
                Presentation::Visual* const>
                children =
                    node->VisualChildren();
            for (std::uint32_t index =
                     children.Size();
                 index > 0U;
                 --index) {
                appended =
                    stack.TryPushBack(
                        children[index - 1U]);
                if (!appended) {
                    return appended.GetStatus();
                }
            }
        }
        Base::Result<void> render =
            renderer->SetOverlays(
                renderOverlays.AsSpan(),
                overlayOrigins.AsSpan());
        if (!render) return render.GetStatus();
        return hitTests.SetOverlays(
            inputOverlays.AsSpan(),
            overlayOrigins.AsSpan());
    }

    void ClearOverlays() noexcept {
        hitTests.ClearOverlays();
        renderOverlays.Clear();
        inputOverlays.Clear();
        overlayOrigins.Clear();
        if (renderer != nullptr) {
            static_cast<void>(
                renderer->SetOverlays(
                    {}, {}));
        }
    }

    void CloseAllOverlays() noexcept {
        for (Presentation::UIElement* overlay :
             inputOverlays) {
            if (overlay == nullptr) continue;
            const Core::TypeId type =
                overlay->RuntimeType();
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::Popup::
                        StaticTypeId())) {
                auto* popup =
                    static_cast<Controls::Popup*>(
                        overlay);
                static_cast<void>(
                    popup->SetIsOpen(false));
                static_cast<void>(
                    popup->SetPlacementTarget({}));
            } else if (
                metadata->Types().IsDerivedFrom(
                    type,
                    Controls::ContextMenu::
                        StaticTypeId())) {
                auto* menu =
                    static_cast<
                        Controls::ContextMenu*>(
                        overlay);
                static_cast<void>(
                    menu->SetIsOpen(false));
                static_cast<void>(
                    menu->SetPlacementTarget({}));
            }
        }
    }

    static bool IsVisualDescendantOrSelf(
        const Presentation::Visual& root,
        const Presentation::Visual& target)
        noexcept {
        const Presentation::Visual* current =
            &target;
        while (current != &root) {
            current = current->VisualParent();
            if (current == nullptr) return false;
        }
        return true;
    }

    Base::Result<void> RestoreOverlayFocus()
        noexcept {
        if (!overlayFocusReturn ||
            focus == nullptr) {
            overlayFocusReturn.Reset();
            return {};
        }
        Base::Ref<Presentation::UIElement>
            target =
                std::move(overlayFocusReturn);
        Base::Result<bool> restored =
            focus->SetFocus(target.Get());
        if (!restored &&
            restored.GetStatus().code !=
                Base::ErrorCode::NotFound &&
            restored.GetStatus().code !=
                Base::ErrorCode::InvalidState) {
            return restored.GetStatus();
        }
        return {};
    }

    Base::Result<void> DismissOverlaysForPointer(
        const Presentation::PointerInput& input,
        Presentation::UIElement* target)
        noexcept {
        if (input.action !=
                Presentation::PointerAction::Down ||
            inputOverlays.Empty()) {
            return {};
        }
        bool closedFocusedOverlay = false;
        for (std::uint32_t index =
                 inputOverlays.Size();
             index > 0U;
             --index) {
            Presentation::UIElement* overlay =
                inputOverlays[index - 1U];
            if (overlay == nullptr) continue;
            if (target != nullptr &&
                IsVisualDescendantOrSelf(
                    *overlay, *target)) {
                return {};
            }
            const Core::TypeId type =
                overlay->RuntimeType();
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::Popup::
                        StaticTypeId())) {
                auto* popup =
                    static_cast<Controls::Popup*>(
                        overlay);
                if (!popup->StaysOpen()) {
                    Base::Result<void> closed =
                        popup->SetIsOpen(false);
                    if (!closed) {
                        return closed.GetStatus();
                    }
                    static_cast<void>(
                        popup->SetPlacementTarget(
                            {}));
                }
            } else if (
                metadata->Types().IsDerivedFrom(
                    type,
                    Controls::ContextMenu::
                        StaticTypeId())) {
                Base::Result<void> closed =
                    static_cast<
                        Controls::ContextMenu*>(
                        overlay)->SetIsOpen(false);
                if (!closed) {
                    return closed.GetStatus();
                }
                closedFocusedOverlay = true;
                static_cast<void>(
                    static_cast<
                        Controls::ContextMenu*>(
                        overlay)->
                        SetPlacementTarget({}));
            }
        }
        return closedFocusedOverlay
            ? RestoreOverlayFocus()
            : Base::Result<void>();
    }

    Base::Result<bool> DismissTopOverlayForEscape()
        noexcept {
        for (std::uint32_t index =
                 inputOverlays.Size();
             index > 0U;
             --index) {
            Presentation::UIElement* overlay =
                inputOverlays[index - 1U];
            if (overlay == nullptr) continue;
            const Core::TypeId type =
                overlay->RuntimeType();
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::Popup::
                        StaticTypeId())) {
                Base::Result<void> closed =
                    static_cast<Controls::Popup*>(
                        overlay)->SetIsOpen(false);
                if (!closed) {
                    return closed.GetStatus();
                }
                static_cast<void>(
                    static_cast<Controls::Popup*>(
                        overlay)->
                        SetPlacementTarget({}));
                Base::Result<void> restored =
                    RestoreOverlayFocus();
                return restored
                    ? Base::Result<bool>(true)
                    : Base::Result<bool>(
                          restored.GetStatus());
            }
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::ContextMenu::
                        StaticTypeId())) {
                Base::Result<void> closed =
                    static_cast<
                        Controls::ContextMenu*>(
                        overlay)->SetIsOpen(false);
                if (!closed) {
                    return closed.GetStatus();
                }
                static_cast<void>(
                    static_cast<
                        Controls::ContextMenu*>(
                        overlay)->
                        SetPlacementTarget({}));
                Base::Result<void> restored =
                    RestoreOverlayFocus();
                return restored
                    ? Base::Result<bool>(true)
                    : Base::Result<bool>(
                          restored.GetStatus());
            }
        }
        return false;
    }

    Base::Result<void> OpenContextMenuForPointer(
        const Presentation::PointerInput& input,
        Presentation::UIElement* hitTarget)
        noexcept {
        if (input.action !=
                Presentation::PointerAction::Down ||
            input.changedButton !=
                Presentation::MouseButton::Right) {
            return {};
        }
        Presentation::Visual* current =
            hitTarget;
        while (current != nullptr) {
            Presentation::UIElement* element =
                current->AsUIElement();
            if (element != nullptr) {
                Base::Ref<Controls::ContextMenu>
                    menu =
                        Controls::
                            ContextMenuService::
                                GetContextMenu(
                                    *element);
                if (menu) {
                    if (focus != nullptr &&
                        !overlayFocusReturn) {
                        Presentation::UIElement*
                            focused =
                                focus->
                                    FocusedNode();
                        if (focused != nullptr) {
                            overlayFocusReturn =
                                Base::Ref<
                                    Presentation::
                                        UIElement>::
                                    TryFromBorrowed(
                                        *focused);
                        }
                    }
                    Base::Ref<
                        Presentation::UIElement>
                        target =
                            Base::Ref<
                                Presentation::
                                    UIElement>::
                                TryFromBorrowed(
                                    *element);
                    if (target) {
                        Base::Result<void>
                            placed =
                                menu->
                                    SetPlacementTarget(
                                        std::move(
                                            target));
                        if (!placed) {
                            return placed.GetStatus();
                        }
                    }
                    Base::Result<void> opened =
                        menu->SetIsOpen(true);
                    if (!opened) {
                        return opened.GetStatus();
                    }
                    if (focus != nullptr) {
                        Base::Result<bool> focused =
                            focus->SetFocus(
                                menu.Get());
                        if (!focused) {
                            static_cast<void>(
                                menu->
                                    SetIsOpen(
                                        false));
                            return focused.GetStatus();
                        }
                    }
                    return {};
                }
            }
            current = current->VisualParent();
        }
        return {};
    }

    Base::Result<void> UpdateToolTipForPointer(
        const Presentation::PointerInput& input,
        Presentation::UIElement* hitTarget)
        noexcept {
        if (input.action ==
                Presentation::PointerAction::Down) {
            if (activeToolTip) {
                Base::Result<void> closed =
                    activeToolTip->SetIsOpen(false);
                if (!closed) return closed.GetStatus();
                static_cast<void>(
                    activeToolTip->
                        SetPlacementTarget({}));
            }
            pendingToolTip.Reset();
            activeToolTip.Reset();
            toolTipTarget.Reset();
            toolTipElapsed = 0U;
            toolTipVisibleElapsed = 0U;
            return {};
        }
        if (input.action !=
            Presentation::PointerAction::Move) {
            return {};
        }
        Base::Ref<Controls::ToolTip> next;
        Base::Ref<Presentation::UIElement>
            nextTarget;
        Presentation::Visual* current =
            hitTarget;
        while (current != nullptr) {
            Presentation::UIElement* element =
                current->AsUIElement();
            if (element != nullptr) {
                next =
                    Controls::ToolTipService::
                        GetToolTip(*element);
                if (next) {
                    nextTarget =
                        Base::Ref<
                            Presentation::UIElement>::
                            TryFromBorrowed(
                                *element);
                    break;
                }
            }
            current = current->VisualParent();
        }
        if (next.Get() == pendingToolTip.Get() &&
            nextTarget.Get() == toolTipTarget.Get()) {
            return {};
        }
        if (activeToolTip) {
            Base::Result<void> closed =
                activeToolTip->SetIsOpen(false);
            if (!closed) return closed.GetStatus();
            static_cast<void>(
                activeToolTip->
                    SetPlacementTarget({}));
        }
        pendingToolTip = std::move(next);
        activeToolTip.Reset();
        toolTipTarget = std::move(nextTarget);
        toolTipElapsed = 0U;
        toolTipVisibleElapsed = 0U;
        if (pendingToolTip && toolTipTarget) {
            return pendingToolTip->
                SetPlacementTarget(toolTipTarget);
        }
        return {};
    }

    Base::Result<std::uint32_t>
    AdvanceToolTipTime(
        std::uint32_t elapsedMilliseconds)
        noexcept {
        if (!pendingToolTip ||
            !toolTipTarget) {
            return 0U;
        }
        if (!activeToolTip) {
            const std::uint32_t delay =
                Controls::ToolTipService::
                    InitialShowDelay(
                        *toolTipTarget);
            toolTipElapsed =
                elapsedMilliseconds >
                        UINT32_MAX -
                            toolTipElapsed
                    ? UINT32_MAX
                    : toolTipElapsed +
                        elapsedMilliseconds;
            if (toolTipElapsed < delay) {
                return 0U;
            }
            Base::Result<void> opened =
                pendingToolTip->
                    SetIsOpen(true);
            if (!opened) {
                return opened.GetStatus();
            }
            activeToolTip = pendingToolTip;
            toolTipVisibleElapsed = 0U;
            return 1U;
        }
        toolTipVisibleElapsed =
            elapsedMilliseconds >
                    UINT32_MAX -
                        toolTipVisibleElapsed
                ? UINT32_MAX
                : toolTipVisibleElapsed +
                    elapsedMilliseconds;
        const std::uint32_t duration =
            Controls::ToolTipService::
                ShowDuration(*toolTipTarget);
        if (toolTipVisibleElapsed < duration) {
            return 0U;
        }
        Base::Result<void> closed =
            activeToolTip->SetIsOpen(false);
        if (!closed) return closed.GetStatus();
        static_cast<void>(
            activeToolTip->SetPlacementTarget({}));
        pendingToolTip.Reset();
        activeToolTip.Reset();
        toolTipTarget.Reset();
        overlayFocusReturn.Reset();
        toolTipElapsed = 0U;
        toolTipVisibleElapsed = 0U;
        return 1U;
    }

    Base::Result<Presentation::Visual*> ResolveVisual(
        Base::Object& object, Core::TypeId type) noexcept {
        if (object.RuntimeType() != type ||
            !metadata->Types().IsDerivedFrom(
                type, Presentation::Visual::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "ViewRuntime root is not a registered Visual");
        }
        return static_cast<Presentation::Visual*>(&object);
    }

    Base::Result<Presentation::UIElement*> ResolveUIElement(
        Base::Object& object, Core::TypeId type) noexcept {
        Base::Result<Presentation::Visual*> visual =
            ResolveVisual(object, type);
        if (!visual) return visual.GetStatus();
        Presentation::UIElement* element =
            visual.Value()->AsUIElement();
        if (element == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "ViewRuntime root is not a UIElement");
        }
        return element;
    }

    Presentation::FrameworkElement* ResolveFrameworkElement(
        Base::Object& object, Core::TypeId type) noexcept {
        Base::Result<Presentation::Visual*> visual =
            ResolveVisual(object, type);
        return visual ? visual.Value()->AsFrameworkElement() : nullptr;
    }

    Base::Result<void> CreateTemplateServices() noexcept {
        Base::Result<void> status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            templates, *tree, *values,
            Core::Detail::MetadataDomainAccess::
                DependencyProperties(*metadata),
            layout, renderer, metadataRuntime, bindings);
        if (!status) return status.GetStatus();
        status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            visualStates, *values, *templates,
            *animations,
            Core::Detail::MetadataDomainAccess::
                DependencyProperties(*metadata));
        if (!status) return status.GetStatus();
        status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            styles, *values,
            Core::Detail::MetadataDomainAccess::
                DependencyProperties(*metadata));
        if (!status) return status.GetStatus();
        styles->SetTriggerActionHandler(
            &Impl::ExecuteStyleTriggerActions, this);
        presentationServices.Configure(
            *allocator,
            *metadata,
            *values,
            *bindings,
            *styles,
            *templates,
            *visualStates,
            ResourceEnvironment());
        return {};
    }

    static Base::Result<void> GeneratedItemSubtreeChanged(
        Presentation::Visual& root,
        Controls::ItemSubtreeChange change,
        void* context) noexcept {
        auto* runtime = static_cast<Impl*>(context);
        if (runtime == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Generated item subtree runtime context is null");
        }
        if (change ==
            Controls::ItemSubtreeChange::Unmounting) {
            Base::Result<Presentation::VisualHandle>
                rootHandle =
                    runtime->tree->GetHandle(root);
            if (rootHandle) {
                for (std::uint32_t index = 0U;
                     index <
                         runtime->
                             pendingGeneratedPresentation.
                                 Size();) {
                    if (runtime->
                            pendingGeneratedPresentation[
                                index].index !=
                            rootHandle.Value().index ||
                        runtime->
                            pendingGeneratedPresentation[
                                index].generation !=
                            rootHandle.Value().generation) {
                        ++index;
                        continue;
                    }
                    for (std::uint32_t move =
                             index + 1U;
                         move <
                             runtime->
                                 pendingGeneratedPresentation.
                                     Size();
                         ++move) {
                        runtime->
                            pendingGeneratedPresentation[
                                move - 1U] =
                            runtime->
                                pendingGeneratedPresentation[
                                    move];
                    }
                    runtime->
                        pendingGeneratedPresentation.
                            PopBack();
                    return {};
                }
            }
            runtime->presentationServices.Detach(
                &root, {});
            return {};
        }
        if (runtime->bindings != nullptr &&
            runtime->bindings->IsFlushing()) {
            Base::Result<Presentation::VisualHandle>
                handle =
                    runtime->tree->GetHandle(root);
            if (!handle) return handle.GetStatus();
            return runtime->
                pendingGeneratedPresentation.
                    TryPushBack(handle.Value());
        }
        Base::Result<void> applied =
            runtime->presentationServices.Apply(root);
        if (!applied) {
            runtime->presentationServices.Detach(
                &root, {});
            return applied.GetStatus();
        }
        Base::Result<std::uint32_t> started =
            runtime->StartLoadedAnimations(&root);
        if (!started) {
            runtime->presentationServices.Detach(
                &root, {});
            return started.GetStatus();
        }
        return {};
    }

    Base::Result<void>
    FlushGeneratedPresentation() noexcept {
        constexpr std::uint32_t MaximumWaves = 16U;
        for (std::uint32_t wave = 0U;
             wave < MaximumWaves;
             ++wave) {
            if (pendingGeneratedPresentation.Empty()) {
                return {};
            }
            Base::Vector<Presentation::VisualHandle>
                pending =
                    std::move(
                        pendingGeneratedPresentation);
            pendingGeneratedPresentation.Clear();
            for (const Presentation::VisualHandle handle :
                 pending) {
                Presentation::Visual* subtreeRoot =
                    tree->ResolveHandle(handle);
                if (subtreeRoot == nullptr) continue;
                Base::Result<void> applied =
                    presentationServices.Apply(
                        *subtreeRoot);
                if (!applied) return applied.GetStatus();
                Base::Result<std::uint32_t> started =
                    StartLoadedAnimations(subtreeRoot);
                if (!started) return started.GetStatus();
            }
            Base::Result<std::uint32_t> rebound =
                bindings->Flush();
            if (!rebound) return rebound.GetStatus();
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Generated item presentation exceeded the bounded activation waves");
    }

    void DestroyTemplateServices() noexcept {
        presentationServices.Reset();
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            styles);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            visualStates);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            templates);
    }

    Base::Result<void> VisitAndAttach(
        Presentation::Visual& rootVisual) noexcept {
        Base::Vector<Presentation::Visual*> stack(allocator);
        Base::Result<void> pushed =
            stack.TryPushBack(&rootVisual);
        if (!pushed) return pushed.GetStatus();
        while (!stack.Empty()) {
            Presentation::Visual* node = stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;
            const Core::TypeId type = node->RuntimeType();
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::Control::StaticTypeId())) {
                Detail::ControlRuntimeAccess::Attach(
                    *static_cast<Controls::Control*>(node),
                    events);
            }
            AttachTextService(
                *node,
                textRuntime != nullptr
                    ? textRuntime->Service()
                    : nullptr);
            AttachPathService(
                *node, MeshServices());
            if (controlInteractions != nullptr &&
                metadata->Types().IsDerivedFrom(
                    type, Controls::ButtonBase::StaticTypeId())) {
                Base::Result<void> attached =
                    controlInteractions->Attach(
                        *static_cast<Controls::ButtonBase*>(node));
                if (!attached) return attached.GetStatus();
            }
            if (metadata->Types().IsDerivedFrom(
                    type, Controls::TextBox::StaticTypeId())) {
                auto& textBox =
                    *static_cast<Controls::TextBox*>(node);
                if (options.textInputMethodHost != nullptr) {
                    Base::Result<void> hosted =
                        textBox.SetInputMethodHost(
                            options.textInputMethodHost);
                    if (!hosted) return hosted.GetStatus();
                }
                if (textBoxInteractions != nullptr) {
                    Base::Result<void> attached =
                        textBoxInteractions->Attach(textBox);
                    if (!attached) return attached.GetStatus();
                }
            }
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::PasswordBox::
                        StaticTypeId())) {
                auto& passwordBox =
                    *static_cast<
                        Controls::PasswordBox*>(
                            node);
                if (options.textInputMethodHost !=
                    nullptr) {
                    Base::Result<void> hosted =
                        passwordBox.
                            SetInputMethodHost(
                                options.
                                    textInputMethodHost);
                    if (!hosted) {
                        return hosted.GetStatus();
                    }
                }
                if (textBoxInteractions !=
                    nullptr) {
                    Base::Result<void> attached =
                        textBoxInteractions->
                            Attach(passwordBox);
                    if (!attached) {
                        return attached.GetStatus();
                    }
                }
            }
            if (scrollInteractions != nullptr &&
                metadata->Types().IsDerivedFrom(
                    type,
                    Controls::ScrollViewer::StaticTypeId())) {
                Base::Result<void> attached =
                    scrollInteractions->Attach(
                        *static_cast<Controls::ScrollViewer*>(
                            node));
                if (!attached) return attached.GetStatus();
            }
            if (sliderInteractions != nullptr &&
                metadata->Types().IsDerivedFrom(
                    type,
                    Controls::Slider::StaticTypeId())) {
                Base::Result<void> attached =
                    sliderInteractions->Attach(
                        *static_cast<Controls::Slider*>(
                            node));
                if (!attached) return attached.GetStatus();
            }
            if (listBoxInteractions != nullptr &&
                metadata->Types().IsDerivedFrom(
                    type,
                    Controls::ListBox::StaticTypeId())) {
                Base::Result<void> attached =
                    listBoxInteractions->Attach(
                        *static_cast<Controls::ListBox*>(
                            node));
                if (!attached) return attached.GetStatus();
            }
            if (comboBoxInteractions != nullptr &&
                metadata->Types().IsDerivedFrom(
                    type,
                    Controls::ComboBox::StaticTypeId())) {
                Base::Result<void> attached =
                    comboBoxInteractions->Attach(
                        *static_cast<Controls::ComboBox*>(
                            node));
                if (!attached) return attached.GetStatus();
            }
            if (treeViewInteractions != nullptr &&
                metadata->Types().IsDerivedFrom(
                    type,
                    Controls::TreeView::StaticTypeId())) {
                Base::Result<void> attached =
                    treeViewInteractions->Attach(
                        *static_cast<Controls::TreeView*>(
                            node));
                if (!attached) {
                    return attached.GetStatus();
                }
            }
            if (menuInteractions != nullptr &&
                metadata->Types().IsDerivedFrom(
                    type,
                    Controls::Menu::StaticTypeId())) {
                Base::Result<void> attached =
                    menuInteractions->Attach(
                        *static_cast<Controls::Menu*>(
                            node));
                if (!attached) {
                    return attached.GetStatus();
                }
            }
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::ItemsControl::StaticTypeId())) {
                auto& itemsControl =
                    *static_cast<Controls::ItemsControl*>(
                        node);
                Controls::Panel* host =
                    itemsControl.ItemsHost();
                if (host != nullptr &&
                    itemsControl.RealizedItemCount() == 0U) {
                    Controls::ItemContainerGenerator*
                        generator = nullptr;
                    Base::Result<void> created =
                        CreateRuntimeObject(
                            *allocator,
                            Base::MemoryTag::Presentation,
                            generator,
                            *tree,
                            *layout,
                            *values,
                            styles,
                            renderer,
                            templates,
                            &Impl::GeneratedItemSubtreeChanged,
                            this);
                    if (!created) return created.GetStatus();
                    Base::Result<void> attached;
                    if (metadata->Types().IsDerivedFrom(
                            host->RuntimeType(),
                            Controls::VirtualizingStackPanel::
                                StaticTypeId())) {
                        attached =
                            generator->AttachVirtualized(
                                itemsControl,
                                *static_cast<
                                    Controls::
                                        VirtualizingStackPanel*>(
                                            host));
                    } else {
                        attached = generator->Attach(
                            itemsControl, *host);
                    }
                    if (!attached) {
                        DestroyRuntimeObject(
                            *allocator,
                            Base::MemoryTag::Presentation,
                            generator);
                        return attached.GetStatus();
                    }
                    Base::Result<void> presented =
                        presentationServices.Apply(
                            *host);
                    if (!presented) {
                        presentationServices.Detach(
                            host, {});
                        static_cast<void>(
                            generator->Detach());
                        DestroyRuntimeObject(
                            *allocator,
                            Base::MemoryTag::Presentation,
                            generator);
                        return presented.GetStatus();
                    }
                    Base::Result<void> tracked =
                        itemGenerators.TryPushBack(
                            generator);
                    if (!tracked) {
                        static_cast<void>(
                            generator->Detach());
                        DestroyRuntimeObject(
                            *allocator,
                            Base::MemoryTag::Presentation,
                            generator);
                        return tracked.GetStatus();
                    }
                }
            }
            const Base::Span<Presentation::Visual* const>
                children = node->VisualChildren();
            for (std::uint32_t index = 0U;
                 index < children.Size(); ++index) {
                pushed = stack.TryPushBack(children[index]);
                if (!pushed) return pushed.GetStatus();
            }
        }
        return {};
    }

    void ClearTextInputHosts(
        Presentation::Visual* node) noexcept {
        if (node == nullptr) return;
        if (metadata->Types().IsDerivedFrom(
                node->RuntimeType(),
                Controls::TextBox::StaticTypeId())) {
            static_cast<void>(
                static_cast<Controls::TextBox*>(node)->
                    SetInputMethodHost(nullptr));
        }
        if (metadata->Types().IsDerivedFrom(
                node->RuntimeType(),
                Controls::PasswordBox::
                    StaticTypeId())) {
            static_cast<void>(
                static_cast<
                    Controls::PasswordBox*>(node)->
                    SetInputMethodHost(nullptr));
        }
        for (Presentation::Visual* child :
             node->VisualChildren()) {
            ClearTextInputHosts(child);
        }
    }

    Base::Result<Base::StringView> AnimationAttachedString(
        Animation::Timeline& timeline,
        Core::DependencyPropertyHandle property) noexcept {
        Base::Result<Core::PropertyValue> value =
            timeline.GetValue(property);
        if (!value) return value.GetStatus();
        if (value.Value().Kind() != Core::ValueKind::String) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Storyboard attached property must be a string");
        }
        return value.Value().AsString();
    }

    struct ResolvedAnimationProperty final {
        Core::DependencyObject* target = nullptr;
        Core::DependencyPropertyHandle property;
    };

    Base::Result<ResolvedAnimationProperty>
    ResolveAnimationProperty(
        Core::DependencyObject& target,
        Base::StringView authoredPath) noexcept {
        Base::StringView path = authoredPath;
        while (!path.Empty() &&
               (path[0] == ' ' || path[0] == '\t')) {
            path = path.Substr(1U, path.SizeBytes() - 1U);
        }
        while (!path.Empty() &&
               (path[path.SizeBytes() - 1U] == ' ' ||
                path[path.SizeBytes() - 1U] == '\t')) {
            path = path.Substr(0U, path.SizeBytes() - 1U);
        }
        bool compoundParenthesizedPath = false;
        for (std::uint32_t index = 0U;
             index + 1U < path.SizeBytes(); ++index) {
            if (path[index] == ')' &&
                (path[index + 1U] == '.' ||
                 path[index + 1U] == '[')) {
                compoundParenthesizedPath = true;
                break;
            }
        }
        if (!compoundParenthesizedPath &&
            path.SizeBytes() >= 2U &&
            path[0] == '(' &&
            path[path.SizeBytes() - 1U] == ')') {
            path = path.Substr(1U, path.SizeBytes() - 2U);
        }
        std::uint32_t indexedOpen = UINT32_MAX;
        std::uint32_t indexedClose = UINT32_MAX;
        for (std::uint32_t index = 0U;
             index < path.SizeBytes(); ++index) {
            if (path[index] == '[' &&
                indexedOpen == UINT32_MAX) {
                indexedOpen = index;
            } else if (path[index] == ']' &&
                       indexedOpen != UINT32_MAX) {
                indexedClose = index;
                break;
            }
        }

        Core::DependencyObject* propertyTarget = &target;
        bool indexedPathResolved = false;
        if (indexedOpen != UINT32_MAX) {
            if (indexedClose == UINT32_MAX ||
                indexedClose == indexedOpen + 1U) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "Indexed Storyboard TargetProperty path has an invalid index");
            }
            std::uint64_t parsedIndex = 0U;
            for (std::uint32_t index = indexedOpen + 1U;
                 index < indexedClose; ++index) {
                if (path[index] < '0' || path[index] > '9') {
                    return Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        "Indexed Storyboard TargetProperty index must be numeric");
                }
                parsedIndex =
                    parsedIndex * 10U +
                    static_cast<std::uint64_t>(
                        path[index] - '0');
                if (parsedIndex > UINT32_MAX) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Indexed Storyboard TargetProperty index is too large");
                }
            }

            const Base::StringView beforeIndex =
                path.Substr(0U, indexedOpen);
            Base::StringView terminalPath =
                path.Substr(
                    indexedClose + 1U,
                    path.SizeBytes() -
                        indexedClose - 1U);
            if (!terminalPath.Empty() &&
                terminalPath[0] == '.') {
                terminalPath = terminalPath.Substr(
                    1U, terminalPath.SizeBytes() - 1U);
            }
            if (terminalPath.SizeBytes() >= 2U &&
                terminalPath[0] == '(' &&
                terminalPath[
                    terminalPath.SizeBytes() - 1U] == ')') {
                terminalPath = terminalPath.Substr(
                    1U, terminalPath.SizeBytes() - 2U);
            }

            auto endsWith =
                [](Base::StringView value,
                   Base::StringView suffix) noexcept {
                    return value.SizeBytes() >=
                               suffix.SizeBytes() &&
                        value.Substr(
                            value.SizeBytes() -
                                suffix.SizeBytes(),
                            suffix.SizeBytes()) ==
                            suffix;
                };
            const bool gradientStops =
                endsWith(
                    beforeIndex,
                    Base::StringView(
                        ").(GradientBrush.GradientStops)")) ||
                endsWith(
                    beforeIndex,
                    Base::StringView(
                        ".GradientStops"));
            const bool transformChildren =
                beforeIndex ==
                    Base::StringView(
                        "(UIElement.RenderTransform).(TransformGroup.Children)") ||
                beforeIndex ==
                    Base::StringView(
                        "RenderTransform.Children") ||
                beforeIndex ==
                    Base::StringView(
                        "(FrameworkElement.LayoutTransform).(TransformGroup.Children)") ||
                beforeIndex ==
                    Base::StringView(
                        "LayoutTransform.Children") ||
                beforeIndex ==
                    Base::StringView(
                        "(TransformGroup.Children)");
            if (gradientStops) {
                Base::StringView brushOwnerPath;
                if (!beforeIndex.Empty() &&
                    beforeIndex[0] == '(') {
                    std::uint32_t close = UINT32_MAX;
                    for (std::uint32_t index = 1U;
                         index < beforeIndex.SizeBytes();
                         ++index) {
                        if (beforeIndex[index] == ')') {
                            close = index;
                            break;
                        }
                    }
                    if (close == UINT32_MAX ||
                        close <= 1U) {
                        return Base::Status::Failure(
                            Base::ErrorCode::ValidationFailed,
                            "Storyboard GradientStops owner path is invalid");
                    }
                    brushOwnerPath =
                        beforeIndex.Substr(
                            1U, close - 1U);
                } else {
                    std::uint32_t separator = UINT32_MAX;
                    for (std::uint32_t index = 0U;
                         index < beforeIndex.SizeBytes();
                         ++index) {
                        if (beforeIndex[index] == '.') {
                            separator = index;
                            break;
                        }
                    }
                    if (separator == UINT32_MAX ||
                        separator == 0U) {
                        return Base::Status::Failure(
                            Base::ErrorCode::ValidationFailed,
                            "Storyboard GradientStops owner property is missing");
                    }
                    brushOwnerPath =
                        beforeIndex.Substr(
                            0U, separator);
                }
                std::uint32_t ownerDot = UINT32_MAX;
                for (std::uint32_t index = 0U;
                     index < brushOwnerPath.SizeBytes();
                     ++index) {
                    if (brushOwnerPath[index] == '.') {
                        ownerDot = index;
                    }
                }
                const Base::StringView brushProperty =
                    ownerDot == UINT32_MAX
                    ? brushOwnerPath
                    : brushOwnerPath.Substr(
                          ownerDot + 1U,
                          brushOwnerPath.SizeBytes() -
                              ownerDot - 1U);
                const Core::DependencyProperty* background =
                    Core::Detail::MetadataDomainAccess::
                        DependencyProperties(*metadata)
                            .Find(
                                target.RuntimeType(),
                                brushProperty);
                if (background == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard brush property was not found");
                }
                Base::Result<Core::PropertyValue> value =
                    target.GetValue(background->Handle());
                if (!value ||
                    value.Value().Kind() !=
                        Core::ValueKind::Object ||
                    !value.Value().AsObject() ||
                    !metadata->Types().IsDerivedFrom(
                        value.Value().AsObject()->RuntimeType(),
                        Presentation::GradientBrush::StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard target property is not a GradientBrush");
                }
                auto& brush = static_cast<
                    Presentation::GradientBrush&>(
                        *value.Value().AsObject());
                const auto stops = brush.GradientStops();
                if (parsedIndex >= stops.Size() ||
                    !stops[static_cast<std::uint32_t>(
                        parsedIndex)]) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Storyboard GradientStops index is out of range");
                }
                propertyTarget =
                    stops[static_cast<std::uint32_t>(
                        parsedIndex)].Get();
                path = terminalPath;
                indexedPathResolved = true;
            } else if (transformChildren) {
                Base::Ref<Presentation::Transform> transform;
                if (beforeIndex ==
                        Base::StringView(
                            "(TransformGroup.Children)") &&
                    metadata->Types().IsDerivedFrom(
                        target.RuntimeType(),
                        Presentation::TransformGroup::StaticTypeId())) {
                    transform =
                        Base::Ref<Presentation::Transform>::
                            TryFromBorrowed(
                                static_cast<
                                    Presentation::Transform&>(
                                        target));
                } else {
                    const bool layoutPath =
                        beforeIndex ==
                            Base::StringView(
                                "(FrameworkElement.LayoutTransform).(TransformGroup.Children)") ||
                        beforeIndex ==
                            Base::StringView(
                                "LayoutTransform.Children");
                    if (layoutPath) {
                        if (!metadata->Types().IsDerivedFrom(
                                target.RuntimeType(),
                                Presentation::FrameworkElement::StaticTypeId())) {
                            return Base::Status::Failure(
                                Base::ErrorCode::InvalidArgument,
                                "Storyboard LayoutTransform target is not a FrameworkElement");
                        }
                        transform =
                            static_cast<Presentation::FrameworkElement&>(
                                target).LayoutTransform();
                    } else {
                        if (!metadata->Types().IsDerivedFrom(
                                target.RuntimeType(),
                                Presentation::UIElement::StaticTypeId())) {
                            return Base::Status::Failure(
                                Base::ErrorCode::InvalidArgument,
                                "Storyboard RenderTransform target is not a UIElement");
                        }
                        transform =
                            static_cast<Presentation::UIElement&>(
                                target).RenderTransform();
                    }
                }
                if (!transform ||
                    !metadata->Types().IsDerivedFrom(
                        transform->RuntimeType(),
                        Presentation::TransformGroup::StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard transform path has no TransformGroup");
                }
                auto& group = static_cast<
                    Presentation::TransformGroup&>(
                        *transform);
                const auto children = group.Children();
                if (parsedIndex >= children.Size() ||
                    !children[static_cast<std::uint32_t>(
                        parsedIndex)]) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Storyboard TransformGroup index is out of range");
                }
                propertyTarget =
                    children[static_cast<std::uint32_t>(
                        parsedIndex)].Get();
                path = terminalPath;
                indexedPathResolved = true;
            } else {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Indexed Storyboard TargetProperty collection is not supported");
            }
        }

        if (!indexedPathResolved &&
            indexedOpen == UINT32_MAX &&
            compoundParenthesizedPath &&
            path.SizeBytes() >= 7U &&
            path[0] == '(' &&
            path[path.SizeBytes() - 1U] == ')') {
            std::uint32_t separator = UINT32_MAX;
            for (std::uint32_t index = 1U;
                 index + 2U < path.SizeBytes();
                 ++index) {
                if (path[index] == ')' &&
                    path[index + 1U] == '.' &&
                    path[index + 2U] == '(') {
                    separator = index;
                    break;
                }
            }
            if (separator != UINT32_MAX) {
                Base::StringView ownerPath =
                    path.Substr(
                        1U,
                        separator - 1U);
                std::uint32_t ownerDot =
                    UINT32_MAX;
                for (std::uint32_t index = 0U;
                     index < ownerPath.SizeBytes();
                     ++index) {
                    if (ownerPath[index] == '.') {
                        ownerDot = index;
                    }
                }
                const Base::StringView ownerProperty =
                    ownerDot == UINT32_MAX
                    ? ownerPath
                    : ownerPath.Substr(
                          ownerDot + 1U,
                          ownerPath.SizeBytes() -
                              ownerDot - 1U);
                const std::uint32_t terminalStart =
                    separator + 3U;
                Base::StringView terminalPath =
                    path.Substr(
                        terminalStart,
                        path.SizeBytes() -
                            terminalStart - 1U);
                const Core::DependencyProperty*
                    ownerDependency =
                        Core::Detail::
                            MetadataDomainAccess::
                                DependencyProperties(
                                    *metadata)
                                    .Find(
                                        target.
                                            RuntimeType(),
                                        ownerProperty);
                if (ownerDependency == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard compound object property was not found");
                }
                Base::Result<Core::PropertyValue>
                    ownerValue = target.GetValue(
                        ownerDependency->Handle());
                if (!ownerValue ||
                    ownerValue.Value().Kind() !=
                        Core::ValueKind::Object ||
                    !ownerValue.Value().AsObject() ||
                    !metadata->Types().IsDerivedFrom(
                        ownerValue.Value().
                            AsObject()->RuntimeType(),
                        Core::DependencyObject::
                            StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard compound object property has no DependencyObject value");
                }
                propertyTarget =
                    static_cast<
                        Core::DependencyObject*>(
                        ownerValue.Value().
                            AsObject().Get());
                path = terminalPath;
                indexedPathResolved = true;
            }
        }

        std::uint32_t dot = UINT32_MAX;
        if (!indexedPathResolved) {
            for (std::uint32_t index = 0U;
                 index < path.SizeBytes(); ++index) {
                if (path[index] == '.') {
                    dot = index;
                    break;
                }
            }
        }
        if (path.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Storyboard TargetProperty is empty");
        }

        if (dot != UINT32_MAX) {
            Base::StringView ownerProperty =
                path.Substr(0U, dot);
            Base::StringView nestedProperty =
                path.Substr(
                    dot + 1U,
                    path.SizeBytes() - dot - 1U);
            if (ownerProperty ==
                    Base::StringView("RenderTransform") ||
                ownerProperty ==
                    Base::StringView("LayoutTransform")) {
                const bool layoutPath =
                    ownerProperty ==
                    Base::StringView(
                        "LayoutTransform");
                Base::Ref<Presentation::Transform> transform;
                if (layoutPath) {
                    if (!metadata->Types().IsDerivedFrom(
                            target.RuntimeType(),
                            Presentation::FrameworkElement::StaticTypeId())) {
                        return Base::Status::Failure(
                            Base::ErrorCode::InvalidArgument,
                            "LayoutTransform animation target is not a FrameworkElement");
                    }
                    transform =
                        static_cast<Presentation::FrameworkElement&>(
                            target).LayoutTransform();
                } else {
                    if (!metadata->Types().IsDerivedFrom(
                            target.RuntimeType(),
                            Presentation::UIElement::StaticTypeId())) {
                        return Base::Status::Failure(
                            Base::ErrorCode::InvalidArgument,
                            "RenderTransform animation target is not a UIElement");
                    }
                    transform =
                        static_cast<Presentation::UIElement&>(
                            target).RenderTransform();
                }
                if (!transform) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        layoutPath
                            ? "Storyboard target has no LayoutTransform object"
                            : "Storyboard target has no RenderTransform object");
                }
                propertyTarget = transform.Get();
                path = nestedProperty;
            } else if (
                nestedProperty == Base::StringView("Color")) {
                const Core::DependencyProperty*
                    ownerDependency =
                        Core::Detail::
                            MetadataDomainAccess::
                                DependencyProperties(
                                    *metadata)
                                    .Find(
                                        target.
                                            RuntimeType(),
                                        ownerProperty);
                if (ownerDependency == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard object property was not found on the target");
                }
                Base::Result<Core::PropertyValue>
                    ownerValue = target.GetValue(
                        ownerDependency->Handle());
                if (!ownerValue ||
                    ownerValue.Value().Kind() !=
                        Core::ValueKind::Object ||
                    !ownerValue.Value().AsObject() ||
                    !metadata->Types().IsDerivedFrom(
                        ownerValue.Value().
                            AsObject()->RuntimeType(),
                        Core::DependencyObject::
                            StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard object property has no DependencyObject value");
                }
                propertyTarget =
                    static_cast<
                        Core::DependencyObject*>(
                            ownerValue.Value().
                                AsObject().Get());
                path = nestedProperty;
            } else {
                // Owner-qualified direct properties such as
                // FrameworkElement.MinWidth resolve on the original target.
                path = nestedProperty;
            }
        }
        if (path.SizeBytes() >= 2U &&
            path[0] == '(' &&
            path[path.SizeBytes() - 1U] == ')') {
            path = path.Substr(1U, path.SizeBytes() - 2U);
        }
        std::uint32_t ownerDot = UINT32_MAX;
        for (std::uint32_t index = 0U;
             index < path.SizeBytes(); ++index) {
            if (path[index] == '.') ownerDot = index;
        }
        if (ownerDot != UINT32_MAX) {
            path = path.Substr(
                ownerDot + 1U,
                path.SizeBytes() - ownerDot - 1U);
        }
        const Core::DependencyProperty* property =
            Core::Detail::MetadataDomainAccess::
                DependencyProperties(*metadata)
                    .Find(propertyTarget->RuntimeType(), path);
        if (property == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Storyboard TargetProperty was not found on the target");
        }
        return ResolvedAnimationProperty{
            propertyTarget, property->Handle()};
    }

    struct StoryboardTimingContext final {
        Presentation::AnimationTime beginTimeMicroseconds = 0U;
        Presentation::AnimationTime durationMicroseconds = 0U;
        Presentation::RepeatBehavior repeat;
        double speedRatio = 1.0;
        bool hasDuration = false;
        bool hasRepeat = false;
        bool autoReverse = false;
    };

    StoryboardTimingContext ComposeStoryboardTiming(
        const StoryboardTimingContext* inherited,
        const Animation::Timeline& storyboard) noexcept {
        StoryboardTimingContext result =
            inherited != nullptr
            ? *inherited
            : StoryboardTimingContext{};
        const Presentation::TimelineTiming& authored =
            storyboard.Timing();
        if (UINT64_MAX - result.beginTimeMicroseconds <
            authored.beginTimeMicroseconds) {
            result.beginTimeMicroseconds = UINT64_MAX;
        } else {
            result.beginTimeMicroseconds +=
                authored.beginTimeMicroseconds;
        }
        if (!storyboard.Duration().Empty()) {
            result.durationMicroseconds =
                authored.durationMicroseconds;
            result.hasDuration = true;
        }
        if (!storyboard.RepeatBehavior().Empty()) {
            result.repeat = authored.repeat;
            result.hasRepeat = true;
        }
        result.speedRatio *= authored.speedRatio;
        result.autoReverse =
            result.autoReverse || authored.autoReverse;
        return result;
    }

    Presentation::TimelineTiming EffectiveTimelineTiming(
        const Animation::Timeline& timeline,
        const StoryboardTimingContext* inherited) noexcept {
        Presentation::TimelineTiming result =
            timeline.Timing();
        if (inherited == nullptr) return result;
        if (UINT64_MAX - inherited->beginTimeMicroseconds <
            result.beginTimeMicroseconds) {
            result.beginTimeMicroseconds = UINT64_MAX;
        } else {
            result.beginTimeMicroseconds +=
                inherited->beginTimeMicroseconds;
        }
        if (inherited->hasDuration) {
            result.durationMicroseconds =
                inherited->durationMicroseconds;
        }
        if (inherited->hasRepeat) {
            result.repeat = inherited->repeat;
        }
        result.speedRatio *= inherited->speedRatio;
        result.autoReverse =
            result.autoReverse || inherited->autoReverse;
        return result;
    }

    Base::Result<std::uint32_t>
    RetainStartedAnimation(
        Base::Result<
            Presentation::AnimationHandle>
            started,
        Base::Vector<
            Presentation::AnimationHandle>*
            retainedHandles) noexcept {
        if (!started) {
            return started.GetStatus();
        }
        if (retainedHandles != nullptr) {
            Base::Result<void> retained =
                retainedHandles->TryPushBack(
                    started.Value());
            if (!retained) {
                static_cast<void>(
                    animations->Remove(
                        started.Value()));
                return retained.GetStatus();
            }
        }
        return std::uint32_t{1U};
    }

    Base::Result<std::uint32_t> BeginTimeline(
        Animation::Timeline& timeline,
        Presentation::FrameworkElement& triggerOwner,
        const StoryboardTimingContext* inherited = nullptr,
        Base::Vector<
            Presentation::AnimationHandle>*
            retainedHandles = nullptr,
        Detail::DataTemplateTriggerContext*
            dataTemplateContext = nullptr) noexcept {
        if (animations == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Storyboard requires the animation manager");
        }
        if (timeline.RuntimeType() ==
            Animation::Storyboard::StaticTypeId()) {
            auto& nested =
                static_cast<Animation::Storyboard&>(timeline);
            const StoryboardTimingContext timing =
                ComposeStoryboardTiming(
                    inherited, nested);
            std::uint32_t count = 0U;
            for (const Base::Ref<Animation::Timeline>& child :
                 nested.Timelines()) {
                if (!child) continue;
                Base::Result<std::uint32_t> started =
                    BeginTimeline(
                        *child, triggerOwner, &timing,
                        retainedHandles,
                        dataTemplateContext);
                if (!started) return started.GetStatus();
                if (count > UINT32_MAX - started.Value()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Storyboard child count overflow");
                }
                count += started.Value();
            }
            return count;
        }

        Base::Result<Base::StringView> targetName =
            AnimationAttachedString(
                timeline,
                Animation::Storyboard::TargetNameProperty);
        if (!targetName) return targetName.GetStatus();
        Base::Result<Base::StringView> targetPath =
            AnimationAttachedString(
                timeline,
                Animation::Storyboard::TargetPropertyProperty);
        if (!targetPath) return targetPath.GetStatus();

        Base::Object* targetObject =
            targetName.Value().Empty()
            ? static_cast<Base::Object*>(
                  &triggerOwner)
            : dataTemplateContext != nullptr
                ? dataTemplateContext->FindName(
                      targetName.Value())
                : loadedDocument.names.Find(
                      targetName.Value());
        if (targetObject == nullptr ||
            !metadata->Types().IsDerivedFrom(
                targetObject->RuntimeType(),
                Core::DependencyObject::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Storyboard target name does not resolve to a DependencyObject");
        }
        auto& target =
            static_cast<Core::DependencyObject&>(*targetObject);
        Base::Result<ResolvedAnimationProperty> property =
            ResolveAnimationProperty(target, targetPath.Value());
        if (!property) return property.GetStatus();
        Core::DependencyObject& propertyTarget =
            *property.Value().target;
        const Core::DependencyPropertyHandle propertyHandle =
            property.Value().property;

        const Core::TypeId type = timeline.RuntimeType();
        if (type == Animation::DoubleAnimation::StaticTypeId()) {
            auto& animation =
                static_cast<Animation::DoubleAnimation&>(timeline);
            Presentation::DoubleAnimation runtime =
                animation.RuntimeAnimation();
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<Presentation::AnimationHandle> started =
                animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type == Animation::ColorAnimation::StaticTypeId()) {
            auto& animation =
                static_cast<Animation::ColorAnimation&>(timeline);
            Presentation::ColorAnimation runtime =
                animation.RuntimeAnimation();
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<Presentation::AnimationHandle> started =
                animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            Animation::PointAnimation::
                StaticTypeId()) {
            auto& animation =
                static_cast<
                    Animation::PointAnimation&>(
                        timeline);
            Presentation::PointAnimation runtime =
                animation.RuntimeAnimation();
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Presentation::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            Animation::RectAnimation::
                StaticTypeId()) {
            auto& animation =
                static_cast<
                    Animation::RectAnimation&>(
                        timeline);
            Presentation::RectAnimation runtime =
                animation.RuntimeAnimation();
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Presentation::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            Animation::ThicknessAnimation::
                StaticTypeId()) {
            auto& animation =
                static_cast<
                    Animation::ThicknessAnimation&>(
                        timeline);
            Presentation::ThicknessAnimation runtime =
                animation.RuntimeAnimation();
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Presentation::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            Animation::DoubleAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                Animation::DoubleAnimationUsingKeyFrames&>(timeline);
            Base::Vector<Presentation::DoubleKeyFrame> frames(allocator);
            for (const Base::Ref<Animation::DoubleKeyFrame>& frame :
                 animation.KeyFrames()) {
                if (!frame) continue;
                Base::Result<void> appended =
                    frames.TryPushBack(frame->RuntimeFrame());
                if (!appended) return appended.GetStatus();
            }
            for (std::uint32_t index = 1U;
                 index < frames.Size(); ++index) {
                Presentation::DoubleKeyFrame current =
                    frames[index];
                std::uint32_t position = index;
                while (position > 0U &&
                       frames[position - 1U]
                               .keyTimeMicroseconds >
                           current.keyTimeMicroseconds) {
                    frames[position] =
                        frames[position - 1U];
                    --position;
                }
                frames[position] = current;
            }
            Base::Result<Core::PropertyValue> base =
                propertyTarget.GetValue(propertyHandle);
            if (!base) return base.GetStatus();
            Base::Result<double> baseDouble =
                Core::ValueCodec<double>::Decode(base.Value());
            Presentation::DoubleKeyFrameAnimation runtime;
            if (baseDouble) {
                runtime.baseValue = baseDouble.Value();
            } else if (!frames.Empty() &&
                       frames.Front().keyTimeMicroseconds == 0U) {
                // A zero-time key frame defines the initial animated value;
                // no interpolation can observe the underlying base value.
                // This also lets XAML start a key-frame animation on a
                // property whose unset metadata representation is not a
                // concrete double.
                runtime.baseValue = frames.Front().value;
            } else {
                return baseDouble.GetStatus();
            }
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            if (runtime.timing.durationMicroseconds == 0U &&
                !frames.Empty()) {
                runtime.timing.durationMicroseconds =
                    frames.Back().keyTimeMicroseconds;
            }
            runtime.keyFrames = frames.AsSpan();
            Base::Result<Presentation::AnimationHandle> started =
                animations->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            Animation::ColorAnimationUsingKeyFrames::
                StaticTypeId()) {
            auto& animation = static_cast<
                Animation::ColorAnimationUsingKeyFrames&>(
                    timeline);
            Base::Vector<Presentation::ColorKeyFrame>
                frames(allocator);
            for (const Base::Ref<
                     Animation::ColorKeyFrame>& frame :
                 animation.KeyFrames()) {
                if (!frame) continue;
                Base::Result<void> appended =
                    frames.TryPushBack(
                        frame->RuntimeFrame());
                if (!appended) {
                    return appended.GetStatus();
                }
            }
            for (std::uint32_t index = 1U;
                 index < frames.Size();
                 ++index) {
                Presentation::ColorKeyFrame current =
                    frames[index];
                std::uint32_t position = index;
                while (position > 0U &&
                       frames[position - 1U]
                               .keyTimeMicroseconds >
                           current.keyTimeMicroseconds) {
                    frames[position] =
                        frames[position - 1U];
                    --position;
                }
                frames[position] = current;
            }
            Base::Result<Core::PropertyValue> base =
                propertyTarget.GetValue(
                    propertyHandle);
            if (!base) return base.GetStatus();
            Base::Result<Base::Color> baseColor =
                Core::ValueCodec<Base::Color>::Decode(
                    base.Value());
            if (!baseColor) {
                return baseColor.GetStatus();
            }
            Presentation::ColorKeyFrameAnimation
                runtime;
            runtime.baseValue = baseColor.Value();
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            if (runtime.timing.durationMicroseconds ==
                    0U &&
                !frames.Empty()) {
                runtime.timing.durationMicroseconds =
                    frames.Back()
                        .keyTimeMicroseconds;
            }
            runtime.keyFrames = frames.AsSpan();
            Base::Result<
                Presentation::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }

        Base::Vector<Presentation::DiscreteAnimationKeyFrame>
            frames(allocator);
        if (type ==
            Animation::ThicknessAnimationUsingKeyFrames::
                StaticTypeId()) {
            auto& animation = static_cast<
                Animation::ThicknessAnimationUsingKeyFrames&>(
                    timeline);
            for (const Base::Ref<
                     Animation::ThicknessKeyFrame>& frame :
                 animation.KeyFrames()) {
                if (!frame) continue;
                Presentation::DiscreteAnimationKeyFrame runtime;
                runtime.keyTimeMicroseconds =
                    frame->KeyTimeMicroseconds();
                Base::Result<Core::PropertyValue> encoded =
                    Core::ValueCodec<
                        Base::Thickness>::Encode(
                            frame->Value());
                if (!encoded) return encoded.GetStatus();
                runtime.value =
                    std::move(encoded).Value();
                Base::Result<void> appended =
                    frames.TryPushBack(
                        std::move(runtime));
                if (!appended) {
                    return appended.GetStatus();
                }
            }
        } else if (type ==
            Animation::BooleanAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                Animation::BooleanAnimationUsingKeyFrames&>(timeline);
            for (const Base::Ref<
                     Animation::DiscreteBooleanKeyFrame>& frame :
                 animation.KeyFrames()) {
                if (!frame) continue;
                Presentation::DiscreteAnimationKeyFrame runtime;
                runtime.keyTimeMicroseconds =
                    frame->KeyTimeMicroseconds();
                Base::Result<Core::PropertyValue> encoded =
                    Core::ValueCodec<bool>::Encode(frame->Value());
                if (!encoded) return encoded.GetStatus();
                runtime.value = std::move(encoded).Value();
                Base::Result<void> appended =
                    frames.TryPushBack(std::move(runtime));
                if (!appended) return appended.GetStatus();
            }
        } else if (type ==
            Animation::ObjectAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                Animation::ObjectAnimationUsingKeyFrames&>(timeline);
            for (const Base::Ref<
                     Animation::DiscreteObjectKeyFrame>& frame :
                 animation.KeyFrames()) {
                if (!frame) continue;
                Presentation::DiscreteAnimationKeyFrame runtime;
                runtime.keyTimeMicroseconds =
                    frame->KeyTimeMicroseconds();
                runtime.value = frame->Value();
                Base::Result<void> appended =
                    frames.TryPushBack(std::move(runtime));
                if (!appended) return appended.GetStatus();
            }
        } else {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Storyboard contains an unsupported Timeline type");
        }
        for (std::uint32_t index = 1U;
             index < frames.Size(); ++index) {
            Presentation::DiscreteAnimationKeyFrame current =
                std::move(frames[index]);
            std::uint32_t position = index;
            while (position > 0U &&
                   frames[position - 1U]
                           .keyTimeMicroseconds >
                       current.keyTimeMicroseconds) {
                frames[position] =
                    std::move(frames[position - 1U]);
                --position;
            }
            frames[position] = std::move(current);
        }
        Base::Result<Core::PropertyValue> base =
            propertyTarget.GetValue(propertyHandle);
        if (!base) return base.GetStatus();
        Presentation::DiscreteAnimation runtime;
        runtime.baseValue = base.Value();
        runtime.timing =
            EffectiveTimelineTiming(
                timeline, inherited);
        if (runtime.timing.durationMicroseconds == 0U &&
            !frames.Empty()) {
            runtime.timing.durationMicroseconds =
                frames.Back().keyTimeMicroseconds;
        }
        runtime.keyFrames = frames.AsSpan();
        Base::Result<Presentation::AnimationHandle> started =
            animations->Begin(
                propertyTarget, propertyHandle, runtime);
        return RetainStartedAnimation(
            std::move(started),
            retainedHandles);
    }

    Base::Result<bool> DataTemplateTriggerValuesMatch(
        const Core::PropertyValue& actual,
        Core::PropertyValue expected) noexcept {
        if (actual.Kind() == Core::ValueKind::Object &&
            !actual.IsNullObject() &&
            actual.AsObject() &&
            actual.AsObject()->RuntimeType() ==
                Controls::BoxedItemValue::StaticTypeId()) {
            return DataTemplateTriggerValuesMatch(
                static_cast<const Controls::BoxedItemValue&>(
                    *actual.AsObject()).Value(),
                std::move(expected));
        }
        if (expected.Kind() == Core::ValueKind::String &&
            expected.Type() != actual.Type()) {
            if (metadataRuntime == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "DataTemplate Trigger metadata is unavailable");
            }
            Base::Result<Core::PropertyValue> converted =
                metadataRuntime->TryConvertText(
                    actual.Type(), expected.AsString());
            if (!converted) {
                // WPF data conditions simply do not match when the authored
                // value cannot be converted to the source property's type.
                return false;
            }
            expected = std::move(converted).Value();
        }
        return actual == expected;
    }

    Base::Result<bool> EvaluateDataTemplateCondition(
        Detail::DataTemplateTriggerContext& context,
        Detail::DataTemplateTriggerCondition& condition) noexcept {
        Core::PropertyValue current;
        if (condition.dependencySource &&
            condition.property.IsValid()) {
            Base::Result<Core::PropertyValue> value =
                condition.dependencySource->GetValue(
                    condition.property);
            if (!value) return value.GetStatus();
            current = std::move(value).Value();
        } else {
            if (!condition.binding || metadataRuntime == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "DataTemplate DataTrigger Binding is unavailable");
            }
            Base::Object* source =
                condition.binding->ElementName().Empty()
                ? condition.source.Get()
                : context.FindName(
                      condition.binding->ElementName());
            if (source == nullptr &&
                !condition.binding->ElementName().Empty()) {
                source = loadedDocument.names.Find(
                    condition.binding->ElementName());
            }
            if (source == nullptr) {
                return false;
            }
            Base::Result<Core::BindingPathPlan> plan =
                Core::BindingPathPlan::Compile(
                    *metadataRuntime,
                    source->RuntimeType(),
                    condition.binding->Path());
            if (!plan) return plan.GetStatus();
            Base::Result<Core::PropertyValue> value =
                plan.Value().Get(*metadataRuntime, *source);
            if (!value) return value.GetStatus();
            current = std::move(value).Value();
        }
        return DataTemplateTriggerValuesMatch(
            current, condition.value);
    }

    Base::Result<void> EvaluateDataTemplateTrigger(
        Detail::DataTemplateTriggerContext& context,
        std::uint32_t triggerIndex) noexcept {
        if (triggerIndex >= context.triggers.Size() ||
            context.root == nullptr ||
            values == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "DataTemplate Trigger runtime is unavailable");
        }
        Detail::DataTemplatePropertyTrigger& trigger =
            context.triggers[triggerIndex];
        bool active = !trigger.conditions.Empty();
        for (Detail::DataTemplateTriggerCondition& condition :
             trigger.conditions) {
            Base::Result<bool> matches =
                EvaluateDataTemplateCondition(context, condition);
            if (!matches) return matches.GetStatus();
            if (!matches.Value()) {
                active = false;
                break;
            }
        }
        if (active == trigger.active) return {};

        if (active) {
            for (const Detail::DataTemplateTriggerSetter&
                     setter :
                 trigger.setters) {
                if (!setter.target) continue;
                Base::Result<void> applied =
                    values->SetTriggerValue(
                        *setter.target,
                        setter.property,
                        setter.value);
                if (!applied) {
                    return applied.GetStatus();
                }
            }
        } else {
            for (const Detail::DataTemplateTriggerSetter&
                     setter :
                 trigger.setters) {
                if (!setter.target) continue;
                Base::Result<void> cleared =
                    values->ClearTriggerValue(
                        *setter.target,
                        setter.property);
                if (!cleared) {
                    return cleared.GetStatus();
                }
            }
        }

        Base::Span<const Base::Ref<Base::Object>> actions =
            active
            ? trigger.enterActions.AsSpan()
            : trigger.exitActions.AsSpan();
        for (const Base::Ref<Base::Object>& authored :
             actions) {
            if (!authored ||
                !metadata->Types().IsDerivedFrom(
                    authored->RuntimeType(),
                    Animation::TriggerAction::
                        StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "DataTemplate Trigger contains an invalid action");
            }
            Base::Result<void> executed =
                ExecuteAnimationAction(
                    static_cast<
                        Animation::TriggerAction&>(
                            *authored),
                    *context.root,
                    &context);
            if (!executed) {
                return executed.GetStatus();
            }
        }
        trigger.active = active;
        return {};
    }

    Base::Result<std::uint32_t>
    StartDataTemplateTriggers(
        Detail::DataTemplateTriggerContext&
            context) noexcept {
        std::uint32_t count = 0U;
        for (std::uint32_t triggerIndex = 0U;
             triggerIndex < context.triggers.Size();
             ++triggerIndex) {
            Detail::DataTemplatePropertyTrigger&
                trigger =
                    context.triggers[triggerIndex];
            for (std::uint32_t conditionIndex = 0U;
                 conditionIndex <
                     trigger.conditions.Size();
                 ++conditionIndex) {
                Detail::DataTemplateTriggerCondition&
                    condition =
                        trigger.conditions[conditionIndex];
                if ((!condition.dependencySource ||
                     !condition.property.IsValid()) &&
                    condition.binding) {
                    Base::Object* source =
                        condition.binding->
                                ElementName().Empty()
                        ? condition.source.Get()
                        : context.FindName(
                              condition.binding->
                                  ElementName());
                    if (source == nullptr &&
                        !condition.binding->ElementName().Empty()) {
                        source = loadedDocument.names.Find(
                            condition.binding->ElementName());
                    }
                    if (source != nullptr &&
                        metadata->Types().IsDerivedFrom(
                            source->RuntimeType(),
                            Core::DependencyObject::
                                StaticTypeId())) {
                        const Core::DependencyProperty*
                            property =
                                Core::Detail::
                                    MetadataDomainAccess::
                                    DependencyProperties(
                                        *metadata)
                                        .Find(
                                            source->
                                                RuntimeType(),
                                            condition.binding->
                                                Path());
                        if (property != nullptr) {
                            condition.dependencySource =
                                Base::Ref<
                                    Core::
                                        DependencyObject>::
                                    FromBorrowed(
                                        *static_cast<
                                            Core::
                                                DependencyObject*>(
                                                    source));
                            condition.property =
                                property->Handle();
                        }
                    }
                }
                if (!condition.dependencySource ||
                    !condition.property.IsValid()) {
                    continue;
                }
                bool alreadyAttached = false;
                for (const DataTemplateTriggerSubscription&
                         existing :
                     dataTemplateTriggerSubscriptions) {
                    alreadyAttached =
                        alreadyAttached ||
                        (existing.context != nullptr &&
                         existing.context->
                                 triggerContext.Get() ==
                             &context &&
                         existing.context->
                                 triggerIndex ==
                             triggerIndex &&
                         existing.context->
                                 conditionIndex ==
                             conditionIndex);
                }
                if (alreadyAttached) continue;

                DataTemplateTriggerHandlerContext*
                    handlerContext = nullptr;
                Base::Result<void> created =
                    CreateRuntimeObject(
                        *allocator,
                        Base::MemoryTag::Presentation,
                        handlerContext);
                if (!created) {
                    return created.GetStatus();
                }
                handlerContext->runtime = this;
                handlerContext->triggerContext =
                    Base::Ref<
                        Detail::
                            DataTemplateTriggerContext>::
                        FromBorrowed(context);
                handlerContext->triggerIndex =
                    triggerIndex;
                handlerContext->conditionIndex =
                    conditionIndex;
                auto callback =
                    [handlerContext](
                        Core::DependencyObject& object,
                        const Core::
                            DependencyPropertyChangedEventArgs&
                                args) noexcept {
                        handlerContext->Invoke(
                            object, args);
                    };
                Core::DependencyPropertyChangedEventHandler
                    handler(callback);
                Base::Result<void> subscribed =
                    condition.dependencySource->
                        TryAddValueChangedHandler(
                            condition.property,
                            handler);
                if (!subscribed) {
                    DestroyRuntimeObject(
                        *allocator,
                        Base::MemoryTag::Presentation,
                        handlerContext);
                    return subscribed.GetStatus();
                }
                DataTemplateTriggerSubscription record;
                record.source =
                    condition.dependencySource.Get();
                record.property = condition.property;
                record.handler = handler;
                record.context = handlerContext;
                Base::Result<void> retained =
                    dataTemplateTriggerSubscriptions.
                        TryPushBack(std::move(record));
                if (!retained) {
                    static_cast<void>(
                        condition.dependencySource->
                            RemoveValueChangedHandler(
                                condition.property,
                                handler));
                    DestroyRuntimeObject(
                        *allocator,
                        Base::MemoryTag::Presentation,
                        handlerContext);
                    return retained.GetStatus();
                }
                ++count;
            }
            Base::Result<void> evaluated =
                EvaluateDataTemplateTrigger(
                    context, triggerIndex);
            if (!evaluated) {
                return evaluated.GetStatus();
            }
        }
        return count;
    }

    Base::Result<std::uint32_t> StartLoadedAnimations(
        Presentation::Visual* visual,
        const Presentation::NameScope* names = nullptr) noexcept {
        if (visual == nullptr) return std::uint32_t{0U};
        std::uint32_t count = 0U;
        Presentation::FrameworkElement* element =
            visual->AsFrameworkElement();
        if (element != nullptr) {
            if (commands != nullptr &&
                metadata->Types().IsDerivedFrom(
                    element->RuntimeType(),
                    Controls::Grid::StaticTypeId())) {
                auto& grid = static_cast<Controls::Grid&>(*element);
                for (const Base::Ref<Presentation::KeyBinding>& binding :
                     grid.InputBindings()) {
                    if (!binding) continue;
                    Base::Result<Presentation::InputBindingHandle> added =
                        commands->TryAddInputBinding(*element, binding);
                    if (!added) return added.GetStatus();
                }
            }
            for (const Base::Ref<Base::Object>& authored :
                 element->AuthoredTriggers()) {
                if (!authored) {
                    continue;
                }
                if (authored->RuntimeType() ==
                    Detail::
                        DataTemplateTriggerContext::
                            StaticTypeId()) {
                    Base::Result<std::uint32_t> started =
                        StartDataTemplateTriggers(
                            static_cast<
                                Detail::
                                    DataTemplateTriggerContext&>(
                                        *authored));
                    if (!started) {
                        return started.GetStatus();
                    }
                    if (count >
                        UINT32_MAX - started.Value()) {
                        return Base::Status::Failure(
                            Base::ErrorCode::OutOfRange,
                            "DataTemplate Trigger subscription count overflow");
                    }
                    count += started.Value();
                    continue;
                }
                if (authored->RuntimeType() ==
                    Animation::StoryboardCompletedTrigger::
                        StaticTypeId()) {
                    Base::Result<void> retained =
                        storyboardCompletedSubscriptions.
                            TryPushBack({
                                static_cast<
                                    Animation::
                                        StoryboardCompletedTrigger*>(
                                            authored.Get()),
                                element,
                                names});
                    if (!retained) {
                        return retained.GetStatus();
                    }
                    continue;
                }
                if (authored->RuntimeType() !=
                    Animation::EventTrigger::StaticTypeId()) {
                    continue;
                }
                auto& trigger =
                    static_cast<Animation::EventTrigger&>(*authored);
                const Base::StringView routedEvent =
                    trigger.RoutedEvent();
                Base::Object* eventSource =
                    trigger.SourceName().Empty()
                    ? static_cast<Base::Object*>(
                          element)
                    : names != nullptr
                        ? names->Find(trigger.SourceName())
                        : loadedDocument.names.Find(
                              trigger.SourceName());
                if (eventSource == nullptr ||
                    !metadata->Types().IsDerivedFrom(
                        eventSource->RuntimeType(),
                        Presentation::UIElement::
                            StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "EventTrigger SourceName did not resolve to a UIElement");
                }
                auto* eventElement =
                    static_cast<
                        Presentation::UIElement*>(
                            eventSource);
                if (routedEvent != Base::StringView("Loaded") &&
                    routedEvent !=
                        Base::StringView("FrameworkElement.Loaded")) {
                    Base::StringView eventName = routedEvent;
                    std::uint32_t dot = UINT32_MAX;
                    for (std::uint32_t index = 0U;
                         index < eventName.SizeBytes(); ++index) {
                        if (eventName[index] == '.') dot = index;
                    }
                    if (dot != UINT32_MAX) {
                        eventName = eventName.Substr(
                            dot + 1U,
                            eventName.SizeBytes() - dot - 1U);
                    }
                    const Core::EventInfo* event =
                        metadata->Types().FindEvent(
                            eventElement->RuntimeType(),
                            eventName,
                            true);
                    if (event == nullptr) {
                        return Base::Status::Failure(
                            Base::ErrorCode::NotFound,
                            "EventTrigger RoutedEvent was not found on its source");
                    }
                    const Core::RoutedEventHandle eventHandle{
                        event->Id()};
                    AnimationEventContext* eventContext = nullptr;
                    Base::Result<void> created =
                        CreateRuntimeObject(
                            *allocator,
                            Base::MemoryTag::Presentation,
                            eventContext);
                    if (!created) return created.GetStatus();
                    eventContext->runtime = this;
                    eventContext->trigger = &trigger;
                    eventContext->owner = element;
                    eventContext->names = names;
                    auto callback =
                        [eventContext](
                            Base::Object* sender,
                            const Presentation::RoutedEventArgs& args) noexcept {
                            eventContext->Invoke(sender, args);
                        };
                    Presentation::RoutedEventHandler handler(callback);
                    Base::Result<void> subscribed =
                        eventElement->TryAddHandler(
                            eventHandle, handler);
                    if (!subscribed) {
                        DestroyRuntimeObject(
                            *allocator,
                            Base::MemoryTag::Presentation,
                            eventContext);
                        return subscribed.GetStatus();
                    }
                    AnimationEventSubscription subscription;
                    subscription.owner =
                        eventElement;
                    subscription.event = eventHandle;
                    subscription.handler = handler;
                    subscription.context = eventContext;
                    Base::Result<void> retained =
                        animationEventSubscriptions.TryPushBack(
                            std::move(subscription));
                    if (!retained) {
                        static_cast<void>(
                            element->RemoveHandler(
                                eventHandle, handler));
                        DestroyRuntimeObject(
                            *allocator,
                            Base::MemoryTag::Presentation,
                            eventContext);
                        return retained.GetStatus();
                    }
                    continue;
                }
                for (const Base::Ref<
                         Animation::TriggerAction>& action :
                     trigger.Actions()) {
                    if (!action) continue;
                    Base::Result<void> executed =
                        ExecuteAnimationAction(
                            *action, *element, nullptr, names);
                    if (!executed) {
                        return executed.GetStatus();
                    }
                    if (count == UINT32_MAX) {
                        return Base::Status::Failure(
                            Base::ErrorCode::OutOfRange,
                            "Loaded animation count overflow");
                    }
                    ++count;
                }
            }
        }
        for (Presentation::Visual* child :
             visual->VisualChildren()) {
            Base::Result<std::uint32_t> started =
                StartLoadedAnimations(child, names);
            if (!started) return started.GetStatus();
            if (count > UINT32_MAX - started.Value()) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Loaded animation count overflow");
            }
            count += started.Value();
        }
        return count;
    }

    bool IsInVisualSubtree(
        Presentation::Visual* node,
        const Presentation::Visual& fragmentRoot) const noexcept {
        while (node != nullptr) {
            if (node == &fragmentRoot) return true;
            node = node->LogicalParent() != nullptr
                ? node->LogicalParent()
                : node->VisualParent();
        }
        return false;
    }

    void ClearAnimationSubscriptionsFor(
        Presentation::Visual& fragmentRoot) noexcept {
        for (std::uint32_t index = 0U;
             index < dataTemplateTriggerSubscriptions.Size();) {
            DataTemplateTriggerSubscription& subscription =
                dataTemplateTriggerSubscriptions[index];
            const bool sourceMatches =
                subscription.source != nullptr &&
                metadata->Types().IsDerivedFrom(
                    subscription.source->RuntimeType(),
                    Presentation::Visual::StaticTypeId()) &&
                IsInVisualSubtree(
                    static_cast<Presentation::Visual*>(
                        subscription.source), fragmentRoot);
            const bool contextMatches =
                subscription.context != nullptr &&
                subscription.context->triggerContext &&
                subscription.context->triggerContext->root != nullptr &&
                IsInVisualSubtree(
                    subscription.context->
                        triggerContext->root,
                    fragmentRoot);
            const bool matches =
                sourceMatches || contextMatches;
            if (!matches) {
                ++index;
                continue;
            }
            static_cast<void>(
                subscription.source->RemoveValueChangedHandler(
                    subscription.property, subscription.handler));
            DestroyRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                subscription.context);
            for (std::uint32_t next = index + 1U;
                 next < dataTemplateTriggerSubscriptions.Size(); ++next) {
                dataTemplateTriggerSubscriptions[next - 1U] =
                    std::move(dataTemplateTriggerSubscriptions[next]);
            }
            dataTemplateTriggerSubscriptions.PopBack();
        }
        for (std::uint32_t index = 0U;
             index < animationEventSubscriptions.Size();) {
            AnimationEventSubscription& subscription =
                animationEventSubscriptions[index];
            if (subscription.owner == nullptr ||
                !IsInVisualSubtree(
                    subscription.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            static_cast<void>(subscription.owner->RemoveHandler(
                subscription.event, subscription.handler));
            DestroyRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                subscription.context);
            for (std::uint32_t next = index + 1U;
                 next < animationEventSubscriptions.Size(); ++next) {
                animationEventSubscriptions[next - 1U] =
                    std::move(animationEventSubscriptions[next]);
            }
            animationEventSubscriptions.PopBack();
        }
        for (std::uint32_t index = 0U;
             index < storyboardSessions.Size();) {
            StoryboardSession& session = storyboardSessions[index];
            if (session.owner == nullptr ||
                !IsInVisualSubtree(session.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            CancelStoryboardCompletionSessions(session.handles.AsSpan());
            if (animations != nullptr) {
                for (Presentation::AnimationHandle handle : session.handles) {
                    static_cast<void>(animations->Remove(handle));
                }
            }
            for (std::uint32_t next = index + 1U;
                 next < storyboardSessions.Size(); ++next) {
                storyboardSessions[next - 1U] =
                    std::move(storyboardSessions[next]);
            }
            storyboardSessions.PopBack();
        }
        for (std::uint32_t index = 0U;
             index < storyboardCompletedSubscriptions.Size();) {
            const StoryboardCompletedSubscription& subscription =
                storyboardCompletedSubscriptions[index];
            if (subscription.owner == nullptr ||
                !IsInVisualSubtree(
                    subscription.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            for (std::uint32_t next = index + 1U;
                 next < storyboardCompletedSubscriptions.Size(); ++next) {
                storyboardCompletedSubscriptions[next - 1U] =
                    std::move(storyboardCompletedSubscriptions[next]);
            }
            storyboardCompletedSubscriptions.PopBack();
        }
    }

    void ClearAnimationEventSubscriptions() noexcept {
        for (DataTemplateTriggerSubscription&
                 subscription :
             dataTemplateTriggerSubscriptions) {
            if (subscription.source != nullptr) {
                static_cast<void>(
                    subscription.source->
                        RemoveValueChangedHandler(
                            subscription.property,
                            subscription.handler));
            }
            DestroyRuntimeObject(
                *allocator,
                Base::MemoryTag::Presentation,
                subscription.context);
        }
        dataTemplateTriggerSubscriptions.Clear();
        for (AnimationEventSubscription& subscription :
             animationEventSubscriptions) {
            if (subscription.owner != nullptr) {
                static_cast<void>(
                    subscription.owner->RemoveHandler(
                        subscription.event,
                        subscription.handler));
            }
            DestroyRuntimeObject(
                *allocator,
                Base::MemoryTag::Presentation,
                subscription.context);
        }
        animationEventSubscriptions.Clear();
        storyboardCompletionSessions.Clear();
        storyboardCompletedSubscriptions.Clear();
        animationEventStatus = Base::Status::Ok();
    }

    void ClearRuntimeEvents(
        Presentation::Visual* node) noexcept {
        if (node == nullptr) return;
        if (metadata->Types().IsDerivedFrom(
                node->RuntimeType(),
                Controls::Control::StaticTypeId())) {
            Detail::ControlRuntimeAccess::Attach(
                *static_cast<Controls::Control*>(node),
                nullptr);
        }
        for (Presentation::Visual* child :
             node->VisualChildren()) {
            ClearRuntimeEvents(child);
        }
    }

    void BeginDestroyInteractions() noexcept {
        CloseAllOverlays();
        ClearOverlays();
        ClearAnimationEventSubscriptions();
        if (activeToolTip) {
            static_cast<void>(
                activeToolTip->SetIsOpen(false));
        }
        pendingToolTip.Reset();
        activeToolTip.Reset();
        toolTipTarget.Reset();
        ClearTextInputHosts(RootVisual());
        ClearRuntimeEvents(RootVisual());
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            menuInteractions);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            treeViewInteractions);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            comboBoxInteractions);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            listBoxInteractions);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            sliderInteractions);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            scrollInteractions);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            textBoxInteractions);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            controlInteractions);
    }

    void FinishDestroyInteractions() noexcept {
        while (!itemGenerators.Empty()) {
            Controls::ItemContainerGenerator*
                generator = itemGenerators.Back();
            itemGenerators.PopBack();
            if (generator != nullptr) {
                static_cast<void>(
                    generator->Detach());
                DestroyRuntimeObject(
                    *allocator,
                    Base::MemoryTag::Presentation,
                    generator);
            }
        }
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            textInput);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            keyboard);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            pointer);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            focus);
    }

    void DestroyInteractions() noexcept {
        BeginDestroyInteractions();
        FinishDestroyInteractions();
    }

    Base::Result<void> CreateInteractions() noexcept {
        Presentation::Visual* rootVisual = RootVisual();
        if (rootVisual == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Runtime root is not a registered Visual");
        }
        Base::Result<void> status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            focus, *tree, *events);
        if (!status) return status.GetStatus();
        status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            pointer, hitTests, *events, *rootVisual);
        if (!status) return status.GetStatus();
        status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            keyboard, *focus, *events, *tree, commands);
        if (!status) return status.GetStatus();
        status = CreateRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            textInput, *focus, *events, *tree);
        if (!status) return status.GetStatus();

        if (options.attachControlInteractions) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                controlInteractions,
                *tree, *events, *pointer, *focus,
                *commands, visualStates);
            if (!status) return status.GetStatus();
            status = controlInteractions->Initialize();
            if (!status) return status.GetStatus();
            status = CreateRuntimeObject(
                *allocator,
                Base::MemoryTag::Presentation,
                scrollInteractions,
                *tree,
                *events);
            if (!status) return status.GetStatus();
            status = CreateRuntimeObject(
                *allocator,
                Base::MemoryTag::Presentation,
                sliderInteractions,
                *tree,
                *events,
                *pointer,
                *focus);
            if (!status) return status.GetStatus();
            status = CreateRuntimeObject(
                *allocator,
                Base::MemoryTag::Presentation,
                listBoxInteractions,
                *tree,
                *events,
                *focus,
                visualStates);
            if (!status) return status.GetStatus();
            status = CreateRuntimeObject(
                *allocator,
                Base::MemoryTag::Presentation,
                comboBoxInteractions,
                *tree,
                *events,
                *focus);
            if (!status) return status.GetStatus();
            status = CreateRuntimeObject(
                *allocator,
                Base::MemoryTag::Presentation,
                treeViewInteractions,
                *tree,
                *events,
                *focus,
                visualStates);
            if (!status) return status.GetStatus();
            status = CreateRuntimeObject(
                *allocator,
                Base::MemoryTag::Presentation,
                menuInteractions,
                *tree,
                *events,
                *focus,
                *commands);
            if (!status) return status.GetStatus();
        }
        if (options.attachTextEditing &&
            options.clipboard != nullptr) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                textBoxInteractions,
                *tree, *events, *pointer, *focus,
                *options.clipboard);
            if (!status) return status.GetStatus();
        }
        status = VisitAndAttach(*rootVisual);
        if (!status) {
            return status.GetStatus();
        }
        return {};
    }

    void ShutdownServices() noexcept {
        BeginDestroyInteractions();
        DetachRuntimePresentation();
        FinishDestroyInteractions();
        static_cast<void>(UnmountAllFragments());
        DestroyTemplateServices();
        if (imageRuntime != nullptr) {
            imageRuntime->Shutdown(ImageServices());
        }
        if (tree != nullptr) {
            tree->SetLifecycleHandler(nullptr);
        }
        VisitTextServices(RootVisual(), nullptr);
        VisitPathServices(RootVisual(), nullptr);
        if (animations != nullptr) {
            static_cast<void>(animations->RemoveAll());
        }
        storyboardSessions.Clear();
        if (visualMount != nullptr && visualMount->IsMounted()) {
            static_cast<void>(visualMount->Unmount({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
        }
        mounted = false;
        root.Reset();
        ClearLoadedDocument();
        if (effectLifetime) effectLifetime->Invalidate();

        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation, visualMount);

        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            commands);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            events);
        if (bindings != nullptr) bindings->Shutdown();
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            bindings);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            renderer);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            layout);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            tree);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Render,
            textRuntime);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Render,
            imageRuntime);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            animations);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            values);
        DestroyRuntimeObject(
            *allocator, Base::MemoryTag::Presentation,
            objectServices);
        schema = nullptr;
        metadataRuntime = nullptr;
        metadata = nullptr;
        endpointBackend.Reset();
        if (endpointBound && endpoint) {
            Integration::Detail::RenderEndpointAccess::Unbind(
                *endpoint, this);
        }
        endpointBound = false;
        endpoint.Reset();
        initialized = false;
    }

    Base::Result<void> InitializeServices(
        const ViewRuntimeOptions& requested) noexcept {
        if (initialized) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "ViewRuntime is already initialized");
        }
        if (terminal) {
            return RuntimeInvalidState(
                "ViewRuntime cannot be restarted after shutdown or failed startup");
        }
        options = requested;

        endpoint = options.renderEndpoint;
        if (!endpoint) {
            Base::Result<Base::Ref<Integration::RenderEndpoint>>
                headless =
                    Integration::Detail::RenderEndpointAccess::
                        CreateHeadless(
                            Integration::RenderSubmissionMode::Immediate,
                            allocator);
            if (!headless) {
                terminal = true;
                return headless.GetStatus();
            }
            endpoint = std::move(headless).Value();
        }
        Base::Result<void> endpointBinding =
            Integration::Detail::RenderEndpointAccess::Bind(
                *endpoint, this);
        if (!endpointBinding) {
            endpoint.Reset();
            terminal = true;
            return endpointBinding.GetStatus();
        }
        endpointBound = true;
        endpointGeneration = endpoint->Generation();
        endpointBackend.SetEndpoint(endpoint);

        Base::Result<Base::Ref<Markup::EffectLifetime>> lifetime =
            Base::MakeRefWithAllocator<Markup::EffectLifetime>(
                *allocator);
        Base::Result<void> status = lifetime
            ? Base::Result<void>()
            : Base::Result<void>(lifetime.GetStatus());
        if (status) effectLifetime = std::move(lifetime).Value();
        if (status) status = EnsureDefaultXamlProviders();
        if (status && !schemaBundle->IsPrepared()) {
            status = schemaBundle->Prepare(modules);
        }
        if (!status || !schemaBundle->IsPrepared()) {
            terminal = true;
            return status
                ? RuntimeInvalidState(
                      "ViewRuntime schema bundle was not prepared")
                : status.GetStatus();
        }
        metadata = &schemaBundle->Metadata();
        metadataRuntime = &schemaBundle->Runtime();

        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                objectServices, dispatcher,
                Core::Detail::MetadataDomainAccess::
                    DependencyProperties(*metadata),
                *metadataRuntime);
        }
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                values, dispatcher,
                Core::Detail::MetadataDomainAccess::
                    DependencyProperties(*metadata));
        }
        if (status) status = values->Initialize();
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                animations, dispatcher, *values, allocator);
        }
        if (status) status = animations->Initialize();
        if (status) {
            animations->SetAutomaticTickingEnabled(
                options.automaticAnimationClock);
        }
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                tree, dispatcher, *values);
        }
        if (status) status = tree->Initialize();
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                layout, dispatcher);
        }
        if (status) status = layout->Initialize();
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                renderer, dispatcher, SelectedBackend());
        }
        if (status) status = renderer->Initialize();
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Render,
                imageRuntime, allocator);
        }
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Render,
                textRuntime, allocator);
        }
        if (status) {
            status = textRuntime->Initialize(
                SelectedBackend(), options.text);
        }
        if (status) {
            tree->SetLifecycleHandler(
                &TextLifecycleHook, this);
        }
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                bindings, dispatcher);
        }
        if (status) status = bindings->Initialize();
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                events,
                Core::Detail::MetadataDomainAccess::
                    RoutedEventState(*metadata));
        }
        if (status) {
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                commands, *tree);
        }
        if (status) status = CreateTemplateServices();
        if (status) {
            status = RebuildDynamicResourceEnvironment();
        }
        if (status && !schemaBundle->IsFrozen()) {
            status = schemaBundle->Finalize(
                SchemaBundleServices{allocator});
        }
        if (status && schemaBundle->IsFrozen()) {
            schema = &schemaBundle->Schema();
            status = CreateRuntimeObject(
                *allocator, Base::MemoryTag::Presentation,
                visualMount, *tree, *layout, renderer);
        }
        if (!status) {
            ShutdownServices();
            terminal = true;
            return status.GetStatus();
        }
        initialized = true;
        return {};
    }

    Base::Result<void> CommitResourceLayer(
        UiDocument document,
        Presentation::ResourceDictionary& target,
        bool merge) noexcept {
        const Base::Ref<Base::Object>& rootObject =
            document.Root();
        if (!rootObject ||
            rootObject->RuntimeType() !=
                Presentation::ResourceDictionary::StaticTypeId()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "ViewRuntime resource document root must be ResourceDictionary");
        }
        auto& dictionary =
            static_cast<Presentation::ResourceDictionary&>(
                *rootObject);
        if (merge) {
            Base::Result<void> merged =
                target.TryAddMerged(dictionary);
            if (!merged) return merged.GetStatus();
            Base::Result<void> rebuilt =
                RebuildDynamicResourceEnvironment();
            if (rebuilt) return {};
            Base::Result<bool> removed =
                target.RemoveMerged(dictionary);
            Base::Result<void> restored =
                removed && removed.Value()
                ? RebuildDynamicResourceEnvironment()
                : Base::Result<void>(
                      removed
                      ? Base::Status::Failure(
                            Base::ErrorCode::InvalidState,
                            "ViewRuntime resource merge rollback lost its dictionary")
                      : removed.GetStatus());
            return restored
                ? Base::Result<void>(rebuilt.GetStatus())
                : restored;
        }

        Presentation::ResourceDictionary previous =
            std::move(target);
        target = std::move(dictionary);
        Base::Result<void> rebuilt =
            RebuildDynamicResourceEnvironment();
        if (rebuilt) return {};
        target = std::move(previous);
        Base::Result<void> restored =
            RebuildDynamicResourceEnvironment();
        return restored
            ? Base::Result<void>(rebuilt.GetStatus())
            : restored;
    }

    Base::Result<void> LoadResourceLayer(
        Base::StringView uri,
        Presentation::ResourceDictionary& target,
        Core::IDiagnosticSink* diagnostics,
        bool merge = false) noexcept {
        if (!initialized) {
            return RuntimeNotInitialized(
                "ViewRuntime must be initialized before loading resources");
        }
        if (mounted || root || loadedDocument.root) {
            return RuntimeInvalidState(
                "ViewRuntime resource layers must be loaded before a document");
        }
        Base::Result<Markup::LoadOptions> loadOptions =
            LoadOptions();
        if (!loadOptions) {
            return loadOptions.GetStatus();
        }
        Markup::Loader loader(
            *schema,
            xamlSources,
            diagnostics,
            allocator);
        Base::Result<UiDocument> loaded =
            loader.Load(uri, loadOptions.Value());
        if (!loaded) {
            return loaded.GetStatus();
        }
        return CommitResourceLayer(
            std::move(loaded).Value(),
            target,
            merge);
    }

    Base::Result<void> LoadCompiledResourceLayer(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        Presentation::ResourceDictionary& target,
        bool merge = false) noexcept {
        if (!initialized) {
            return RuntimeNotInitialized(
                "ViewRuntime must be initialized before loading resources");
        }
        if (mounted || root || loadedDocument.root) {
            return RuntimeInvalidState(
                "ViewRuntime resource layers must be loaded before a document");
        }
        Base::Result<Markup::LoadOptions> loadOptions =
            LoadOptions();
        if (!loadOptions) return loadOptions.GetStatus();
        Markup::Loader loader(
            *schema, xamlSources, nullptr, allocator);
        Base::Result<UiDocument> loaded =
            loader.LoadCompiled(
                bytes, originUri, loadOptions.Value());
        if (!loaded) return loaded.GetStatus();

        return CommitResourceLayer(
            std::move(loaded).Value(),
            target,
            merge);
    }

    Base::Result<void> ValidateDocumentRoot(
        const Base::Ref<Base::Object>& requestedRoot) noexcept {
        if (!requestedRoot) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "ViewRuntime root must not be null");
        }
        if (!metadata->Types().IsDerivedFrom(
                requestedRoot->RuntimeType(),
                Presentation::Visual::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "ViewRuntime root must derive from Visual");
        }
        Base::Result<Presentation::UIElement*> rootLayout =
            ResolveUIElement(*requestedRoot, requestedRoot->RuntimeType());
        return rootLayout
            ? Base::Result<void>()
            : Base::Result<void>(rootLayout.GetStatus());
    }

    Base::Result<void> MountRoot(
        Base::Ref<Base::Object> requestedRoot,
        Presentation::Size availableSize) noexcept {
        if (!initialized) {
            return RuntimeNotInitialized(
                "ViewRuntime must be initialized before mounting");
        }
        if (mounted || root) {
            return RuntimeInvalidState(
                "ViewRuntime already has a mounted root");
        }
        Base::Result<void> validRoot = ValidateDocumentRoot(requestedRoot);
        if (!validRoot) return validRoot.GetStatus();
        if (loadedDocument.root &&
            loadedDocument.root.Get() != requestedRoot.Get()) {
            return RuntimeInvalidState(
                "Mounted root does not match the staged XAML document");
        }
        Base::Result<Presentation::Visual*> rootVisual =
            ResolveVisual(*requestedRoot, requestedRoot->RuntimeType());
        if (!rootVisual) return rootVisual.GetStatus();
        Base::Result<Presentation::UIElement*> rootLayout =
            ResolveUIElement(*requestedRoot, requestedRoot->RuntimeType());
        if (!rootLayout) return rootLayout.GetStatus();
        Base::Result<void> rootTracked =
            loadedDocument.visualContent.TryAddNode(*rootVisual.Value());
        if (!rootTracked) return rootTracked.GetStatus();
        if (visualMount == nullptr) {
            return RuntimeNotInitialized(
                "ViewRuntime visual mount service is unavailable");
        }
        Base::Result<void> mountedResult = visualMount->Mount(
            *rootVisual.Value(),
            *rootLayout.Value(),
            ResolveFrameworkElement(*requestedRoot, requestedRoot->RuntimeType()),
            {loadedDocument.visualContent.mountEdges.Data(),
             loadedDocument.visualContent.mountEdges.Size()},
            availableSize);
        if (!mountedResult) return mountedResult.GetStatus();
        root = std::move(requestedRoot);
        mounted = true;
        Base::Result<void> presentation =
            presentationServices.Apply(*rootVisual.Value());
        if (!presentation) {
            DetachRuntimePresentation();
            static_cast<void>(visualMount->Unmount({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return presentation.GetStatus();
        }
        Base::Result<void> interactions =
            CreateInteractions();
        if (!interactions) {
            BeginDestroyInteractions();
            DetachRuntimePresentation();
            FinishDestroyInteractions();
            static_cast<void>(visualMount->Unmount({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return interactions.GetStatus();
        }
        Base::Result<void> completed =
            visualMount->CompleteDeferredEdges({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()});
        if (!completed) {
            BeginDestroyInteractions();
            DetachRuntimePresentation();
            FinishDestroyInteractions();
            static_cast<void>(visualMount->Unmount({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return completed.GetStatus();
        }
        presentation =
            presentationServices.Apply(
                *rootVisual.Value());
        if (!presentation) {
            BeginDestroyInteractions();
            DetachRuntimePresentation();
            FinishDestroyInteractions();
            static_cast<void>(visualMount->Unmount({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return presentation.GetStatus();
        }
        Base::Result<void> effects = loadedDocument.effects.Commit();
        if (!effects) {
            BeginDestroyInteractions();
            DetachRuntimePresentation();
            FinishDestroyInteractions();
            static_cast<void>(visualMount->Unmount({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return effects.GetStatus();
        }
        Base::Result<std::uint32_t> startedAnimations =
            StartLoadedAnimations(rootVisual.Value());
        if (!startedAnimations) {
            if (animations != nullptr) {
                static_cast<void>(animations->RemoveAll());
            }
            storyboardSessions.Clear();
            BeginDestroyInteractions();
            DetachRuntimePresentation();
            FinishDestroyInteractions();
            static_cast<void>(visualMount->Unmount({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return startedAnimations.GetStatus();
        }
        return {};
    }

    Base::Result<void> DetachFragment(
        FragmentMount& fragment) noexcept {
        if (!fragment.document.root) return {};
        Base::Result<Presentation::Visual*> rootVisual =
            ResolveVisual(
                *fragment.document.root,
                fragment.document.root->RuntimeType());
        if (!rootVisual) return rootVisual.GetStatus();
        ClearAnimationSubscriptionsFor(*rootVisual.Value());

        presentationServices.Detach(
            rootVisual.Value(),
            {fragment.document.visualContent.nodes.Data(),
             fragment.document.visualContent.nodes.Size()});

        Presentation::MountService mounts(
            *tree, layout, renderer);
        std::uint32_t remaining = 0U;
        for (const Presentation::VisualTreeMountEdge& edge :
             fragment.document.visualContent.mountEdges) {
            if (edge.state.IsAttached()) ++remaining;
        }
        while (remaining > 0U) {
            bool progressed = false;
            for (Presentation::VisualTreeMountEdge& edge :
                 fragment.document.visualContent.mountEdges) {
                if (!edge.state.IsAttached()) continue;
                bool hasMountedChild = false;
                for (const Presentation::VisualTreeMountEdge& candidate :
                     fragment.document.visualContent.mountEdges) {
                    if (candidate.state.IsAttached() &&
                        candidate.parent == edge.child) {
                        hasMountedChild = true;
                        break;
                    }
                }
                if (hasMountedChild) continue;
                Base::Result<void> detached = mounts.Detach(edge.state);
                if (!detached) return detached.GetStatus();
                --remaining;
                progressed = true;
            }
            if (!progressed) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "content fragment edges cannot be detached leaf-first");
            }
        }
        if (fragment.rootEdge.IsAttached()) {
            Base::Result<void> detached = mounts.Detach(fragment.rootEdge);
            if (!detached) return detached.GetStatus();
        }
        if (fragment.host != nullptr) {
            Base::Result<void> cleared = fragment.host->SetContent(nullptr);
            if (!cleared) return cleared.GetStatus();
        }
        fragment.document.Clear();
        return {};
    }

    Base::Result<void> UnmountFragmentAt(
        std::uint32_t index) noexcept {
        if (index >= fragmentMounts.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "content fragment index is out of range");
        }
        Base::Result<void> detached = DetachFragment(fragmentMounts[index]);
        if (!detached) return detached.GetStatus();
        for (std::uint32_t next = index + 1U;
             next < fragmentMounts.Size(); ++next) {
            fragmentMounts[next - 1U] =
                std::move(fragmentMounts[next]);
        }
        fragmentMounts.PopBack();
        return {};
    }

    Base::Result<void> UnmountAllFragments() noexcept {
        while (!fragmentMounts.Empty()) {
            Base::Result<void> detached =
                UnmountFragmentAt(fragmentMounts.Size() - 1U);
            if (!detached) return detached.GetStatus();
        }
        return {};
    }

    Base::Result<void> DetachMountedRoot(
        bool clearDocument) noexcept {
        if (!initialized) return {};
        if (!mounted) {
            if (clearDocument && loadedDocument.root) {
                ClearLoadedDocument();
            }
            return {};
        }
        if (animations != nullptr) {
            Base::Result<void> removed =
                animations->RemoveAll();
            if (!removed) return removed.GetStatus();
        }
        storyboardSessions.Clear();
        BeginDestroyInteractions();
        DetachRuntimePresentation();
        FinishDestroyInteractions();
        Base::Result<void> fragments = UnmountAllFragments();
        if (!fragments) return fragments.GetStatus();
        Base::Result<void> unmounted =
            visualMount->Unmount({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()});
        mounted = false;
        root.Reset();
        if (clearDocument) ClearLoadedDocument();
        return unmounted;
    }

    Base::Result<void> UnmountRoot() noexcept {
        return DetachMountedRoot(true);
    }
};

void ViewRuntime::Impl::
DataTemplateTriggerHandlerContext::Invoke(
    Core::DependencyObject&,
    const Core::DependencyPropertyChangedEventArgs&)
    noexcept {
    if (runtime == nullptr ||
        !triggerContext ||
        !runtime->animationEventStatus.IsOk()) {
        return;
    }
    Base::Result<void> evaluated =
        runtime->EvaluateDataTemplateTrigger(
            *triggerContext,
            triggerIndex);
    if (!evaluated) {
        runtime->animationEventStatus =
            evaluated.GetStatus();
    }
}

Base::Result<void>
ViewRuntime::Impl::ExecuteAnimationAction(
    Animation::TriggerAction& action,
    Presentation::FrameworkElement& owner,
    Detail::DataTemplateTriggerContext*
        dataTemplateContext,
    const Presentation::NameScope* names) noexcept {
    const Core::TypeId type =
        action.RuntimeType();
    if (type ==
        Animation::ChangePropertyAction::StaticTypeId()) {
        auto& change =
            static_cast<Animation::ChangePropertyAction&>(
                action);
        Base::Object* targetObject =
            change.TargetName().Empty()
            ? static_cast<Base::Object*>(&owner)
            : dataTemplateContext != nullptr
                ? dataTemplateContext->FindName(
                      change.TargetName())
                : names != nullptr
                    ? names->Find(change.TargetName())
                    : loadedDocument.names.Find(
                          change.TargetName());
        if (targetObject == nullptr ||
            !metadata->Types().IsDerivedFrom(
                targetObject->RuntimeType(),
                Core::DependencyObject::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "ChangePropertyAction TargetName did not resolve to a DependencyObject");
        }
        auto& target =
            static_cast<Core::DependencyObject&>(
                *targetObject);
        Base::Result<ResolvedAnimationProperty> resolved =
            ResolveAnimationProperty(
                target, change.PropertyName());
        if (!resolved) return resolved.GetStatus();

        Core::DependencyObject& propertyTarget =
            *resolved.Value().target;
        const Core::DependencyPropertyHandle propertyHandle =
            resolved.Value().property;
        const Core::DependencyProperty* property =
            Core::Detail::MetadataDomainAccess::
                DependencyProperties(*metadata)
                    .Find(propertyHandle);
        if (property == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "ChangePropertyAction property metadata was not found");
        }

        Core::PropertyValue value = change.Value();
        if (value.Kind() == Core::ValueKind::String &&
            value.Type() != property->ValueType()) {
            Base::Result<Core::Value> converted =
                metadataRuntime->TryConvertText(
                    property->ValueType(),
                    value.AsString());
            if (!converted) return converted.GetStatus();
            value = std::move(converted).Value();
        } else if (value.IsNullObject() &&
                   value.Type() != property->ValueType()) {
            value = Core::PropertyValue::NullObject(
                property->ValueType());
        }
        if (property->ValueType() ==
                Presentation::Brush::StaticTypeId() &&
            value.Type() == Core::TypeOf<Base::Color>()) {
            Base::Result<Base::Color> color =
                Core::ValueCodec<Base::Color>::Decode(
                    value);
            if (!color) return color.GetStatus();
            Base::Result<
                Base::Ref<Presentation::Brush>>
                brush =
                    Presentation::MakeSolidColorBrush(
                        color.Value());
            if (!brush) return brush.GetStatus();
            value = Core::PropertyValue::FromObject(
                Presentation::Brush::StaticTypeId(),
                Base::Ref<Base::Object>(
                    std::move(brush).Value()));
        }
        return propertyTarget.SetCurrentValue(
            propertyHandle, value);
    }

    if (type == Animation::SetFocusAction::StaticTypeId()) {
        auto& setFocus = static_cast<Animation::SetFocusAction&>(action);
        if (!setFocus.Engage() || focus == nullptr) return {};
        Presentation::UIElement* target = owner.AsUIElement();
        if (target == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "SetFocusAction owner is not a UIElement");
        }
        Base::Result<bool> focused = focus->SetFocus(target);
        return focused
            ? Base::Result<void>()
            : Base::Result<void>(focused.GetStatus());
    }

    if (type == Animation::RemoveElementAction::StaticTypeId()) {
        auto& remove = static_cast<Animation::RemoveElementAction&>(action);
        Base::Object* targetObject = static_cast<Base::Object*>(&owner);
        Base::Ref<Presentation::BindingSpec> targetBinding =
            remove.TargetObject();
        if (targetBinding) {
            if (targetBinding->RelativeSource() !=
                    Presentation::BindingRelativeSource::Ancestor ||
                targetBinding->AncestorType() !=
                    Base::StringView("ContextMenu") ||
                targetBinding->Path() !=
                    Base::StringView("PlacementTarget")) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "RemoveElementAction TargetObject binding is not supported");
            }
            Presentation::Visual* current = &owner;
            Controls::ContextMenu* contextMenu = nullptr;
            while (current != nullptr) {
                if (metadata->Types().IsDerivedFrom(
                        current->RuntimeType(),
                        Controls::ContextMenu::StaticTypeId())) {
                    contextMenu = static_cast<Controls::ContextMenu*>(
                        current);
                    break;
                }
                current = current->LogicalParent() != nullptr
                    ? current->LogicalParent()
                    : current->VisualParent();
            }
            if (contextMenu == nullptr ||
                !contextMenu->PlacementTarget()) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "RemoveElementAction ContextMenu PlacementTarget was not found");
            }
            targetObject = contextMenu->PlacementTarget().Get();
        }
        if (targetObject == nullptr ||
            !metadata->Types().IsDerivedFrom(
                targetObject->RuntimeType(),
                Presentation::UIElement::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "RemoveElementAction target is not a UIElement");
        }
        auto& target = static_cast<Presentation::UIElement&>(*targetObject);
        Presentation::Visual* current = target.LogicalParent() != nullptr
            ? target.LogicalParent() : target.VisualParent();
        while (current != nullptr) {
            if (metadata->Types().IsDerivedFrom(
                    current->RuntimeType(),
                    Controls::ItemsControl::StaticTypeId())) {
                auto& items = static_cast<Controls::ItemsControl&>(*current);
                std::uint32_t index = UINT32_MAX;
                for (std::uint32_t candidate = 0U;
                     candidate < items.ItemCount(); ++candidate) {
                    Base::Ref<Base::Object> item = items.ItemAt(candidate);
                    if (item.Get() == &target) {
                        index = candidate;
                        break;
                    }
                }
                if (index != UINT32_MAX) {
                    Base::Result<Base::Ref<Base::Object>> removed =
                        items.Items().RemoveAt(index);
                    return removed
                        ? Base::Result<void>()
                        : Base::Result<void>(removed.GetStatus());
                }
            }
            current = current->LogicalParent() != nullptr
                ? current->LogicalParent() : current->VisualParent();
        }
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "RemoveElementAction target is not owned by an ItemsControl");
    }

    if (animations == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Storyboard action requires the animation manager");
    }
    if (type ==
        Animation::BeginStoryboard::StaticTypeId()) {
        auto& begin =
            static_cast<Animation::BeginStoryboard&>(
                action);
        if (!begin.StoryboardValue()) return {};
        if (!begin.Name().Empty()) {
            for (std::uint32_t index = 0U;
                 index < storyboardSessions.Size();
                 ++index) {
                StoryboardSession& existing =
                    storyboardSessions[index];
                if (existing.name.View() != begin.Name()) {
                    continue;
                }
                CancelStoryboardCompletionSessions(
                    existing.handles.AsSpan());
                for (Presentation::AnimationHandle handle :
                     existing.handles) {
                    static_cast<void>(
                        animations->Remove(handle));
                }
                for (std::uint32_t next = index + 1U;
                     next < storyboardSessions.Size();
                     ++next) {
                    storyboardSessions[next - 1U] =
                        std::move(
                            storyboardSessions[next]);
                }
                storyboardSessions.PopBack();
                break;
            }
        }
        StoryboardCompletionSession completion(allocator);
        completion.storyboard = begin.StoryboardValue();
        completion.owner = &owner;
        Base::Result<std::uint32_t> started =
            BeginTimeline(
                *begin.StoryboardValue(),
                owner, nullptr,
                &completion.handles,
                dataTemplateContext);
        if (!started) {
            for (Presentation::AnimationHandle handle :
                 completion.handles) {
                static_cast<void>(
                    animations->Remove(handle));
            }
            return started.GetStatus();
        }
        StoryboardSession namedSession(allocator);
        if (!begin.Name().Empty()) {
            namedSession.owner = &owner;
            Base::Result<void> named =
                namedSession.name.TryAssign(begin.Name());
            if (named) {
                named = namedSession.handles.TryAppend(
                    completion.handles.AsSpan());
            }
            if (!named) {
                for (Presentation::AnimationHandle handle :
                     completion.handles) {
                    static_cast<void>(
                        animations->Remove(handle));
                }
                return named.GetStatus();
            }
        }
        Base::Result<void> retained =
            storyboardCompletionSessions.TryPushBack(
                std::move(completion));
        if (!retained) {
            for (Presentation::AnimationHandle handle :
                 completion.handles) {
                static_cast<void>(
                    animations->Remove(handle));
            }
            return retained.GetStatus();
        }
        if (!begin.Name().Empty()) {
            retained = storyboardSessions.TryPushBack(
                std::move(namedSession));
            if (!retained) {
                for (Presentation::AnimationHandle handle :
                     storyboardCompletionSessions.Back().
                         handles) {
                    static_cast<void>(
                        animations->Remove(handle));
                }
                storyboardCompletionSessions.PopBack();
                return retained.GetStatus();
            }
        }
        return {};
    }

    if (type == Animation::ControlStoryboardAction::StaticTypeId()) {
        auto& control = static_cast<Animation::ControlStoryboardAction&>(action);
        if (!control.StoryboardValue()) return {};
        if (control.ControlOption() == Animation::ControlStoryboardAction::Option::Play) {
            Animation::BeginStoryboard begin;
            Base::Result<void> assigned = begin.SetStoryboard(control.StoryboardValue());
            return assigned ? ExecuteAnimationAction(begin, owner) : assigned;
        }
        bool found = false;
        for (StoryboardCompletionSession& session : storyboardCompletionSessions) {
            if (session.owner != &owner || session.storyboard.Get() != control.StoryboardValue().Get()) continue;
            found = true;
            for (Presentation::AnimationHandle handle : session.handles) {
                Base::Result<void> result;
                if (control.ControlOption() == Animation::ControlStoryboardAction::Option::Stop) result = animations->Stop(handle);
                else if (control.ControlOption() == Animation::ControlStoryboardAction::Option::Pause) result = animations->Pause(handle);
                else if (control.ControlOption() == Animation::ControlStoryboardAction::Option::Resume) result = animations->Resume(handle);
                else return Base::Status::Failure(Base::ErrorCode::Unsupported, "ControlStoryboardAction option is not implemented");
                if (!result) return result.GetStatus();
            }
        }
        return found ? Base::Result<void>{} : Base::Status::Failure(
            Base::ErrorCode::NotFound, "ControlStoryboardAction storyboard was not started");
    }

    if (!metadata->Types().IsDerivedFrom(
            type,
            Animation::
                ControllableStoryboardAction::
                    StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "EventTrigger contains an unsupported action");
    }
    auto& control =
        static_cast<
            Animation::ControllableStoryboardAction&>(
                action);
    std::uint32_t sessionIndex = UINT32_MAX;
    for (std::uint32_t index = 0U;
         index < storyboardSessions.Size();
         ++index) {
        if (storyboardSessions[index].name.View() ==
                control.BeginStoryboardName()) {
            sessionIndex = index;
            break;
        }
    }
    if (sessionIndex == UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "Controllable Storyboard was not started");
    }
    StoryboardSession& session =
        storyboardSessions[sessionIndex];
    for (Presentation::AnimationHandle handle :
         session.handles) {
        Base::Result<void> result;
        if (type ==
            Animation::PauseStoryboard::
                StaticTypeId()) {
            result = animations->Pause(handle);
        } else if (type ==
            Animation::ResumeStoryboard::
                StaticTypeId()) {
            result = animations->Resume(handle);
        } else if (type ==
            Animation::StopStoryboard::
                StaticTypeId()) {
            result = animations->Stop(handle);
        } else if (type ==
            Animation::RemoveStoryboard::
                StaticTypeId()) {
            result = animations->Remove(handle);
        } else if (type ==
            Animation::SeekStoryboard::
                StaticTypeId()) {
            result = animations->Seek(
                handle,
                static_cast<
                    Animation::SeekStoryboard&>(
                        action).
                    OffsetMicroseconds());
        } else {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Controllable Storyboard action is unsupported");
        }
        if (!result) return result.GetStatus();
    }
    if (type ==
            Animation::StopStoryboard::StaticTypeId() ||
        type ==
            Animation::RemoveStoryboard::StaticTypeId()) {
        CancelStoryboardCompletionSessions(
            session.handles.AsSpan());
    }
    if (type ==
        Animation::RemoveStoryboard::StaticTypeId()) {
        for (std::uint32_t next =
                 sessionIndex + 1U;
             next < storyboardSessions.Size();
             ++next) {
            storyboardSessions[next - 1U] =
                std::move(
                    storyboardSessions[next]);
        }
        storyboardSessions.PopBack();
    }
    return {};
}

void ViewRuntime::Impl::
CancelStoryboardCompletionSessions(
    Base::Span<const Presentation::AnimationHandle>
        handles) noexcept {
    for (std::uint32_t index = 0U;
         index < storyboardCompletionSessions.Size();) {
        bool matches = false;
        for (Presentation::AnimationHandle sessionHandle :
             storyboardCompletionSessions[index].handles) {
            for (Presentation::AnimationHandle handle :
                 handles) {
                if (sessionHandle == handle) {
                    matches = true;
                    break;
                }
            }
            if (matches) break;
        }
        if (!matches) {
            ++index;
            continue;
        }
        for (std::uint32_t next = index + 1U;
             next < storyboardCompletionSessions.Size();
             ++next) {
            storyboardCompletionSessions[next - 1U] =
                std::move(
                    storyboardCompletionSessions[next]);
        }
        storyboardCompletionSessions.PopBack();
    }
}

Base::Result<std::uint32_t>
ViewRuntime::Impl::ProcessStoryboardCompletions() noexcept {
    std::uint32_t actionCount = 0U;
    std::uint32_t index = 0U;
    while (index < storyboardCompletionSessions.Size()) {
        StoryboardCompletionSession& session =
            storyboardCompletionSessions[index];
        bool completed = true;
        for (Presentation::AnimationHandle handle :
             session.handles) {
            const Presentation::AnimationState state =
                animations->State(handle);
            if (state ==
                    Presentation::AnimationState::Active ||
                state ==
                    Presentation::AnimationState::Paused) {
                completed = false;
                break;
            }
        }
        if (!completed) {
            ++index;
            continue;
        }

        Base::Ref<Animation::Storyboard> storyboard =
            session.storyboard;
        for (std::uint32_t next = index + 1U;
             next < storyboardCompletionSessions.Size();
             ++next) {
            storyboardCompletionSessions[next - 1U] =
                std::move(
                    storyboardCompletionSessions[next]);
        }
        storyboardCompletionSessions.PopBack();

        for (const StoryboardCompletedSubscription&
                 subscription :
             storyboardCompletedSubscriptions) {
            if (subscription.trigger == nullptr ||
                subscription.owner == nullptr ||
                subscription.trigger->StoryboardValue().Get() !=
                    storyboard.Get()) {
                continue;
            }
            for (const Base::Ref<
                     Animation::TriggerAction>& action :
                 subscription.trigger->Actions()) {
                if (!action) continue;
                Base::Result<void> executed =
                    ExecuteAnimationAction(
                        *action, *subscription.owner,
                        nullptr, subscription.names);
                if (!executed) {
                    return executed.GetStatus();
                }
                if (actionCount == UINT32_MAX) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Storyboard completed action count overflow");
                }
                ++actionCount;
            }
        }
    }
    return actionCount;
}

ViewRuntime::ViewRuntime(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    void* memory = allocator_->Allocate({
        sizeof(Impl), alignof(Impl),
        Base::MemoryTag::Markup});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl), alignof(Impl),
            Base::MemoryTag::Markup);
    }
    impl_ = new (memory) Impl(*allocator_);
}

ViewRuntime::ViewRuntime(
    SchemaBundle& schemaBundle,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    void* memory = allocator_->Allocate({
        sizeof(Impl), alignof(Impl),
        Base::MemoryTag::Markup});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl), alignof(Impl),
            Base::MemoryTag::Markup);
    }
    impl_ = new (memory) Impl(*allocator_, &schemaBundle);
}

ViewRuntime::ViewRuntime(
    SchemaBundle& schemaBundle,
    Markup::DocumentCache& documentCache,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    void* memory = allocator_->Allocate({
        sizeof(Impl), alignof(Impl),
        Base::MemoryTag::Markup});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl), alignof(Impl),
            Base::MemoryTag::Markup);
    }
    impl_ = new (memory) Impl(
        *allocator_, &schemaBundle, &documentCache);
}

ViewRuntime::~ViewRuntime() noexcept {
    Shutdown();
    if (impl_ != nullptr) {
        impl_->~Impl();
        allocator_->Deallocate(
            impl_, sizeof(Impl), alignof(Impl),
            Base::MemoryTag::Markup);
        impl_ = nullptr;
    }
}

Base::Result<void> ViewRuntime::AddModule(
    const ModuleRegistration& registration) noexcept {
    if (impl_ == nullptr || impl_->initialized || impl_->terminal ||
        impl_->usesSharedSchema) {
        return RuntimeInvalidState(
            "ViewRuntime modules require an owned, uninitialized schema bundle");
    }
    return impl_->modules.Add(registration);
}

Base::Result<void> ViewRuntime::Initialize() noexcept {
    return Initialize({});
}

Base::Result<void> ViewRuntime::Initialize(
    const ViewRuntimeOptions& options) noexcept {
    return impl_->InitializeServices(options);
}

void ViewRuntime::Shutdown() noexcept {
    if (impl_ == nullptr || impl_->terminal) return;
    impl_->ShutdownServices();
    impl_->terminal = true;
}

bool ViewRuntime::IsInitialized() const noexcept {
    return impl_ != nullptr && impl_->initialized;
}

bool ViewRuntime::IsMounted() const noexcept {
    return impl_ != nullptr && impl_->mounted;
}

Base::Result<UiDocument> ViewRuntime::Load(
    Base::StringView uri,
    Core::IDiagnosticSink* diagnostics) noexcept {
    if (!IsInitialized()) {
        return RuntimeNotInitialized(
            "ViewRuntime must be initialized before XAML loading");
    }
    Base::Result<Markup::LoadOptions> options =
        impl_->LoadOptions(true);
    if (!options) return options.GetStatus();
    Markup::Loader loader(
        *impl_->schema,
        impl_->xamlSources,
        diagnostics,
        allocator_);
    return loader.Load(uri, options.Value());
}

Base::Result<UiDocument> ViewRuntime::Parse(
    Base::StringView source,
    const Base::ResourceUri& baseUri,
    Core::IDiagnosticSink* diagnostics) noexcept {
    if (!IsInitialized()) {
        return RuntimeNotInitialized(
            "ViewRuntime must be initialized before XAML parsing");
    }
    Base::Result<Markup::LoadOptions> options =
        impl_->LoadOptions(true);
    if (!options) return options.GetStatus();
    Markup::Loader loader(
        *impl_->schema,
        impl_->xamlSources,
        diagnostics,
        allocator_);
    return loader.Parse(source, baseUri, options.Value());
}

Base::Result<UiDocument> ViewRuntime::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri) noexcept {
    if (!IsInitialized()) {
        return RuntimeNotInitialized(
            "ViewRuntime must be initialized before compiled XAML loading");
    }
    Base::Result<Markup::LoadOptions> options =
        impl_->LoadOptions(true);
    if (!options) return options.GetStatus();
    Markup::Loader loader(
        *impl_->schema,
        impl_->xamlSources,
        nullptr,
        allocator_);
    return loader.LoadCompiled(
        bytes, originUri, options.Value());
}

Base::Result<void> ViewRuntime::RegisterSourceProvider(
    Integration::ISourceProvider& provider,
    Base::StringView scheme,
    Base::StringView assembly) noexcept {
    if (impl_ == nullptr || impl_->terminal) {
        return RuntimeInvalidState(
            "ViewRuntime cannot register a XAML source provider");
    }
    return impl_->xamlSources.TryRegister(
        provider, scheme, assembly);
}

Base::Result<void> ViewRuntime::LoadResources(
    RuntimeResourceLayer layer,
    Base::StringView uri,
    RuntimeResourceLoadMode mode,
    Core::IDiagnosticSink* diagnostics) noexcept {
    Base::Result<Presentation::ResourceDictionary*> target =
        impl_->ResolveResourceLayer(layer);
    if (!target) return target.GetStatus();
    return impl_->LoadResourceLayer(
        uri,
        *target.Value(),
        diagnostics,
        mode == RuntimeResourceLoadMode::Merge);
}

Base::Result<void>
ViewRuntime::LoadCompiledResources(
    RuntimeResourceLayer layer,
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    RuntimeResourceLoadMode mode) noexcept {
    Base::Result<Presentation::ResourceDictionary*> target =
        impl_->ResolveResourceLayer(layer);
    if (!target) return target.GetStatus();
    return impl_->LoadCompiledResourceLayer(
        bytes,
        originUri,
        *target.Value(),
        mode == RuntimeResourceLoadMode::Merge);
}

Base::Result<void> ViewRuntime::SetResourceDictionary(
    RuntimeResourceLayer layer,
    Presentation::ResourceDictionary& dictionary,
    RuntimeResourceLoadMode mode) noexcept {
    if (impl_ == nullptr || !impl_->initialized) {
        return RuntimeNotInitialized(
            "ViewRuntime must be initialized before setting resources");
    }
    if (impl_->mounted || impl_->root ||
        impl_->loadedDocument.root) {
        return RuntimeInvalidState(
            "ViewRuntime resource layers must be set before a document is mounted");
    }
    Base::Result<Presentation::ResourceDictionary*> target =
        impl_->ResolveResourceLayer(layer);
    if (!target) return target.GetStatus();
    Base::Result<Presentation::ResourceDictionary> shared =
        dictionary.Share();
    if (!shared) return shared.GetStatus();
    if (mode == RuntimeResourceLoadMode::Merge) {
        Base::Result<void> merged =
            target.Value()->TryAddMerged(shared.Value());
        if (!merged) return merged.GetStatus();
        return impl_->RebuildDynamicResourceEnvironment();
    }

    Presentation::ResourceDictionary previous =
        std::move(*target.Value());
    *target.Value() = std::move(shared).Value();
    Base::Result<void> rebuilt =
        impl_->RebuildDynamicResourceEnvironment();
    if (rebuilt) return {};
    *target.Value() = std::move(previous);
    Base::Result<void> restored =
        impl_->RebuildDynamicResourceEnvironment();
    return restored
        ? Base::Result<void>(rebuilt.GetStatus())
        : restored;
}

Base::Result<void> ViewRuntime::LoadBuiltInTheme(
    BuiltInTheme theme) noexcept {
    if (impl_ == nullptr) {
        return RuntimeInvalidState(
            "ViewRuntime has no implementation");
    }
    if (theme != BuiltInTheme::Light &&
        theme != BuiltInTheme::Dark) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Built-in theme value is invalid");
    }
    const std::uint8_t* paletteBytes =
        theme == BuiltInTheme::Light
        ? Detail::AeroThemeLightCompiled
        : Detail::AeroThemeDarkCompiled;
    const std::uint32_t paletteSize =
        theme == BuiltInTheme::Light
        ? Detail::AeroThemeLightCompiledSize
        : Detail::AeroThemeDarkCompiledSize;
    Base::Result<Base::ResourceUri> paletteUri =
        BuiltInThemeUri(
            theme == BuiltInTheme::Light
            ? Base::StringView("Light.xaml")
            : Base::StringView("Dark.xaml"));
    if (!paletteUri) return paletteUri.GetStatus();
    Base::Result<Base::ResourceUri> genericUri =
        BuiltInThemeUri(
            Base::StringView("Generic.xaml"));
    if (!genericUri) return genericUri.GetStatus();

    Presentation::ResourceDictionary previous =
        std::move(impl_->themeResources);
    Base::Result<void> loaded = paletteSize != 0U
        ? LoadCompiledResources(
              RuntimeResourceLayer::Theme,
              {paletteBytes, paletteSize},
              paletteUri.Value())
        : LoadResources(
              RuntimeResourceLayer::Theme,
              paletteUri.Value().Canonical());
    if (loaded) {
        loaded = Detail::AeroThemeGenericCompiledSize != 0U
            ? LoadCompiledResources(
                  RuntimeResourceLayer::Theme,
                  {Detail::AeroThemeGenericCompiled,
                   Detail::AeroThemeGenericCompiledSize},
                  genericUri.Value(),
                  RuntimeResourceLoadMode::Merge)
            : LoadResources(
                  RuntimeResourceLayer::Theme,
                  genericUri.Value().Canonical(),
                  RuntimeResourceLoadMode::Merge);
    }
    if (!loaded) {
        impl_->themeResources =
            std::move(previous);
        Base::Result<void> restored =
            impl_->RebuildDynamicResourceEnvironment();
        return restored
            ? Base::Result<void>(loaded.GetStatus())
            : Base::Result<void>(restored.GetStatus());
    }
    return {};
}

Base::Result<void> ViewRuntime::Mount(
    Presentation::Size availableSize) noexcept {
    if (!impl_->loadedDocument.root) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "ViewRuntime has no staged XAML root");
    }
    return impl_->MountRoot(
        impl_->loadedDocument.root, availableSize);
}

Base::Result<void> ViewRuntime::Mount(
    Base::Ref<Base::Object> root,
    Presentation::Size availableSize) noexcept {
    return impl_->MountRoot(
        std::move(root), availableSize);
}

Base::Result<void> ViewRuntime::Mount(
    UiDocument&& document,
    Presentation::Size availableSize) noexcept {
    Base::Result<void> ready = impl_->BeginDocumentLoad();
    if (!ready) return ready.GetStatus();
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ViewRuntime cannot mount an empty UI document");
    }
    if (Detail::UiDocumentAccess::RuntimeLifetime(document) !=
        impl_->effectLifetime.Get()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "UI document belongs to another View");
    }
    Base::Result<void> valid = impl_->ValidateDocumentRoot(document.Root());
    if (!valid) return valid.GetStatus();
    impl_->loadedDocument =
        Detail::UiDocumentAccess::Take(document);
    return impl_->MountRoot(
        impl_->loadedDocument.root, availableSize);
}

Base::Result<void> ViewRuntime::ReplaceMountedDocument(
    UiDocument&& document,
    Presentation::Size availableSize) noexcept {
    if (impl_ == nullptr || !impl_->initialized || !impl_->mounted) {
        return RuntimeInvalidState(
            "ViewRuntime document replacement requires a mounted view");
    }
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "ViewRuntime cannot replace a document with an empty document");
    }
    if (Detail::UiDocumentAccess::RuntimeLifetime(document) !=
        impl_->effectLifetime.Get()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Replacement UI document belongs to another View");
    }
    Base::Result<void> valid = impl_->ValidateDocumentRoot(document.Root());
    if (!valid) return valid.GetStatus();

    Markup::LoaderResult next =
        Detail::UiDocumentAccess::Take(document);
    if (!next.root ||
        !impl_->metadata->Types().IsDerivedFrom(
            next.root->RuntimeType(),
            Presentation::Visual::StaticTypeId())) {
        next.Clear();
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Replacement UI document root must derive from Visual");
    }

    Base::Result<void> detached =
        impl_->DetachMountedRoot(false);
    if (!detached) {
        Base::Result<void> restored = impl_->MountRoot(
            impl_->loadedDocument.root, availableSize);
        next.Clear();
        return restored ? detached : restored;
    }

    Markup::LoaderResult previous =
        std::move(impl_->loadedDocument);
    impl_->loadedDocument = std::move(next);
    Base::Result<void> mounted = impl_->MountRoot(
        impl_->loadedDocument.root, availableSize);
    if (mounted) {
        previous.Clear();
        return {};
    }

    impl_->loadedDocument = std::move(previous);
    Base::Result<void> restored = impl_->MountRoot(
        impl_->loadedDocument.root, availableSize);
    return restored ? mounted : restored;
}

Base::Result<void> ViewRuntime::MountContent(
    Controls::ContentControl& host,
    UiDocument&& document) noexcept {
    if (impl_ == nullptr || !impl_->initialized || !impl_->mounted ||
        impl_->tree == nullptr || impl_->layout == nullptr) {
        return RuntimeInvalidState(
            "content fragment mounting requires a mounted ViewRuntime");
    }
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "content fragment document must not be empty");
    }
    if (Detail::UiDocumentAccess::RuntimeLifetime(document) !=
        impl_->effectLifetime.Get()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "content fragment document belongs to another View");
    }
    if (host.OwningTree() != impl_->tree) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "content fragment host does not belong to this View");
    }

    std::uint32_t existing = UINT32_MAX;
    for (std::uint32_t index = 0U;
         index < impl_->fragmentMounts.Size(); ++index) {
        if (impl_->fragmentMounts[index].host == &host) {
            existing = index;
            break;
        }
    }
    if (existing != UINT32_MAX) {
        Base::Result<void> unmounted = impl_->UnmountFragmentAt(existing);
        if (!unmounted) return unmounted.GetStatus();
    } else if (host.Content() != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
              "content fragment host already owns non-fragment content");
    }
    Base::Result<void> capacity = impl_->fragmentMounts.TryReserve(
        impl_->fragmentMounts.Size() + 1U);
    if (!capacity) return capacity.GetStatus();

    Impl::FragmentMount fragment;
    fragment.host = &host;
    fragment.document = Detail::UiDocumentAccess::Take(document);
    Base::Result<Presentation::Visual*> rootVisual =
        impl_->ResolveVisual(
            *fragment.document.root,
            fragment.document.root->RuntimeType());
    Base::Result<Presentation::UIElement*> rootElement =
        impl_->ResolveUIElement(
            *fragment.document.root,
            fragment.document.root->RuntimeType());
    if (!rootVisual || !rootElement) {
        fragment.document.Clear();
        return !rootVisual
            ? Base::Result<void>(rootVisual.GetStatus())
            : Base::Result<void>(rootElement.GetStatus());
    }
    Base::Result<void> tracked =
        fragment.document.visualContent.TryAddNode(*rootVisual.Value());
    if (!tracked) {
        fragment.document.Clear();
        return tracked.GetStatus();
    }
    Base::Result<void> assigned = host.SetOwnedContent(
        fragment.document.root, *rootElement.Value());
    if (!assigned) {
        fragment.document.Clear();
        return assigned.GetStatus();
    }

    Presentation::MountService mounts(
        *impl_->tree, impl_->layout, impl_->renderer);
    Base::Result<Presentation::MountEdgeState> rootMounted =
        mounts.Attach(host, *rootVisual.Value());
    if (!rootMounted) {
        static_cast<void>(host.SetContent(nullptr));
        fragment.document.Clear();
        return rootMounted.GetStatus();
    }
    fragment.rootEdge = std::move(rootMounted).Value();

    const auto detachFailedFragment = [&]() noexcept {
        static_cast<void>(impl_->DetachFragment(fragment));
    };
    const auto attachEdges = [&](bool deferred) noexcept
        -> Base::Result<void> {
        std::uint32_t attached = 0U;
        for (const Presentation::VisualTreeMountEdge& edge :
             fragment.document.visualContent.mountEdges) {
            if (edge.state.logicalAttached) ++attached;
        }
        while (attached < fragment.document.visualContent.mountEdges.Size()) {
            bool progressed = false;
            for (Presentation::VisualTreeMountEdge& edge :
                 fragment.document.visualContent.mountEdges) {
                if (edge.state.logicalAttached || edge.parent == nullptr ||
                    edge.child == nullptr ||
                    edge.parent->OwningTree() != impl_->tree ||
                    (deferred && edge.child->OwningTree() == impl_->tree)) {
                    continue;
                }
                Base::Result<Presentation::MountEdgeState> mounted =
                    mounts.Attach(*edge.parent, *edge.child);
                if (!mounted) return mounted.GetStatus();
                edge.state = std::move(mounted).Value();
                ++attached;
                progressed = true;
            }
            if (!progressed) break;
        }
        return {};
    };

    Base::Result<void> attached = attachEdges(false);
    if (!attached) {
        detachFailedFragment();
        return attached.GetStatus();
    }
    Base::Result<void> applied =
        impl_->presentationServices.Apply(*rootVisual.Value());
    if (!applied) {
        detachFailedFragment();
        return applied.GetStatus();
    }
    attached = attachEdges(true);
    if (!attached) {
        detachFailedFragment();
        return attached.GetStatus();
    }
    applied = impl_->presentationServices.Apply(*rootVisual.Value());
    if (!applied) {
        detachFailedFragment();
        return applied.GetStatus();
    }
    Base::Result<void> effects = fragment.document.effects.Commit();
    if (!effects) {
        detachFailedFragment();
        return effects.GetStatus();
    }
    Base::Result<std::uint32_t> animations =
        impl_->StartLoadedAnimations(
            rootVisual.Value(), &fragment.document.names);
    if (!animations) {
        detachFailedFragment();
        return animations.GetStatus();
    }
    Base::Result<void> retained =
        impl_->fragmentMounts.TryPushBack(std::move(fragment));
    if (!retained) {
        return retained.GetStatus();
    }
    return {};
}

Base::Result<void> ViewRuntime::UnmountContent(
    Controls::ContentControl& host) noexcept {
    if (impl_ == nullptr || !impl_->initialized || !impl_->mounted) {
        return RuntimeInvalidState(
            "content fragment unmounting requires a mounted ViewRuntime");
    }
    for (std::uint32_t index = 0U;
         index < impl_->fragmentMounts.Size(); ++index) {
        if (impl_->fragmentMounts[index].host == &host) {
            return impl_->UnmountFragmentAt(index);
        }
    }
    return host.Content() == nullptr
        ? Base::Result<void>()
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::InvalidState,
              "content host does not contain a mounted XAML fragment"));
}

Base::Result<void> ViewRuntime::Resize(
    Presentation::Size availableSize) noexcept {
    if (!IsMounted() || impl_ == nullptr ||
        impl_->visualMount == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "ViewRuntime resize requires a mounted visual tree");
    }
    return impl_->visualMount->Resize(availableSize);
}

Base::Result<void> ViewRuntime::Unmount() noexcept {
    return impl_->UnmountRoot();
}

Base::Result<RuntimeFrameResult>
ViewRuntime::RunFrame() noexcept {
    if (!IsInitialized()) {
        return RuntimeNotInitialized(
            "ViewRuntime must be initialized before running frames");
    }
    if (!impl_->animationEventStatus.IsOk()) {
        return impl_->animationEventStatus;
    }
    if (impl_->styles != nullptr &&
        !impl_->styles->LastActionStatus().IsOk()) {
        return impl_->styles->LastActionStatus();
    }
    bool endpointGenerationChanged = false;
    if (impl_->endpoint) {
        const Base::Status endpointStatus =
            Integration::Detail::RenderEndpointAccess::
                FrameStatus(*impl_->endpoint);
        if (!endpointStatus.IsOk()) {
            return endpointStatus;
        }
        const std::uint64_t generation =
            impl_->endpoint->Generation();
        if (generation !=
            impl_->endpointGeneration) {
            endpointGenerationChanged = true;
            Presentation::Visual* rootVisual =
                impl_->RootVisual();
            Presentation::FrameworkElement* root =
                rootVisual != nullptr
                ? rootVisual->AsFrameworkElement()
                : nullptr;
            if (root != nullptr) {
                Base::Result<void> invalidated =
                    impl_->renderer->Invalidate(*root);
                if (!invalidated) {
                    return invalidated.GetStatus();
                }
            }
            impl_->VisitPathServices(
                rootVisual,
                impl_->MeshServices(),
                true);
            impl_->endpointGeneration = generation;
        }
    }
    if (impl_->textRuntime != nullptr) {
        Base::Result<bool> synchronized =
            impl_->textRuntime->SynchronizeBackend(
                endpointGenerationChanged);
        if (!synchronized) {
            return synchronized.GetStatus();
        }
        if (synchronized.Value()) {
            impl_->VisitTextServices(
                impl_->RootVisual(),
                impl_->textRuntime->Service(),
                true);
        }
    }
    if (impl_->imageRuntime != nullptr) {
        Base::Result<bool> synchronized =
            impl_->imageRuntime->Synchronize(
                impl_->RootVisual(),
                impl_->loadedDocument.canonicalUri,
                impl_->xamlSources,
                impl_->ImageServices(),
                endpointGenerationChanged);
        if (!synchronized) {
            return synchronized.GetStatus();
        }
        if (synchronized.Value()) {
            Presentation::Visual* rootVisual =
                impl_->RootVisual();
            Presentation::FrameworkElement* root =
                rootVisual != nullptr
                ? rootVisual->AsFrameworkElement()
                : nullptr;
            if (root != nullptr) {
                Base::Result<void> invalidated =
                    impl_->renderer->Invalidate(*root);
                if (!invalidated) {
                    return invalidated.GetStatus();
                }
            }
        }
    }
    const Core::DispatcherFramePhase phases[] = {
        Core::DispatcherFramePhase::BeginFrame,
        Core::DispatcherFramePhase::Input,
        Core::DispatcherFramePhase::PropertyChanges,
        Core::DispatcherFramePhase::DataBind,
        Core::DispatcherFramePhase::Animation,
        Core::DispatcherFramePhase::Lifecycle,
        Core::DispatcherFramePhase::Layout,
        Core::DispatcherFramePhase::RenderCommit,
        Core::DispatcherFramePhase::EndFrame};
    const bool traceEndpointFrame =
        impl_->traceEndpointFrame;
    const auto frameStarted =
        std::chrono::steady_clock::now();
    RuntimeFrameResult result;
    for (Core::DispatcherFramePhase phase : phases) {
        if (phase ==
                Core::DispatcherFramePhase::Layout &&
            impl_->visualMount != nullptr &&
            impl_->visualMount->IsMounted()) {
            Base::Result<void> completed =
                impl_->visualMount->CompleteDeferredEdges({
                    impl_->loadedDocument.visualContent.mountEdges.Data(),
                    impl_->loadedDocument.visualContent.mountEdges.Size()});
            if (!completed) return completed.GetStatus();
        }
        if (phase ==
            Core::DispatcherFramePhase::
                RenderCommit) {
            Base::Result<void> overlays =
                impl_->SynchronizeOverlays();
            if (!overlays) {
                return overlays.GetStatus();
            }
        }
        Base::Result<std::uint32_t> ran =
            impl_->dispatcher.RunFramePhase(phase);
        if (!ran) return ran.GetStatus();
        if (phase ==
            Core::DispatcherFramePhase::DataBind) {
            Base::Result<void> presented =
                impl_->FlushGeneratedPresentation();
            if (!presented) {
                return presented.GetStatus();
            }
        }
        if (traceEndpointFrame) {
            const auto elapsed =
                std::chrono::duration_cast<
                    std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() -
                    frameStarted);
            std::fprintf(
                stderr,
                "Aero endpoint frame phase=%u elapsed=%lldms callbacks=%u\n",
                static_cast<unsigned>(phase),
                static_cast<long long>(
                    elapsed.count()),
                ran.Value());
        }
        if (phase == Core::DispatcherFramePhase::Layout &&
            !impl_->layout->LastFlushStatus().IsOk()) {
            return impl_->layout->LastFlushStatus();
        }
        if (phase == Core::DispatcherFramePhase::Animation &&
            impl_->animations != nullptr) {
            const Base::Status animationStatus =
                impl_->animations->LastTickStatus();
            if (!animationStatus.IsOk()) {
                return animationStatus;
            }
            Base::Result<std::uint32_t> completed =
                impl_->ProcessStoryboardCompletions();
            if (!completed) {
                return completed.GetStatus();
            }
            if (result.callbackCount >
                UINT32_MAX - completed.Value()) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Frame callback count overflow");
            }
            result.callbackCount += completed.Value();
        }
        if (phase == Core::DispatcherFramePhase::Lifecycle &&
            impl_->animations != nullptr) {
            Base::Result<std::uint32_t> initialValues =
                impl_->animations->ApplyPendingInitialValues();
            if (!initialValues) return initialValues.GetStatus();
            if (result.callbackCount >
                UINT32_MAX - initialValues.Value()) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Initial animation callback count overflow");
            }
            result.callbackCount += initialValues.Value();
        }
        if (result.callbackCount >
            UINT32_MAX - ran.Value()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Frame callback count overflow");
        }
        result.callbackCount += ran.Value();
        if (phase ==
            Core::DispatcherFramePhase::RenderCommit) {
            const Base::Status committed =
                impl_->renderer->LastCommitStatus();
            if (!committed.IsOk()) return committed;
            if (impl_->animations != nullptr) {
                impl_->animations->CommitPendingInitialValues();
            }
        }
    }
    if (impl_->textRuntime != nullptr) {
        Base::Result<std::uint32_t> collected =
            impl_->textRuntime->CollectGarbage();
        if (!collected) return collected.GetStatus();
    }
    result.frameNumber = ++impl_->frameNumber;
    impl_->traceEndpointFrame = false;
    const Presentation::LayoutDiagnostics layout =
        impl_->layout->Diagnostics();
    result.layout.passVersion = layout.passVersion;
    result.layout.measuredCount = layout.measuredCount;
    result.layout.arrangedCount = layout.arrangedCount;
    result.layout.pendingMeasureCount =
        layout.pendingMeasureCount;
    result.layout.pendingArrangeCount =
        layout.pendingArrangeCount;
    const Presentation::RenderDiagnostics render =
        impl_->renderer->Diagnostics();
    result.render.snapshotVersion = render.commitVersion;
    result.render.nodeCount = render.nodeCount;
    result.render.commandCount = render.commandCount;
    result.render.glyphCommandCount =
        render.glyphCommandCount;
    result.render.dirtyCount = render.dirtyCount;
    result.render.snapshotHash = render.planHash;
    if (impl_->endpoint) {
        const Integration::RenderFrameStatistics
            endpointStatistics =
                impl_->endpoint->
                    LastFrameStatistics();
        result.render.drawPacketCount =
            endpointStatistics.drawPacketCount;
        result.render.batchCount =
            endpointStatistics.batchCount;
        result.render.drawCallCount =
            endpointStatistics.drawCallCount;
        result.render.mergedPacketCount =
            endpointStatistics.mergedPacketCount;
        result.render.barrierCount =
            endpointStatistics.barrierCount;
        result.render.instanceCount =
            endpointStatistics.instanceCount;
        result.render.stateBindingCount =
            endpointStatistics.stateBindingCount;
        result.render.batchingEnabled =
            endpointStatistics.batchingEnabled;
    }
    return result;
}

Base::Result<Presentation::PointerDispatchResult>
ViewRuntime::DispatchPointer(
    const Presentation::PointerInput& input) noexcept {
    if (!IsMounted() || impl_->pointer == nullptr) {
        return RuntimeNotInitialized(
            "Pointer input requires a mounted ViewRuntime");
    }
    Base::Result<
        Presentation::PointerDispatchResult>
        dispatched =
            impl_->pointer->Dispatch(input);
    if (!dispatched) {
        return dispatched.GetStatus();
    }
    Presentation::UIElement* target =
        dispatched.Value().hit.target;
    Base::Result<void> dismissed =
        impl_->DismissOverlaysForPointer(
            input, target);
    if (!dismissed) {
        return dismissed.GetStatus();
    }
    Base::Result<void> toolTip =
        impl_->UpdateToolTipForPointer(
            input, target);
    if (!toolTip) {
        return toolTip.GetStatus();
    }
    Base::Result<void> contextMenu =
        impl_->OpenContextMenuForPointer(
            input, target);
    if (!contextMenu) {
        return contextMenu.GetStatus();
    }
    return dispatched;
}

Base::Result<Presentation::KeyboardDispatchResult>
ViewRuntime::DispatchKeyboard(
    const Presentation::KeyboardInput& input) noexcept {
    if (!IsMounted() || impl_->keyboard == nullptr) {
        return RuntimeNotInitialized(
            "Keyboard input requires a mounted ViewRuntime");
    }
    if (input.action ==
            Presentation::KeyboardAction::Down &&
        input.key ==
            Presentation::KeyboardKeyEscape) {
        Base::Result<bool> dismissed =
            impl_->DismissTopOverlayForEscape();
        if (!dismissed) {
            return dismissed.GetStatus();
        }
        if (dismissed.Value()) {
            Presentation::KeyboardDispatchResult
                result;
            result.routed = true;
            return result;
        }
    }
    return impl_->keyboard->Dispatch(input);
}

Base::Result<Presentation::TextInputDispatchResult>
ViewRuntime::DispatchText(
    const Presentation::TextInput& input) noexcept {
    if (!IsMounted() || impl_->textInput == nullptr) {
        return RuntimeNotInitialized(
            "Text input requires a mounted ViewRuntime");
    }
    return impl_->textInput->Dispatch(input);
}

Base::Result<std::uint32_t>
ViewRuntime::AdvanceTime(
    std::uint32_t elapsedMilliseconds) noexcept {
    if (!IsMounted() || impl_->animations == nullptr) {
        return RuntimeNotInitialized(
            "View timing requires a mounted animation manager");
    }
    std::uint32_t actionCount = 0U;
    if (impl_->controlInteractions != nullptr) {
        Base::Result<std::uint32_t> controls =
            impl_->controlInteractions->AdvanceTime(
                elapsedMilliseconds);
        if (!controls) return controls.GetStatus();
        actionCount = controls.Value();
    }
    Base::Result<std::uint32_t> toolTips =
        impl_->AdvanceToolTipTime(
            elapsedMilliseconds);
    if (!toolTips) return toolTips.GetStatus();
    if (actionCount > UINT32_MAX - toolTips.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Control timing action count overflow");
    }
    actionCount += toolTips.Value();
    Base::Result<std::uint32_t> animations =
        impl_->animations->AdvanceBy(
            static_cast<Presentation::AnimationTime>(
                elapsedMilliseconds) * 1000U);
    if (!animations) return animations.GetStatus();
    Base::Result<std::uint32_t> completed =
        impl_->ProcessStoryboardCompletions();
    if (!completed) return completed.GetStatus();
    if (actionCount > UINT32_MAX - animations.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "View timing action count overflow");
    }
    actionCount += animations.Value();
    if (actionCount > UINT32_MAX - completed.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Storyboard completed action count overflow");
    }
    return actionCount + completed.Value();
}

Base::Result<std::uint32_t>
ViewRuntime::AdvanceAnimationTime(
    std::uint32_t elapsedMilliseconds) noexcept {
    if (!IsMounted() || impl_->animations == nullptr) {
        return RuntimeNotInitialized(
            "Animation timing requires a mounted ViewRuntime");
    }
    Base::Result<std::uint32_t> advanced =
        impl_->animations->AdvanceBy(
        static_cast<Presentation::AnimationTime>(
            elapsedMilliseconds) * 1000U);
    if (!advanced) return advanced.GetStatus();
    Base::Result<std::uint32_t> completed =
        impl_->ProcessStoryboardCompletions();
    if (!completed) return completed.GetStatus();
    if (advanced.Value() >
        UINT32_MAX - completed.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Animation action count overflow");
    }
    return advanced.Value() + completed.Value();
}

Base::Result<void>
ViewRuntime::SetRenderBatchingEnabledForTesting(
    bool enabled) noexcept {
    if (!IsInitialized() ||
        impl_ == nullptr ||
        !impl_->endpoint) {
        return RuntimeNotInitialized(
            "View batching control requires an initialized render endpoint");
    }
    Base::Result<void> changed =
        impl_->endpoint->
            SetBatchingEnabledForTesting(enabled);
    if (!changed) return changed.GetStatus();
    Presentation::Visual* rootVisual =
        impl_->RootVisual();
    Presentation::FrameworkElement* root =
        rootVisual != nullptr
        ? rootVisual->AsFrameworkElement()
        : nullptr;
    return root != nullptr
        ? impl_->renderer->Invalidate(*root)
        : Base::Result<void>();
}

Base::Result<void> ViewRuntime::SetRenderEndpoint(
    Base::Ref<Integration::RenderEndpoint> endpoint,
    bool automaticAnimationClock) noexcept {
    const auto transitionStarted =
        std::chrono::steady_clock::now();
    auto tracePhase =
        [&](const char* phase) noexcept {
            const auto elapsed =
                std::chrono::duration_cast<
                    std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() -
                    transitionStarted);
            std::fprintf(
                stderr,
                "Aero endpoint transition %s=%lldms\n",
                phase,
                static_cast<long long>(
                    elapsed.count()));
        };
    if (!IsInitialized() || impl_ == nullptr) {
        return RuntimeNotInitialized(
            "View endpoint replacement requires an initialized runtime");
    }
    if (!endpoint) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View endpoint replacement requires a render endpoint");
    }
    if (endpoint.Get() == impl_->endpoint.Get()) {
        impl_->options.automaticAnimationClock =
            automaticAnimationClock;
        if (impl_->animations != nullptr) {
            impl_->animations->SetAutomaticTickingEnabled(
                automaticAnimationClock);
        }
        return {};
    }

    Base::Result<void> bound =
        Integration::Detail::RenderEndpointAccess::Bind(
            *endpoint, this);
    if (!bound) return bound.GetStatus();
    tracePhase("bound");

    Base::Ref<Integration::RenderEndpoint> previous =
        impl_->endpoint;
    if (previous) {
        Base::Result<void> idle =
            previous->WaitIdle();
        if (!idle) {
            Integration::Detail::RenderEndpointAccess::Unbind(
                *endpoint, this);
            return idle.GetStatus();
        }
    }
    tracePhase("previous-idle");

    Aero::Detail::ImageBackendServices*
        previousImages = impl_->ImageServices();
    if (impl_->imageRuntime != nullptr) {
        impl_->imageRuntime->ReleaseBackendResources(
            previousImages);
    }
    impl_->VisitTextServices(
        impl_->RootVisual(), nullptr);
    impl_->VisitPathServices(
        impl_->RootVisual(), nullptr);
    tracePhase("resources-detached");

    impl_->endpoint = endpoint;
    impl_->endpointBound = true;
    impl_->endpointGeneration =
        endpoint->Generation();
    impl_->endpointBackend.SetEndpoint(endpoint);
    impl_->options.renderEndpoint = endpoint;
    impl_->options.automaticAnimationClock =
        automaticAnimationClock;
    if (impl_->animations != nullptr) {
        impl_->animations->SetAutomaticTickingEnabled(
            automaticAnimationClock);
    }
    tracePhase("endpoint-swapped");

    Base::Result<void> status;
    if (impl_->textRuntime != nullptr) {
        Base::Result<bool> synchronized =
            impl_->textRuntime->SynchronizeBackend(true);
        if (!synchronized) {
            status = synchronized.GetStatus();
        } else {
            tracePhase("text-backend-ready");
            impl_->VisitTextServices(
                impl_->RootVisual(),
                impl_->textRuntime->Service(),
                true);
            tracePhase("text-tree-attached");
        }
    }
    if (status && impl_->imageRuntime != nullptr) {
        Base::Result<bool> synchronized =
            impl_->imageRuntime->Synchronize(
                impl_->RootVisual(),
                impl_->loadedDocument.canonicalUri,
                impl_->xamlSources,
                impl_->ImageServices(),
                true);
        if (!synchronized) {
            status = synchronized.GetStatus();
        }
        tracePhase("images-synchronized");
    }
    if (status) {
        impl_->VisitPathServices(
            impl_->RootVisual(),
            impl_->MeshServices(),
            true);
        tracePhase("paths-attached");
        Presentation::Visual* rootVisual =
            impl_->RootVisual();
        Presentation::FrameworkElement* root =
            rootVisual != nullptr
            ? rootVisual->AsFrameworkElement()
            : nullptr;
        if (root != nullptr) {
            status = impl_->renderer->Invalidate(*root);
        }
        tracePhase("renderer-invalidated");
    }

    if (previous) {
        Integration::Detail::RenderEndpointAccess::Unbind(
            *previous, this);
    }
    impl_->traceEndpointFrame = true;
    tracePhase("complete");
    return status;
}

const Base::Ref<Base::Object>&
ViewRuntime::Root() const noexcept {
    static const Base::Ref<Base::Object> empty;
    return impl_ != nullptr ? impl_->root : empty;
}

Base::Object* ViewRuntime::FindNamedObject(
    Base::StringView name,
    Core::TypeId expectedType) noexcept {
    if (impl_ == nullptr || name.Empty()) {
        return nullptr;
    }
    Base::Object* object = impl_->loadedDocument.names.Find(name);
    if (object == nullptr || expectedType == Core::InvalidTypeId) {
        return object;
    }
    return impl_->metadata->Types().IsAssignableFrom(
        expectedType, object->RuntimeType()) ? object : nullptr;
}

Core::MetadataDomain* ViewRuntime::Metadata() noexcept {
    return IsInitialized() ? impl_->metadata : nullptr;
}

Core::MetadataRuntime*
ViewRuntime::MetadataRuntime() noexcept {
    return impl_ != nullptr ? impl_->metadataRuntime : nullptr;
}

Core::EffectiveValueEngine*
ViewRuntime::EffectiveValues() noexcept {
    return impl_ != nullptr ? impl_->values : nullptr;
}

Presentation::AnimationManager*
ViewRuntime::Animations() noexcept {
    return impl_ != nullptr ? impl_->animations : nullptr;
}

Presentation::ObjectTree* ViewRuntime::Tree() noexcept {
    return impl_ != nullptr ? impl_->tree : nullptr;
}

Presentation::LayoutManager* ViewRuntime::Layout() noexcept {
    return impl_ != nullptr ? impl_->layout : nullptr;
}

Presentation::RenderManager* ViewRuntime::Renderer() noexcept {
    return impl_ != nullptr ? impl_->renderer : nullptr;
}

Presentation::BindingManager* ViewRuntime::Bindings() noexcept {
    return impl_ != nullptr ? impl_->bindings : nullptr;
}

Presentation::CommandManager* ViewRuntime::Commands() noexcept {
    return impl_ != nullptr ? impl_->commands : nullptr;
}

Presentation::RoutedEventManager*
ViewRuntime::RoutedEvents() noexcept {
    return impl_ != nullptr ? impl_->events : nullptr;
}

Presentation::FocusManager* ViewRuntime::Focus() noexcept {
    return impl_ != nullptr ? impl_->focus : nullptr;
}

Controls::TemplateManager* ViewRuntime::Templates() noexcept {
    return impl_ != nullptr ? impl_->templates : nullptr;
}

Controls::VisualStateManager*
ViewRuntime::VisualStates() noexcept {
    return impl_ != nullptr ? impl_->visualStates : nullptr;
}

Markup::Schema* ViewRuntime::Schema() noexcept {
    return impl_ != nullptr ? impl_->schema : nullptr;
}

Markup::SourceProviderRegistry*
ViewRuntime::Sources() noexcept {
    return impl_ != nullptr
        ? &impl_->xamlSources
        : nullptr;
}

Markup::EmbeddedSourceProvider*
ViewRuntime::EmbeddedSources() noexcept {
    return impl_ != nullptr
        ? &impl_->embeddedXaml
        : nullptr;
}

Markup::DocumentCache* ViewRuntime::DocumentCache() noexcept {
    return impl_ != nullptr ? impl_->documentCache : nullptr;
}

const Base::ResourceUri& ViewRuntime::CurrentDocumentUri() const noexcept {
    static const Base::ResourceUri empty;
    return impl_ != nullptr
        ? impl_->loadedDocument.canonicalUri
        : empty;
}

Base::Span<const Base::ResourceUri>
ViewRuntime::CurrentDocumentDependencies() const noexcept {
    return impl_ != nullptr
        ? Base::Span<const Base::ResourceUri>{
              impl_->loadedDocument.dependencies.Data(),
              impl_->loadedDocument.dependencies.Size()}
        : Base::Span<const Base::ResourceUri>{};
}

Presentation::ResourceDictionary*
ViewRuntime::ApplicationResources() noexcept {
    return impl_ != nullptr
        ? &impl_->applicationResources
        : nullptr;
}

Presentation::ResourceDictionary*
ViewRuntime::ThemeResources() noexcept {
    return impl_ != nullptr
        ? &impl_->themeResources
        : nullptr;
}

Presentation::ResourceDictionary*
ViewRuntime::SystemResources() noexcept {
    return impl_ != nullptr
        ? &impl_->systemResources
        : nullptr;
}

Presentation::StyleManager*
ViewRuntime::Styles() noexcept {
    return impl_ != nullptr ? impl_->styles : nullptr;
}

std::uint32_t ViewRuntime::NamedObjectCount() const noexcept {
    return impl_ != nullptr ? impl_->loadedDocument.names.Size() : 0U;
}

} // namespace Aero
