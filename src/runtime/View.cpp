#include <Aero/View.hpp>
#include <Aero/Base/Hash.hpp>
#include "runtime/GuiData.hpp"
#include <Aero/FrameworkElement.hpp>
#include "runtime/ImageCache.hpp"
#include "runtime/TextPipeline.hpp"
#include "markup/GuiSchema.hpp"
#include "controls/ControlInternals.hpp"
#include "controls/ControlBehavior.hpp"

#include <Aero/Controls/Primitives.hpp>
#include <Aero/Controls/Standard.hpp>
#include <Aero/Controls/Base.hpp>
#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Panels.hpp>
#include <Aero/Documents.hpp>
#include "controls/Metadata.hpp"
#include "controls/TemplateInternals.hpp"
#include <Aero/Styling.hpp>
#include <Aero/Controls/Text.hpp>
#include <Aero/Meta/Registry.hpp>
#include "gui/PropertyInternal.hpp"
#include "gui/MetaInternals.hpp"
#include "markup/Loader.hpp"
#include "markup/LoadInternals.hpp"
#include "markup/LoaderResult.hpp"
#include "markup/XamlDocumentInternal.hpp"
#include <Aero/Markup/Schema.hpp>
#include <Aero/Integration/Platform.hpp>
#include <Aero/Data.hpp>
#include "media/AnimationModel.hpp"
#include "media/AnimationInternals.hpp"
#include <Aero/Animation.hpp>
#include <Aero/Input.hpp>
#include "gui/ElementInternal.hpp"
#include <Aero/Media/Brushes.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/BuiltinThemes.generated.hpp>

#include "runtime/DataTemplateTriggerState.hpp"
#include "integration/RenderDeviceInternal.hpp"
#include "render/RenderTree.hpp"

#include <new>
#include <utility>
#include "gui/LayoutInternal.hpp"
#include "gui/BindingInternal.hpp"
#include "gui/AnimationInternal.hpp"
#include "gui/StyleInternal.hpp"
#include "gui/RoutedEventInternal.hpp"
#include "gui/InputInternal.hpp"

namespace Aero::Detail {
namespace MediaAnimation = ::Aero::Media::Animation;
namespace {

Base::Status ViewInvalidState(const char* message) noexcept {
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

template<class T>
Base::Result<const T*> ResolveUiValue(
    Aero::FrameworkElement& element,
    Core::DependencyPropertyHandle property,
    const Aero::ResourceEnvironment& resources,
    const char* incompatibleMessage) noexcept {
    Base::Result<Core::Value> explicitValue = element.GetValue(property);
    if (!explicitValue) return explicitValue.GetStatus();
    if (explicitValue.Value().Kind() == Core::ValueKind::Object &&
        !explicitValue.Value().IsNullObject() &&
        explicitValue.Value().AsObject()) {
        Base::Object* object = explicitValue.Value().AsObject().Get();
        if (object->RuntimeType() != T::StaticTypeId()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument, incompatibleMessage);
        }
        return static_cast<const T*>(object);
    }

    Base::Result<Core::Value> implicit = Aero::ResourceResolver::Lookup(
        &element, element.RuntimeType(), nullptr, resources);
    if (!implicit) {
        return implicit.GetStatus().code == Base::ErrorCode::NotFound
            ? Base::Result<const T*>(static_cast<const T*>(nullptr))
            : Base::Result<const T*>(implicit.GetStatus());
    }
    if (implicit.Value().Kind() != Core::ValueKind::Object ||
        implicit.Value().IsNullObject() || !implicit.Value().AsObject() ||
        implicit.Value().AsObject()->RuntimeType() != T::StaticTypeId()) {
        return static_cast<const T*>(nullptr);
    }
    return static_cast<const T*>(implicit.Value().AsObject().Get());
}

template<class T, class... TRest>
constexpr std::size_t PackedObjectBytes() noexcept {
    if constexpr (sizeof...(TRest) == 0U) {
        return sizeof(T) + alignof(T) - 1U;
    } else {
        return sizeof(T) + alignof(T) - 1U +
            PackedObjectBytes<TRest...>();
    }
}

template<class T, class... TRest>
constexpr std::size_t MaximumObjectAlignment() noexcept {
    if constexpr (sizeof...(TRest) == 0U) {
        return alignof(T);
    } else {
        constexpr std::size_t rest =
            MaximumObjectAlignment<TRest...>();
        return alignof(T) > rest ? alignof(T) : rest;
    }
}

class ViewArena final {
public:
    explicit ViewArena(Base::IAllocator& allocator) noexcept
        : allocator_(&allocator) {}

    ViewArena(const ViewArena&) = delete;
    ViewArena& operator=(const ViewArena&) = delete;

    ~ViewArena() noexcept { Reset(); }

    Base::Result<void> Initialize(
        std::size_t capacity,
        std::size_t alignment) noexcept {
        if (memory_ != nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "View arena is already initialized");
        }
        memory_ = allocator_->Allocate({
            capacity, alignment, Base::MemoryTag::Ui});
        if (memory_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "View arena allocation failed");
        }
        capacity_ = capacity;
        alignment_ = alignment;
        offset_ = 0U;
        return {};
    }

    template<class T, class... TArgs>
    Base::Result<void> Create(
        T*& output,
        TArgs&&... arguments) noexcept {
        if (output != nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "View object is already constructed");
        }
        if (memory_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "View arena is not initialized");
        }
        const std::uintptr_t base =
            reinterpret_cast<std::uintptr_t>(memory_);
        const std::uintptr_t current = base + offset_;
        const std::uintptr_t aligned =
            (current + alignof(T) - 1U) &
            ~(static_cast<std::uintptr_t>(alignof(T)) - 1U);
        const std::size_t next =
            static_cast<std::size_t>(aligned - base) + sizeof(T);
        if (next > capacity_) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfMemory,
                "View arena capacity was exceeded");
        }
        output = new (reinterpret_cast<void*>(aligned)) T(
            std::forward<TArgs>(arguments)...);
        offset_ = next;
        return {};
    }

    template<class T>
    void Destroy(T*& object) noexcept {
        if (object == nullptr) return;
        object->~T();
        object = nullptr;
    }

    void Reset() noexcept {
        if (memory_ == nullptr) return;
        allocator_->Deallocate(
            memory_, capacity_, alignment_, Base::MemoryTag::Ui);
        memory_ = nullptr;
        capacity_ = 0U;
        alignment_ = alignof(std::max_align_t);
        offset_ = 0U;
    }

private:
    Base::IAllocator* allocator_ = nullptr;
    void* memory_ = nullptr;
    std::size_t capacity_ = 0U;
    std::size_t alignment_ = alignof(std::max_align_t);
    std::size_t offset_ = 0U;
};

constexpr std::size_t ViewArenaCapacity = PackedObjectBytes<
    Core::ObjectFactoryScope,
    Core::EffectiveValueEngine,
    AnimationEngine,
    Aero::ElementTree,
    LayoutEngine,
    Render::RenderTree,
    ImageCache,
    TextPipeline,
    BindingEngine,
    EventRouter,
    InputRouter,
    Controls::Detail::ControlBehavior,
    Aero::Detail::TemplateEngine,
    StyleEngine>();
constexpr std::size_t ViewArenaAlignment = MaximumObjectAlignment<
    Core::ObjectFactoryScope,
    Core::EffectiveValueEngine,
    AnimationEngine,
    Aero::ElementTree,
    LayoutEngine,
    Render::RenderTree,
    ImageCache,
    TextPipeline,
    BindingEngine,
    EventRouter,
    InputRouter,
    Controls::Detail::ControlBehavior,
    Aero::Detail::TemplateEngine,
    StyleEngine>();

template<class T, class... TArgs>
Base::Result<void> AllocateObject(
    Base::IAllocator& allocator,
    Base::MemoryTag tag,
    T*& output,
    TArgs&&... arguments) noexcept {
    if (output != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Object is already allocated");
    }
    void* memory = allocator.Allocate({
        sizeof(T), alignof(T), tag});
    if (memory == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Object allocation failed");
    }
    output = new (memory) T(
        std::forward<TArgs>(arguments)...);
    return {};
}

template<class T>
void FreeObject(
    Base::IAllocator& allocator,
    Base::MemoryTag tag,
    T*& object) noexcept {
    if (object == nullptr) return;
    object->~T();
    allocator.Deallocate(
        object, sizeof(T), alignof(T), tag);
    object = nullptr;
}


} // namespace

struct ViewData final {
    struct FragmentMount final {
        Controls::ContentControl* host = nullptr;
        Markup::LoaderResult document;
        Aero::Detail::ElementAttachment rootEdge;
    };

    ViewData(
        Base::IAllocator& value,
        GuiSchema& sharedSchema,
        Markup::DocumentCache& sharedDocuments) noexcept
        : allocator(&value),
          arena(value),
          schemaBundle(&sharedSchema),
          documentCache(&sharedDocuments),
          storyboardSessions(&value),
          storyboardCompletionSessions(&value),
          storyboardCompletedSubscriptions(&value),
          itemGenerators(&value),
          fragmentMounts(&value) {}

    Base::IAllocator* allocator = nullptr;
    ViewArena arena;
    Core::Dispatcher dispatcher;
    GuiSchema* schemaBundle = nullptr;
    Markup::DocumentCache* documentCache = nullptr;
    Core::MetaRegistry* metadata = nullptr;
    Integration::ViewOptions options;
    Base::Ref<Integration::RenderDevice> device;
    bool deviceBound = false;
    std::uint64_t deviceGeneration = 0U;

    Core::ObjectFactoryScope* objectFactory = nullptr;
    Core::EffectiveValueEngine* values = nullptr;
    Aero::Detail::AnimationEngine* animations = nullptr;
    Aero::ElementTree* tree = nullptr;
    Aero::Detail::LayoutEngine* layout = nullptr;
    Render::RenderTree* renderer = nullptr;
    Aero::Detail::ImageCache* images = nullptr;
    Aero::Detail::TextPipeline* text = nullptr;
    Aero::Detail::BindingEngine* bindings = nullptr;
    Aero::Detail::EventRouter* events = nullptr;
    Aero::Detail::InputRouter* input = nullptr;

    Aero::Detail::TemplateEngine* templates = nullptr;
    Controls::VisualStateManager* visualStates = nullptr;
    Aero::Detail::StyleEngine* styles = nullptr;
    Aero::Detail::ElementHost elementHost;

    Markup::Schema* schema = nullptr;
    Aero::Detail::RootAttachment rootAttachment;
    Aero::Visual* attachedRootVisual = nullptr;
    Aero::UIElement* attachedRootLayout = nullptr;
    Aero::FrameworkElement* attachedRootRender = nullptr;
    Markup::SourceProviders xamlSources;
    Markup::EmbeddedSourceProvider embeddedXaml;
    Markup::FileSourceProvider fileXaml;
    Aero::ResourceDictionary applicationResources;
    Aero::ResourceDictionary themeResources;
    Aero::ResourceDictionary systemResources;
    Aero::ResourceDictionary dynamicResourceEnvironment;

    Controls::Detail::ControlBehavior* controlBehaviors = nullptr;
    struct StoryboardSession final {
        explicit StoryboardSession(
            Base::IAllocator* allocator) noexcept
            : handles(allocator) {}

        Aero::FrameworkElement* owner = nullptr;
        Base::String name;
        Base::Vector<
            Aero::Detail::Animation::AnimationHandle>
            handles;
    };
    Base::Vector<StoryboardSession>
        storyboardSessions;
    struct StoryboardCompletionSession final {
        explicit StoryboardCompletionSession(
            Base::IAllocator* allocator) noexcept
            : handles(allocator) {}

        Base::Ref<MediaAnimation::Storyboard> storyboard;
        Aero::FrameworkElement* owner = nullptr;
        Base::Vector<
            Aero::Detail::Animation::AnimationHandle>
            handles;
    };
    struct StoryboardCompletedSubscription final {
        MediaAnimation::StoryboardCompletedTrigger* trigger =
            nullptr;
        Aero::FrameworkElement* owner = nullptr;
        const Aero::NameScope* names = nullptr;
    };
    Base::Vector<StoryboardCompletionSession>
        storyboardCompletionSessions;
    Base::Vector<StoryboardCompletedSubscription>
        storyboardCompletedSubscriptions;
    Base::Result<void> ExecuteAnimationAction(
        MediaAnimation::TriggerAction& action,
        Aero::FrameworkElement& owner,
        Aero::Detail::DataTemplateTriggerState*
            dataTemplateContext = nullptr,
        const Aero::NameScope* names = nullptr) noexcept;
    void CancelStoryboardCompletionSessions(
        Base::Span<const Aero::Detail::Animation::AnimationHandle>
            handles) noexcept;
    Base::Result<std::uint32_t>
    ProcessStoryboardCompletions() noexcept;
    static Base::Result<void> ExecuteStyleTriggerActions(
        ::Aero::DependencyObject& owner,
        Base::Span<const Base::Ref<Base::Object>>
            actions,
        void* context) noexcept {
        auto* runtime = static_cast<ViewData*>(context);
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
                    MediaAnimation::TriggerAction::
                        StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Style Trigger contains an invalid action");
            }
            Base::Result<void> executed =
                runtime->ExecuteAnimationAction(
                    static_cast<MediaAnimation::TriggerAction&>(
                        *authored),
                    element);
            if (!executed) return executed.GetStatus();
        }
        return {};
    }
    struct AnimationEventState final {
        ViewData* runtime = nullptr;
        MediaAnimation::EventTrigger* trigger = nullptr;
        Aero::FrameworkElement* owner = nullptr;
        const Aero::NameScope* names = nullptr;

        Base::Result<bool> EvaluateComparison(
            const MediaAnimation::ComparisonCondition& condition) noexcept {
            const Base::Ref<Data::Binding> binding =
                condition.LeftOperand();
            if (!binding || runtime == nullptr ||
                runtime->metadata == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "ConditionBehavior requires a bound left operand");
            }
            if (binding->GetElementName().Empty()) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "ConditionBehavior currently requires Binding ElementName");
            }
            Base::Object* source = names != nullptr
                ? names->Find(binding->GetElementName())
                : runtime->loadedDocument.names.Find(
                      binding->GetElementName());
            if (source == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "ConditionBehavior Binding ElementName was not found");
            }
            Base::Result<Core::BindingPathPlan> plan =
                Core::BindingPathPlan::Compile(
                    *runtime->metadata,
                    source->RuntimeType(), binding->GetPath().GetPath());
            if (!plan) return plan.GetStatus();
            Base::Result<Core::PropertyValue> current =
                plan.Value().Get(*runtime->metadata, *source);
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
                MediaAnimation::ComparisonCondition::Operator::Equal) {
                return current.Value().Equals(expected);
            }
            if (comparison ==
                MediaAnimation::ComparisonCondition::Operator::NotEqual) {
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
                case MediaAnimation::ComparisonCondition::Operator::LessThan:
                    return left < right;
                case MediaAnimation::ComparisonCondition::Operator::LessThanOrEqual:
                    return left <= right;
                case MediaAnimation::ComparisonCondition::Operator::GreaterThan:
                    return left > right;
                case MediaAnimation::ComparisonCondition::Operator::GreaterThanOrEqual:
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
                case MediaAnimation::ComparisonCondition::Operator::LessThan:
                    return result < 0;
                case MediaAnimation::ComparisonCondition::Operator::LessThanOrEqual:
                    return result <= 0;
                case MediaAnimation::ComparisonCondition::Operator::GreaterThan:
                    return result > 0;
                case MediaAnimation::ComparisonCondition::Operator::GreaterThanOrEqual:
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
                    MediaAnimation::ConditionBehavior::StaticTypeId()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::Unsupported,
                        "EventTrigger contains an unsupported behavior");
                }
                const Base::Ref<MediaAnimation::ConditionalExpression> expression =
                    static_cast<MediaAnimation::ConditionBehavior&>(*behavior).Expression();
                if (!expression) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidState,
                        "ConditionBehavior has no expression");
                }
                bool expressionResult = false;
                for (const Base::Ref<MediaAnimation::ComparisonCondition>& condition :
                     expression->Conditions()) {
                    if (!condition) continue;
                    Base::Result<bool> matches = EvaluateComparison(*condition);
                    if (!matches) return matches.GetStatus();
                    expressionResult = matches.Value();
                    if (!expressionResult && expression->Chaining() ==
                        MediaAnimation::ConditionalExpression::ForwardChaining::And) {
                        return false;
                    }
                    if (expressionResult && expression->Chaining() ==
                        MediaAnimation::ConditionalExpression::ForwardChaining::Or) {
                        break;
                    }
                }
                if (!expressionResult) return false;
            }
            return true;
        }

        void Invoke(
            Base::Object*,
            Aero::RoutedEventArgs&) noexcept {
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
            for (const Base::Ref<MediaAnimation::TriggerAction>& action :
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
        Aero::UIElement* owner = nullptr;
        Aero::RoutedEventHandle event;
        Aero::RoutedEventHandler handler;
        AnimationEventState* context = nullptr;
    };
    Base::Vector<AnimationEventSubscription>
        animationEventSubscriptions;
    struct DataTemplateTriggerHandlerState final {
        ViewData* runtime = nullptr;
        Base::Ref<
            Aero::Detail::DataTemplateTriggerState>
            triggerContext;
        std::uint32_t triggerIndex = 0U;
        std::uint32_t conditionIndex = 0U;

        void Invoke(
            ::Aero::DependencyObject&,
            const Core::
                DependencyPropertyChangedEventArgs&)
            noexcept;
    };
    struct DataTemplateTriggerSubscription final {
        ::Aero::DependencyObject* source = nullptr;
        Core::DependencyPropertyHandle property;
        Core::DependencyPropertyChangedEventHandler
            handler;
        DataTemplateTriggerHandlerState* context =
            nullptr;
    };
    Base::Vector<DataTemplateTriggerSubscription>
        dataTemplateTriggerSubscriptions;
    Base::Status animationEventStatus;
    Base::Vector<Controls::ItemContainerGenerator*>
        itemGenerators;
    Base::Vector<Aero::VisualHandle>
        pendingGeneratedVisuals;
    Base::Vector<Aero::FrameworkElement*>
        renderOverlays;
    Base::Vector<Aero::UIElement*>
        inputOverlays;
    Base::Vector<Aero::Point>
        overlayOrigins;
    Base::Ref<Controls::ToolTip>
        pendingToolTip;
    Base::Ref<Controls::ToolTip>
        activeToolTip;
    Base::Ref<Aero::UIElement>
        toolTipTarget;
    Base::Ref<Aero::UIElement>
        overlayFocusReturn;
    std::uint32_t toolTipElapsed = 0U;
    std::uint32_t toolTipVisibleElapsed = 0U;

    Markup::LoaderResult loadedDocument;
    Base::Vector<FragmentMount> fragmentMounts;

    bool HasAttachedRoot() const noexcept {
        return rootAttachment.IsAttached();
    }

    Base::Result<void> AttachVisualGraph(
        Visual& rootVisual,
        UIElement& rootLayout,
        FrameworkElement* rootRender,
        Base::Span<Aero::Detail::VisualEdge> edges,
        Size availableSize) noexcept {
        if (tree == nullptr || layout == nullptr || HasAttachedRoot() ||
            !IsValidLayoutSize(availableSize)) {
            return ViewInvalidState(
                "GUI root cannot be attached in its current state");
        }
        tree->AttachPresentation(layout, renderer);
        Base::Result<Aero::Detail::RootAttachment> rootAttached =
            tree->AttachRoot(rootVisual, availableSize);
        if (!rootAttached) return rootAttached.GetStatus();
        rootAttachment = std::move(rootAttached).Value();
        attachedRootVisual = &rootVisual;
        attachedRootLayout = &rootLayout;
        attachedRootRender = rootRender;

        std::uint32_t attached = 0U;
        while (attached < edges.Size()) {
            bool progressed = false;
            for (Aero::Detail::VisualEdge& edge : edges) {
                if (edge.state.logicalAttached || edge.parent == nullptr ||
                    edge.child == nullptr ||
                    Aero::Detail::ElementPrivate::Tree(*edge.parent) != tree) {
                    continue;
                }
                Base::Result<Aero::Detail::ElementAttachment> edgeAttached =
                    tree->AttachElement(*edge.parent, *edge.child);
                if (!edgeAttached) {
                    static_cast<void>(DetachVisualGraph(edges));
                    return edgeAttached.GetStatus();
                }
                edge.state = std::move(edgeAttached).Value();
                ++attached;
                progressed = true;
            }
            if (!progressed) break;
        }
        return {};
    }

    Base::Result<void> CompleteVisualEdges(
        Base::Span<Aero::Detail::VisualEdge> edges) noexcept {
        if (!HasAttachedRoot() || tree == nullptr) {
            return ViewInvalidState(
                "Deferred visual edges require an attached root");
        }
        std::uint32_t attached = 0U;
        for (const Aero::Detail::VisualEdge& edge : edges) {
            if (edge.state.logicalAttached) ++attached;
        }
        while (attached < edges.Size()) {
            bool progressed = false;
            for (Aero::Detail::VisualEdge& edge : edges) {
                if (edge.state.logicalAttached ||
                    (edge.child != nullptr &&
                     Aero::Detail::ElementPrivate::Tree(*edge.child) == tree) ||
                    edge.parent == nullptr || edge.child == nullptr ||
                    Aero::Detail::ElementPrivate::Tree(*edge.parent) != tree) {
                    continue;
                }
                Base::Result<Aero::Detail::ElementAttachment> edgeAttached =
                    tree->AttachElement(*edge.parent, *edge.child);
                if (!edgeAttached) return edgeAttached.GetStatus();
                edge.state = std::move(edgeAttached).Value();
                ++attached;
                progressed = true;
            }
            if (!progressed) break;
        }
        return {};
    }

    Base::Result<void> ResizeVisualRoot(Size availableSize) noexcept {
        if (!HasAttachedRoot() || attachedRootLayout == nullptr ||
            layout == nullptr) {
            return RuntimeNotInitialized(
                "View resize requires an attached layout root");
        }
        if (!IsValidLayoutSize(availableSize)) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View dimensions are invalid");
        }
        Base::Result<void> resized =
            layout->SetRoot(attachedRootLayout, availableSize);
        if (!resized) return resized.GetStatus();
        if (renderer != nullptr && attachedRootRender != nullptr) {
            return renderer->Invalidate(*attachedRootRender);
        }
        return {};
    }

    Base::Result<void> DetachVisualGraph(
        Base::Span<Aero::Detail::VisualEdge> edges) noexcept {
        if (!HasAttachedRoot() && attachedRootVisual == nullptr) return {};
        if (tree == nullptr) {
            return ViewInvalidState(
                "GUI context is unavailable during root detach");
        }

        std::uint32_t remaining = 0U;
        for (const Aero::Detail::VisualEdge& edge : edges) {
            if (edge.state.IsAttached()) ++remaining;
        }
        while (remaining > 0U) {
            bool progressed = false;
            for (Aero::Detail::VisualEdge& edge : edges) {
                if (!edge.state.IsAttached()) continue;
                bool hasAttachedChild = false;
                for (const Aero::Detail::VisualEdge& candidate : edges) {
                    if (candidate.state.IsAttached() &&
                        candidate.parent == edge.child) {
                        hasAttachedChild = true;
                        break;
                    }
                }
                if (hasAttachedChild) continue;
                Base::Result<void> detached =
                    tree->DetachElement(edge.state);
                if (!detached) return detached.GetStatus();
                --remaining;
                progressed = true;
            }
            if (!progressed) {
                return ViewInvalidState(
                    "Visual edges cannot be detached leaf-first");
            }
        }

        Base::Result<void> rootDetached = tree->DetachRoot(rootAttachment);
        if (!rootDetached) return rootDetached.GetStatus();
        rootAttachment = {};
        attachedRootVisual = nullptr;
        attachedRootLayout = nullptr;
        attachedRootRender = nullptr;
        return {};
    }
    Markup::LoadState loadContext;
    Base::Ref<Markup::EffectLifetime> effectLifetime;
    Base::Ref<Base::Object> root;
    std::uint64_t frameNumber = 0U;
    bool initialized = false;
    bool mounted = false;
    bool terminal = false;

    Base::Result<void> EnsureDefaultXamlProviders() noexcept {
        Base::Result<Base::ResourceUri> light =
            ::Aero::Detail::BuiltInThemeUri(Base::StringView("Light.xaml"));
        if (!light) return light.GetStatus();
        Base::Result<void> status = embeddedXaml.TryAdd(
            light.Value(),
            {Aero::Detail::AeroThemeLightSource,
             static_cast<std::uint32_t>(
                 sizeof(Aero::Detail::AeroThemeLightSource))});
        if (!status) return status.GetStatus();
        Base::Result<Base::ResourceUri> dark =
            ::Aero::Detail::BuiltInThemeUri(Base::StringView("Dark.xaml"));
        if (!dark) return dark.GetStatus();
        status = embeddedXaml.TryAdd(
            dark.Value(),
            {Aero::Detail::AeroThemeDarkSource,
             static_cast<std::uint32_t>(
                 sizeof(Aero::Detail::AeroThemeDarkSource))});
        if (!status) return status.GetStatus();
        Base::Result<Base::ResourceUri> generic =
            ::Aero::Detail::BuiltInThemeUri(Base::StringView("Generic.xaml"));
        if (!generic) return generic.GetStatus();
        status = embeddedXaml.TryAdd(
            generic.Value(),
            {Aero::Detail::AeroThemeGenericSource,
             static_cast<std::uint32_t>(
                 sizeof(Aero::Detail::AeroThemeGenericSource))});
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
                "View must be initialized before XAML loading");
        }
        if (mounted || root || loadedDocument.root) {
            return ViewInvalidState(
                "View already owns a loaded document");
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
            &Core::Detail::MetadataPrivate::
                DependencyProperties(*metadata);
        loadContext.effectLifetime = effectLifetime;
        loadContext.effectCommitMode = deferredEffects
            ? Markup::EffectCommitMode::Deferred
            : Markup::EffectCommitMode::Immediate;
        Markup::Detail::LoadOptionsPrivate::SetContext(
            result, &loadContext);
        return result;
    }

    void AttachTextLayout(
        Aero::Visual& node,
        Controls::Detail::TextBlockLayout* service,
        bool invalidate = false) noexcept {
        if (metadata == nullptr) return;
        const Core::TypeId type = node.RuntimeType();
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::TextBlock::StaticTypeId())) {
            Controls::Detail::ControlPrivate::Attach(
                *static_cast<Controls::TextBlock*>(&node),
                service,
                invalidate);
        }
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::TextBox::StaticTypeId())) {
            Controls::Detail::ControlPrivate::Attach(
                *static_cast<Controls::TextBox*>(&node),
                service,
                invalidate);
        }
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::PasswordBox::
                    StaticTypeId())) {
            Controls::Detail::ControlPrivate::Attach(
                *static_cast<Controls::PasswordBox*>(
                    &node),
                service,
                invalidate);
        }
    }

    Aero::Detail::MeshResources*
    GetMeshResources() noexcept {
        return device ? device->Resources().meshes : nullptr;
    }

    Aero::Detail::ImageResources*
    GetImageResources() noexcept {
        return device ? device->Resources().images : nullptr;
    }

    void AttachPathResources(
        Aero::Visual& node,
        Aero::Detail::MeshResources* service,
        bool invalidate = false) noexcept {
        if (metadata == nullptr) return;
        const Core::TypeId type = node.RuntimeType();
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::Path::StaticTypeId())) {
            Controls::Detail::ControlPrivate::Attach(
                *static_cast<Controls::Path*>(&node),
                service,
                invalidate);
        }
    }

    void VisitTextElements(
        Aero::Visual* rootVisual,
        Controls::Detail::TextBlockLayout* service,
        bool invalidate = false,
        bool ancestorsVisible = true) noexcept {
        if (rootVisual == nullptr) return;
        bool effectivelyVisible = ancestorsVisible;
        if (Aero::UIElement* element =
                rootVisual->AsUIElement();
            element != nullptr) {
            effectivelyVisible =
                ancestorsVisible &&
                element->GetVisibility() ==
                    Aero::Visibility::Visible;
        }
        AttachTextLayout(
            *rootVisual,
            service,
            invalidate && effectivelyVisible);
        for (Aero::Visual* child :
             Aero::Detail::ElementPrivate::VisualChildren(*rootVisual)) {
            VisitTextElements(
                child,
                service,
                invalidate,
                effectivelyVisible);
        }
    }

    void VisitPaths(
        Aero::Visual* rootVisual,
        Aero::Detail::MeshResources* service,
        bool invalidate = false,
        bool ancestorsVisible = true) noexcept {
        if (rootVisual == nullptr) return;
        bool effectivelyVisible = ancestorsVisible;
        if (Aero::UIElement* element =
                rootVisual->AsUIElement();
            element != nullptr) {
            effectivelyVisible =
                ancestorsVisible &&
                element->GetVisibility() ==
                    Aero::Visibility::Visible;
        }
        AttachPathResources(
            *rootVisual,
            service,
            invalidate && effectivelyVisible);
        for (Aero::Visual* child :
             Aero::Detail::ElementPrivate::VisualChildren(*rootVisual)) {
            VisitPaths(
                child,
                service,
                invalidate,
                effectivelyVisible);
        }
    }

    static void TextLifecycleHook(
        const Aero::ElementTreeLifecycleEvent& event,
        void* context) noexcept {
        auto* runtime = static_cast<ViewData*>(context);
        if (runtime == nullptr || event.node == nullptr) {
            return;
        }
        runtime->AttachTextLayout(
            *event.node,
            event.loaded && runtime->text != nullptr
                ? runtime->text->Layout()
                : nullptr);
        runtime->AttachPathResources(
            *event.node,
            event.loaded
                ? runtime->GetMeshResources()
                : nullptr);
    }

    void ClearLoadedDocument() noexcept {
        loadedDocument.Clear();
    }

    Aero::ResourceEnvironment ResourceEnvironment() const noexcept {
        return {
            &applicationResources,
            &themeResources,
            &systemResources};
    }

    Base::Result<Aero::ResourceDictionary*>
    ResolveResourceLayer(
        ResourceLayer layer) noexcept {
        switch (layer) {
        case ResourceLayer::Application:
            return &applicationResources;
        case ResourceLayer::Theme:
            return &themeResources;
        case ResourceLayer::System:
            return &systemResources;
        }
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View resource layer is invalid");
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

    void DetachUi() noexcept {
        DetachUi(
            RootVisual(),
            {loadedDocument.visualContent.nodes.Data(),
             loadedDocument.visualContent.nodes.Size()});
    }

    Aero::Visual* RootVisual() noexcept {
        if (!root) return nullptr;
        if (!metadata->Types().IsDerivedFrom(
                root->RuntimeType(),
                Aero::Visual::StaticTypeId())) {
            return nullptr;
        }
        return static_cast<Aero::Visual*>(root.Get());
    }

    Base::Result<void> SynchronizeOverlays() noexcept {
        renderOverlays.Clear();
        inputOverlays.Clear();
        overlayOrigins.Clear();
        Aero::Visual* rootVisual =
            RootVisual();
        if (rootVisual == nullptr ||
            renderer == nullptr) {
            if (input != nullptr) input->ClearOverlays();
            return {};
        }
        Base::Vector<Aero::Visual*> stack(
            allocator);
        Base::Result<void> appended =
            stack.TryPushBack(rootVisual);
        if (!appended) return appended.GetStatus();
        while (!stack.Empty()) {
            Aero::Visual* node =
                stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;
            const Core::TypeId type =
                node->RuntimeType();
            bool open = false;
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::Primitives::Popup::
                        StaticTypeId())) {
                open =
                    static_cast<Controls::Primitives::Popup*>(
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
                Aero::Visual* ancestor =
                    node;
                while (ancestor != nullptr) {
                    Aero::UIElement*
                        element =
                            ancestor->AsUIElement();
                    if (element != nullptr &&
                        !element->GetIsVisible()) {
                        open = false;
                        break;
                    }
                    ancestor =
                        ancestor->GetVisualParent();
                }
            }
            if (open) {
                Aero::FrameworkElement*
                    framework =
                        node->AsFrameworkElement();
                Aero::UIElement* input =
                    node->AsUIElement();
                if (framework != nullptr &&
                    input != nullptr) {
                    auto rootOrigin = [](
                        Aero::UIElement&
                            element) noexcept {
                        Aero::Point
                            result{};
                        Aero::Visual*
                            current = &element;
                        while (current != nullptr) {
                            Aero::UIElement*
                                currentElement =
                                    current->
                                        AsUIElement();
                            if (currentElement !=
                                nullptr) {
                                Aero::FrameworkElement*
                                    currentFramework =
                                        currentElement->
                                            AsFrameworkElement();
                                if (currentFramework !=
                                    nullptr) {
                                    result =
                                        Aero::Media::TransformPoint(
                                                currentFramework->
                                                    GetLocalVisualTransform(),
                                                result);
                                }
                                const Aero::Rect slot =
                                        currentElement->
                                            GetLayoutSlot();
                                result.x += slot.x;
                                result.y += slot.y;
                            }
                            current =
                                current->
                                    GetVisualParent();
                        }
                        return result;
                    };
                    Aero::Point origin =
                        rootOrigin(*input);
                    if (metadata->Types().
                            IsDerivedFrom(
                                type,
                                Controls::
                                    ContextMenu::
                                        StaticTypeId())) {
                        Base::Ref<
                            Aero::UIElement>
                            target =
                                static_cast<
                                    Controls::
                                        ContextMenu*>(
                                    node)->
                                    PlacementTarget();
                        if (target &&
                            target->
                                GetIsArrangeValid()) {
                            origin =
                                rootOrigin(*target);
                            origin.y +=
                                target->
                                    GetRenderSize().
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
                Aero::Visual* const>
                children =
                    Aero::Detail::ElementPrivate::VisualChildren(*node);
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
        return input != nullptr
            ? input->SetOverlays(
                  inputOverlays.AsSpan(),
                  overlayOrigins.AsSpan())
            : Base::Result<void>();
    }

    void ClearOverlays() noexcept {
        if (input != nullptr) input->ClearOverlays();
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
        for (Aero::UIElement* overlay :
             inputOverlays) {
            if (overlay == nullptr) continue;
            const Core::TypeId type =
                overlay->RuntimeType();
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::Primitives::Popup::
                        StaticTypeId())) {
                auto* popup =
                    static_cast<Controls::Primitives::Popup*>(
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
        const Aero::Visual& root,
        const Aero::Visual& target)
        noexcept {
        const Aero::Visual* current =
            &target;
        while (current != &root) {
            current = current->GetVisualParent();
            if (current == nullptr) return false;
        }
        return true;
    }

    Base::Result<void> RestoreOverlayFocus()
        noexcept {
        if (!overlayFocusReturn ||
            input == nullptr) {
            overlayFocusReturn.Reset();
            return {};
        }
        Base::Ref<Aero::UIElement>
            target =
                std::move(overlayFocusReturn);
        Base::Result<bool> restored =
            input->SetFocus(target.Get());
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
        const Input::PointerInput& input,
        Aero::UIElement* target)
        noexcept {
        if (input.action !=
                Input::PointerAction::Down ||
            inputOverlays.Empty()) {
            return {};
        }
        bool closedFocusedOverlay = false;
        for (std::uint32_t index =
                 inputOverlays.Size();
             index > 0U;
             --index) {
            Aero::UIElement* overlay =
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
                    Controls::Primitives::Popup::
                        StaticTypeId())) {
                auto* popup =
                    static_cast<Controls::Primitives::Popup*>(
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
            Aero::UIElement* overlay =
                inputOverlays[index - 1U];
            if (overlay == nullptr) continue;
            const Core::TypeId type =
                overlay->RuntimeType();
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::Primitives::Popup::
                        StaticTypeId())) {
                Base::Result<void> closed =
                    static_cast<Controls::Primitives::Popup*>(
                        overlay)->SetIsOpen(false);
                if (!closed) {
                    return closed.GetStatus();
                }
                static_cast<void>(
                    static_cast<Controls::Primitives::Popup*>(
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
        const Input::PointerInput& input,
        Aero::UIElement* hitTarget)
        noexcept {
        if (input.action !=
                Input::PointerAction::Down ||
            input.changedButton !=
                Input::MouseButton::Right) {
            return {};
        }
        Aero::Visual* current =
            hitTarget;
        while (current != nullptr) {
            Aero::UIElement* element =
                current->AsUIElement();
            if (element != nullptr) {
                Base::Ref<Controls::ContextMenu>
                    menu =
                        Controls::
                            ContextMenuService::
                                GetContextMenu(
                                    *element);
                if (menu) {
                    if (this->input != nullptr &&
                        !overlayFocusReturn) {
                        Aero::UIElement*
                            focused =
                                this->input->GetFocusedElement();
                        if (focused != nullptr) {
                            overlayFocusReturn =
                                Base::Ref<
                                    Aero::UIElement>::
                                    TryFromBorrowed(
                                        *focused);
                        }
                    }
                    Base::Ref<
                        Aero::UIElement>
                        target =
                            Base::Ref<
                                Aero::UIElement>::
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
                    if (this->input != nullptr) {
                        Base::Result<bool> focused =
                            this->input->SetFocus(
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
            current = current->GetVisualParent();
        }
        return {};
    }

    Base::Result<void> UpdateToolTipForPointer(
        const Input::PointerInput& input,
        Aero::UIElement* hitTarget)
        noexcept {
        if (input.action ==
                Input::PointerAction::Down) {
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
            Input::PointerAction::Move) {
            return {};
        }
        Base::Ref<Controls::ToolTip> next;
        Base::Ref<Aero::UIElement>
            nextTarget;
        Aero::Visual* current =
            hitTarget;
        while (current != nullptr) {
            Aero::UIElement* element =
                current->AsUIElement();
            if (element != nullptr) {
                next =
                    Controls::ToolTipService::
                        GetToolTip(*element);
                if (next) {
                    nextTarget =
                        Base::Ref<
                            Aero::UIElement>::
                            TryFromBorrowed(
                                *element);
                    break;
                }
            }
            current = current->GetVisualParent();
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

    Base::Result<Aero::Visual*> ResolveVisual(
        Base::Object& object, Core::TypeId type) noexcept {
        if (object.RuntimeType() != type ||
            !metadata->Types().IsDerivedFrom(
                type, Aero::Visual::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View root is not a registered Visual");
        }
        return static_cast<Aero::Visual*>(&object);
    }

    Base::Result<Aero::UIElement*> ResolveUIElement(
        Base::Object& object, Core::TypeId type) noexcept {
        Base::Result<Aero::Visual*> visual =
            ResolveVisual(object, type);
        if (!visual) return visual.GetStatus();
        Aero::UIElement* element =
            visual.Value()->AsUIElement();
        if (element == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View root is not a UIElement");
        }
        return element;
    }

    Aero::FrameworkElement* ResolveFrameworkElement(
        Base::Object& object, Core::TypeId type) noexcept {
        Base::Result<Aero::Visual*> visual =
            ResolveVisual(object, type);
        return visual ? visual.Value()->AsFrameworkElement() : nullptr;
    }

    static Base::Object* FindNameForElement(
        void* context,
        Base::StringView name,
        Core::TypeId expectedType) noexcept {
        auto* runtime = static_cast<ViewData*>(context);
        if (runtime == nullptr || name.Empty()) return nullptr;
        Base::Object* object = runtime->loadedDocument.names.Find(name);
        if (object == nullptr || expectedType == Core::InvalidTypeId) {
            return object;
        }
        return runtime->metadata != nullptr &&
            runtime->metadata->Types().IsAssignableFrom(
                expectedType, object->RuntimeType())
            ? object
            : nullptr;
    }

    Base::Result<void> ApplyUi(Aero::Visual& root) noexcept {
        if (metadata == nullptr || values == nullptr || bindings == nullptr ||
            events == nullptr || input == nullptr || styles == nullptr ||
            templates == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "View UI state is unavailable");
        }

        const Aero::ResourceEnvironment resources = ResourceEnvironment();
        Base::Vector<Aero::Visual*> stack(allocator);
        Base::Result<void> pushed = stack.TryPushBack(&root);
        if (!pushed) return pushed.GetStatus();
        while (!stack.Empty()) {
            Aero::Visual* node = stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;

            if (Aero::UIElement* ui = node->AsUIElement()) {
                ElementPrivate::SetViewServices(*ui, &elementHost);
            }

            Base::Result<std::uint32_t> activated =
                bindings->ActivateDeferred(
                    *static_cast<::Aero::DependencyObject*>(node));
            if (!activated) return activated.GetStatus();

            Aero::FrameworkElement* element = node->AsFrameworkElement();
            if (element != nullptr) {
                Base::Result<const Aero::Style*> resolved =
                    ResolveUiValue<Aero::Style>(
                        *element, Aero::FrameworkElement::StyleProperty,
                        resources,
                        "FrameworkElement Style value is not a Style");
                if (!resolved) return resolved.GetStatus();
                const Aero::Style* style = resolved.Value();
                if (style != nullptr) {
                    if (!style->IsSealed()) {
                        return Base::Status::Failure(
                            Base::ErrorCode::InvalidState,
                            "Implicit Style is not sealed");
                    }
                    if (styles->AppliedStyle(*element) != style) {
                        Base::Result<void> applied = styles->Apply(*element, *style);
                        if (!applied) return applied.GetStatus();
                    }
                }
            }

            Base::Result<std::uint32_t> styleValues = values->Flush();
            if (!styleValues) return styleValues.GetStatus();

            if (metadata->Types().IsDerivedFrom(
                    node->RuntimeType(), Controls::Control::StaticTypeId())) {
                auto& control = *static_cast<Controls::Control*>(node);
                Controls::Detail::ControlPrivate::AttachTemplateEngine(
                    control, templates);
                Base::Result<const Controls::ControlTemplate*> resolved =
                    ResolveUiValue<Controls::ControlTemplate>(
                        control, Controls::Control::TemplateProperty, resources,
                        "Control Template value is not a ControlTemplate");
                if (!resolved) return resolved.GetStatus();
                const Controls::ControlTemplate* controlTemplate =
                    resolved.Value();
                if (controlTemplate != nullptr) {
                    const Controls::Detail::TemplateHandle existing =
                        templates->AppliedHandle(control);
                    if (!existing.IsValid() ||
                        templates->AppliedTemplate(existing) != controlTemplate) {
                        Base::Result<Controls::Detail::TemplateHandle> applied =
                            templates->Apply(control, *controlTemplate);
                        if (!applied) return applied.GetStatus();
                    }
                }
            }

            for (Aero::Visual* child :
                 Aero::Detail::ElementPrivate::VisualChildren(*node)) {
                pushed = stack.TryPushBack(child);
                if (!pushed) return pushed.GetStatus();
            }
        }
        Base::Result<std::uint32_t> appliedValues = values->Flush();
        return appliedValues ? Base::Result<void>()
                             : Base::Result<void>(appliedValues.GetStatus());
    }

    void DetachUi(
        Aero::Visual* root,
        Base::Span<Aero::Visual* const> declarationNodes) noexcept {
        if (values == nullptr) return;

        Base::Vector<Aero::Visual*> reachable(allocator);
        if (root != nullptr) {
            (void)reachable.TryPushBack(root);
            for (std::uint32_t index = 0U; index < reachable.Size(); ++index) {
                Aero::Visual* node = reachable[index];
                if (node == nullptr) continue;
                for (Aero::Visual* child :
                     Aero::Detail::ElementPrivate::VisualChildren(*node)) {
                    if (child != nullptr) (void)reachable.TryPushBack(child);
                }
            }
        }

        for (Aero::Visual* node : reachable) {
            if (node == nullptr) continue;
            if (Aero::UIElement* ui = node->AsUIElement()) {
                ElementPrivate::SetViewServices(*ui, nullptr);
            }
            if (bindings != nullptr) (void)bindings->DetachObject(*node);
            Aero::FrameworkElement* element = node->AsFrameworkElement();
            if (element != nullptr && styles != nullptr) {
                (void)styles->DetachObject(*element);
            }
        }
        for (std::uint32_t index = reachable.Size(); index > 0U; --index) {
            Aero::Visual* node = reachable[index - 1U];
            if (node == nullptr || metadata == nullptr ||
                !metadata->Types().IsDerivedFrom(
                    node->RuntimeType(), Controls::Control::StaticTypeId())) {
                continue;
            }
            auto& control = *static_cast<Controls::Control*>(node);
            if (visualStates != nullptr) {
                (void)Controls::Detail::TemplatePrivate::Clear(
                    *visualStates, control);
            }
            if (templates != nullptr) (void)templates->Clear(control);
        }
        for (Aero::Visual* node : declarationNodes) {
            if (node != nullptr) (void)values->DetachObject(*node);
        }
    }

    Base::Result<void> CreateUiEngines() noexcept {
        Base::Result<void> status = arena.Create(
            templates, *tree, *values,
            Core::Detail::MetadataPrivate::
                DependencyProperties(*metadata),
            layout, renderer, metadata, bindings);
        if (!status) return status.GetStatus();
        Base::Result<Controls::VisualStateManager*> createdStates =
            Controls::Detail::TemplatePrivate::Create(
                *values,
                *templates,
                *animations,
                Core::Detail::MetadataPrivate::
                    DependencyProperties(*metadata));
        if (!createdStates) return createdStates.GetStatus();
        visualStates = createdStates.Value();
        status = arena.Create(
            styles, *values,
            Core::Detail::MetadataPrivate::
                DependencyProperties(*metadata));
        if (!status) return status.GetStatus();
        styles->SetTriggerActionHandler(
            &ViewData::ExecuteStyleTriggerActions, this);
        elementHost.events = events;
        elementHost.input = input;
        elementHost.nameScopeContext = this;
        elementHost.findName = &ViewData::FindNameForElement;
        return {};
    }

    static Base::Result<void> GeneratedItemSubtreeChanged(
        Aero::Visual& root,
        Controls::ItemSubtreeChange change,
        void* context) noexcept {
        auto* runtime = static_cast<ViewData*>(context);
        if (runtime == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Generated item subtree runtime context is null");
        }
        if (change ==
            Controls::ItemSubtreeChange::Unmounting) {
            Base::Result<Aero::VisualHandle>
                rootHandle =
                    runtime->tree->GetHandle(root);
            if (rootHandle) {
                for (std::uint32_t index = 0U;
                     index <
                         runtime->
                             pendingGeneratedVisuals.
                                 Size();) {
                    if (runtime->
                            pendingGeneratedVisuals[
                                index].index !=
                            rootHandle.Value().index ||
                        runtime->
                            pendingGeneratedVisuals[
                                index].generation !=
                            rootHandle.Value().generation) {
                        ++index;
                        continue;
                    }
                    for (std::uint32_t move =
                             index + 1U;
                         move <
                             runtime->
                                 pendingGeneratedVisuals.
                                     Size();
                         ++move) {
                        runtime->
                            pendingGeneratedVisuals[
                                move - 1U] =
                            runtime->
                                pendingGeneratedVisuals[
                                    move];
                    }
                    runtime->
                        pendingGeneratedVisuals.
                            PopBack();
                    return {};
                }
            }
            runtime->DetachUi(
                &root, {});
            return {};
        }
        if (runtime->bindings != nullptr &&
            runtime->bindings->IsFlushing()) {
            Base::Result<Aero::VisualHandle>
                handle =
                    runtime->tree->GetHandle(root);
            if (!handle) return handle.GetStatus();
            return runtime->
                pendingGeneratedVisuals.
                    TryPushBack(handle.Value());
        }
        Base::Result<void> applied =
            runtime->ApplyUi(root);
        if (!applied) {
            runtime->DetachUi(
                &root, {});
            return applied.GetStatus();
        }
        Base::Result<std::uint32_t> started =
            runtime->StartLoadedAnimations(&root);
        if (!started) {
            runtime->DetachUi(
                &root, {});
            return started.GetStatus();
        }
        return {};
    }

    Base::Result<void>
    FlushGeneratedVisuals() noexcept {
        constexpr std::uint32_t MaximumWaves = 16U;
        for (std::uint32_t wave = 0U;
             wave < MaximumWaves;
             ++wave) {
            if (pendingGeneratedVisuals.Empty()) {
                return {};
            }
            Base::Vector<Aero::VisualHandle>
                pending =
                    std::move(
                        pendingGeneratedVisuals);
            pendingGeneratedVisuals.Clear();
            for (const Aero::VisualHandle handle :
                 pending) {
                Aero::Visual* subtreeRoot =
                    tree->ResolveHandle(handle);
                if (subtreeRoot == nullptr) continue;
                Base::Result<void> applied =
                    ApplyUi(
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
            "Generated item visual activation exceeded the bounded activation waves");
    }

    void DestroyUiEngines() noexcept {
        elementHost = {};
        arena.Destroy(styles);
        delete visualStates;
        visualStates = nullptr;
        arena.Destroy(templates);
    }

    Base::Result<void> VisitAndAttach(
        Aero::Visual& rootVisual) noexcept {
        Base::Vector<Aero::Visual*> stack(allocator);
        Base::Result<void> pushed =
            stack.TryPushBack(&rootVisual);
        if (!pushed) return pushed.GetStatus();
        while (!stack.Empty()) {
            Aero::Visual* node = stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;
            const Core::TypeId type = node->RuntimeType();
            if (controlBehaviors != nullptr) {
                Base::Result<void> attached = controlBehaviors->Attach(
                    *node, options.textInputMethodHost);
                if (!attached) return attached.GetStatus();
            }
            AttachTextLayout(
                *node,
                text != nullptr
                    ? text->Layout()
                    : nullptr);
            AttachPathResources(*node, GetMeshResources());
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
                    Base::Result<Controls::ItemContainerGenerator*>
                        created = Controls::Detail::
                            ControlPrivate::Create(
                                *tree,
                                *layout,
                                *values,
                                styles,
                                renderer,
                                templates,
                                &ViewData::GeneratedItemSubtreeChanged,
                                this);
                    if (!created) return created.GetStatus();
                    Controls::ItemContainerGenerator* generator =
                        created.Value();
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
                        delete generator;
                        generator = nullptr;
                        return attached.GetStatus();
                    }
                    Base::Result<void> generatedUiApplied =
                        ApplyUi(
                            *host);
                    if (!generatedUiApplied) {
                        DetachUi(
                            host, {});
                        static_cast<void>(
                            generator->Detach());
                        delete generator;
                        generator = nullptr;
                        return generatedUiApplied.GetStatus();
                    }
                    Base::Result<void> tracked =
                        itemGenerators.TryPushBack(
                            generator);
                    if (!tracked) {
                        static_cast<void>(
                            generator->Detach());
                        delete generator;
                        generator = nullptr;
                        return tracked.GetStatus();
                    }
                }
            }
            const Base::Span<Aero::Visual* const>
                children = Aero::Detail::ElementPrivate::VisualChildren(*node);
            for (std::uint32_t index = 0U;
                 index < children.Size(); ++index) {
                pushed = stack.TryPushBack(children[index]);
                if (!pushed) return pushed.GetStatus();
            }
        }
        return {};
    }

    void ClearTextInputHosts(
        Aero::Visual* node) noexcept {
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
        for (Aero::Visual* child :
             Aero::Detail::ElementPrivate::VisualChildren(*node)) {
            ClearTextInputHosts(child);
        }
    }

    Base::Result<Base::StringView> AnimationAttachedString(
        MediaAnimation::Timeline& timeline,
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
        ::Aero::DependencyObject* target = nullptr;
        Core::DependencyPropertyHandle property;
    };

    Base::Result<ResolvedAnimationProperty>
    ResolveAnimationProperty(
        ::Aero::DependencyObject& target,
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

        ::Aero::DependencyObject* propertyTarget = &target;
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
                    Core::Detail::MetadataPrivate::
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
                        Media::GradientBrush::StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard target property is not a GradientBrush");
                }
                auto& brush = static_cast<
                    Media::GradientBrush&>(
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
                Base::Ref<Media::Transform> transform;
                if (beforeIndex ==
                        Base::StringView(
                            "(TransformGroup.Children)") &&
                    metadata->Types().IsDerivedFrom(
                        target.RuntimeType(),
                        Media::TransformGroup::StaticTypeId())) {
                    transform =
                        Base::Ref<Media::Transform>::
                            TryFromBorrowed(
                                static_cast<
                                    Media::Transform&>(
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
                                Aero::FrameworkElement::StaticTypeId())) {
                            return Base::Status::Failure(
                                Base::ErrorCode::InvalidArgument,
                                "Storyboard LayoutTransform target is not a FrameworkElement");
                        }
                        transform =
                            static_cast<Aero::FrameworkElement&>(
                                target).GetLayoutTransform();
                    } else {
                        if (!metadata->Types().IsDerivedFrom(
                                target.RuntimeType(),
                                Aero::UIElement::StaticTypeId())) {
                            return Base::Status::Failure(
                                Base::ErrorCode::InvalidArgument,
                                "Storyboard RenderTransform target is not a UIElement");
                        }
                        transform =
                            static_cast<Aero::UIElement&>(
                                target).GetRenderTransform();
                    }
                }
                if (!transform ||
                    !metadata->Types().IsDerivedFrom(
                        transform->RuntimeType(),
                        Media::TransformGroup::StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard transform path has no TransformGroup");
                }
                auto& group = static_cast<
                    Media::TransformGroup&>(
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
                            MetadataPrivate::
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
                        ::Aero::DependencyObject::
                            StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard compound object property has no DependencyObject value");
                }
                propertyTarget =
                    static_cast<
                        ::Aero::DependencyObject*>(
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
                Base::Ref<Media::Transform> transform;
                if (layoutPath) {
                    if (!metadata->Types().IsDerivedFrom(
                            target.RuntimeType(),
                            Aero::FrameworkElement::StaticTypeId())) {
                        return Base::Status::Failure(
                            Base::ErrorCode::InvalidArgument,
                            "LayoutTransform animation target is not a FrameworkElement");
                    }
                    transform =
                        static_cast<Aero::FrameworkElement&>(
                            target).GetLayoutTransform();
                } else {
                    if (!metadata->Types().IsDerivedFrom(
                            target.RuntimeType(),
                            Aero::UIElement::StaticTypeId())) {
                        return Base::Status::Failure(
                            Base::ErrorCode::InvalidArgument,
                            "RenderTransform animation target is not a UIElement");
                    }
                    transform =
                        static_cast<Aero::UIElement&>(
                            target).GetRenderTransform();
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
                            MetadataPrivate::
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
                        ::Aero::DependencyObject::
                            StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard object property has no DependencyObject value");
                }
                propertyTarget =
                    static_cast<
                        ::Aero::DependencyObject*>(
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
            Core::Detail::MetadataPrivate::
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

    struct StoryboardTimingState final {
        Aero::Detail::Animation::AnimationTime beginTimeMicroseconds = 0U;
        Aero::Detail::Animation::AnimationTime durationMicroseconds = 0U;
        Aero::Detail::Animation::RepeatBehavior repeat;
        double speedRatio = 1.0;
        bool hasDuration = false;
        bool hasRepeat = false;
        bool autoReverse = false;
    };

    StoryboardTimingState ComposeStoryboardTiming(
        const StoryboardTimingState* inherited,
        const MediaAnimation::Timeline& storyboard) noexcept {
        StoryboardTimingState result =
            inherited != nullptr
            ? *inherited
            : StoryboardTimingState{};
        const Aero::Detail::Animation::TimelineTiming authored =
            Aero::Detail::AnimationPrivate::Timing(storyboard);
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

    Aero::Detail::Animation::TimelineTiming EffectiveTimelineTiming(
        const MediaAnimation::Timeline& timeline,
        const StoryboardTimingState* inherited) noexcept {
        Aero::Detail::Animation::TimelineTiming result =
            Aero::Detail::AnimationPrivate::Timing(timeline);
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
            Aero::Detail::Animation::AnimationHandle>
            started,
        Base::Vector<
            Aero::Detail::Animation::AnimationHandle>*
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
        MediaAnimation::Timeline& timeline,
        Aero::FrameworkElement& triggerOwner,
        const StoryboardTimingState* inherited = nullptr,
        Base::Vector<
            Aero::Detail::Animation::AnimationHandle>*
            retainedHandles = nullptr,
        Aero::Detail::DataTemplateTriggerState*
            dataTemplateContext = nullptr) noexcept {
        if (animations == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Storyboard requires the animation manager");
        }
        if (timeline.RuntimeType() ==
            MediaAnimation::Storyboard::StaticTypeId()) {
            auto& nested =
                static_cast<MediaAnimation::Storyboard&>(timeline);
            const StoryboardTimingState timing =
                ComposeStoryboardTiming(
                    inherited, nested);
            std::uint32_t count = 0U;
            for (const Base::Ref<MediaAnimation::Timeline>& child :
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
                MediaAnimation::Storyboard::TargetNameProperty);
        if (!targetName) return targetName.GetStatus();
        Base::Result<Base::StringView> targetPath =
            AnimationAttachedString(
                timeline,
                MediaAnimation::Storyboard::TargetPropertyProperty);
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
                ::Aero::DependencyObject::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Storyboard target name does not resolve to a DependencyObject");
        }
        auto& target =
            static_cast<::Aero::DependencyObject&>(*targetObject);
        Base::Result<ResolvedAnimationProperty> property =
            ResolveAnimationProperty(target, targetPath.Value());
        if (!property) return property.GetStatus();
        ::Aero::DependencyObject& propertyTarget =
            *property.Value().target;
        const Core::DependencyPropertyHandle propertyHandle =
            property.Value().property;

        const Core::TypeId type = timeline.RuntimeType();
        if (type == MediaAnimation::DoubleAnimation::StaticTypeId()) {
            auto& animation =
                static_cast<MediaAnimation::DoubleAnimation&>(timeline);
            Aero::Detail::Animation::DoubleAnimation runtime =
                Aero::Detail::AnimationPrivate::Double(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<Aero::Detail::Animation::AnimationHandle> started =
                animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type == MediaAnimation::ColorAnimation::StaticTypeId()) {
            auto& animation =
                static_cast<MediaAnimation::ColorAnimation&>(timeline);
            Aero::Detail::Animation::ColorAnimation runtime =
                Aero::Detail::AnimationPrivate::Color(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<Aero::Detail::Animation::AnimationHandle> started =
                animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            MediaAnimation::PointAnimation::
                StaticTypeId()) {
            auto& animation =
                static_cast<
                    MediaAnimation::PointAnimation&>(
                        timeline);
            Aero::Detail::Animation::PointAnimation runtime =
                Aero::Detail::AnimationPrivate::Point(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Aero::Detail::Animation::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            MediaAnimation::RectAnimation::
                StaticTypeId()) {
            auto& animation =
                static_cast<
                    MediaAnimation::RectAnimation&>(
                        timeline);
            Aero::Detail::Animation::RectAnimation runtime =
                Aero::Detail::AnimationPrivate::Rect(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Aero::Detail::Animation::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            MediaAnimation::ThicknessAnimation::
                StaticTypeId()) {
            auto& animation =
                static_cast<
                    MediaAnimation::ThicknessAnimation&>(
                        timeline);
            Aero::Detail::Animation::ThicknessAnimation runtime =
                Aero::Detail::AnimationPrivate::Thickness(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Aero::Detail::Animation::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            MediaAnimation::DoubleAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::DoubleAnimationUsingKeyFrames&>(timeline);
            Base::Vector<Aero::Detail::Animation::DoubleKeyFrame> frames(allocator);
            for (const Base::Ref<MediaAnimation::DoubleKeyFrame>& frame :
                 animation.KeyFrames()) {
                if (!frame) continue;
                Base::Result<void> appended =
                    frames.TryPushBack(Aero::Detail::AnimationPrivate::DoubleFrame(*frame));
                if (!appended) return appended.GetStatus();
            }
            for (std::uint32_t index = 1U;
                 index < frames.Size(); ++index) {
                Aero::Detail::Animation::DoubleKeyFrame current =
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
            Aero::Detail::Animation::DoubleKeyFrameAnimation runtime;
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
            Base::Result<Aero::Detail::Animation::AnimationHandle> started =
                animations->Begin(
                    propertyTarget, propertyHandle, runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (type ==
            MediaAnimation::ColorAnimationUsingKeyFrames::
                StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::ColorAnimationUsingKeyFrames&>(
                    timeline);
            Base::Vector<Aero::Detail::Animation::ColorKeyFrame>
                frames(allocator);
            for (const Base::Ref<
                     MediaAnimation::ColorKeyFrame>& frame :
                 animation.KeyFrames()) {
                if (!frame) continue;
                Base::Result<void> appended =
                    frames.TryPushBack(
                        Aero::Detail::AnimationPrivate::ColorFrame(*frame));
                if (!appended) {
                    return appended.GetStatus();
                }
            }
            for (std::uint32_t index = 1U;
                 index < frames.Size();
                 ++index) {
                Aero::Detail::Animation::ColorKeyFrame current =
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
            Aero::Detail::Animation::ColorKeyFrameAnimation
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
                Aero::Detail::Animation::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }

        Base::Vector<Aero::Detail::Animation::DiscreteAnimationKeyFrame>
            frames(allocator);
        if (type ==
            MediaAnimation::ThicknessAnimationUsingKeyFrames::
                StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::ThicknessAnimationUsingKeyFrames&>(
                    timeline);
            for (const Base::Ref<
                     MediaAnimation::ThicknessKeyFrame>& frame :
                 animation.KeyFrames()) {
                if (!frame) continue;
                Aero::Detail::Animation::DiscreteAnimationKeyFrame runtime;
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
            MediaAnimation::BooleanAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::BooleanAnimationUsingKeyFrames&>(timeline);
            for (const Base::Ref<
                     MediaAnimation::DiscreteBooleanKeyFrame>& frame :
                 animation.KeyFrames()) {
                if (!frame) continue;
                Aero::Detail::Animation::DiscreteAnimationKeyFrame runtime;
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
            MediaAnimation::ObjectAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::ObjectAnimationUsingKeyFrames&>(timeline);
            for (const Base::Ref<
                     MediaAnimation::DiscreteObjectKeyFrame>& frame :
                 animation.KeyFrames()) {
                if (!frame) continue;
                Aero::Detail::Animation::DiscreteAnimationKeyFrame runtime;
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
            Aero::Detail::Animation::DiscreteAnimationKeyFrame current =
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
        Aero::Detail::Animation::DiscreteAnimation runtime;
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
        Base::Result<Aero::Detail::Animation::AnimationHandle> started =
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
            if (metadata == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "DataTemplate Trigger metadata is unavailable");
            }
            Base::Result<Core::PropertyValue> converted =
                metadata->TryConvertText(
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
        Aero::Detail::DataTemplateTriggerState& context,
        Aero::Detail::DataTemplateTriggerCondition& condition) noexcept {
        Core::PropertyValue current;
        if (condition.dependencySource &&
            condition.property.IsValid()) {
            Base::Result<Core::PropertyValue> value =
                condition.dependencySource->GetValue(
                    condition.property);
            if (!value) return value.GetStatus();
            current = std::move(value).Value();
        } else {
            if (!condition.binding || metadata == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "DataTemplate DataTrigger Binding is unavailable");
            }
            Base::Object* source =
                condition.binding->GetElementName().Empty()
                ? condition.source.Get()
                : context.FindName(
                      condition.binding->GetElementName());
            if (source == nullptr &&
                !condition.binding->GetElementName().Empty()) {
                source = loadedDocument.names.Find(
                    condition.binding->GetElementName());
            }
            if (source == nullptr) {
                return false;
            }
            Base::Result<Core::BindingPathPlan> plan =
                Core::BindingPathPlan::Compile(
                    *metadata,
                    source->RuntimeType(),
                    condition.binding->GetPath().GetPath());
            if (!plan) return plan.GetStatus();
            Base::Result<Core::PropertyValue> value =
                plan.Value().Get(*metadata, *source);
            if (!value) return value.GetStatus();
            current = std::move(value).Value();
        }
        return DataTemplateTriggerValuesMatch(
            current, condition.value);
    }

    Base::Result<void> EnsureDataTemplateProviderTokens(
        Aero::Detail::DataTemplateTriggerState& context) noexcept {
        if (values == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "DataTemplate Trigger value engine is unavailable");
        }
        if (context.providerOrigin == 0U) {
            Base::Result<std::uint32_t> origin =
                values->AllocateProviderOrigin();
            if (!origin) return origin.GetStatus();
            context.providerOrigin = origin.Value();
        }

        std::uint64_t ordinal = 0U;
        for (Aero::Detail::DataTemplatePropertyTrigger& trigger :
             context.triggers) {
            for (Aero::Detail::DataTemplateTriggerSetter& setter :
                 trigger.setters) {
                if (ordinal > UINT32_MAX) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "DataTemplate Trigger setter ordinal limit reached");
                }
                const Core::PropertyProviderToken expected{
                    Core::PropertyValueRank::TemplateTrigger,
                    context.providerOrigin,
                    static_cast<std::uint32_t>(ordinal)};
                if (setter.token.IsValid() && setter.token != expected) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidState,
                        "DataTemplate Trigger provider token is inconsistent");
                }
                setter.token = expected;
                ++ordinal;
            }
        }
        return {};
    }

    Base::Result<void> EvaluateDataTemplateTrigger(
        Aero::Detail::DataTemplateTriggerState& context,
        std::uint32_t triggerIndex) noexcept {
        if (triggerIndex >= context.triggers.Size() ||
            context.root == nullptr ||
            values == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "DataTemplate Trigger runtime is unavailable");
        }
        Base::Result<void> providerTokens =
            EnsureDataTemplateProviderTokens(context);
        if (!providerTokens) return providerTokens.GetStatus();

        Aero::Detail::DataTemplatePropertyTrigger& trigger =
            context.triggers[triggerIndex];
        bool active = !trigger.conditions.Empty();
        for (Aero::Detail::DataTemplateTriggerCondition& condition :
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
            for (const Aero::Detail::DataTemplateTriggerSetter& setter :
                 trigger.setters) {
                if (!setter.target) continue;
                Base::Result<void> applied =
                    values->SetProviderContribution(
                        *setter.target,
                        setter.property,
                        setter.token,
                        setter.value);
                if (!applied) {
                    return applied.GetStatus();
                }
            }
        } else {
            for (const Aero::Detail::DataTemplateTriggerSetter& setter :
                 trigger.setters) {
                if (!setter.target) continue;
                Base::Result<bool> cleared =
                    values->ClearProviderContribution(
                        *setter.target,
                        setter.property,
                        setter.token);
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
                    MediaAnimation::TriggerAction::
                        StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "DataTemplate Trigger contains an invalid action");
            }
            Base::Result<void> executed =
                ExecuteAnimationAction(
                    static_cast<
                        MediaAnimation::TriggerAction&>(
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
        Aero::Detail::DataTemplateTriggerState&
            context) noexcept {
        std::uint32_t count = 0U;
        for (std::uint32_t triggerIndex = 0U;
             triggerIndex < context.triggers.Size();
             ++triggerIndex) {
            Aero::Detail::DataTemplatePropertyTrigger&
                trigger =
                    context.triggers[triggerIndex];
            for (std::uint32_t conditionIndex = 0U;
                 conditionIndex <
                     trigger.conditions.Size();
                 ++conditionIndex) {
                Aero::Detail::DataTemplateTriggerCondition&
                    condition =
                        trigger.conditions[conditionIndex];
                if ((!condition.dependencySource ||
                     !condition.property.IsValid()) &&
                    condition.binding) {
                    Base::Object* source =
                        condition.binding->GetElementName().Empty()
                        ? condition.source.Get()
                        : context.FindName(
                              condition.binding->GetElementName());
                    if (source == nullptr &&
                        !condition.binding->GetElementName().Empty()) {
                        source = loadedDocument.names.Find(
                            condition.binding->GetElementName());
                    }
                    if (source != nullptr &&
                        metadata->Types().IsDerivedFrom(
                            source->RuntimeType(),
                            ::Aero::DependencyObject::
                                StaticTypeId())) {
                        const Core::DependencyProperty*
                            property =
                                Core::Detail::
                                    MetadataPrivate::
                                    DependencyProperties(
                                        *metadata)
                                        .Find(
                                            source->
                                                RuntimeType(),
                                            condition.binding->GetPath().GetPath());
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

                DataTemplateTriggerHandlerState*
                    handlerContext = nullptr;
                Base::Result<void> created =
                    AllocateObject(
                        *allocator,
                        Base::MemoryTag::Ui,
                        handlerContext);
                if (!created) {
                    return created.GetStatus();
                }
                handlerContext->runtime = this;
                handlerContext->triggerContext =
                    Base::Ref<
                        Aero::Detail::
                            DataTemplateTriggerState>::
                        FromBorrowed(context);
                handlerContext->triggerIndex =
                    triggerIndex;
                handlerContext->conditionIndex =
                    conditionIndex;
                auto callback =
                    [handlerContext](
                        ::Aero::DependencyObject& object,
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
                    FreeObject(
                        *allocator,
                        Base::MemoryTag::Ui,
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
                    FreeObject(
                        *allocator,
                        Base::MemoryTag::Ui,
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
        Aero::Visual* visual,
        const Aero::NameScope* names = nullptr) noexcept {
        if (visual == nullptr) return std::uint32_t{0U};
        std::uint32_t count = 0U;
        Aero::FrameworkElement* element =
            visual->AsFrameworkElement();
        if (element != nullptr) {
            if (input != nullptr &&
                metadata->Types().IsDerivedFrom(
                    element->RuntimeType(),
                    Controls::Grid::StaticTypeId())) {
                auto& grid = static_cast<Controls::Grid&>(*element);
                for (const Base::Ref<Input::KeyBinding>& binding :
                     grid.InputBindings()) {
                    if (!binding) continue;
                    Base::Result<Input::InputBindingHandle> added =
                        input->TryAddInputBinding(*element, binding);
                    if (!added) return added.GetStatus();
                }
            }
            for (const Base::Ref<Base::Object>& authored :
                 Aero::Detail::ElementPrivate::AuthoredTriggers(*element)) {
                if (!authored) {
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::Detail::
                        DataTemplateTriggerState::
                            StaticTypeId()) {
                    Base::Result<std::uint32_t> started =
                        StartDataTemplateTriggers(
                            static_cast<
                                Aero::Detail::
                                    DataTemplateTriggerState&>(
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
                    MediaAnimation::StoryboardCompletedTrigger::
                        StaticTypeId()) {
                    Base::Result<void> retained =
                        storyboardCompletedSubscriptions.
                            TryPushBack({
                                static_cast<
                                    MediaAnimation::
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
                    MediaAnimation::EventTrigger::StaticTypeId()) {
                    continue;
                }
                auto& trigger =
                    static_cast<MediaAnimation::EventTrigger&>(*authored);
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
                        Aero::UIElement::
                            StaticTypeId())) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "EventTrigger SourceName did not resolve to a UIElement");
                }
                auto* eventElement =
                    static_cast<
                        Aero::UIElement*>(
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
                    const Aero::RoutedEventHandle eventHandle{
                        event->Id()};
                    AnimationEventState* eventContext = nullptr;
                    Base::Result<void> created =
                        AllocateObject(
                            *allocator,
                            Base::MemoryTag::Ui,
                            eventContext);
                    if (!created) return created.GetStatus();
                    eventContext->runtime = this;
                    eventContext->trigger = &trigger;
                    eventContext->owner = element;
                    eventContext->names = names;
                    auto callback =
                        [eventContext](
                            Base::Object* sender,
                            Aero::RoutedEventArgs& args) noexcept {
                            eventContext->Invoke(sender, args);
                        };
                    Aero::RoutedEventHandler handler(callback);
                    Base::Result<void> subscribed =
                        eventElement->TryAddHandler(
                            eventHandle, handler);
                    if (!subscribed) {
                        FreeObject(
                            *allocator,
                            Base::MemoryTag::Ui,
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
                        FreeObject(
                            *allocator,
                            Base::MemoryTag::Ui,
                            eventContext);
                        return retained.GetStatus();
                    }
                    continue;
                }
                for (const Base::Ref<
                         MediaAnimation::TriggerAction>& action :
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
        for (Aero::Visual* child :
             Aero::Detail::ElementPrivate::VisualChildren(*visual)) {
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
        Aero::Visual* node,
        const Aero::Visual& fragmentRoot) const noexcept {
        while (node != nullptr) {
            if (node == &fragmentRoot) return true;
            node = node->GetLogicalParent() != nullptr
                ? node->GetLogicalParent()
                : node->GetVisualParent();
        }
        return false;
    }

    void ClearDataTemplateTriggerProviders(
        Aero::Detail::DataTemplateTriggerState& context) noexcept {
        if (values != nullptr) {
            for (Aero::Detail::DataTemplatePropertyTrigger& trigger :
                 context.triggers) {
                for (Aero::Detail::DataTemplateTriggerSetter& setter :
                     trigger.setters) {
                    if (!setter.target || !setter.token.IsValid()) continue;
                    static_cast<void>(
                        values->ClearProviderContribution(
                            *setter.target,
                            setter.property,
                            setter.token));
                    setter.token = {};
                }
                trigger.active = false;
            }
        }
        context.providerOrigin = 0U;
    }

    void ClearDataTemplateTriggerProvidersInSubtree(
        Aero::Visual& visual) noexcept {
        Aero::FrameworkElement* element =
            visual.AsFrameworkElement();
        if (element != nullptr) {
            for (const Base::Ref<Base::Object>& authored :
                 Aero::Detail::ElementPrivate::AuthoredTriggers(*element)) {
                if (authored && authored->RuntimeType() ==
                    Aero::Detail::DataTemplateTriggerState::StaticTypeId()) {
                    ClearDataTemplateTriggerProviders(
                        static_cast<Aero::Detail::DataTemplateTriggerState&>(
                            *authored));
                }
            }
        }
        for (Aero::Visual* child : Aero::Detail::ElementPrivate::VisualChildren(visual)) {
            if (child != nullptr) {
                ClearDataTemplateTriggerProvidersInSubtree(*child);
            }
        }
    }

    void ClearAnimationSubscriptionsFor(
        Aero::Visual& fragmentRoot) noexcept {
        ClearDataTemplateTriggerProvidersInSubtree(fragmentRoot);
        for (std::uint32_t index = 0U;
             index < dataTemplateTriggerSubscriptions.Size();) {
            DataTemplateTriggerSubscription& subscription =
                dataTemplateTriggerSubscriptions[index];
            const bool sourceMatches =
                subscription.source != nullptr &&
                metadata->Types().IsDerivedFrom(
                    subscription.source->RuntimeType(),
                    Aero::Visual::StaticTypeId()) &&
                IsInVisualSubtree(
                    static_cast<Aero::Visual*>(
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
            FreeObject(
                *allocator, Base::MemoryTag::Ui,
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
            FreeObject(
                *allocator, Base::MemoryTag::Ui,
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
                for (Aero::Detail::Animation::AnimationHandle handle : session.handles) {
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
            FreeObject(
                *allocator,
                Base::MemoryTag::Ui,
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
            FreeObject(
                *allocator,
                Base::MemoryTag::Ui,
                subscription.context);
        }
        animationEventSubscriptions.Clear();
        storyboardCompletionSessions.Clear();
        storyboardCompletedSubscriptions.Clear();
        animationEventStatus = Base::Status::Ok();
    }

    void ClearElementEvents(
        Aero::Visual* node) noexcept {
        if (node == nullptr) return;
        if (metadata->Types().IsDerivedFrom(
                node->RuntimeType(),
                Controls::Control::StaticTypeId())) {
            Controls::Detail::ControlBehavior::SetVisualStateManager(*static_cast<Controls::Control*>(node), nullptr);
        }
        for (Aero::Visual* child :
             Aero::Detail::ElementPrivate::VisualChildren(*node)) {
            ClearElementEvents(child);
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
        ClearElementEvents(RootVisual());
        arena.Destroy(controlBehaviors);
    }

    void FinishDestroyInteractions() noexcept {
        while (!itemGenerators.Empty()) {
            Controls::ItemContainerGenerator*
                generator = itemGenerators.Back();
            itemGenerators.PopBack();
            if (generator != nullptr) {
                static_cast<void>(
                    generator->Detach());
                delete generator;
                generator = nullptr;
            }
        }
        if (input != nullptr) {
            input->SetRoot(nullptr);
        }
    }

    void DestroyInteractions() noexcept {
        BeginDestroyInteractions();
        FinishDestroyInteractions();
    }

    Base::Result<void> CreateInteractions() noexcept {
        Aero::Visual* rootVisual = RootVisual();
        if (rootVisual == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View root is not a registered Visual");
        }
        if (input == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "InputRouter is unavailable");
        }
        input->SetRoot(rootVisual);
        Base::Result<void> status;

        if (options.attachControlInteractions || options.attachTextEditing) {
            status = arena.Create(
                controlBehaviors,
                *allocator, *metadata, *tree, *events, *input,
                visualStates, options.clipboard,
                options.attachControlInteractions,
                options.attachTextEditing);
            if (!status) return status.GetStatus();
            status = controlBehaviors->Initialize();
            if (!status) return status.GetStatus();
        }
        status = VisitAndAttach(*rootVisual);
        if (!status) {
            return status.GetStatus();
        }
        return {};
    }

    void Shutdown() noexcept {
        BeginDestroyInteractions();
        DetachUi();
        FinishDestroyInteractions();
        static_cast<void>(UnmountAllFragments());
        DestroyUiEngines();
        if (images != nullptr) {
            images->Shutdown(GetImageResources());
        }
        if (tree != nullptr) {
            tree->SetLifecycleHandler(nullptr);
        }
        VisitTextElements(RootVisual(), nullptr);
        VisitPaths(RootVisual(), nullptr);
        if (animations != nullptr) {
            static_cast<void>(animations->RemoveAll());
        }
        storyboardSessions.Clear();
        if (HasAttachedRoot()) {
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
        }
        mounted = false;
        root.Reset();
        ClearLoadedDocument();
        if (effectLifetime) effectLifetime->Invalidate();


        arena.Destroy(input);
        arena.Destroy(events);
        if (bindings != nullptr) bindings->Shutdown();
        arena.Destroy(bindings);
        arena.Destroy(renderer);
        arena.Destroy(layout);
        arena.Destroy(tree);
        arena.Destroy(text);
        arena.Destroy(images);
        arena.Destroy(animations);
        arena.Destroy(values);
        arena.Destroy(objectFactory);
        arena.Reset();
        schema = nullptr;
        metadata = nullptr;
        if (deviceBound && device) {
            device->Unbind(this);
        }
        deviceBound = false;
        device.Reset();
        initialized = false;
    }

    Base::Result<void> Initialize(
        const Integration::ViewOptions& requested) noexcept {
        if (initialized) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "View is already initialized");
        }
        if (terminal) {
            return ViewInvalidState(
                "View cannot be restarted after shutdown or failed startup");
        }
        options = requested;

        device = options.renderDevice;
        if (!device) {
            Base::Result<Base::Ref<Integration::RenderDevice>>
                headless =
                    Integration::Detail::CreateHeadlessRenderDevice(allocator);
            if (!headless) {
                terminal = true;
                return headless.GetStatus();
            }
            device = std::move(headless).Value();
        }
        Base::Result<void> deviceBinding =
            device->Bind(this);
        if (!deviceBinding) {
            device.Reset();
            terminal = true;
            return deviceBinding.GetStatus();
        }
        deviceBound = true;
        deviceGeneration = device->Generation();

        Base::Result<Base::Ref<Markup::EffectLifetime>> lifetime =
            Base::MakeRefWithAllocator<Markup::EffectLifetime>(
                *allocator);
        Base::Result<void> status = lifetime
            ? Base::Result<void>()
            : Base::Result<void>(lifetime.GetStatus());
        if (status) effectLifetime = std::move(lifetime).Value();
        if (status) status = EnsureDefaultXamlProviders();
        if (!status || schemaBundle == nullptr ||
            !schemaBundle->IsFrozen()) {
            terminal = true;
            return status
                ? ViewInvalidState(
                      "GUI schema is not initialized")
                : status.GetStatus();
        }
        metadata = &schemaBundle->Metadata();
        schema = &schemaBundle->Schema();

        if (status) {
            status = arena.Initialize(
                ViewArenaCapacity,
                ViewArenaAlignment);
        }
        if (status) {
            status = arena.Create(
                objectFactory, dispatcher,
                Core::Detail::MetadataPrivate::
                    DependencyProperties(*metadata),
                *metadata);
        }
        if (status) {
            status = arena.Create(
                values, dispatcher,
                Core::Detail::MetadataPrivate::
                    DependencyProperties(*metadata));
        }
        if (status) status = values->Initialize();
        if (status) {
            status = arena.Create(
                animations, dispatcher, *values, allocator);
        }
        if (status) status = animations->Initialize();
        if (status) {
            animations->SetAutomaticTickingEnabled(
                options.automaticAnimationClock);
        }
        if (status) {
            status = arena.Create(
                tree, dispatcher, *values);
        }
        if (status) status = tree->Initialize();
        if (status) {
            status = arena.Create(
                layout, dispatcher);
        }
        if (status) status = layout->Initialize();
        if (status) {
            status = arena.Create(
                renderer, dispatcher);
        }
        if (status) status = renderer->Initialize();
        if (status) {
            status = arena.Create(
                images, allocator);
        }
        if (status) {
            status = arena.Create(
                text, allocator);
        }
        if (status) {
            status = text->Initialize(
                *device, options.text);
        }
        if (status) {
            tree->SetLifecycleHandler(
                &TextLifecycleHook, this);
        }
        if (status) {
            status = arena.Create(
                bindings, dispatcher);
        }
        if (status) status = bindings->Initialize();
        if (status) {
            status = arena.Create(
                events,
                Core::Detail::MetadataPrivate::
                    RoutedEventState(*metadata));
        }
        if (status) {
            status = arena.Create(
                input, *tree, *events);
        }
        if (status) status = CreateUiEngines();
        if (status) {
            status = RebuildDynamicResourceEnvironment();
        }
        if (status) {
            tree->AttachPresentation(layout, renderer);
        }
        if (!status) {
            Shutdown();
            terminal = true;
            return status.GetStatus();
        }
        initialized = true;
        return {};
    }

    Base::Result<void> CommitResourceLayer(
        UiDocument document,
        Aero::ResourceDictionary& target,
        bool merge) noexcept {
        const Base::Ref<Base::Object>& rootObject =
            document.Root();
        if (!rootObject ||
            rootObject->RuntimeType() !=
                Aero::ResourceDictionary::StaticTypeId()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View resource document root must be ResourceDictionary");
        }
        auto& dictionary =
            static_cast<Aero::ResourceDictionary&>(
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
                            "View resource merge rollback lost its dictionary")
                      : removed.GetStatus());
            return restored
                ? Base::Result<void>(rebuilt.GetStatus())
                : restored;
        }

        Aero::ResourceDictionary previous =
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
        Aero::ResourceDictionary& target,
        Core::IDiagnosticSink* diagnostics,
        bool merge = false) noexcept {
        if (!initialized) {
            return RuntimeNotInitialized(
                "View must be initialized before loading resources");
        }
        if (mounted || root || loadedDocument.root) {
            return ViewInvalidState(
                "View resource layers must be loaded before a document");
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
        Aero::ResourceDictionary& target,
        bool merge = false) noexcept {
        if (!initialized) {
            return RuntimeNotInitialized(
                "View must be initialized before loading resources");
        }
        if (mounted || root || loadedDocument.root) {
            return ViewInvalidState(
                "View resource layers must be loaded before a document");
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
                "View root must not be null");
        }
        if (!metadata->Types().IsDerivedFrom(
                requestedRoot->RuntimeType(),
                Aero::Visual::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View root must derive from Visual");
        }
        Base::Result<Aero::UIElement*> rootLayout =
            ResolveUIElement(*requestedRoot, requestedRoot->RuntimeType());
        return rootLayout
            ? Base::Result<void>()
            : Base::Result<void>(rootLayout.GetStatus());
    }

    Base::Result<void> MountRoot(
        Base::Ref<Base::Object> requestedRoot,
        Aero::Size availableSize) noexcept {
        if (!initialized) {
            return RuntimeNotInitialized(
                "View must be initialized before mounting");
        }
        if (mounted || root) {
            return ViewInvalidState(
                "View already has a mounted root");
        }
        Base::Result<void> validRoot = ValidateDocumentRoot(requestedRoot);
        if (!validRoot) return validRoot.GetStatus();
        if (loadedDocument.root &&
            loadedDocument.root.Get() != requestedRoot.Get()) {
            return ViewInvalidState(
                "Mounted root does not match the staged XAML document");
        }
        Base::Result<Aero::Visual*> rootVisual =
            ResolveVisual(*requestedRoot, requestedRoot->RuntimeType());
        if (!rootVisual) return rootVisual.GetStatus();
        Base::Result<Aero::UIElement*> rootLayout =
            ResolveUIElement(*requestedRoot, requestedRoot->RuntimeType());
        if (!rootLayout) return rootLayout.GetStatus();
        Base::Result<void> rootTracked =
            loadedDocument.visualContent.TryAddNode(*rootVisual.Value());
        if (!rootTracked) return rootTracked.GetStatus();
        Base::Result<void> mountedResult = AttachVisualGraph(
            *rootVisual.Value(),
            *rootLayout.Value(),
            ResolveFrameworkElement(*requestedRoot, requestedRoot->RuntimeType()),
            {loadedDocument.visualContent.mountEdges.Data(),
             loadedDocument.visualContent.mountEdges.Size()},
            availableSize);
        if (!mountedResult) return mountedResult.GetStatus();
        root = std::move(requestedRoot);
        mounted = true;
        Base::Result<void> uiApplied =
            ApplyUi(*rootVisual.Value());
        if (!uiApplied) {
            DetachUi();
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return uiApplied.GetStatus();
        }
        Base::Result<void> interactions =
            CreateInteractions();
        if (!interactions) {
            BeginDestroyInteractions();
            DetachUi();
            FinishDestroyInteractions();
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return interactions.GetStatus();
        }
        Base::Result<void> completed =
            CompleteVisualEdges({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()});
        if (!completed) {
            BeginDestroyInteractions();
            DetachUi();
            FinishDestroyInteractions();
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return completed.GetStatus();
        }
        uiApplied =
            ApplyUi(
                *rootVisual.Value());
        if (!uiApplied) {
            BeginDestroyInteractions();
            DetachUi();
            FinishDestroyInteractions();
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return uiApplied.GetStatus();
        }
        Base::Result<void> effects = loadedDocument.effects.Commit();
        if (!effects) {
            BeginDestroyInteractions();
            DetachUi();
            FinishDestroyInteractions();
            static_cast<void>(DetachVisualGraph({
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
            DetachUi();
            FinishDestroyInteractions();
            static_cast<void>(DetachVisualGraph({
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
        Base::Result<Aero::Visual*> rootVisual =
            ResolveVisual(
                *fragment.document.root,
                fragment.document.root->RuntimeType());
        if (!rootVisual) return rootVisual.GetStatus();
        ClearAnimationSubscriptionsFor(*rootVisual.Value());

        DetachUi(
            rootVisual.Value(),
            {fragment.document.visualContent.nodes.Data(),
             fragment.document.visualContent.nodes.Size()});

        ElementTree& context = *tree;
        std::uint32_t remaining = 0U;
        for (const Aero::Detail::VisualEdge& edge :
             fragment.document.visualContent.mountEdges) {
            if (edge.state.IsAttached()) ++remaining;
        }
        while (remaining > 0U) {
            bool progressed = false;
            for (Aero::Detail::VisualEdge& edge :
                 fragment.document.visualContent.mountEdges) {
                if (!edge.state.IsAttached()) continue;
                bool hasMountedChild = false;
                for (const Aero::Detail::VisualEdge& candidate :
                     fragment.document.visualContent.mountEdges) {
                    if (candidate.state.IsAttached() &&
                        candidate.parent == edge.child) {
                        hasMountedChild = true;
                        break;
                    }
                }
                if (hasMountedChild) continue;
                Base::Result<void> detached = context.DetachElement(edge.state);
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
            Base::Result<void> detached = context.DetachElement(fragment.rootEdge);
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
        DetachUi();
        FinishDestroyInteractions();
        Base::Result<void> fragments = UnmountAllFragments();
        if (!fragments) return fragments.GetStatus();
        Base::Result<void> unmounted =
            DetachVisualGraph({
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

void ViewData::
DataTemplateTriggerHandlerState::Invoke(
    ::Aero::DependencyObject&,
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
ViewData::ExecuteAnimationAction(
    MediaAnimation::TriggerAction& action,
    Aero::FrameworkElement& owner,
    Aero::Detail::DataTemplateTriggerState*
        dataTemplateContext,
    const Aero::NameScope* names) noexcept {
    const Core::TypeId type =
        action.RuntimeType();
    if (type ==
        MediaAnimation::ChangePropertyAction::StaticTypeId()) {
        auto& change =
            static_cast<MediaAnimation::ChangePropertyAction&>(
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
                ::Aero::DependencyObject::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "ChangePropertyAction TargetName did not resolve to a DependencyObject");
        }
        auto& target =
            static_cast<::Aero::DependencyObject&>(
                *targetObject);
        Base::Result<ResolvedAnimationProperty> resolved =
            ResolveAnimationProperty(
                target, change.PropertyName());
        if (!resolved) return resolved.GetStatus();

        ::Aero::DependencyObject& propertyTarget =
            *resolved.Value().target;
        const Core::DependencyPropertyHandle propertyHandle =
            resolved.Value().property;
        const Core::DependencyProperty* property =
            Core::Detail::MetadataPrivate::
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
                metadata->TryConvertText(
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
                Media::Brush::StaticTypeId() &&
            value.Type() == Core::TypeOf<Base::Color>()) {
            Base::Result<Base::Color> color =
                Core::ValueCodec<Base::Color>::Decode(
                    value);
            if (!color) return color.GetStatus();
            Base::Result<
                Base::Ref<Media::Brush>>
                brush =
                    Media::MakeSolidColorBrush(
                        color.Value());
            if (!brush) return brush.GetStatus();
            value = Core::PropertyValue::FromObject(
                Media::Brush::StaticTypeId(),
                Base::Ref<Base::Object>(
                    std::move(brush).Value()));
        }
        return propertyTarget.SetCurrentValue(
            propertyHandle, value);
    }

    if (type == MediaAnimation::SetFocusAction::StaticTypeId()) {
        auto& setFocus = static_cast<MediaAnimation::SetFocusAction&>(action);
        if (!setFocus.Engage() || input == nullptr) return {};
        Aero::UIElement* target = owner.AsUIElement();
        if (target == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "SetFocusAction owner is not a UIElement");
        }
        Base::Result<bool> focused = input->SetFocus(target);
        return focused
            ? Base::Result<void>()
            : Base::Result<void>(focused.GetStatus());
    }

    if (type == MediaAnimation::RemoveElementAction::StaticTypeId()) {
        auto& remove = static_cast<MediaAnimation::RemoveElementAction&>(action);
        Base::Object* targetObject = static_cast<Base::Object*>(&owner);
        Base::Ref<Data::Binding> targetBinding =
            remove.TargetObject();
        if (targetBinding) {
            const Base::Ref<Data::RelativeSource> relative = targetBinding->GetRelativeSource();
            if (!relative || relative->GetMode() != Data::RelativeSourceMode::FindAncestor ||
                relative->GetAncestorType() != Base::StringView("ContextMenu") ||
                targetBinding->GetPath().GetPath() != Base::StringView("PlacementTarget")) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "RemoveElementAction TargetObject binding is not supported");
            }
            Aero::Visual* current = &owner;
            Controls::ContextMenu* contextMenu = nullptr;
            while (current != nullptr) {
                if (metadata->Types().IsDerivedFrom(
                        current->RuntimeType(),
                        Controls::ContextMenu::StaticTypeId())) {
                    contextMenu = static_cast<Controls::ContextMenu*>(
                        current);
                    break;
                }
                current = current->GetLogicalParent() != nullptr
                    ? current->GetLogicalParent()
                    : current->GetVisualParent();
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
                Aero::UIElement::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "RemoveElementAction target is not a UIElement");
        }
        auto& target = static_cast<Aero::UIElement&>(*targetObject);
        Aero::Visual* current = target.GetLogicalParent() != nullptr
            ? target.GetLogicalParent() : target.GetVisualParent();
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
                        items.GetItems().RemoveAt(index);
                    return removed
                        ? Base::Result<void>()
                        : Base::Result<void>(removed.GetStatus());
                }
            }
            current = current->GetLogicalParent() != nullptr
                ? current->GetLogicalParent() : current->GetVisualParent();
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
        MediaAnimation::BeginStoryboard::StaticTypeId()) {
        auto& begin =
            static_cast<MediaAnimation::BeginStoryboard&>(
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
                for (Aero::Detail::Animation::AnimationHandle handle :
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
            for (Aero::Detail::Animation::AnimationHandle handle :
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
                for (Aero::Detail::Animation::AnimationHandle handle :
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
            for (Aero::Detail::Animation::AnimationHandle handle :
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
                for (Aero::Detail::Animation::AnimationHandle handle :
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

    if (type == MediaAnimation::ControlStoryboardAction::StaticTypeId()) {
        auto& control = static_cast<MediaAnimation::ControlStoryboardAction&>(action);
        if (!control.StoryboardValue()) return {};
        if (control.ControlOption() == MediaAnimation::ControlStoryboardAction::Option::Play) {
            MediaAnimation::BeginStoryboard begin;
            Base::Result<void> assigned = begin.SetStoryboard(control.StoryboardValue());
            return assigned ? ExecuteAnimationAction(begin, owner) : assigned;
        }
        bool found = false;
        for (StoryboardCompletionSession& session : storyboardCompletionSessions) {
            if (session.owner != &owner || session.storyboard.Get() != control.StoryboardValue().Get()) continue;
            found = true;
            for (Aero::Detail::Animation::AnimationHandle handle : session.handles) {
                Base::Result<void> result;
                if (control.ControlOption() == MediaAnimation::ControlStoryboardAction::Option::Stop) result = animations->Stop(handle);
                else if (control.ControlOption() == MediaAnimation::ControlStoryboardAction::Option::Pause) result = animations->Pause(handle);
                else if (control.ControlOption() == MediaAnimation::ControlStoryboardAction::Option::Resume) result = animations->Resume(handle);
                else return Base::Status::Failure(Base::ErrorCode::Unsupported, "ControlStoryboardAction option is not implemented");
                if (!result) return result.GetStatus();
            }
        }
        return found ? Base::Result<void>{} : Base::Status::Failure(
            Base::ErrorCode::NotFound, "ControlStoryboardAction storyboard was not started");
    }

    if (!metadata->Types().IsDerivedFrom(
            type,
            MediaAnimation::
                ControllableStoryboardAction::
                    StaticTypeId())) {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "EventTrigger contains an unsupported action");
    }
    auto& control =
        static_cast<
            MediaAnimation::ControllableStoryboardAction&>(
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
    for (Aero::Detail::Animation::AnimationHandle handle :
         session.handles) {
        Base::Result<void> result;
        if (type ==
            MediaAnimation::PauseStoryboard::
                StaticTypeId()) {
            result = animations->Pause(handle);
        } else if (type ==
            MediaAnimation::ResumeStoryboard::
                StaticTypeId()) {
            result = animations->Resume(handle);
        } else if (type ==
            MediaAnimation::StopStoryboard::
                StaticTypeId()) {
            result = animations->Stop(handle);
        } else if (type ==
            MediaAnimation::RemoveStoryboard::
                StaticTypeId()) {
            result = animations->Remove(handle);
        } else if (type ==
            MediaAnimation::SeekStoryboard::
                StaticTypeId()) {
            result = animations->Seek(
                handle,
                static_cast<
                    MediaAnimation::SeekStoryboard&>(
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
            MediaAnimation::StopStoryboard::StaticTypeId() ||
        type ==
            MediaAnimation::RemoveStoryboard::StaticTypeId()) {
        CancelStoryboardCompletionSessions(
            session.handles.AsSpan());
    }
    if (type ==
        MediaAnimation::RemoveStoryboard::StaticTypeId()) {
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

void ViewData::
CancelStoryboardCompletionSessions(
    Base::Span<const Aero::Detail::Animation::AnimationHandle>
        handles) noexcept {
    for (std::uint32_t index = 0U;
         index < storyboardCompletionSessions.Size();) {
        bool matches = false;
        for (Aero::Detail::Animation::AnimationHandle sessionHandle :
             storyboardCompletionSessions[index].handles) {
            for (Aero::Detail::Animation::AnimationHandle handle :
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
ViewData::ProcessStoryboardCompletions() noexcept {
    std::uint32_t actionCount = 0U;
    std::uint32_t index = 0U;
    while (index < storyboardCompletionSessions.Size()) {
        StoryboardCompletionSession& session =
            storyboardCompletionSessions[index];
        bool completed = true;
        for (Aero::Detail::Animation::AnimationHandle handle :
             session.handles) {
            const Aero::Detail::Animation::AnimationState state =
                animations->State(handle);
            if (state ==
                    Aero::Detail::Animation::AnimationState::Active ||
                state ==
                    Aero::Detail::Animation::AnimationState::Paused) {
                completed = false;
                break;
            }
        }
        if (!completed) {
            ++index;
            continue;
        }

        Base::Ref<MediaAnimation::Storyboard> storyboard =
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
                     MediaAnimation::TriggerAction>& action :
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


} // namespace Aero::Detail

namespace Aero {
namespace {

Base::Status ViewInvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status ViewNotInitialized(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotInitialized, message);
}

} // namespace

View::View(
    ConstructionToken,
    GUI& gui,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()),
      gui_(gui.impl_) {
    GUI::Impl& guiState = static_cast<GUI::Impl&>(*gui.impl_);
    void* memory = allocator_->Allocate({
        sizeof(::Aero::Detail::ViewData), alignof(::Aero::Detail::ViewData),
        Base::MemoryTag::Markup});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(::Aero::Detail::ViewData), alignof(::Aero::Detail::ViewData),
            Base::MemoryTag::Markup);
    }
    state_ = new (memory) ::Aero::Detail::ViewData(
        *allocator_, guiState.schema, guiState.documents);
}

View::~View() noexcept {
    Shutdown();
    if (state_ != nullptr) {
        state_->~ViewData();
        allocator_->Deallocate(
            state_, sizeof(::Aero::Detail::ViewData),
            alignof(::Aero::Detail::ViewData),
            Base::MemoryTag::Markup);
        state_ = nullptr;
    }
    gui_.Reset();
}

Base::Result<void> View::Initialize(
    const Integration::ViewOptions& options) noexcept {
    if (state_ == nullptr || !gui_) {
        return ViewInvalidState("View has no GUI state");
    }
    const GUI::Impl& guiState = static_cast<const GUI::Impl&>(*gui_);
    if (!guiState.initialized) {
        return ViewNotInitialized(
            "GUI must be initialized before creating a View");
    }
    return state_->Initialize(options);
}

void View::Shutdown() noexcept {
    if (state_ == nullptr || state_->terminal) return;
    state_->Shutdown();
    state_->terminal = true;
}

bool View::IsInitialized() const noexcept {
    return state_ != nullptr && state_->initialized;
}

bool View::IsMounted() const noexcept {
    return state_ != nullptr && state_->mounted;
}

Base::Result<UiDocument> View::LoadDocument(
    Base::StringView uri,
    Core::IDiagnosticSink* diagnostics) noexcept {
    if (!IsInitialized()) {
        return ViewNotInitialized(
            "View must be initialized before XAML loading");
    }
    Base::Result<Markup::LoadOptions> options =
        state_->LoadOptions(true);
    if (!options) return options.GetStatus();
    Markup::Loader loader(
        *state_->schema,
        state_->xamlSources,
        diagnostics,
        allocator_);
    return loader.Load(uri, options.Value());
}

Base::Result<UiDocument> View::ParseDocument(
    Base::StringView source,
    const Base::ResourceUri& baseUri,
    Core::IDiagnosticSink* diagnostics) noexcept {
    if (!IsInitialized()) {
        return ViewNotInitialized(
            "View must be initialized before XAML parsing");
    }
    Base::Result<Markup::LoadOptions> options =
        state_->LoadOptions(true);
    if (!options) return options.GetStatus();
    Markup::Loader loader(
        *state_->schema,
        state_->xamlSources,
        diagnostics,
        allocator_);
    return loader.Parse(source, baseUri, options.Value());
}

Base::Result<UiDocument> View::LoadCompiledDocument(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri) noexcept {
    if (!IsInitialized()) {
        return ViewNotInitialized(
            "View must be initialized before compiled XAML loading");
    }
    Base::Result<Markup::LoadOptions> options =
        state_->LoadOptions(true);
    if (!options) return options.GetStatus();
    Markup::Loader loader(
        *state_->schema,
        state_->xamlSources,
        nullptr,
        allocator_);
    return loader.LoadCompiled(
        bytes, originUri, options.Value());
}

Base::Result<void> View::AddSourceProvider(
    Integration::ISourceProvider& provider,
    Base::StringView scheme,
    Base::StringView assembly) noexcept {
    if (state_ == nullptr || state_->terminal) {
        return ViewInvalidState(
            "View cannot register a XAML source provider");
    }
    return state_->xamlSources.TryRegister(
        provider, scheme, assembly);
}

Base::Result<void> View::LoadResources(
    ResourceLayer layer,
    Base::StringView uri,
    ResourceLoadMode mode,
    Core::IDiagnosticSink* diagnostics) noexcept {
    Base::Result<Aero::ResourceDictionary*> target =
        state_->ResolveResourceLayer(layer);
    if (!target) return target.GetStatus();
    return state_->LoadResourceLayer(
        uri,
        *target.Value(),
        diagnostics,
        mode == ResourceLoadMode::Merge);
}

Base::Result<void>
View::LoadCompiledResources(
    ResourceLayer layer,
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    ResourceLoadMode mode) noexcept {
    Base::Result<Aero::ResourceDictionary*> target =
        state_->ResolveResourceLayer(layer);
    if (!target) return target.GetStatus();
    return state_->LoadCompiledResourceLayer(
        bytes,
        originUri,
        *target.Value(),
        mode == ResourceLoadMode::Merge);
}

Base::Result<void> View::SetResourceDictionary(
    ResourceLayer layer,
    Aero::ResourceDictionary& dictionary,
    ResourceLoadMode mode) noexcept {
    if (state_ == nullptr || !state_->initialized) {
        return ViewNotInitialized(
            "View must be initialized before setting resources");
    }
    if (state_->mounted || state_->root ||
        state_->loadedDocument.root) {
        return ViewInvalidState(
            "View resource layers must be set before a document is mounted");
    }
    Base::Result<Aero::ResourceDictionary*> target =
        state_->ResolveResourceLayer(layer);
    if (!target) return target.GetStatus();
    Base::Result<Aero::ResourceDictionary> shared =
        dictionary.Share();
    if (!shared) return shared.GetStatus();
    if (mode == ResourceLoadMode::Merge) {
        Base::Result<void> merged =
            target.Value()->TryAddMerged(shared.Value());
        if (!merged) return merged.GetStatus();
        return state_->RebuildDynamicResourceEnvironment();
    }

    Aero::ResourceDictionary previous =
        std::move(*target.Value());
    *target.Value() = std::move(shared).Value();
    Base::Result<void> rebuilt =
        state_->RebuildDynamicResourceEnvironment();
    if (rebuilt) return {};
    *target.Value() = std::move(previous);
    Base::Result<void> restored =
        state_->RebuildDynamicResourceEnvironment();
    return restored
        ? Base::Result<void>(rebuilt.GetStatus())
        : restored;
}

Base::Result<void> View::LoadBuiltInTheme(
    BuiltInTheme theme) noexcept {
    if (state_ == nullptr) {
        return ViewInvalidState(
            "View has no implementation");
    }
    if (theme != BuiltInTheme::Light &&
        theme != BuiltInTheme::Dark) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Built-in theme value is invalid");
    }
    const std::uint8_t* paletteBytes =
        theme == BuiltInTheme::Light
        ? Aero::Detail::AeroThemeLightCompiled
        : Aero::Detail::AeroThemeDarkCompiled;
    const std::uint32_t paletteSize =
        theme == BuiltInTheme::Light
        ? Aero::Detail::AeroThemeLightCompiledSize
        : Aero::Detail::AeroThemeDarkCompiledSize;
    Base::Result<Base::ResourceUri> paletteUri =
        ::Aero::Detail::BuiltInThemeUri(
            theme == BuiltInTheme::Light
            ? Base::StringView("Light.xaml")
            : Base::StringView("Dark.xaml"));
    if (!paletteUri) return paletteUri.GetStatus();
    Base::Result<Base::ResourceUri> genericUri =
        ::Aero::Detail::BuiltInThemeUri(
            Base::StringView("Generic.xaml"));
    if (!genericUri) return genericUri.GetStatus();

    Aero::ResourceDictionary previous =
        std::move(state_->themeResources);
    Base::Result<void> loaded = paletteSize != 0U
        ? LoadCompiledResources(
              ResourceLayer::Theme,
              {paletteBytes, paletteSize},
              paletteUri.Value())
        : LoadResources(
              ResourceLayer::Theme,
              paletteUri.Value().Canonical());
    if (loaded) {
        loaded = Aero::Detail::AeroThemeGenericCompiledSize != 0U
            ? LoadCompiledResources(
                  ResourceLayer::Theme,
                  {Aero::Detail::AeroThemeGenericCompiled,
                   Aero::Detail::AeroThemeGenericCompiledSize},
                  genericUri.Value(),
                  ResourceLoadMode::Merge)
            : LoadResources(
                  ResourceLayer::Theme,
                  genericUri.Value().Canonical(),
                  ResourceLoadMode::Merge);
    }
    if (!loaded) {
        state_->themeResources =
            std::move(previous);
        Base::Result<void> restored =
            state_->RebuildDynamicResourceEnvironment();
        return restored
            ? Base::Result<void>(loaded.GetStatus())
            : Base::Result<void>(restored.GetStatus());
    }
    return {};
}

Base::Result<void> View::SetContent(
    UiDocument&& document,
    Aero::Size availableSize) noexcept {
    return IsMounted()
        ? ReplaceMountedDocument(std::move(document), availableSize)
        : Mount(std::move(document), availableSize);
}

Base::Result<void> View::SetContent(
    Base::Ref<FrameworkElement> root,
    Aero::Size availableSize) noexcept {
    if (!IsInitialized()) {
        return ViewNotInitialized("View is not initialized");
    }
    if (IsMounted()) {
        return ViewInvalidState(
            "A programmatic View root cannot replace mounted content");
    }
    return Mount(
        Base::Ref<Base::Object>(std::move(root)),
        availableSize);
}

Base::Result<void> View::Mount(
    Aero::Size availableSize) noexcept {
    if (!state_->loadedDocument.root) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "View has no staged XAML root");
    }
    return state_->MountRoot(
        state_->loadedDocument.root, availableSize);
}

Base::Result<void> View::Mount(
    Base::Ref<Base::Object> root,
    Aero::Size availableSize) noexcept {
    return state_->MountRoot(
        std::move(root), availableSize);
}

Base::Result<void> View::Mount(
    UiDocument&& document,
    Aero::Size availableSize) noexcept {
    Base::Result<void> ready = state_->BeginDocumentLoad();
    if (!ready) return ready.GetStatus();
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View cannot mount an empty UI document");
    }
    if (Aero::Detail::XamlDocumentPrivate::RuntimeLifetime(document) !=
        state_->effectLifetime.Get()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "UI document belongs to another View");
    }
    Base::Result<void> valid = state_->ValidateDocumentRoot(document.Root());
    if (!valid) return valid.GetStatus();
    state_->loadedDocument =
        Aero::Detail::XamlDocumentPrivate::Take(document);
    return state_->MountRoot(
        state_->loadedDocument.root, availableSize);
}

Base::Result<void> View::ReplaceMountedDocument(
    UiDocument&& document,
    Aero::Size availableSize) noexcept {
    if (state_ == nullptr || !state_->initialized || !state_->mounted) {
        return ViewInvalidState(
            "View document replacement requires a mounted view");
    }
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View cannot replace a document with an empty document");
    }
    if (Aero::Detail::XamlDocumentPrivate::RuntimeLifetime(document) !=
        state_->effectLifetime.Get()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Replacement UI document belongs to another View");
    }
    Base::Result<void> valid = state_->ValidateDocumentRoot(document.Root());
    if (!valid) return valid.GetStatus();

    Markup::LoaderResult next =
        Aero::Detail::XamlDocumentPrivate::Take(document);
    if (!next.root ||
        !state_->metadata->Types().IsDerivedFrom(
            next.root->RuntimeType(),
            Aero::Visual::StaticTypeId())) {
        next.Clear();
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Replacement UI document root must derive from Visual");
    }

    Base::Result<void> detached =
        state_->DetachMountedRoot(false);
    if (!detached) {
        Base::Result<void> restored = state_->MountRoot(
            state_->loadedDocument.root, availableSize);
        next.Clear();
        return restored ? detached : restored;
    }

    Markup::LoaderResult previous =
        std::move(state_->loadedDocument);
    state_->loadedDocument = std::move(next);
    Base::Result<void> mounted = state_->MountRoot(
        state_->loadedDocument.root, availableSize);
    if (mounted) {
        previous.Clear();
        return {};
    }

    state_->loadedDocument = std::move(previous);
    Base::Result<void> restored = state_->MountRoot(
        state_->loadedDocument.root, availableSize);
    return restored ? mounted : restored;
}

Base::Result<void> View::MountContent(
    Controls::ContentControl& host,
    UiDocument&& document) noexcept {
    if (state_ == nullptr || !state_->initialized || !state_->mounted ||
        state_->tree == nullptr || state_->layout == nullptr) {
        return ViewInvalidState(
            "content fragment mounting requires a mounted View");
    }
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "content fragment document must not be empty");
    }
    if (Aero::Detail::XamlDocumentPrivate::RuntimeLifetime(document) !=
        state_->effectLifetime.Get()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "content fragment document belongs to another View");
    }
    if (Aero::Detail::ElementPrivate::Tree(host) != state_->tree) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "content fragment host does not belong to this View");
    }

    std::uint32_t existing = UINT32_MAX;
    for (std::uint32_t index = 0U;
         index < state_->fragmentMounts.Size(); ++index) {
        if (state_->fragmentMounts[index].host == &host) {
            existing = index;
            break;
        }
    }
    if (existing != UINT32_MAX) {
        Base::Result<void> unmounted = state_->UnmountFragmentAt(existing);
        if (!unmounted) return unmounted.GetStatus();
    } else if (Controls::Detail::ControlPrivate::ContentElement(host) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
              "content fragment host already owns non-fragment content");
    }
    Base::Result<void> capacity = state_->fragmentMounts.TryReserve(
        state_->fragmentMounts.Size() + 1U);
    if (!capacity) return capacity.GetStatus();

    ::Aero::Detail::ViewData::FragmentMount fragment;
    fragment.host = &host;
    fragment.document = Aero::Detail::XamlDocumentPrivate::Take(document);
    Base::Result<Aero::Visual*> rootVisual =
        state_->ResolveVisual(
            *fragment.document.root,
            fragment.document.root->RuntimeType());
    Base::Result<Aero::UIElement*> rootElement =
        state_->ResolveUIElement(
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
    Base::Result<void> assigned = Controls::Detail::ControlPrivate::SetOwnedContent(host,
        fragment.document.root, *rootElement.Value());
    if (!assigned) {
        fragment.document.Clear();
        return assigned.GetStatus();
    }

    ElementTree& context = *state_->tree;
    Base::Result<Aero::Detail::ElementAttachment> rootMounted =
        context.AttachElement(host, *rootVisual.Value());
    if (!rootMounted) {
        static_cast<void>(host.SetContent(nullptr));
        fragment.document.Clear();
        return rootMounted.GetStatus();
    }
    fragment.rootEdge = std::move(rootMounted).Value();

    const auto detachFailedFragment = [&]() noexcept {
        static_cast<void>(state_->DetachFragment(fragment));
    };
    const auto attachEdges = [&](bool deferred) noexcept
        -> Base::Result<void> {
        std::uint32_t attached = 0U;
        for (const Aero::Detail::VisualEdge& edge :
             fragment.document.visualContent.mountEdges) {
            if (edge.state.logicalAttached) ++attached;
        }
        while (attached < fragment.document.visualContent.mountEdges.Size()) {
            bool progressed = false;
            for (Aero::Detail::VisualEdge& edge :
                 fragment.document.visualContent.mountEdges) {
                if (edge.state.logicalAttached || edge.parent == nullptr ||
                    edge.child == nullptr ||
                    Aero::Detail::ElementPrivate::Tree(*edge.parent) != state_->tree ||
                    (deferred && Aero::Detail::ElementPrivate::Tree(*edge.child) == state_->tree)) {
                    continue;
                }
                Base::Result<Aero::Detail::ElementAttachment> mounted =
                    context.AttachElement(*edge.parent, *edge.child);
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
        state_->ApplyUi(*rootVisual.Value());
    if (!applied) {
        detachFailedFragment();
        return applied.GetStatus();
    }
    attached = attachEdges(true);
    if (!attached) {
        detachFailedFragment();
        return attached.GetStatus();
    }
    applied = state_->ApplyUi(*rootVisual.Value());
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
        state_->StartLoadedAnimations(
            rootVisual.Value(), &fragment.document.names);
    if (!animations) {
        detachFailedFragment();
        return animations.GetStatus();
    }
    Base::Result<void> retained =
        state_->fragmentMounts.TryPushBack(std::move(fragment));
    if (!retained) {
        return retained.GetStatus();
    }
    return {};
}

Base::Result<void> View::UnmountContent(
    Controls::ContentControl& host) noexcept {
    if (state_ == nullptr || !state_->initialized || !state_->mounted) {
        return ViewInvalidState(
            "content fragment unmounting requires a mounted View");
    }
    for (std::uint32_t index = 0U;
         index < state_->fragmentMounts.Size(); ++index) {
        if (state_->fragmentMounts[index].host == &host) {
            return state_->UnmountFragmentAt(index);
        }
    }
    return Controls::Detail::ControlPrivate::ContentElement(host) == nullptr
        ? Base::Result<void>()
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::InvalidState,
              "content host does not contain a mounted XAML fragment"));
}

Base::Result<void> View::Resize(
    Aero::Size availableSize) noexcept {
    if (!IsMounted() || state_ == nullptr ||
        !state_->HasAttachedRoot()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "View resize requires a mounted visual tree");
    }
    return state_->ResizeVisualRoot(availableSize);
}

Base::Result<void> View::Unmount() noexcept {
    return state_->UnmountRoot();
}

Base::Result<ViewFrameResult> View::Update(
    std::uint32_t elapsedMilliseconds) noexcept {
    std::uint32_t timedCallbacks = 0U;
    if (elapsedMilliseconds != 0U) {
        Base::Result<std::uint32_t> advanced =
            AdvanceClocks(elapsedMilliseconds);
        if (!advanced) return advanced.GetStatus();
        timedCallbacks = advanced.Value();
    }
    Base::Result<ViewFrameResult> frame = ExecuteFrame();
    if (!frame) return frame.GetStatus();
    if (frame.Value().callbackCount > UINT32_MAX - timedCallbacks) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "View callback count overflow");
    }
    frame.Value().callbackCount += timedCallbacks;
    return std::move(frame).Value();
}

Base::Result<ViewFrameResult>
View::ExecuteFrame() noexcept {
    if (!IsInitialized()) {
        return ViewNotInitialized(
            "View must be initialized before running frames");
    }
    if (!state_->animationEventStatus.IsOk()) {
        return state_->animationEventStatus;
    }
    if (state_->styles != nullptr &&
        !state_->styles->LastActionStatus().IsOk()) {
        return state_->styles->LastActionStatus();
    }
    bool deviceGenerationChanged = false;
    if (state_->device) {
        const Base::Status deviceStatus =
            state_->device->GetFrameStatus();
        if (!deviceStatus.IsOk()) {
            return deviceStatus;
        }
        const std::uint64_t generation =
            state_->device->Generation();
        if (generation !=
            state_->deviceGeneration) {
            deviceGenerationChanged = true;
            Aero::Visual* rootVisual =
                state_->RootVisual();
            Aero::FrameworkElement* root =
                rootVisual != nullptr
                ? rootVisual->AsFrameworkElement()
                : nullptr;
            if (root != nullptr) {
                Base::Result<void> invalidated =
                    state_->renderer->Invalidate(*root);
                if (!invalidated) {
                    return invalidated.GetStatus();
                }
            }
            state_->VisitPaths(
                rootVisual,
                state_->GetMeshResources(),
                true);
            state_->deviceGeneration = generation;
        }
    }
    if (state_->text != nullptr) {
        Base::Result<bool> synchronized =
            state_->text->SynchronizeBackend(
                *state_->device, deviceGenerationChanged);
        if (!synchronized) {
            return synchronized.GetStatus();
        }
        if (synchronized.Value()) {
            state_->VisitTextElements(
                state_->RootVisual(),
                state_->text->Layout(),
                true);
        }
    }
    if (state_->images != nullptr) {
        Base::Result<bool> synchronized =
            state_->images->Synchronize(
                state_->RootVisual(),
                state_->loadedDocument.canonicalUri,
                state_->xamlSources,
                state_->GetImageResources(),
                deviceGenerationChanged);
        if (!synchronized) {
            return synchronized.GetStatus();
        }
        if (synchronized.Value()) {
            Aero::Visual* rootVisual =
                state_->RootVisual();
            Aero::FrameworkElement* root =
                rootVisual != nullptr
                ? rootVisual->AsFrameworkElement()
                : nullptr;
            if (root != nullptr) {
                Base::Result<void> invalidated =
                    state_->renderer->Invalidate(*root);
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
    ViewFrameResult result;
    for (Core::DispatcherFramePhase phase : phases) {
        if (phase ==
                Core::DispatcherFramePhase::Layout &&
            state_->HasAttachedRoot()) {
            Base::Result<void> completed =
                state_->CompleteVisualEdges({
                    state_->loadedDocument.visualContent.mountEdges.Data(),
                    state_->loadedDocument.visualContent.mountEdges.Size()});
            if (!completed) return completed.GetStatus();
        }
        if (phase ==
            Core::DispatcherFramePhase::
                RenderCommit) {
            Base::Result<void> overlays =
                state_->SynchronizeOverlays();
            if (!overlays) {
                return overlays.GetStatus();
            }
        }
        const std::uint64_t renderVersionBefore =
            phase == Core::DispatcherFramePhase::RenderCommit
                ? state_->renderer->CurrentFrame().Version() : 0U;
        Base::Result<std::uint32_t> ran =
            state_->dispatcher.RunFramePhase(phase);
        if (!ran) return ran.GetStatus();
        if (phase ==
            Core::DispatcherFramePhase::DataBind) {
            Base::Result<void> generatedVisualsFlushed =
                state_->FlushGeneratedVisuals();
            if (!generatedVisualsFlushed) {
                return generatedVisualsFlushed.GetStatus();
            }
        }
        if (phase == Core::DispatcherFramePhase::Layout &&
            !state_->layout->LastFlushStatus().IsOk()) {
            return state_->layout->LastFlushStatus();
        }
        if (phase == Core::DispatcherFramePhase::Animation &&
            state_->animations != nullptr) {
            const Base::Status animationStatus =
                state_->animations->LastTickStatus();
            if (!animationStatus.IsOk()) {
                return animationStatus;
            }
            Base::Result<std::uint32_t> completed =
                state_->ProcessStoryboardCompletions();
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
            state_->animations != nullptr) {
            Base::Result<std::uint32_t> initialValues =
                state_->animations->ApplyPendingInitialValues();
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
                state_->renderer->LastCommitStatus();
            if (!committed.IsOk()) return committed;
            const Render::RenderFrame& frame = state_->renderer->CurrentFrame();
            if (state_->device && frame.Version() != renderVersionBefore) {
                Base::Result<void> submitted =
                    state_->device->Submit(frame);
                if (!submitted) return submitted.GetStatus();
            }
            if (state_->animations != nullptr) {
                state_->animations->CommitPendingInitialValues();
            }
        }
    }
    if (state_->text != nullptr) {
        Base::Result<std::uint32_t> collected =
            state_->text->CollectGarbage();
        if (!collected) return collected.GetStatus();
    }
    result.frameNumber = ++state_->frameNumber;
    const Aero::LayoutDiagnostics layout =
        state_->layout->Diagnostics();
    result.layout.passVersion = layout.passVersion;
    result.layout.measuredCount = layout.measuredCount;
    result.layout.arrangedCount = layout.arrangedCount;
    result.layout.pendingMeasureCount =
        layout.pendingMeasureCount;
    result.layout.pendingArrangeCount =
        layout.pendingArrangeCount;
    const Render::RenderDiagnostics render =
        state_->renderer->Diagnostics();
    result.render.snapshotVersion = render.commitVersion;
    result.render.nodeCount = render.nodeCount;
    result.render.commandCount = render.commandCount;
    result.render.glyphCommandCount =
        render.glyphCommandCount;
    result.render.dirtyCount = render.dirtyCount;
    result.render.snapshotHash = render.frameHash;
    if (state_->device) {
        const Integration::RenderFrameStatistics
            deviceStatistics =
                state_->device->
                    LastFrameStatistics();
        result.render.drawPacketCount =
            deviceStatistics.drawPacketCount;
        result.render.batchCount =
            deviceStatistics.batchCount;
        result.render.drawCallCount =
            deviceStatistics.drawCallCount;
        result.render.mergedPacketCount =
            deviceStatistics.mergedPacketCount;
        result.render.barrierCount =
            deviceStatistics.barrierCount;
        result.render.instanceCount =
            deviceStatistics.instanceCount;
        result.render.stateBindingCount =
            deviceStatistics.stateBindingCount;
        result.render.batchingEnabled =
            deviceStatistics.batchingEnabled;
    }
    return result;
}

Base::Result<Input::PointerDispatchResult>
View::DispatchPointer(
    const Input::PointerInput& input) noexcept {
    if (!IsMounted() || state_->input == nullptr) {
        return ViewNotInitialized(
            "Pointer input requires a mounted View");
    }
    Base::Result<
        Input::PointerDispatchResult>
        dispatched =
            state_->input->DispatchPointer(input);
    if (!dispatched) {
        return dispatched.GetStatus();
    }
    Aero::UIElement* target =
        dispatched.Value().hit.target;
    Base::Result<void> dismissed =
        state_->DismissOverlaysForPointer(
            input, target);
    if (!dismissed) {
        return dismissed.GetStatus();
    }
    Base::Result<void> toolTip =
        state_->UpdateToolTipForPointer(
            input, target);
    if (!toolTip) {
        return toolTip.GetStatus();
    }
    Base::Result<void> contextMenu =
        state_->OpenContextMenuForPointer(
            input, target);
    if (!contextMenu) {
        return contextMenu.GetStatus();
    }
    return dispatched;
}

Base::Result<Input::KeyboardDispatchResult>
View::DispatchKeyboard(
    const Input::KeyboardInput& input) noexcept {
    if (!IsMounted() || state_->input == nullptr) {
        return ViewNotInitialized(
            "Keyboard input requires a mounted View");
    }
    if (input.action ==
            Input::KeyboardAction::Down &&
        input.key ==
            Input::KeyboardKeyEscape) {
        Base::Result<bool> dismissed =
            state_->DismissTopOverlayForEscape();
        if (!dismissed) {
            return dismissed.GetStatus();
        }
        if (dismissed.Value()) {
            Input::KeyboardDispatchResult
                result;
            result.routed = true;
            return result;
        }
    }
    return state_->input->DispatchKeyboard(input);
}

Base::Result<Input::TextInputDispatchResult>
View::DispatchText(
    const Input::TextInput& input) noexcept {
    if (!IsMounted() || state_->input == nullptr) {
        return ViewNotInitialized(
            "Text input requires a mounted View");
    }
    return state_->input->DispatchText(input);
}

Base::Result<std::uint32_t>
View::AdvanceClocks(
    std::uint32_t elapsedMilliseconds) noexcept {
    if (!IsMounted() || state_->animations == nullptr) {
        return ViewNotInitialized(
            "View timing requires a mounted animation manager");
    }
    std::uint32_t actionCount = 0U;
    if (state_->controlBehaviors != nullptr) {
        Base::Result<std::uint32_t> controls =
            state_->controlBehaviors->AdvanceTime(
                elapsedMilliseconds);
        if (!controls) return controls.GetStatus();
        actionCount = controls.Value();
    }
    Base::Result<std::uint32_t> toolTips =
        state_->AdvanceToolTipTime(
            elapsedMilliseconds);
    if (!toolTips) return toolTips.GetStatus();
    if (actionCount > UINT32_MAX - toolTips.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Control timing action count overflow");
    }
    actionCount += toolTips.Value();
    if (state_->options.automaticAnimationClock) {
        return actionCount;
    }
    Base::Result<std::uint32_t> animations =
        state_->animations->AdvanceBy(
            static_cast<Aero::Detail::Animation::AnimationTime>(
                elapsedMilliseconds) * 1000U);
    if (!animations) return animations.GetStatus();
    Base::Result<std::uint32_t> completed =
        state_->ProcessStoryboardCompletions();
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
View::AdvanceAnimations(
    std::uint32_t elapsedMilliseconds) noexcept {
    if (!IsMounted() || state_->animations == nullptr) {
        return ViewNotInitialized(
            "Animation timing requires a mounted View");
    }
    Base::Result<std::uint32_t> advanced =
        state_->animations->AdvanceBy(
        static_cast<Aero::Detail::Animation::AnimationTime>(
            elapsedMilliseconds) * 1000U);
    if (!advanced) return advanced.GetStatus();
    Base::Result<std::uint32_t> completed =
        state_->ProcessStoryboardCompletions();
    if (!completed) return completed.GetStatus();
    if (advanced.Value() >
        UINT32_MAX - completed.Value()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Animation action count overflow");
    }
    return advanced.Value() + completed.Value();
}

Base::Result<void> View::SetRenderDevice(
    Base::Ref<Integration::RenderDevice> device,
    bool automaticAnimationClock) noexcept {
    if (!IsInitialized() || state_ == nullptr) {
        return ViewNotInitialized(
            "View render-device replacement requires an initialized View");
    }
    if (!device) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View requires a render device");
    }
    if (device.Get() == state_->device.Get()) {
        state_->options.automaticAnimationClock =
            automaticAnimationClock;
        if (state_->animations != nullptr) {
            state_->animations->SetAutomaticTickingEnabled(
                automaticAnimationClock);
        }
        return {};
    }

    Base::Result<void> bound =
        device->Bind(this);
    if (!bound) return bound.GetStatus();

    Base::Ref<Integration::RenderDevice> previous =
        state_->device;
    if (previous) {
        Base::Result<void> idle =
            previous->WaitIdle();
        if (!idle) {
            device->Unbind(this);
            return idle.GetStatus();
        }
    }

    Aero::Detail::ImageResources*
        previousImages = state_->GetImageResources();
    if (state_->images != nullptr) {
        state_->images->ReleaseBackendResources(
            previousImages);
    }
    state_->VisitTextElements(
        state_->RootVisual(), nullptr);
    state_->VisitPaths(
        state_->RootVisual(), nullptr);

    state_->device = device;
    state_->deviceBound = true;
    state_->deviceGeneration =
        device->Generation();
    state_->options.renderDevice = device;
    state_->options.automaticAnimationClock =
        automaticAnimationClock;
    if (state_->animations != nullptr) {
        state_->animations->SetAutomaticTickingEnabled(
            automaticAnimationClock);
    }

    Base::Result<void> status;
    if (state_->text != nullptr) {
        Base::Result<bool> synchronized =
            state_->text->SynchronizeBackend(*state_->device, true);
        if (!synchronized) {
            status = synchronized.GetStatus();
        } else {
            state_->VisitTextElements(
                state_->RootVisual(),
                state_->text->Layout(),
                true);
        }
    }
    if (status && state_->images != nullptr) {
        Base::Result<bool> synchronized =
            state_->images->Synchronize(
                state_->RootVisual(),
                state_->loadedDocument.canonicalUri,
                state_->xamlSources,
                state_->GetImageResources(),
                true);
        if (!synchronized) {
            status = synchronized.GetStatus();
        }
    }
    if (status) {
        state_->VisitPaths(
            state_->RootVisual(),
            state_->GetMeshResources(),
            true);
        Aero::Visual* rootVisual =
            state_->RootVisual();
        Aero::FrameworkElement* root =
            rootVisual != nullptr
            ? rootVisual->AsFrameworkElement()
            : nullptr;
        if (root != nullptr) {
            status = state_->renderer->Invalidate(*root);
        }
    }

    if (previous) {
        previous->Unbind(this);
    }
    return status;
}

const Base::Ref<Base::Object>&
View::Root() const noexcept {
    static const Base::Ref<Base::Object> empty;
    return state_ != nullptr ? state_->root : empty;
}

Base::Object* View::FindNamedObject(
    Base::StringView name,
    Core::TypeId expectedType) noexcept {
    if (state_ == nullptr || name.Empty()) {
        return nullptr;
    }
    Base::Object* object = state_->loadedDocument.names.Find(name);
    if (object == nullptr || expectedType == Core::InvalidTypeId) {
        return object;
    }
    return state_->metadata->Types().IsAssignableFrom(
        expectedType, object->RuntimeType()) ? object : nullptr;
}

std::uint32_t View::NamedObjectCount() const noexcept {
    return state_ != nullptr ? state_->loadedDocument.names.Size() : 0U;
}

bool View::IsInstanceOf(
    const Base::Object& object,
    Core::TypeId baseType) const noexcept {
    return state_ != nullptr && state_->metadata != nullptr &&
        state_->metadata->Types().IsDerivedFrom(
            object.RuntimeType(), baseType);
}

Base::Result<void> View::QueryReloadSource(
    const Base::ResourceUri& uri,
    std::uint64_t& sourceIdentity,
    std::uint64_t& revision) noexcept {
    if (state_ == nullptr || uri.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML reload source is unavailable");
    }
    Base::Result<Markup::SourceProviderResolution> resolved =
        state_->xamlSources.ResolveDetailed(uri);
    if (!resolved) return resolved.GetStatus();
    sourceIdentity = resolved.Value().cacheIdentity;
    Base::Result<std::uint64_t> probed =
        resolved.Value().provider->Revision(uri);
    if (probed && probed.Value() != 0U) {
        revision = probed.Value();
        return {};
    }
    Base::Result<Markup::Source> source =
        resolved.Value().provider->Load(uri);
    if (!source) return source.GetStatus();
    revision = source.Value().revision != 0U
        ? source.Value().revision
        : Base::HashBytes(
              source.Value().bytes.Data(),
              source.Value().bytes.Size());
    return {};
}

bool View::TryGetCachedReloadRevision(
    const Base::ResourceUri& uri,
    std::uint64_t sourceIdentity,
    std::uint64_t& revision) noexcept {
    return state_ != nullptr && state_->documentCache != nullptr &&
        state_->documentCache->TryGetSourceRevision(
            uri, sourceIdentity, revision);
}

Base::Result<std::uint32_t> View::InvalidateReloadDocuments(
    const Base::ResourceUri& uri,
    bool includeDependents) noexcept {
    if (state_ == nullptr || state_->documentCache == nullptr) {
        return std::uint32_t{0U};
    }
    return state_->documentCache->Invalidate(uri, includeDependents);
}

} // namespace Aero
