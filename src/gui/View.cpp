#include <Aero/View.hpp>
#include <Aero/Gui.hpp>
#include <AeroAudio/Audio.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/Media/Geometry.hpp>
#include <Aero/Triggers/Behavior.hpp>
#include <Aero/Base/Hash.hpp>
#include "gui/GuiData.hpp"
#include "gui/ViewRenderer.hpp"
#include "gui/ViewState.hpp"
#include <Aero/FrameworkElement.hpp>
#include "gui/media/ImageCache.hpp"
#include "gui/text/TextPipeline.hpp"
#include "render/RenderTargetState.hpp"

#include "gui/controls/ControlRuntime.hpp"
#include "gui/controls/ItemsRuntime.hpp"
#include "gui/controls/TemplateRuntime.hpp"
#include "gui/controls/ControlBehavior.hpp"
#include "gui/metadata/MetadataRuntime.hpp"
#include "gui/property/PropertyRuntime.hpp"
#include "gui/base/FreezableRuntime.hpp"
#include "gui/base/ElementRuntime.hpp"
#include "gui/base/RoutedEventRuntime.hpp"
#include "gui/input/InputRuntime.hpp"
#include "gui/layout/LayoutRuntime.hpp"
#include "gui/binding/BindingRuntime.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/resources/StyleRuntime.hpp"
#include "gui/media/AnimationRuntime.hpp"
#include "gui/media/BrushRuntime.hpp"
#include "gui/media/EffectRuntime.hpp"
#include "gui/media/TransformRuntime.hpp"

#include <Aero/Controls.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Controls.hpp>
#include <Aero/Documents.hpp>
#include "gui/controls/Metadata.hpp"
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Controls/TextBoxBase.hpp>
#include <Aero/Controls/TextBox.hpp>
#include <Aero/Controls/PasswordBox.hpp>




#include <Aero/InputInterop.hpp>
#include <Aero/Data/Binding.hpp>
#include "gui/media/AnimationModel.hpp"
#include <Aero/Media/Animation.hpp>
#include <Aero/Input.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Media/MediaElement.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Media/Transforms.hpp>
#include <Aero/BuiltinThemes.generated.hpp>

#include "gui/controls/DataTemplateTriggerState.hpp"
#include "render/RenderDeviceState.hpp"
#include "render/RenderTree.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>
#include <utility>

namespace Aero {

namespace {

RenderingEventHandler& LegacyCompositionRenderingHandlers() noexcept {
    thread_local RenderingEventHandler handlers;
    return handlers;
}

} // namespace

void Media::CompositionTarget::AddRendering(
    const RenderingEventHandler& handler) noexcept {
    if (!handler.Empty()) {
        LegacyCompositionRenderingHandlers().Add(handler);
    }
}

bool Media::CompositionTarget::RemoveRendering(
    const RenderingEventHandler& handler) noexcept {
    return LegacyCompositionRenderingHandlers().Remove(handler);
}

} // namespace Aero

namespace Aero {

using namespace ::Aero;
namespace MediaAnimation = ::Aero::Media::Animation;
namespace {

Base::Status ViewInvalidState(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::InvalidState, message);
}

Base::Status AeroNotInitialized(const char* message) noexcept {
    return Base::Status::Failure(
        Base::ErrorCode::NotInitialized, message);
}

Base::Result<void> ValidateViewport(
    const ViewViewport& viewport) noexcept {
    if (!IsValidLayoutSize(viewport.logicalSize) ||
        !std::isfinite(viewport.dpiScale) ||
        viewport.dpiScale <= 0.0 ||
        ((viewport.logicalSize.width == 0.0) !=
            (viewport.pixelWidth == 0U)) ||
        ((viewport.logicalSize.height == 0.0) !=
            (viewport.pixelHeight == 0U))) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View viewport is invalid");
    }
    return {};
}

Base::Result<ViewViewport> MakeLogicalViewport(
    Size logicalSize,
    double dpiScale) noexcept {
    if (!IsValidLayoutSize(logicalSize) ||
        !std::isfinite(dpiScale) || dpiScale <= 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View viewport is invalid");
    }
    ViewViewport viewport;
    viewport.logicalSize = logicalSize;
    viewport.dpiScale = dpiScale;

    const double pixelWidth = logicalSize.width * dpiScale;
    const double pixelHeight = logicalSize.height * dpiScale;
    constexpr double PixelLimit =
        static_cast<double>((std::numeric_limits<std::uint32_t>::max)());
    if (!std::isfinite(pixelWidth) || !std::isfinite(pixelHeight) ||
        pixelWidth > PixelLimit || pixelHeight > PixelLimit) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "View viewport pixel dimensions are out of range");
    }
    viewport.pixelWidth = static_cast<std::uint32_t>(
        std::floor(pixelWidth + 0.5));
    viewport.pixelHeight = static_cast<std::uint32_t>(
        std::floor(pixelHeight + 0.5));
    Base::Result<void> valid = ValidateViewport(viewport);
    if (!valid) return valid.GetStatus();
    return viewport;
}

Base::Result<Base::ResourceUri> BuiltInThemeUri(
    Base::StringView name) noexcept {
    Base::String text;
    Base::Result<void> assigned = text.Assign(
        Base::StringView(
            "pack://application:,,,/Aero.Themes;component/"));
    if (!assigned) return assigned.GetStatus();
    Base::Result<void> appended = text.Append(name);
    if (!appended) return appended.GetStatus();
    return Base::ResourceUri::Parse(text.View());
}

template<class T>
Base::Result<const T*> ResolveUiValue(
    Aero::FrameworkElement& element,
    Meta::DependencyPropertyHandle property,
    const Aero::ResourceEnvironment& resources,
    const char* incompatibleMessage) noexcept {
    Base::Result<Meta::Value> explicitValue = element.GetValue(property);
    if (!explicitValue) return explicitValue.GetStatus();
    if (explicitValue.Value().Kind() == Meta::ValueKind::Object &&
        !explicitValue.Value().IsNullObject() &&
        explicitValue.Value().AsObject()) {
        Base::Object* object = explicitValue.Value().AsObject().Get();
        if (object->RuntimeType() != T::StaticTypeId()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument, incompatibleMessage);
        }
        return static_cast<const T*>(object);
    }

    Base::Result<Meta::Value> implicit = Aero::ResourceResolver::Lookup(
        &element, element.RuntimeType(), nullptr, resources);
    if (!implicit) {
        return implicit.GetStatus().code == Base::ErrorCode::NotFound
            ? Base::Result<const T*>(static_cast<const T*>(nullptr))
            : Base::Result<const T*>(implicit.GetStatus());
    }
    if (implicit.Value().Kind() != Meta::ValueKind::Object ||
        implicit.Value().IsNullObject() || !implicit.Value().AsObject() ||
        implicit.Value().AsObject()->RuntimeType() != T::StaticTypeId()) {
        return static_cast<const T*>(nullptr);
    }
    return static_cast<const T*>(implicit.Value().AsObject().Get());
}

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

} // namespace Aero

namespace Aero {

using namespace Aero;
namespace MediaAnimation = ::Aero::Media::Animation;

struct ViewState {
    static const ::Aero::Render::RenderFrame* CurrentFrame(
        const View& view) noexcept;
    Base::Result<std::uint32_t> ExecuteFrame(View& view) noexcept;

    struct FragmentMount {
        Controls::ContentControl* host = nullptr;
        Markup::LoaderResult document;
        Aero::ElementAttachment rootEdge;
    };

    ViewState(
        View& owner,
        Gui& guiOwner,
        Base::IAllocator& value,
        Base::Ref<Base::Object> guiState) noexcept
        : allocator(&value),
          guiOwner(&guiOwner),
          gui(std::move(guiState)),
          publicRenderer(owner, value),
          dispatcher(&static_cast<GuiState&>(*gui).dispatcher),
          xamlRuntime(&static_cast<GuiState&>(*gui).xaml),
          schemaBundle(&xamlRuntime->SchemaBundle()),
          documentCache(&xamlRuntime->Documents()),
          storyboardSessions(&value),
          storyboardCompletionSessions(&value),
          storyboardCompletedSubscriptions(&value),
          pendingFocusTargets(&value),
          itemGenerators(&value),
          fragmentMounts(&value) {}

    // Composition roots and Gui-owned services.
    Base::IAllocator* allocator = nullptr;
    Gui* guiOwner = nullptr;
    Base::Ref<Base::Object> gui;
    ViewRenderer publicRenderer;
    RenderingEventHandler renderingHandlers;
    Audio::Engine audio;
    ::Aero::Threading::Dispatcher* dispatcher = nullptr;
    Markup::XamlRuntime* xamlRuntime = nullptr;
    GuiSchema* schemaBundle = nullptr;
    Markup::DocumentCache* documentCache = nullptr;
    ::Aero::Meta::Registry* metadata = nullptr;
    ViewOptions options;
    // Frame/device state. These are direct values; ViewState remains the sole
    // owner and no forwarding object is introduced.
    Base::Status updateStatus;
    Base::Status rendererStatus;
    Base::Ref<RenderDevice> device;
    std::uint64_t deviceGeneration = 0U;
    ViewViewport viewport;

    // Business-domain engines allocated and destroyed by this ViewState.
    Meta::ObjectFactoryScope* objectFactory = nullptr;
    Meta::EffectiveValueEngine* values = nullptr;
    Aero::AnimationEngine* animations = nullptr;
    Aero::ElementTree* tree = nullptr;
    Aero::LayoutEngine* layout = nullptr;
    ::Aero::Render::RenderTree* renderer = nullptr;
    Aero::Media::ImageCache* images = nullptr;
    Aero::Text::TextPipeline* text = nullptr;
    Aero::BindingEngine* bindings = nullptr;
    Aero::EventRouter* events = nullptr;
    Aero::InputRouter* input = nullptr;

    Aero::Controls::TemplateEngine* templates = nullptr;
    VisualStateManager* visualStates = nullptr;
    Aero::StyleEngine* styles = nullptr;
    Aero::ElementHost elementHost;

    // Mount, provider-generation, and resource-layer state.
    Markup::Schema* schema = nullptr;
    Aero::RootAttachment rootAttachment;
    Aero::Media::Visual* attachedRootVisual = nullptr;
    Aero::UIElement* attachedRootLayout = nullptr;
    Aero::FrameworkElement* attachedRootRender = nullptr;
    std::uint64_t seenTextureProviderChange = 0U;
    std::uint64_t seenFontProviderChange = 0U;
    Aero::ResourceDictionary applicationResources;
    Aero::ResourceDictionary themeResources;
    Aero::ResourceDictionary systemResources;
    Aero::ResourceDictionary dynamicResourceEnvironment;

    // Interaction attachment state.
    ::Aero::Controls::ControlBehavior* controlBehaviors = nullptr;

    void ReportFrameFailure(
        Base::Status& slot,
        Base::Status status,
        std::uint16_t diagnosticNumber) noexcept {
        const bool repeated = slot.code == status.code &&
            slot.message == status.message;
        slot = status;
        if (repeated || status.IsOk() || options.diagnostics == nullptr) {
            return;
        }
        Base::Result<Diagnostics::Diagnostic> diagnostic =
            Diagnostics::Diagnostic::Create(
                Diagnostics::MakeDiagnosticCode(
                    Diagnostics::DiagnosticDomain::Render,
                    diagnosticNumber),
                Diagnostics::DiagnosticSeverity::Error,
                Base::StringView(
                    status.message,
                    static_cast<std::uint32_t>(
                        std::strlen(status.message))));
        if (!diagnostic) return;
        static_cast<void>(options.diagnostics->Report(
            std::move(diagnostic).Value()));
    }

    void ReportUpdateFailure(Base::Status status) noexcept {
        ReportFrameFailure(updateStatus, status, 101U);
    }

    void ReportRendererFailure(Base::Status status) noexcept {
        ReportFrameFailure(rendererStatus, status, 102U);
    }

    void ClearUpdateFailure() noexcept { updateStatus = {}; }
    void ClearRendererFailure() noexcept { rendererStatus = {}; }

    // Animation sessions are direct values owned by ViewState.
    struct StoryboardSession {
        explicit StoryboardSession(
            Base::IAllocator* allocator) noexcept
            : handles(allocator) {}

        Aero::FrameworkElement* owner = nullptr;
        Base::String name;
        Base::Vector<
            Aero::Media::Animation::Runtime::AnimationHandle>
            handles;
    };
    Base::Vector<StoryboardSession>
        storyboardSessions;
    struct StoryboardCompletionSession {
        explicit StoryboardCompletionSession(
            Base::IAllocator* allocator) noexcept
            : handles(allocator) {}

        Base::Ref<MediaAnimation::Storyboard> storyboard;
        Aero::FrameworkElement* owner = nullptr;
        Base::Vector<
            Aero::Media::Animation::Runtime::AnimationHandle>
            handles;
    };
    struct StoryboardCompletedSubscription {
        MediaAnimation::StoryboardCompletedTrigger* trigger =
            nullptr;
        Aero::FrameworkElement* owner = nullptr;
        const Aero::NameScope* names = nullptr;
    };
    Base::Vector<StoryboardCompletionSession>
        storyboardCompletionSessions;
    Base::Vector<StoryboardCompletedSubscription>
        storyboardCompletedSubscriptions;
    Base::Vector<Base::WeakRef<Aero::UIElement>>
        pendingFocusTargets;
    Base::Result<void> QueueFocus(Aero::UIElement& target) noexcept {
        Base::Ref<Aero::UIElement> retained =
            Base::Ref<Aero::UIElement>::FromBorrowed(target);
        for (const Base::WeakRef<Aero::UIElement>& pending :
             pendingFocusTargets) {
            Base::Ref<Aero::UIElement> existing = pending.Lock();
            if (existing.Get() == &target) return {};
        }
        return pendingFocusTargets.PushBack(
            Base::WeakRef<Aero::UIElement>(retained));
    }
    Base::Result<std::uint32_t> ProcessPendingFocus() noexcept {
        if (input == nullptr || pendingFocusTargets.Empty()) return 0U;
        std::uint32_t focusedCount = 0U;
        std::uint32_t output = 0U;
        for (std::uint32_t index = 0U;
             index < pendingFocusTargets.Size(); ++index) {
            Base::Ref<Aero::UIElement> target =
                pendingFocusTargets[index].Lock();
            if (!target) continue;
            if (!target->GetIsLoaded()) {
                if (output != index) {
                    pendingFocusTargets[output] =
                        std::move(pendingFocusTargets[index]);
                }
                ++output;
                continue;
            }
            if (!target->GetIsEnabled()) continue;
            Base::Result<bool> focused = input->SetFocus(target.Get());
            if (!focused) return focused.GetStatus();
            if (focused.Value()) ++focusedCount;
        }
        Base::Result<void> resized =
            pendingFocusTargets.Resize(output);
        if (!resized) return resized.GetStatus();
        return focusedCount;
    }
    Base::Result<void> ExecuteAnimationAction(
        MediaAnimation::TriggerAction& action,
        Aero::FrameworkElement& owner,
        Aero::Controls::DataTemplateTriggerState*
            dataTemplateContext = nullptr,
        const Aero::NameScope* names = nullptr) noexcept;
    void CancelStoryboardCompletionSessions(
        Base::Span<const Aero::Media::Animation::Runtime::AnimationHandle>
            handles) noexcept;
    Base::Result<std::uint32_t>
    ProcessStoryboardCompletions() noexcept;
    static Base::Result<void> ExecuteStyleTriggerActions(
        ::Aero::DependencyObject& owner,
        Base::Span<const Base::Ref<Base::Object>>
            actions,
        void* context) noexcept {
        auto* runtime = static_cast<ViewState*>(context);
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
    Base::Result<bool> ConditionBehaviorsAllowExecution(
        Base::Span<const Base::Ref<Base::Object>> behaviors,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        const auto numeric = [](const Meta::PropertyValue& value,
                                long double& output) noexcept {
            switch (value.Kind()) {
            case Meta::ValueKind::SignedInteger:
                output = static_cast<long double>(value.AsSignedInteger());
                return true;
            case Meta::ValueKind::UnsignedInteger:
                output = static_cast<long double>(value.AsUnsignedInteger());
                return true;
            case Meta::ValueKind::Double:
                output = static_cast<long double>(value.AsDouble());
                return true;
            default:
                return false;
            }
        };
        const auto evaluate = [&](const MediaAnimation::ComparisonCondition& condition)
            noexcept -> Base::Result<bool> {
            const Base::Ref<Data::Binding> binding = condition.GetLeftOperand();
            if (!binding) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "ConditionBehavior requires a bound left operand");
            }
            Base::Result<Meta::PropertyValue> current =
                EvaluateAuthoredBinding(
                    *binding, owner, nullptr, names, nullptr);
            if (!current) return current.GetStatus();
            Meta::PropertyValue expected = condition.GetRightOperand();
            if (expected.IsNullObject()) {
                return current.Value().IsNullObject();
            }
            if (expected.Kind() == Meta::ValueKind::String &&
                expected.Type() != current.Value().Type()) {
                Base::Result<Meta::PropertyValue> converted =
                    Meta::PropertyValue::TryFromString(
                        current.Value().Type(), expected.AsString());
                if (!converted) return false;
                expected = std::move(converted).Value();
            }
            const auto comparison = condition.GetComparisonOperator();
            if (comparison ==
                MediaAnimation::ComparisonCondition::Operator::Equal) {
                return current.Value().Equals(expected);
            }
            if (comparison ==
                MediaAnimation::ComparisonCondition::Operator::NotEqual) {
                return !current.Value().Equals(expected);
            }
            long double left = 0.0L;
            long double right = 0.0L;
            if (numeric(current.Value(), left) && numeric(expected, right)) {
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
                    return false;
                }
            }
            if (current.Value().Kind() == Meta::ValueKind::String &&
                expected.Kind() == Meta::ValueKind::String) {
                const int order = current.Value().AsString().Compare(
                    expected.AsString());
                switch (comparison) {
                case MediaAnimation::ComparisonCondition::Operator::LessThan:
                    return order < 0;
                case MediaAnimation::ComparisonCondition::Operator::LessThanOrEqual:
                    return order <= 0;
                case MediaAnimation::ComparisonCondition::Operator::GreaterThan:
                    return order > 0;
                case MediaAnimation::ComparisonCondition::Operator::GreaterThanOrEqual:
                    return order >= 0;
                default:
                    return false;
                }
            }
            return false;
        };

        for (const Base::Ref<Base::Object>& behavior : behaviors) {
            if (!behavior) continue;
            if (behavior->RuntimeType() !=
                MediaAnimation::ConditionBehavior::StaticTypeId()) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "Interaction trigger contains an unsupported behavior");
            }
            const Base::Ref<MediaAnimation::ConditionalExpression> expression =
                static_cast<MediaAnimation::ConditionBehavior&>(
                    *behavior).GetExpression();
            if (!expression) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "ConditionBehavior has no expression");
            }
            const bool conjunction = expression->GetChaining() ==
                MediaAnimation::ConditionalExpression::ForwardChaining::And;
            bool expressionResult = conjunction;
            bool hasCondition = false;
            for (const Base::Ref<MediaAnimation::ComparisonCondition>& condition :
                 expression->GetConditions()) {
                if (!condition) continue;
                hasCondition = true;
                Base::Result<bool> matches = evaluate(*condition);
                if (!matches) return matches.GetStatus();
                expressionResult = matches.Value();
                if (conjunction && !expressionResult) return false;
                if (!conjunction && expressionResult) break;
            }
            if (!hasCondition || !expressionResult) return false;
        }
        return true;
    }

    struct AnimationEventState {
        ViewState* runtime = nullptr;
        MediaAnimation::EventTrigger* trigger = nullptr;
        Aero::FrameworkElement* owner = nullptr;
        const Aero::NameScope* names = nullptr;

        Base::Result<bool> EvaluateComparison(
            const MediaAnimation::ComparisonCondition& condition) noexcept {
            const Base::Ref<Data::Binding> binding =
                condition.GetLeftOperand();
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
            Base::Result<Meta::BindingPathPlan> plan =
                Meta::BindingPathPlan::Compile(
                    *runtime->metadata,
                    source->RuntimeType(), binding->GetPath().GetPath());
            if (!plan) return plan.GetStatus();
            Base::Result<Meta::PropertyValue> current =
                plan.Value().Get(*runtime->metadata, *source);
            if (!current) return current.GetStatus();
            Meta::PropertyValue expected = condition.GetRightOperand();
            if (expected.IsNullObject()) {
                return current.Value().IsNullObject();
            }
            if (expected.Kind() == Meta::ValueKind::String &&
                expected.Type() != current.Value().Type()) {
                Base::Result<Meta::PropertyValue> converted =
                    Meta::PropertyValue::TryFromString(
                        current.Value().Type(), expected.AsString());
                // WPF-style conditions simply do not match when their two
                // operands cannot be converted to a comparable type.
                if (!converted) return false;
                expected = std::move(converted).Value();
            }
            const auto comparison = condition.GetComparisonOperator();
            if (comparison ==
                MediaAnimation::ComparisonCondition::Operator::Equal) {
                return current.Value().Equals(expected);
            }
            if (comparison ==
                MediaAnimation::ComparisonCondition::Operator::NotEqual) {
                return !current.Value().Equals(expected);
            }

            const auto isNumeric = [](Meta::ValueKind kind) noexcept {
                return kind == Meta::ValueKind::SignedInteger ||
                    kind == Meta::ValueKind::UnsignedInteger ||
                    kind == Meta::ValueKind::Double;
            };
            const auto numericValue = [](const Meta::PropertyValue& value) noexcept {
                switch (value.Kind()) {
                case Meta::ValueKind::SignedInteger:
                    return static_cast<long double>(value.AsSignedInteger());
                case Meta::ValueKind::UnsignedInteger:
                    return static_cast<long double>(value.AsUnsignedInteger());
                case Meta::ValueKind::Double:
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
            if (current.Value().Kind() == Meta::ValueKind::String &&
                expected.Kind() == Meta::ValueKind::String) {
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
                 trigger->GetBehaviors()) {
                if (!behavior) continue;
                if (behavior->RuntimeType() !=
                    MediaAnimation::ConditionBehavior::StaticTypeId()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::Unsupported,
                        "EventTrigger contains an unsupported behavior");
                }
                const Base::Ref<MediaAnimation::ConditionalExpression> expression =
                    static_cast<MediaAnimation::ConditionBehavior&>(*behavior).GetExpression();
                if (!expression) {
                    return Base::Status::Failure(
                        Base::ErrorCode::InvalidState,
                        "ConditionBehavior has no expression");
                }
                bool expressionResult = false;
                for (const Base::Ref<MediaAnimation::ComparisonCondition>& condition :
                     expression->GetConditions()) {
                    if (!condition) continue;
                    Base::Result<bool> matches = EvaluateComparison(*condition);
                    if (!matches) return matches.GetStatus();
                    expressionResult = matches.Value();
                    if (!expressionResult && expression->GetChaining() ==
                        MediaAnimation::ConditionalExpression::ForwardChaining::And) {
                        return false;
                    }
                    if (expressionResult && expression->GetChaining() ==
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
                 trigger->GetActions()) {
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
    struct AnimationEventSubscription {
        Base::Object* source = nullptr;
        Aero::Media::Visual* visualOwner = nullptr;
        Aero::RoutedEventHandle event;
        Aero::RoutedEventHandler handler;
        AnimationEventState* context = nullptr;
        bool contentSource = false;
    };
    Base::Vector<AnimationEventSubscription>
        animationEventSubscriptions;
    struct StyleDataTriggerHandlerState {
        ViewState* runtime = nullptr;
        Aero::FrameworkElement* target = nullptr;
        const Aero::Style* style = nullptr;
        std::uint32_t triggerIndex = 0U;
        ::Aero::DependencyObject* source = nullptr;
        Meta::DependencyPropertyHandle property;
        Meta::PropertyValue expected;

        void Invoke(
            ::Aero::DependencyObject&,
            const Meta::DependencyPropertyChangedEventArgs&) noexcept;
    };
    struct StyleDataTriggerSubscription {
        Aero::FrameworkElement* target = nullptr;
        ::Aero::DependencyObject* source = nullptr;
        Meta::DependencyPropertyHandle property;
        Meta::DependencyPropertyChangedEventHandler handler;
        StyleDataTriggerHandlerState* context = nullptr;
    };
    Base::Vector<StyleDataTriggerSubscription>
        styleDataTriggerSubscriptions;

    struct AttachedBehaviorInstance {
        Aero::FrameworkElement* target = nullptr;
        const Interactivity::Behavior* prototype = nullptr;
        Base::Ref<Interactivity::Behavior> instance;
        Base::Vector<Data::BindingHandle> bindings;
    };
    Base::Vector<AttachedBehaviorInstance>
        attachedBehaviorInstances;

    struct PropertyChangedTriggerState {
        ViewState* runtime = nullptr;
        MediaAnimation::PropertyChangedTrigger* trigger = nullptr;
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
        ViewState* runtime = nullptr;
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

    struct KeyTriggerState {
        ViewState* runtime = nullptr;
        MediaAnimation::KeyTrigger* trigger = nullptr;
        Aero::FrameworkElement* owner = nullptr;
        const Aero::NameScope* names = nullptr;

        void Invoke(
            Base::Object*,
            Aero::KeyEventArgs& args) noexcept;
    };
    struct KeyTriggerSubscription {
        Aero::FrameworkElement* owner = nullptr;
        Aero::UIElement* source = nullptr;
        Aero::KeyEventHandler handler;
        KeyTriggerState* context = nullptr;
    };
    Base::Vector<KeyTriggerSubscription>
        keyTriggerSubscriptions;

    struct DataTemplateTriggerHandlerState {
        ViewState* runtime = nullptr;
        Base::Ref<
            Aero::Controls::DataTemplateTriggerState>
            triggerContext;
        std::uint32_t triggerIndex = 0U;
        std::uint32_t conditionIndex = 0U;

        void Invoke(
            ::Aero::DependencyObject&,
            const Meta::
                DependencyPropertyChangedEventArgs&)
            noexcept;
    };
    struct DataTemplateTriggerSubscription {
        ::Aero::DependencyObject* source = nullptr;
        Meta::DependencyPropertyHandle property;
        Meta::DependencyPropertyChangedEventHandler
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
    bool deferGeneratedActivation = false;
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
    const Aero::NameScope* activeFragmentNames = nullptr;

    bool HasAttachedRoot() const noexcept {
        return rootAttachment.IsAttached();
    }

    Base::Result<void> AttachVisualGraph(
        ::Aero::Media::Visual& rootVisual,
        UIElement& rootLayout,
        FrameworkElement* rootRender,
        Base::Span<Aero::Markup::VisualEdge> edges,
        Size availableSize) noexcept {
        if (tree == nullptr || layout == nullptr || HasAttachedRoot() ||
            !IsValidLayoutSize(availableSize)) {
            return ViewInvalidState(
                "Gui root cannot be attached in its current state");
        }
        tree->AttachPresentation(layout, renderer);
        Base::Result<Aero::RootAttachment> rootAttached =
            tree->AttachRoot(rootVisual, availableSize);
        if (!rootAttached) return rootAttached.GetStatus();
        rootAttachment = std::move(rootAttached).Value();
        attachedRootVisual = &rootVisual;
        attachedRootLayout = &rootLayout;
        attachedRootRender = rootRender;

        std::uint32_t attached = 0U;
        while (attached < edges.Size()) {
            bool progressed = false;
            for (Aero::Markup::VisualEdge& edge : edges) {
                if (edge.state.logicalAttached || edge.parent == nullptr ||
                    edge.child == nullptr ||
                    Aero::ElementPrivate::Tree(*edge.parent) != tree) {
                    continue;
                }
                Base::Result<Aero::ElementAttachment> edgeAttached =
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
        Base::Span<Aero::Markup::VisualEdge> edges) noexcept {
        if (!HasAttachedRoot() || tree == nullptr) {
            return ViewInvalidState(
                "Deferred visual edges require an attached root");
        }
        std::uint32_t attached = 0U;
        for (const Aero::Markup::VisualEdge& edge : edges) {
            if (edge.state.logicalAttached) ++attached;
        }
        while (attached < edges.Size()) {
            bool progressed = false;
            for (Aero::Markup::VisualEdge& edge : edges) {
                if (edge.state.logicalAttached ||
                    (edge.child != nullptr &&
                     Aero::ElementPrivate::Tree(*edge.child) == tree) ||
                    edge.parent == nullptr || edge.child == nullptr ||
                    Aero::ElementPrivate::Tree(*edge.parent) != tree) {
                    continue;
                }
                Base::Result<Aero::ElementAttachment> edgeAttached =
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
            return AeroNotInitialized(
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
        if (renderer != nullptr &&
            attachedRootVisual != nullptr) {
            return renderer->Invalidate(
                *attachedRootVisual,
                Aero::Render::RenderInvalidation::State);
        }
        return {};
    }

    Base::Result<void> ApplyViewport(
        const ViewViewport& next) noexcept {
        if (renderer == nullptr) {
            return AeroNotInitialized(
                "View render tree is unavailable");
        }
        const ViewViewport previous = viewport;
        Base::Result<void> updated = renderer->SetViewport(
            next.logicalSize,
            next.pixelWidth,
            next.pixelHeight,
            next.dpiScale);
        if (!updated) return updated.GetStatus();
        if (HasAttachedRoot()) {
            updated = ResizeVisualRoot(next.logicalSize);
            if (!updated) {
                static_cast<void>(renderer->SetViewport(
                    previous.logicalSize,
                    previous.pixelWidth,
                    previous.pixelHeight,
                    previous.dpiScale));
                return updated.GetStatus();
            }
        }
        viewport = next;
        return {};
    }

    Base::Result<void> DetachVisualGraph(
        Base::Span<Aero::Markup::VisualEdge> edges) noexcept {
        if (!HasAttachedRoot() && attachedRootVisual == nullptr) return {};
        if (tree == nullptr) {
            return ViewInvalidState(
                "Gui context is unavailable during root detach");
        }

        const auto reconcileAttachment =
            [this](Aero::Markup::VisualEdge& edge) noexcept {
                auto& state = edge.state;
                if (state.child == nullptr) {
                    state.logicalAttached = false;
                    state.visualAttached = false;
                    state.layoutAttached = false;
                    state.renderAttached = false;
                    return;
                }
                state.logicalAttached =
                    state.logicalParent != nullptr &&
                    state.child->GetLogicalParent() == state.logicalParent;
                state.visualAttached =
                    state.visualParent != nullptr &&
                    state.child->GetVisualParent() == state.visualParent;

                Aero::UIElement* childElement =
                    state.child->AsUIElement();
                Aero::UIElement* parentElement =
                    state.visualParent != nullptr
                    ? state.visualParent->AsUIElement()
                    : nullptr;
                if (childElement != nullptr &&
                    Aero::UIElement::Access::LayoutAttached(*childElement) &&
                    Aero::UIElement::Access::LayoutManager(*childElement) == nullptr) {
                    // The logical subtree has already left its ElementTree, so
                    // no LayoutEngine remains to consume this stale edge bit.
                    Aero::UIElement::Access::LayoutAttached(*childElement) = false;
                    Aero::UIElement::Access::MeasureQueued(*childElement) = false;
                    Aero::UIElement::Access::ArrangeQueued(*childElement) = false;
                }
                state.layoutAttached =
                    layout != nullptr && childElement != nullptr &&
                    parentElement != nullptr &&
                    Aero::UIElement::Access::LayoutAttached(*childElement) &&
                    Aero::UIElement::Access::LayoutManager(*childElement) == layout &&
                    childElement->LayoutParent() == parentElement;

                if (Aero::ElementPrivate::RenderAttached(
                        *state.child) &&
                    Aero::ElementPrivate::RenderRuntime(
                        *state.child) == nullptr) {
                    Aero::ElementPrivate::RenderAttached(
                        *state.child) = false;
                    Aero::ElementPrivate::RenderQueued(
                        *state.child) = false;
                    Aero::ElementPrivate::Rendering(
                        *state.child) = false;
                    Aero::ElementPrivate::NodeId(
                        *state.child) = Base::InvalidRenderNodeId;
                    Aero::ElementPrivate::RenderValid(
                        *state.child) = false;
                }
                state.renderAttached =
                    renderer != nullptr &&
                    Aero::ElementPrivate::RenderAttached(
                        *state.child) &&
                    Aero::ElementPrivate::RenderRuntime(
                        *state.child) == renderer &&
                    Aero::ElementPrivate::RenderParent(
                        *state.child) == state.visualParent;
            };

        std::uint32_t remaining = 0U;
        for (Aero::Markup::VisualEdge& edge : edges) {
            reconcileAttachment(edge);
            if (edge.state.IsAttached()) ++remaining;
        }
        while (remaining > 0U) {
            bool progressed = false;
            for (Aero::Markup::VisualEdge& edge : edges) {
                reconcileAttachment(edge);
                if (!edge.state.IsAttached()) continue;
                bool hasAttachedChild = false;
                for (const Aero::Markup::VisualEdge& candidate : edges) {
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

    Base::Result<void> BeginDocumentLoad() noexcept {
        if (!initialized) {
            return AeroNotInitialized(
                "View must be initialized before XAML loading");
        }
        if (mounted || root || loadedDocument.root) {
            return ViewInvalidState(
                "View already owns a loaded document");
        }
        return {};
    }

    Base::Result<Markup::XamlReaderSettings> XamlSettings(
        bool deferredEffects = false,
        const Markup::XamlReaderSettings* override = nullptr) noexcept {
        Markup::XamlReaderSettings result;
        if (override != nullptr) {
            result = *override;
        }
        loadContext.resources = &dynamicResourceEnvironment;
        loadContext.effectiveValues = values;
        loadContext.bindings = bindings;
        loadContext.fallbackResources =
            &dynamicResourceEnvironment;
        loadContext.documentCache = documentCache;
        loadContext.dispatcher = dispatcher;
        loadContext.dependencyProperties =
            &::Aero::MetadataPrivate::
                DependencyProperties(*metadata);
        loadContext.effectLifetime = effectLifetime;
        loadContext.effectCommitMode = deferredEffects
            ? Markup::EffectCommitMode::Deferred
            : Markup::EffectCommitMode::Immediate;
        return result;
    }

    void AttachTextLayout(
        Aero::Media::Visual& node,
        ::Aero::Controls::TextBlockLayout* service,
        bool invalidate = false) noexcept {
        if (metadata == nullptr) return;
        const Meta::TypeId type = node.RuntimeType();
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::TextBlock::StaticTypeId())) {
            ::Aero::Controls::ControlPrivate::Attach(
                *static_cast<Controls::TextBlock*>(&node),
                service,
                invalidate);
        }
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::TextBox::StaticTypeId())) {
            ::Aero::Controls::ControlPrivate::Attach(
                *static_cast<Controls::TextBox*>(&node),
                service,
                invalidate);
        }
        if (metadata->Types().IsDerivedFrom(
                type,
                Controls::PasswordBox::
                    StaticTypeId())) {
            ::Aero::Controls::ControlPrivate::Attach(
                *static_cast<Controls::PasswordBox*>(
                    &node),
                service,
                invalidate);
        }
    }

    Aero::Render::MeshResources*
    GetMeshResources() noexcept {
        return publicRenderer.Resources().meshes;
    }

    Aero::Render::ImageResources*
    GetImageResources() noexcept {
        return publicRenderer.Resources().images;
    }

    void AttachPathResources(
        Aero::Media::Visual& node,
        Aero::Render::MeshResources* service,
        bool invalidate = false) noexcept {
        if (metadata == nullptr) return;
        const Meta::TypeId type = node.RuntimeType();
        if (metadata->Types().IsDerivedFrom(
                type,
                Shapes::Path::StaticTypeId())) {
            ::Aero::Controls::ControlPrivate::Attach(
                *static_cast<Shapes::Path*>(&node),
                service,
                invalidate);
        }
    }

    void VisitTextElements(
        Aero::Media::Visual* rootVisual,
        ::Aero::Controls::TextBlockLayout* service,
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
        for (Aero::Media::Visual* child :
             Aero::ElementPrivate::VisualChildren(*rootVisual)) {
            VisitTextElements(
                child,
                service,
                invalidate,
                effectivelyVisible);
        }
    }

    void VisitPaths(
        Aero::Media::Visual* rootVisual,
        Aero::Render::MeshResources* service,
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
        for (Aero::Media::Visual* child :
             Aero::ElementPrivate::VisualChildren(*rootVisual)) {
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
        auto* runtime = static_cast<ViewState*>(context);
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
        dynamicResourceEnvironment.Clear();
        Base::Result<void> rebuilt =
            dynamicResourceEnvironment.AddMerged(
                systemResources);
        if (rebuilt) {
            rebuilt =
                dynamicResourceEnvironment.AddMerged(
                    themeResources);
        }
        if (rebuilt) {
            rebuilt =
                dynamicResourceEnvironment.AddMerged(
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

    Aero::Media::Visual* RootVisual() noexcept {
        if (!root) return nullptr;
        if (!metadata->Types().IsDerivedFrom(
                root->RuntimeType(),
                Aero::Media::Visual::StaticTypeId())) {
            return nullptr;
        }
        return static_cast<Aero::Media::Visual*>(root.Get());
    }

    Base::Result<void> SynchronizeOverlays() noexcept {
        renderOverlays.Clear();
        inputOverlays.Clear();
        overlayOrigins.Clear();
        Aero::Media::Visual* rootVisual =
            RootVisual();
        if (rootVisual == nullptr ||
            renderer == nullptr) {
            if (input != nullptr) input->ClearOverlays();
            return {};
        }
        Base::Vector<Aero::Media::Visual*> stack(
            allocator);
        Base::Result<void> appended =
            stack.PushBack(rootVisual);
        if (!appended) return appended.GetStatus();
        while (!stack.Empty()) {
            Aero::Media::Visual* node =
                stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;
            const Meta::TypeId type =
                node->RuntimeType();
            bool open = false;
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::Primitives::Popup::
                        StaticTypeId())) {
                open =
                    static_cast<Controls::Primitives::Popup*>(
                        node)->GetIsOpen();
            } else if (
                metadata->Types().IsDerivedFrom(
                    type,
                    Controls::ContextMenu::
                        StaticTypeId())) {
                open =
                    static_cast<
                        Controls::ContextMenu*>(
                        node)->GetIsOpen();
            }
            if (open) {
                Aero::Media::Visual* ancestor =
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
                        Aero::Media::Visual*
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
                                    GetPlacementTarget();
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
                        renderOverlays.PushBack(
                            framework);
                    if (!appended) {
                        return appended.GetStatus();
                    }
                    appended =
                        overlayOrigins.PushBack(
                            origin);
                    if (!appended) {
                        return appended.GetStatus();
                    }
                    appended =
                        inputOverlays.PushBack(
                            input);
                    if (!appended) {
                        return appended.GetStatus();
                    }
                }
            }
            const Base::Span<
                Aero::Media::Visual* const>
                children =
                    Aero::ElementPrivate::VisualChildren(*node);
            for (std::uint32_t index =
                     children.Size();
                 index > 0U;
                 --index) {
                appended =
                    stack.PushBack(
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
            const Meta::TypeId type =
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
        const Aero::Media::Visual& root,
        const Aero::Media::Visual& target)
        noexcept {
        const Aero::Media::Visual* current =
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
            const Meta::TypeId type =
                overlay->RuntimeType();
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::Primitives::Popup::
                        StaticTypeId())) {
                auto* popup =
                    static_cast<Controls::Primitives::Popup*>(
                        overlay);
                if (!popup->GetStaysOpen()) {
                    popup->SetIsOpen(false);
                    static_cast<void>(
                        popup->SetPlacementTarget(
                            {}));
                }
            } else if (
                metadata->Types().IsDerivedFrom(
                    type,
                    Controls::ContextMenu::
                        StaticTypeId())) {
                static_cast<Controls::ContextMenu*>(
                    overlay)->SetIsOpen(false);
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
            const Meta::TypeId type =
                overlay->RuntimeType();
            if (metadata->Types().IsDerivedFrom(
                    type,
                    Controls::Primitives::Popup::
                        StaticTypeId())) {
                static_cast<Controls::Primitives::Popup*>(
                    overlay)->SetIsOpen(false);
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
                static_cast<Controls::ContextMenu*>(
                    overlay)->SetIsOpen(false);
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
        Aero::Media::Visual* current =
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
                        menu->SetPlacementTarget(std::move(target));
                    }
                    menu->SetIsOpen(true);
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
                activeToolTip->SetIsOpen(false);
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
        Aero::Media::Visual* current =
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
            activeToolTip->SetIsOpen(false);
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
            pendingToolTip->SetPlacementTarget(toolTipTarget);
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
                    GetInitialShowDelay(
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
            pendingToolTip->SetIsOpen(true);
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
                GetShowDuration(*toolTipTarget);
        if (toolTipVisibleElapsed < duration) {
            return 0U;
        }
        activeToolTip->SetIsOpen(false);
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

    Base::Result<Aero::Media::Visual*> ResolveVisual(
        Base::Object& object, Meta::TypeId type) noexcept {
        if (object.RuntimeType() != type ||
            !metadata->Types().IsDerivedFrom(
                type, Aero::Media::Visual::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "View root is not a registered Visual");
        }
        return static_cast<Aero::Media::Visual*>(&object);
    }

    Base::Result<Aero::UIElement*> ResolveUIElement(
        Base::Object& object, Meta::TypeId type) noexcept {
        Base::Result<Aero::Media::Visual*> visual =
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
        Base::Object& object, Meta::TypeId type) noexcept {
        Base::Result<Aero::Media::Visual*> visual =
            ResolveVisual(object, type);
        return visual ? visual.Value()->AsFrameworkElement() : nullptr;
    }

    static Base::Object* FindNameForElement(
        void* context,
        Base::StringView name,
        Meta::TypeId expectedType) noexcept {
        auto* runtime = static_cast<ViewState*>(context);
        if (runtime == nullptr || name.Empty()) return nullptr;
        Base::Object* object = runtime->activeFragmentNames != nullptr
            ? runtime->activeFragmentNames->Find(name)
            : nullptr;
        if (object == nullptr) {
            object = runtime->loadedDocument.names.Find(name);
        }
        if (object == nullptr) {
            for (ViewState::FragmentMount& fragment :
                 runtime->fragmentMounts) {
                object = fragment.document.names.Find(name);
                if (object != nullptr) break;
            }
        }
        if (object == nullptr || expectedType == Meta::InvalidTypeId) {
            return object;
        }
        return runtime->metadata != nullptr &&
            runtime->metadata->Types().IsAssignableFrom(
                expectedType, object->RuntimeType())
            ? object
            : nullptr;
    }

    Base::Result<void> ApplyUi(Aero::Media::Visual& root) noexcept {
        if (metadata == nullptr || values == nullptr || bindings == nullptr ||
            events == nullptr || input == nullptr || styles == nullptr ||
            templates == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "View UI state is unavailable");
        }

        const Aero::ResourceEnvironment resources = ResourceEnvironment();
        Base::Vector<Aero::Media::Visual*> stack(allocator);
        Base::Result<void> pushed = stack.PushBack(&root);
        if (!pushed) return pushed.GetStatus();
        while (!stack.Empty()) {
            Aero::Media::Visual* node = stack.Back();
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
                    if (!style->GetIsSealed()) {
                        return Base::Status::Failure(
                            Base::ErrorCode::InvalidState,
                            "Implicit Style is not sealed");
                    }
                    if (styles->AppliedStyle(*element) != style) {
                        ClearStyleDataTriggersFor(*element);
                        Base::Result<void> applied = styles->Apply(*element, *style);
                        if (!applied) return applied.GetStatus();
                    }
                    Base::Result<std::uint32_t> dataTriggers =
                        StartStyleDataTriggers(*element, *style);
                    if (!dataTriggers) return dataTriggers.GetStatus();
                }
            }

            Base::Result<std::uint32_t> styleValues = values->Flush();
            if (!styleValues) return styleValues.GetStatus();

            if (metadata->Types().IsDerivedFrom(
                    node->RuntimeType(), Controls::Control::StaticTypeId())) {
                auto& control = *static_cast<Controls::Control*>(node);
                ::Aero::Controls::ControlPrivate::AttachTemplateEngine(
                    control, templates);
                Base::Result<const Controls::ControlTemplate*> resolved =
                    ResolveUiValue<Controls::ControlTemplate>(
                        control, Controls::Control::TemplateProperty, resources,
                        "Control Template value is not a ControlTemplate");
                if (!resolved) return resolved.GetStatus();
                const Controls::ControlTemplate* controlTemplate =
                    resolved.Value();
                if (controlTemplate != nullptr) {
                    const ::Aero::Controls::TemplateHandle existing =
                        templates->AppliedHandle(control);
                    if (!existing.IsValid() ||
                        templates->AppliedTemplate(existing) != controlTemplate) {
                        Base::Result<::Aero::Controls::TemplateHandle> applied =
                            templates->Apply(control, *controlTemplate);
                        if (!applied) return applied.GetStatus();
                        // TemplateEngine installs the handle while its
                        // transaction is active. Invoke the control callback
                        // only after Apply has returned so PART_* lookups and
                        // ItemsHost realization cannot re-enter that
                        // transaction.
                        ::Aero::Controls::ControlPrivate::
                            InvokeTemplateApplied(control);
                    }
                }
            }

            for (Aero::Media::Visual* child :
                 Aero::ElementPrivate::VisualChildren(*node)) {
                pushed = stack.PushBack(child);
                if (!pushed) return pushed.GetStatus();
            }
        }
        Base::Result<std::uint32_t> appliedValues = values->Flush();
        return appliedValues ? Base::Result<void>()
                             : Base::Result<void>(appliedValues.GetStatus());
    }

    void DetachUi(
        Aero::Media::Visual* root,
        Base::Span<Aero::Media::Visual* const> declarationNodes) noexcept {
        if (values == nullptr) return;

        Base::Vector<Aero::Media::Visual*> reachable(allocator);
        if (root != nullptr) {
            (void)reachable.PushBack(root);
            for (std::uint32_t index = 0U; index < reachable.Size(); ++index) {
                Aero::Media::Visual* node = reachable[index];
                if (node == nullptr) continue;
                for (Aero::Media::Visual* child :
                     Aero::ElementPrivate::VisualChildren(*node)) {
                    if (child != nullptr) (void)reachable.PushBack(child);
                }
            }
        }

        for (Aero::Media::Visual* node : reachable) {
            if (node == nullptr) continue;
            if (Aero::UIElement* ui = node->AsUIElement()) {
                ElementPrivate::SetViewServices(*ui, nullptr);
            }
            if (bindings != nullptr) (void)bindings->DetachObject(*node);
            Aero::FrameworkElement* element = node->AsFrameworkElement();
            if (element != nullptr && styles != nullptr) {
                ClearStyleDataTriggersFor(*element);
                (void)styles->DetachObject(*element);
            }
        }
        for (std::uint32_t index = reachable.Size(); index > 0U; --index) {
            Aero::Media::Visual* node = reachable[index - 1U];
            if (node == nullptr || metadata == nullptr ||
                !metadata->Types().IsDerivedFrom(
                    node->RuntimeType(), Controls::Control::StaticTypeId())) {
                continue;
            }
            auto& control = *static_cast<Controls::Control*>(node);
            if (visualStates != nullptr) {
                (void)::Aero::Controls::TemplatePrivate::Clear(
                    *visualStates, control);
            }
            if (templates != nullptr) {
                (void)templates->Clear(control);
            }
        }
        for (Aero::Media::Visual* node : declarationNodes) {
            if (node != nullptr) (void)values->DetachObject(*node);
        }
    }

    Base::Result<void> CreateUiEngines() noexcept {
        Base::Result<void> status = AllocateObject(*allocator, Base::MemoryTag::Ui, templates, *tree, *values,
            ::Aero::MetadataPrivate::
                DependencyProperties(*metadata),
            layout, renderer, metadata, bindings,
            &dynamicResourceEnvironment);
        if (!status) return status.GetStatus();
        Base::Result<VisualStateManager*> createdStates =
            ::Aero::Controls::TemplatePrivate::Create(
                *values,
                *templates,
                *animations,
                ::Aero::MetadataPrivate::
                    DependencyProperties(*metadata));
        if (!createdStates) return createdStates.GetStatus();
        visualStates = createdStates.Value();
        status = AllocateObject(*allocator, Base::MemoryTag::Ui, styles, *values,
            ::Aero::MetadataPrivate::
                DependencyProperties(*metadata));
        if (!status) return status.GetStatus();
        styles->SetTriggerActionHandler(
            &ViewState::ExecuteStyleTriggerActions, this);
        elementHost.events = events;
        elementHost.input = input;
        elementHost.bindings = bindings;
        elementHost.templates = templates;
        elementHost.visualStates = visualStates;
        elementHost.textLayout = text != nullptr
            ? static_cast<void*>(text->Layout())
            : nullptr;
        elementHost.meshResources = GetMeshResources();
        elementHost.nameScopeContext = this;
        elementHost.findName = &ViewState::FindNameForElement;
        tree->SetHost(&elementHost);
        return {};
    }

    static Base::Result<void> GeneratedItemSubtreeChanged(
        Aero::Media::Visual& root,
        Controls::ItemSubtreeChange change,
        void* context) noexcept {
        auto* runtime = static_cast<ViewState*>(context);
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
        if (runtime->deferGeneratedActivation ||
            (runtime->bindings != nullptr &&
             runtime->bindings->IsFlushing())) {
            Base::Result<Aero::VisualHandle>
                handle =
                    runtime->tree->GetHandle(root);
            if (!handle) return handle.GetStatus();
            return runtime->
                pendingGeneratedVisuals.
                    PushBack(handle.Value());
        }
        Base::Result<void> applied =
            runtime->ApplyUi(root);
        if (!applied) {
            runtime->DetachUi(
                &root, {});
            return applied.GetStatus();
        }
        Base::Result<void> attached =
            runtime->VisitAndAttach(root);
        if (!attached) {
            runtime->DetachUi(
                &root, {});
            return attached.GetStatus();
        }
        Base::Result<std::uint32_t> rebound =
            runtime->bindings->Flush();
        if (!rebound) {
            runtime->DetachUi(&root, {});
            return rebound.GetStatus();
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
                Aero::Media::Visual* subtreeRoot =
                    tree->ResolveHandle(handle);
                if (subtreeRoot == nullptr) continue;
                Base::Result<void> applied =
                    ApplyUi(
                        *subtreeRoot);
                if (!applied) return applied.GetStatus();
                Base::Result<void> attached =
                    VisitAndAttach(
                        *subtreeRoot);
                if (!attached) return attached.GetStatus();
                Base::Result<std::uint32_t> reboundBeforeTriggers =
                    bindings->Flush();
                if (!reboundBeforeTriggers) {
                    return reboundBeforeTriggers.GetStatus();
                }
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

    Base::Result<void> AttachItemGenerator(
        Controls::ItemsControl& itemsControl) noexcept {
        if (::Aero::Controls::ItemsControl::Access::
                HasAttachedGenerator(itemsControl)) {
            return {};
        }
        Controls::Panel* host = itemsControl.GetItemsHost();
        if (host == nullptr) return {};

        Base::Result<Controls::ItemContainerGenerator*> created =
            ::Aero::Controls::ControlPrivate::Create(
                *tree,
                *layout,
                *values,
                styles,
                renderer,
                templates,
                &ViewState::GeneratedItemSubtreeChanged,
                this);
        if (!created) return created.GetStatus();
        Controls::ItemContainerGenerator* generator = created.Value();
        Base::Result<void> attached;
        if (metadata->Types().IsDerivedFrom(
                host->RuntimeType(),
                Controls::VirtualizingStackPanel::StaticTypeId())) {
            attached = generator->AttachVirtualized(
                itemsControl,
                *static_cast<Controls::VirtualizingStackPanel*>(host));
        } else {
            attached = generator->Attach(itemsControl, *host);
        }
        if (!attached) {
            delete generator;
            return attached.GetStatus();
        }

        Base::Result<void> generatedUiApplied = ApplyUi(*host);
        if (!generatedUiApplied) {
            DetachUi(host, {});
            static_cast<void>(generator->Detach());
            delete generator;
            return generatedUiApplied.GetStatus();
        }
        Base::Result<void> tracked = itemGenerators.PushBack(generator);
        if (!tracked) {
            static_cast<void>(generator->Detach());
            delete generator;
            return tracked.GetStatus();
        }
        return {};
    }

    Base::Result<void> AttachPendingItemGenerators(
        Aero::Media::Visual& rootVisual) noexcept {
        Base::Vector<Aero::Media::Visual*> stack(allocator);
        Base::Result<void> pushed = stack.PushBack(&rootVisual);
        if (!pushed) return pushed.GetStatus();
        while (!stack.Empty()) {
            Aero::Media::Visual* node = stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;
            if (metadata->Types().IsDerivedFrom(
                    node->RuntimeType(),
                    Controls::ItemsControl::StaticTypeId())) {
                Base::Result<void> attached = AttachItemGenerator(
                    *static_cast<Controls::ItemsControl*>(node));
                if (!attached) return attached.GetStatus();
            }
            for (Aero::Media::Visual* child :
                 Aero::ElementPrivate::VisualChildren(*node)) {
                pushed = stack.PushBack(child);
                if (!pushed) return pushed.GetStatus();
            }
        }
        return {};
    }

    void DestroyUiEngines() noexcept {
        if (tree != nullptr) tree->SetHost(nullptr);
        elementHost = {};
        FreeObject(*allocator, Base::MemoryTag::Ui, styles);
        delete visualStates;
        visualStates = nullptr;
        FreeObject(*allocator, Base::MemoryTag::Ui, templates);
    }

    Base::Result<void> VisitAndAttach(
        Aero::Media::Visual& rootVisual) noexcept {
        Base::Vector<Aero::Media::Visual*> stack(allocator);
        Base::Result<void> pushed =
            stack.PushBack(&rootVisual);
        if (!pushed) return pushed.GetStatus();
        while (!stack.Empty()) {
            Aero::Media::Visual* node = stack.Back();
            stack.PopBack();
            if (node == nullptr) continue;
            const Meta::TypeId type = node->RuntimeType();
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
                Base::Result<void> attached = AttachItemGenerator(
                    *static_cast<Controls::ItemsControl*>(node));
                if (!attached) return attached.GetStatus();
            }
            const Base::Span<Aero::Media::Visual* const>
                children = Aero::ElementPrivate::VisualChildren(*node);
            for (std::uint32_t index = 0U;
                 index < children.Size(); ++index) {
                pushed = stack.PushBack(children[index]);
                if (!pushed) return pushed.GetStatus();
            }
        }
        return {};
    }

    void ClearTextInputHosts(
        Aero::Media::Visual* node) noexcept {
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
        for (Aero::Media::Visual* child :
             Aero::ElementPrivate::VisualChildren(*node)) {
            ClearTextInputHosts(child);
        }
    }

    Base::Result<Base::StringView> AnimationAttachedString(
        MediaAnimation::Timeline& timeline,
        Meta::DependencyPropertyHandle property) noexcept {
        Base::Result<Meta::PropertyValue> value =
            timeline.GetValue(property);
        if (!value) return value.GetStatus();
        if (value.Value().Kind() != Meta::ValueKind::String) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "Storyboard attached property must be a string");
        }
        return value.Value().AsString();
    }

    struct ResolvedAnimationProperty {
        ::Aero::DependencyObject* target = nullptr;
        Meta::DependencyPropertyHandle property;
    };

    Base::Result<ResolvedAnimationProperty>
    ResolveAnimationProperty(
        ::Aero::DependencyObject& target,
        Base::StringView authoredPath) noexcept {
        // Object-model geometry uses two indexed collection hops. Resolve the
        // exact WPF path before the generic collection cases below.
        const Base::StringView figuresToken("PathGeometry.Figures");
        const Base::StringView segmentsToken("PathFigure.Segments");
        const auto findText = [](
            Base::StringView text,
            Base::StringView token) noexcept {
            for (std::uint32_t index = 0U;
                 index + token.SizeBytes() <= text.SizeBytes();
                 ++index) {
                if (text.Substr(index, token.SizeBytes()) == token) {
                    return index;
                }
            }
            return UINT32_MAX;
        };
        if (findText(authoredPath, figuresToken) != UINT32_MAX &&
            findText(authoredPath, segmentsToken) != UINT32_MAX) {
            std::uint32_t indices[2]{};
            std::uint32_t found = 0U;
            for (std::uint32_t cursor = 0U;
                 cursor < authoredPath.SizeBytes() && found < 2U;
                 ++cursor) {
                if (authoredPath[cursor] != '[') continue;
                std::uint32_t value = 0U;
                ++cursor;
                bool digit = false;
                while (cursor < authoredPath.SizeBytes() &&
                       authoredPath[cursor] != ']') {
                    if (authoredPath[cursor] < '0' ||
                        authoredPath[cursor] > '9') {
                        return Base::Status::Failure(
                            Base::ErrorCode::ValidationFailed,
                            "PathGeometry Storyboard index must be numeric");
                    }
                    digit = true;
                    value = value * 10U +
                        static_cast<std::uint32_t>(
                            authoredPath[cursor] - '0');
                    ++cursor;
                }
                if (!digit) {
                    return Base::Status::Failure(
                        Base::ErrorCode::ValidationFailed,
                        "PathGeometry Storyboard index is empty");
                }
                indices[found++] = value;
            }
            const Meta::DependencyProperty* dataProperty =
                ::Aero::MetadataPrivate::
                    DependencyProperties(*metadata).Find(
                        target.RuntimeType(), "Data");
            if (found != 2U || dataProperty == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "PathGeometry Storyboard Data property was not found");
            }
            Base::Result<Meta::PropertyValue> data =
                target.GetValue(dataProperty->Handle());
            if (!data ||
                data.Value().Kind() != Meta::ValueKind::Object ||
                !data.Value().AsObject() ||
                data.Value().AsObject()->RuntimeType() !=
                    Media::PathGeometry::StaticTypeId()) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Storyboard target Data is not a PathGeometry");
            }
            auto& geometry = static_cast<Media::PathGeometry&>(
                *data.Value().AsObject());
            const auto figures = geometry.GetFigures();
            if (indices[0] >= figures.Size() || !figures[indices[0]]) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Storyboard PathGeometry figure index is invalid");
            }
            const auto segments = figures[indices[0]]->GetSegments();
            if (indices[1] >= segments.Size() || !segments[indices[1]] ||
                segments[indices[1]]->RuntimeType() !=
                    Media::LineSegment::StaticTypeId()) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "Storyboard PathGeometry segment index is invalid");
            }
            auto* line = static_cast<Media::LineSegment*>(
                segments[indices[1]].Get());
            return ResolvedAnimationProperty{
                line, Media::LineSegment::PointProperty.Handle()};
        }

        const auto findDependencyProperty =
            [this](::Aero::DependencyObject& object,
                   Base::StringView authored) noexcept
            -> const Meta::DependencyProperty* {
            while (!authored.Empty() &&
                   (authored[0] == ' ' || authored[0] == '\t' ||
                    authored[0] == '(')) {
                authored = authored.Substr(
                    1U, authored.SizeBytes() - 1U);
            }
            while (!authored.Empty() &&
                   (authored[authored.SizeBytes() - 1U] == ' ' ||
                    authored[authored.SizeBytes() - 1U] == '\t' ||
                    authored[authored.SizeBytes() - 1U] == ')')) {
                authored = authored.Substr(
                    0U, authored.SizeBytes() - 1U);
            }
            std::uint32_t separator = UINT32_MAX;
            for (std::uint32_t index = 0U;
                 index < authored.SizeBytes(); ++index) {
                if (authored[index] == '.') separator = index;
            }
            auto& properties =
                ::Aero::MetadataPrivate::
                    DependencyProperties(*metadata);
            if (separator == UINT32_MAX) {
                return properties.Find(
                    object.RuntimeType(), authored);
            }
            Base::StringView ownerName = authored.Substr(0U, separator);
            const Base::StringView propertyName = authored.Substr(
                separator + 1U,
                authored.SizeBytes() - separator - 1U);
            // WPF owner-qualified paths may name a different owner of the
            // same dependency property (for example TextElement.Foreground
            // targeting a TextBlock). Prefer the property exposed by the
            // concrete target before falling back to a true attached owner.
            if (const Meta::DependencyProperty* targetProperty =
                    properties.Find(object.RuntimeType(), propertyName)) {
                return targetProperty;
            }
            for (std::uint32_t index = 0U;
                 index < ownerName.SizeBytes(); ++index) {
                if (ownerName[index] == ':') {
                    ownerName = ownerName.Substr(
                        index + 1U,
                        ownerName.SizeBytes() - index - 1U);
                }
            }
            for (const Meta::TypeInfo& type : metadata->Types().Types()) {
                if (type.Name() != ownerName) continue;
                const Meta::DependencyProperty* property =
                    properties.Find(type.Id(), propertyName);
                if (property != nullptr &&
                    (property->IsAttached() ||
                     metadata->Types().IsDerivedFrom(
                         object.RuntimeType(), type.Id()))) {
                    return property;
                }
            }
            return properties.Find(object.RuntimeType(), propertyName);
        };
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
                const Meta::DependencyProperty* background =
                    ::Aero::MetadataPrivate::
                        DependencyProperties(*metadata)
                            .Find(
                                target.RuntimeType(),
                                brushProperty);
                if (background == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard brush property was not found");
                }
                Base::Result<Meta::PropertyValue> value =
                    target.GetValue(background->Handle());
                if (!value ||
                    value.Value().Kind() !=
                        Meta::ValueKind::Object ||
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
                const auto stops = brush.GetGradientStops();
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
                const auto children = group.GetChildren();
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
                const std::uint32_t terminalStart =
                    separator + 3U;
                Base::StringView terminalPath =
                    path.Substr(
                        terminalStart,
                        path.SizeBytes() -
                            terminalStart - 1U);
                const Meta::DependencyProperty*
                    ownerDependency =
                        findDependencyProperty(
                            target, ownerPath);
                if (ownerDependency == nullptr) {
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        "Storyboard compound object property was not found");
                }
                Base::Result<Meta::PropertyValue>
                    ownerValue = target.GetValue(
                        ownerDependency->Handle());
                if (!ownerValue ||
                    ownerValue.Value().Kind() !=
                        Meta::ValueKind::Object ||
                    !ownerValue.Value().AsObject() ||
                    !metadata->Types().IsDerivedFrom(
                        ownerValue.Value().
                            AsObject()->RuntimeType(),
                        ::Aero::DependencyObject::
                            StaticTypeId())) {
                    thread_local char message[384];
                    const Meta::TypeInfo* targetType =
                        metadata->Types().FindType(
                            target.RuntimeType());
                    const Base::StringView targetTypeName =
                        targetType != nullptr
                        ? targetType->Name()
                        : Base::StringView("<unknown>");
                    std::snprintf(
                        message,
                        sizeof(message),
                        "Storyboard compound object property '%.*s' on '%.*s' has no DependencyObject value",
                        static_cast<int>(ownerPath.SizeBytes()),
                        ownerPath.Data(),
                        static_cast<int>(targetTypeName.SizeBytes()),
                        targetTypeName.Data());
                    return Base::Status::Failure(
                        Base::ErrorCode::NotFound,
                        message);
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

        if (!indexedPathResolved) {
            std::uint32_t dotCount = 0U;
            for (char character : path) {
                if (character == '.') ++dotCount;
            }
            if (dotCount >= 2U && path[0] != '(') {
                ::Aero::DependencyObject* currentTarget = propertyTarget;
                std::uint32_t segmentBegin = 0U;
                while (segmentBegin < path.SizeBytes()) {
                    std::uint32_t segmentEnd = segmentBegin;
                    while (segmentEnd < path.SizeBytes() &&
                           path[segmentEnd] != '.') {
                        ++segmentEnd;
                    }
                    const Base::StringView segment = path.Substr(
                        segmentBegin, segmentEnd - segmentBegin);
                    const bool terminal = segmentEnd == path.SizeBytes();
                    const Meta::DependencyProperty* segmentProperty =
                        ::Aero::MetadataPrivate::
                            DependencyProperties(*metadata)
                                .Find(currentTarget->RuntimeType(), segment);
                    if (segmentProperty == nullptr) {
                        return Base::Status::Failure(
                            Base::ErrorCode::NotFound,
                            "Storyboard object path property was not found");
                    }
                    if (terminal) {
                        return ResolvedAnimationProperty{
                            currentTarget, segmentProperty->Handle()};
                    }
                    Base::Result<Meta::PropertyValue> segmentValue =
                        currentTarget->GetValue(segmentProperty->Handle());
                    if (!segmentValue ||
                        segmentValue.Value().Kind() !=
                            Meta::ValueKind::Object ||
                        segmentValue.Value().IsNullObject() ||
                        !segmentValue.Value().AsObject() ||
                        !metadata->Types().IsDerivedFrom(
                            segmentValue.Value().AsObject()->RuntimeType(),
                            ::Aero::DependencyObject::StaticTypeId())) {
                        return Base::Status::Failure(
                            Base::ErrorCode::NotFound,
                            "Storyboard object path has no DependencyObject value");
                    }
                    currentTarget = static_cast<::Aero::DependencyObject*>(
                        segmentValue.Value().AsObject().Get());
                    segmentBegin = segmentEnd + 1U;
                }
            }
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
                const Meta::DependencyProperty*
                    ownerDependency =
                        ::Aero::MetadataPrivate::
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
                Base::Result<Meta::PropertyValue>
                    ownerValue = target.GetValue(
                        ownerDependency->Handle());
                if (!ownerValue ||
                    ownerValue.Value().Kind() !=
                        Meta::ValueKind::Object ||
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
                // Object-property chains such as Effect.Radius descend into
                // the DependencyObject value before resolving the nested
                // property. Owner-qualified direct properties such as
                // FrameworkElement.MinWidth resolve on the original target.
                const Meta::DependencyProperty* ownerDependency =
                    ::Aero::MetadataPrivate::
                            DependencyProperties(
                                *metadata)
                            .Find(
                                target.RuntimeType(),
                                ownerProperty);
                if (ownerDependency != nullptr) {
                    Base::Result<Meta::PropertyValue> ownerValue =
                        target.GetValue(
                            ownerDependency->Handle());
                    if (ownerValue &&
                        ownerValue.Value().Kind() ==
                            Meta::ValueKind::Object &&
                        ownerValue.Value().AsObject() &&
                        metadata->Types().IsDerivedFrom(
                            ownerValue.Value().
                                AsObject()->RuntimeType(),
                            ::Aero::DependencyObject::
                                StaticTypeId())) {
                        propertyTarget =
                            static_cast<
                                ::Aero::DependencyObject*>(
                                    ownerValue.Value().
                                        AsObject().Get());
                        path = nestedProperty;
                    } else {
                        path = nestedProperty;
                    }
                } else {
                    path = nestedProperty;
                }
            }
        }
        // Resolve ordinary and parenthesized object-property chains such as
        // Foreground.Color and Fill.(aero:Brush.Shader).Time.
        if (indexedOpen == UINT32_MAX) {
            ::Aero::DependencyObject* current = propertyTarget;
            std::uint32_t start = 0U;
            std::uint32_t depth = 0U;
            while (start < path.SizeBytes()) {
                std::uint32_t end = start;
                std::uint32_t parentheses = 0U;
                while (end < path.SizeBytes()) {
                    const char character = path[end];
                    if (character == '(') ++parentheses;
                    else if (character == ')' && parentheses != 0U) {
                        --parentheses;
                    } else if (character == '.' && parentheses == 0U) {
                        break;
                    }
                    ++end;
                }
                Base::StringView token = path.Substr(start, end - start);
                while (!token.Empty() &&
                       (token[0] == '(' || token[0] == ' ')) {
                    token = token.Substr(1U, token.SizeBytes() - 1U);
                }
                while (!token.Empty() &&
                       (token[token.SizeBytes() - 1U] == ')' ||
                        token[token.SizeBytes() - 1U] == ' ')) {
                    token = token.Substr(0U, token.SizeBytes() - 1U);
                }
                std::uint32_t owner = UINT32_MAX;
                for (std::uint32_t index = 0U;
                     index < token.SizeBytes(); ++index) {
                    if (token[index] == '.') owner = index;
                }
                if (owner != UINT32_MAX) {
                    token = token.Substr(
                        owner + 1U,
                        token.SizeBytes() - owner - 1U);
                }
                const Meta::DependencyProperty* dependency =
                    ::Aero::MetadataPrivate::
                        DependencyProperties(*metadata).Find(
                            current->RuntimeType(), token);
                if (dependency == nullptr) break;
                if (end >= path.SizeBytes()) {
                    return ResolvedAnimationProperty{
                        current, dependency->Handle()};
                }
                Base::Result<Meta::PropertyValue> value =
                    current->GetValue(dependency->Handle());
                if (!value ||
                    value.Value().Kind() != Meta::ValueKind::Object ||
                    !value.Value().AsObject() ||
                    !metadata->Types().IsDerivedFrom(
                        value.Value().AsObject()->RuntimeType(),
                        ::Aero::DependencyObject::StaticTypeId())) {
                    break;
                }
                current = static_cast<::Aero::DependencyObject*>(
                    value.Value().AsObject().Get());
                start = end + 1U;
                if (++depth > 16U) break;
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
        const Meta::DependencyProperty* property =
            ::Aero::MetadataPrivate::
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

    struct StoryboardTimingState {
        Aero::Media::Animation::AnimationTime beginTimeMicroseconds = 0U;
        Aero::Media::Animation::AnimationTime durationMicroseconds = 0U;
        Aero::Media::Animation::Runtime::RepeatBehavior repeat;
        double speedRatio = 1.0;
        bool hasDuration = false;
        bool hasRepeat = false;
        bool autoReverse = false;
        bool preservesChildDuration = false;
    };

    StoryboardTimingState ComposeStoryboardTiming(
        const StoryboardTimingState* inherited,
        const MediaAnimation::Timeline& storyboard,
        bool preservesChildDuration) noexcept {
        StoryboardTimingState result =
            inherited != nullptr
            ? *inherited
            : StoryboardTimingState{};
        const Aero::Media::Animation::Runtime::TimelineTiming authored =
            Aero::Media::AnimationPrivate::Timing(storyboard);
        if (UINT64_MAX - result.beginTimeMicroseconds <
            authored.beginTimeMicroseconds) {
            result.beginTimeMicroseconds = UINT64_MAX;
        } else {
            result.beginTimeMicroseconds +=
                authored.beginTimeMicroseconds;
        }
        if (!storyboard.GetDuration().Empty()) {
            result.durationMicroseconds =
                authored.durationMicroseconds;
            result.hasDuration = true;
            result.preservesChildDuration =
                preservesChildDuration;
        }
        if (!storyboard.GetRepeatBehavior().Empty()) {
            result.repeat = authored.repeat;
            result.hasRepeat = true;
        }
        result.speedRatio *= authored.speedRatio;
        result.autoReverse =
            result.autoReverse || authored.autoReverse;
        return result;
    }

    Aero::Media::Animation::Runtime::TimelineTiming EffectiveTimelineTiming(
        const MediaAnimation::Timeline& timeline,
        const StoryboardTimingState* inherited) noexcept {
        Aero::Media::Animation::Runtime::TimelineTiming result =
            Aero::Media::AnimationPrivate::Timing(timeline);
        if (inherited == nullptr) return result;
        if (UINT64_MAX - inherited->beginTimeMicroseconds <
            result.beginTimeMicroseconds) {
            result.beginTimeMicroseconds = UINT64_MAX;
        } else {
            result.beginTimeMicroseconds +=
                inherited->beginTimeMicroseconds;
        }
        if (inherited->hasDuration &&
            !inherited->preservesChildDuration) {
            result.durationMicroseconds =
                inherited->durationMicroseconds;
        } else if (inherited->hasDuration &&
                   inherited->preservesChildDuration) {
            const Aero::Media::Animation::AnimationTime childBegin =
                Aero::Media::AnimationPrivate::
                    Timing(timeline).beginTimeMicroseconds;
            const Aero::Media::Animation::AnimationTime available =
                childBegin >= inherited->durationMicroseconds
                ? 0U
                : inherited->durationMicroseconds - childBegin;
            if (result.durationMicroseconds == 0U) {
                result.durationMicroseconds = available;
                result.repeat =
                    Aero::Media::Animation::Runtime::
                        RepeatBehavior::Once();
            } else {
                const long double cycle =
                    static_cast<long double>(
                        result.durationMicroseconds) *
                    (result.autoReverse ? 2.0L : 1.0L);
                const double maximumCount =
                    cycle > 0.0L
                    ? static_cast<double>(
                        static_cast<long double>(available) /
                        cycle)
                    : 1.0;
                if (available == 0U) {
                    result.durationMicroseconds = 0U;
                    result.repeat =
                        Aero::Media::Animation::Runtime::
                            RepeatBehavior::Once();
                } else if (result.repeat.forever ||
                           result.repeat.count >
                               maximumCount) {
                    result.repeat =
                        Aero::Media::Animation::Runtime::
                            RepeatBehavior::Count(
                                std::max(
                                    maximumCount,
                                    1.0e-9));
                }
            }
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
            Aero::Media::Animation::Runtime::AnimationHandle>
            started,
        Base::Vector<
            Aero::Media::Animation::Runtime::AnimationHandle>*
            retainedHandles) noexcept {
        if (!started) {
            return started.GetStatus();
        }
        if (retainedHandles != nullptr) {
            Base::Result<void> retained =
                retainedHandles->PushBack(
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
        const Aero::NameScope* names = nullptr,
        const StoryboardTimingState* inherited = nullptr,
        Base::Vector<
            Aero::Media::Animation::Runtime::AnimationHandle>*
            retainedHandles = nullptr,
        Aero::Controls::DataTemplateTriggerState*
            dataTemplateContext = nullptr) noexcept {
        if (animations == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "Storyboard requires the animation manager");
        }
        if (metadata->Types().IsDerivedFrom(
                timeline.RuntimeType(),
                MediaAnimation::Storyboard::StaticTypeId())) {
            auto& nested =
                static_cast<MediaAnimation::Storyboard&>(timeline);
            const StoryboardTimingState timing =
                ComposeStoryboardTiming(
                    inherited,
                    nested,
                    timeline.RuntimeType() ==
                        MediaAnimation::ParallelTimeline::
                            StaticTypeId());
            std::uint32_t count = 0U;
            for (const Base::Ref<MediaAnimation::Timeline>& child :
                 nested.GetTimelines()) {
                if (!child) continue;
                Base::Result<std::uint32_t> started =
                    BeginTimeline(
                        *child, triggerOwner, names, &timing,
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
                : names != nullptr
                    ? names->Find(targetName.Value())
                    : loadedDocument.names.Find(
                          targetName.Value());
        if (targetObject == nullptr && names != nullptr) {
            targetObject = loadedDocument.names.Find(
                targetName.Value());
        }
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
        const Meta::DependencyPropertyHandle propertyHandle =
            property.Value().property;

        const Meta::TypeId type = timeline.RuntimeType();
        if (type == MediaAnimation::DoubleAnimation::StaticTypeId()) {
            auto& animation =
                static_cast<MediaAnimation::DoubleAnimation&>(timeline);
            Aero::Media::Animation::Runtime::DoubleAnimation runtime =
                Aero::Media::AnimationPrivate::Double(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<Aero::Media::Animation::Runtime::AnimationHandle> started =
                animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }
        if (metadata->Types().IsDerivedFrom(
                type,
                MediaAnimation::DoubleAnimationBase::StaticTypeId())) {
            auto& animation = static_cast<
                MediaAnimation::DoubleAnimationBase&>(timeline);
            Base::Result<Meta::PropertyValue> current =
                propertyTarget.GetValue(propertyHandle);
            if (!current) return current.GetStatus();
            Base::Result<double> origin =
                Meta::ValueCodec<double>::Decode(current.Value());
            if (!origin) return origin.GetStatus();

            Aero::Media::Animation::Runtime::CustomDoubleAnimation runtime;
            runtime.animation =
                Base::Ref<MediaAnimation::DoubleAnimationBase>::
                    TryFromBorrowed(animation);
            if (!runtime.animation) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Custom DoubleAnimation is not reference-counted");
            }
            runtime.defaultOriginValue = origin.Value();
            runtime.defaultDestinationValue =
                animation.ResolveTo(origin.Value());
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Aero::Media::Animation::Runtime::AnimationHandle>
                started = animations->Begin(
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
            Aero::Media::Animation::Runtime::ColorAnimation runtime =
                Aero::Media::AnimationPrivate::Color(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<Aero::Media::Animation::Runtime::AnimationHandle> started =
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
            Aero::Media::Animation::Runtime::PointAnimation runtime =
                Aero::Media::AnimationPrivate::Point(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Aero::Media::Animation::Runtime::AnimationHandle>
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
            Aero::Media::Animation::Runtime::RectAnimation runtime =
                Aero::Media::AnimationPrivate::Rect(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Aero::Media::Animation::Runtime::AnimationHandle>
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
            Aero::Media::Animation::Runtime::ThicknessAnimation runtime =
                Aero::Media::AnimationPrivate::Thickness(animation);
            runtime.timing =
                EffectiveTimelineTiming(
                    animation, inherited);
            Base::Result<
                Aero::Media::Animation::Runtime::AnimationHandle>
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
            Base::Vector<Aero::Media::Animation::Runtime::DoubleKeyFrame> frames(allocator);
            for (const Base::Ref<MediaAnimation::DoubleKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Base::Result<void> appended =
                    frames.PushBack(Aero::Media::AnimationPrivate::DoubleFrame(*frame));
                if (!appended) return appended.GetStatus();
            }
            for (std::uint32_t index = 1U;
                 index < frames.Size(); ++index) {
                Aero::Media::Animation::Runtime::DoubleKeyFrame current =
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
            Base::Result<Meta::PropertyValue> base =
                propertyTarget.GetValue(propertyHandle);
            if (!base) return base.GetStatus();
            Base::Result<double> baseDouble =
                Meta::ValueCodec<double>::Decode(base.Value());
            Aero::Media::Animation::Runtime::DoubleKeyFrameAnimation runtime;
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
            Base::Result<Aero::Media::Animation::Runtime::AnimationHandle> started =
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
            Base::Vector<Aero::Media::Animation::Runtime::ColorKeyFrame>
                frames(allocator);
            for (const Base::Ref<
                     MediaAnimation::ColorKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Base::Result<void> appended =
                    frames.PushBack(
                        Aero::Media::AnimationPrivate::ColorFrame(*frame));
                if (!appended) {
                    return appended.GetStatus();
                }
            }
            for (std::uint32_t index = 1U;
                 index < frames.Size();
                 ++index) {
                Aero::Media::Animation::Runtime::ColorKeyFrame current =
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
            Base::Result<Meta::PropertyValue> base =
                propertyTarget.GetValue(
                    propertyHandle);
            if (!base) return base.GetStatus();
            Base::Result<Base::Color> baseColor =
                Meta::ValueCodec<Base::Color>::Decode(
                    base.Value());
            if (!baseColor) {
                return baseColor.GetStatus();
            }
            Aero::Media::Animation::Runtime::ColorKeyFrameAnimation
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
                Aero::Media::Animation::Runtime::AnimationHandle>
                started = animations->Begin(
                    propertyTarget,
                    propertyHandle,
                    runtime);
            return RetainStartedAnimation(
                std::move(started),
                retainedHandles);
        }

        Base::Vector<Aero::Media::Animation::Runtime::DiscreteAnimationKeyFrame>
            frames(allocator);
        if (type ==
            MediaAnimation::PointAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::PointAnimationUsingKeyFrames&>(timeline);
            for (const Base::Ref<MediaAnimation::PointKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Aero::Media::Animation::Runtime::DiscreteAnimationKeyFrame runtime;
                runtime.keyTimeMicroseconds = frame->GetKeyTimeMicroseconds();
                Base::Result<Meta::PropertyValue> encoded =
                    Meta::ValueCodec<Base::Point>::Encode(frame->GetValue());
                if (!encoded) return encoded.GetStatus();
                runtime.value = std::move(encoded).Value();
                Base::Result<void> appended =
                    frames.PushBack(std::move(runtime));
                if (!appended) return appended.GetStatus();
            }
        } else         if (type ==
            MediaAnimation::ThicknessAnimationUsingKeyFrames::
                StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::ThicknessAnimationUsingKeyFrames&>(
                    timeline);
            for (const Base::Ref<
                     MediaAnimation::ThicknessKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Aero::Media::Animation::Runtime::DiscreteAnimationKeyFrame runtime;
                runtime.keyTimeMicroseconds =
                    frame->GetKeyTimeMicroseconds();
                Base::Result<Meta::PropertyValue> encoded =
                    Meta::ValueCodec<
                        Base::Thickness>::Encode(
                            frame->GetValue());
                if (!encoded) return encoded.GetStatus();
                runtime.value =
                    std::move(encoded).Value();
                Base::Result<void> appended =
                    frames.PushBack(
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
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Aero::Media::Animation::Runtime::DiscreteAnimationKeyFrame runtime;
                runtime.keyTimeMicroseconds =
                    frame->GetKeyTimeMicroseconds();
                Base::Result<Meta::PropertyValue> encoded =
                    Meta::ValueCodec<bool>::Encode(frame->GetValue());
                if (!encoded) return encoded.GetStatus();
                runtime.value = std::move(encoded).Value();
                Base::Result<void> appended =
                    frames.PushBack(std::move(runtime));
                if (!appended) return appended.GetStatus();
            }
        } else if (type ==
            MediaAnimation::ObjectAnimationUsingKeyFrames::StaticTypeId()) {
            auto& animation = static_cast<
                MediaAnimation::ObjectAnimationUsingKeyFrames&>(timeline);
            for (const Base::Ref<
                     MediaAnimation::DiscreteObjectKeyFrame>& frame :
                 animation.GetKeyFrames()) {
                if (!frame) continue;
                Aero::Media::Animation::Runtime::DiscreteAnimationKeyFrame runtime;
                runtime.keyTimeMicroseconds =
                    frame->GetKeyTimeMicroseconds();
                runtime.value = frame->GetValue();
                const Meta::DependencyProperty* targetProperty =
                    propertyTarget.PropertyRegistry().Find(
                        propertyHandle);
                if (targetProperty != nullptr &&
                    runtime.value.IsNullObject() &&
                    runtime.value.Type() !=
                        targetProperty->ValueType()) {
                    runtime.value =
                        Meta::PropertyValue::NullObject(
                            targetProperty->ValueType());
                }
                Base::Result<void> appended =
                    frames.PushBack(std::move(runtime));
                if (!appended) return appended.GetStatus();
            }
        } else {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Storyboard contains an unsupported Timeline type");
        }
        for (std::uint32_t index = 1U;
             index < frames.Size(); ++index) {
            Aero::Media::Animation::Runtime::DiscreteAnimationKeyFrame current =
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
        Base::Result<Meta::PropertyValue> base =
            propertyTarget.GetValue(propertyHandle);
        if (!base) return base.GetStatus();
        Aero::Media::Animation::Runtime::DiscreteAnimation runtime;
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
        Base::Result<Aero::Media::Animation::Runtime::AnimationHandle> started =
            animations->Begin(
                propertyTarget, propertyHandle, runtime);
        return RetainStartedAnimation(
            std::move(started),
            retainedHandles);
    }

    Base::Result<bool> DataTemplateTriggerValuesMatch(
        const Meta::PropertyValue& actual,
        Meta::PropertyValue expected) noexcept {
        if (actual.Kind() == Meta::ValueKind::Object &&
            !actual.IsNullObject() &&
            actual.AsObject() &&
            actual.AsObject()->RuntimeType() ==
                ::Aero::Controls::BoxedItemValue::StaticTypeId()) {
            return DataTemplateTriggerValuesMatch(
                static_cast<const ::Aero::Controls::BoxedItemValue&>(
                    *actual.AsObject()).Value(),
                std::move(expected));
        }
        if (expected.Kind() == Meta::ValueKind::String &&
            expected.Type() != actual.Type()) {
            if (metadata == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "DataTemplate Trigger metadata is unavailable");
            }
            Base::Result<Meta::PropertyValue> converted =
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

    Base::Object* ResolveDataTemplateConditionSource(
        Aero::Controls::DataTemplateTriggerState& context,
        Aero::Controls::DataTemplateTriggerCondition& condition,
        Base::StringView& path) noexcept {
        path = condition.binding
            ? condition.binding->GetPath().GetPath()
            : Base::StringView{};
        Base::Object* source = nullptr;
        if (condition.binding &&
            !condition.binding->GetElementName().Empty()) {
            source = context.FindName(
                condition.binding->GetElementName());
            if (source == nullptr) {
                source = loadedDocument.names.Find(
                    condition.binding->GetElementName());
            }
        } else {
            Base::Ref<Base::Object> retainedSource =
                condition.source.Lock();
            source = retainedSource.Get();
        }

        if (condition.usesDataContext && source != nullptr &&
            metadata != nullptr &&
            metadata->Types().IsDerivedFrom(
                source->RuntimeType(), FrameworkElement::StaticTypeId())) {
            Meta::Value dataContext =
                static_cast<FrameworkElement*>(source)->GetDataContext();
            source = dataContext.Kind() == Meta::ValueKind::Object &&
                    !dataContext.IsNullObject() && dataContext.AsObject()
                ? dataContext.AsObject().Get()
                : nullptr;
        }

        constexpr Base::StringView TemplatedParentPrefix(
            "TemplatedParent.");
        if (source != nullptr &&
            path.SizeBytes() > TemplatedParentPrefix.SizeBytes() &&
            path.Substr(0U, TemplatedParentPrefix.SizeBytes()) ==
                TemplatedParentPrefix &&
            metadata != nullptr &&
            metadata->Types().IsDerivedFrom(
                source->RuntimeType(), FrameworkElement::StaticTypeId())) {
            source = static_cast<FrameworkElement*>(source)->GetTemplatedParent();
            path = path.Substr(
                TemplatedParentPrefix.SizeBytes(),
                path.SizeBytes() - TemplatedParentPrefix.SizeBytes());
        }
        return source;
    }

    Base::Result<bool> EvaluateDataTemplateCondition(
        Aero::Controls::DataTemplateTriggerState& context,
        Aero::Controls::DataTemplateTriggerCondition& condition) noexcept {
        Meta::PropertyValue current;
        Base::Ref<DependencyObject> dependencySource =
            condition.dependencySource.Lock();
        if (dependencySource &&
            condition.property.IsValid()) {
            Base::Result<Meta::PropertyValue> value =
                dependencySource->GetValue(condition.property);
            if (!value) return value.GetStatus();
            current = std::move(value).Value();
        } else {
            if (!condition.binding || metadata == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "DataTemplate DataTrigger Binding is unavailable");
            }
            Base::StringView path;
            Base::Object* source =
                ResolveDataTemplateConditionSource(
                    context, condition, path);
            if (source == nullptr) {
                return false;
            }
            Base::Result<Meta::BindingPathPlan> plan =
                Meta::BindingPathPlan::Compile(
                    *metadata,
                    source->RuntimeType(),
                    path);
            if (!plan) return plan.GetStatus();
            Base::Result<Meta::PropertyValue> value =
                plan.Value().Get(*metadata, *source);
            if (!value) return value.GetStatus();
            current = std::move(value).Value();
        }
        return DataTemplateTriggerValuesMatch(
            current, condition.value);
    }

    Base::Result<void> EnsureDataTemplateProviderTokens(
        Aero::Controls::DataTemplateTriggerState& context) noexcept {
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
        for (Aero::Controls::DataTemplatePropertyTrigger& trigger :
             context.triggers) {
            for (Aero::Controls::DataTemplateTriggerSetter& setter :
                 trigger.setters) {
                if (ordinal > UINT32_MAX) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "DataTemplate Trigger setter ordinal limit reached");
                }
                const Meta::PropertyProviderToken expected{
                    Meta::PropertyValueRank::TemplateTrigger,
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
        Aero::Controls::DataTemplateTriggerState& context,
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

        Aero::Controls::DataTemplatePropertyTrigger& trigger =
            context.triggers[triggerIndex];
        bool active = !trigger.conditions.Empty();
        for (Aero::Controls::DataTemplateTriggerCondition& condition :
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
            for (const Aero::Controls::DataTemplateTriggerSetter& setter :
                 trigger.setters) {
                Base::Ref<DependencyObject> target =
                    setter.target.Lock();
                if (!target) continue;
                Base::Result<void> applied =
                    values->SetProviderContribution(
                        *target,
                        setter.property,
                        setter.token,
                        setter.value);
                if (!applied) {
                    return applied.GetStatus();
                }
            }
        } else {
            for (const Aero::Controls::DataTemplateTriggerSetter& setter :
                 trigger.setters) {
                Base::Ref<DependencyObject> target =
                    setter.target.Lock();
                if (!target) continue;
                Base::Result<bool> cleared =
                    values->ClearProviderContribution(
                        *target,
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

    Base::Result<bool> StyleDataTriggerValuesMatch(
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

    Base::Result<void> EvaluateStyleDataTrigger(
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

    void ClearStyleDataTriggersFor(
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

    Base::Result<std::uint32_t> StartStyleDataTriggers(
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
                    Aero::Media::Visual* current = target.GetLogicalParent();
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
                        Aero::Media::Visual* next = current->GetLogicalParent();
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

    Base::Result<std::uint32_t>
    StartDataTemplateTriggers(
        Aero::Controls::DataTemplateTriggerState&
            context) noexcept {
        std::uint32_t count = 0U;
        for (std::uint32_t triggerIndex = 0U;
             triggerIndex < context.triggers.Size();
             ++triggerIndex) {
            Aero::Controls::DataTemplatePropertyTrigger&
                trigger =
                    context.triggers[triggerIndex];
            for (std::uint32_t conditionIndex = 0U;
                 conditionIndex <
                     trigger.conditions.Size();
                 ++conditionIndex) {
                Aero::Controls::DataTemplateTriggerCondition&
                    condition =
                        trigger.conditions[conditionIndex];
                Base::Ref<DependencyObject> dependencySource =
                    condition.dependencySource.Lock();
                if ((!dependencySource ||
                     !condition.property.IsValid()) &&
                    condition.binding) {
                    Base::StringView path;
                    Base::Object* source =
                        ResolveDataTemplateConditionSource(
                            context, condition, path);
                    if (source != nullptr &&
                        metadata->Types().IsDerivedFrom(
                            source->RuntimeType(),
                            ::Aero::DependencyObject::
                                StaticTypeId())) {
                        const Meta::DependencyProperty*
                            property =
                                ::Aero::MetadataPrivate::
                                    DependencyProperties(
                                        *metadata)
                                        .Find(
                                            source->
                                                RuntimeType(),
                                            path);
                        if (property != nullptr) {
                            dependencySource =
                                Base::Ref<DependencyObject>::FromBorrowed(
                                    *static_cast<DependencyObject*>(source));
                            condition.dependencySource =
                                Base::WeakRef<DependencyObject>(
                                    dependencySource);
                            condition.property = property->Handle();
                        }
                    }
                }
                if (!dependencySource ||
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
                        Aero::Controls::DataTemplateTriggerState>::
                        FromBorrowed(context);
                handlerContext->triggerIndex =
                    triggerIndex;
                handlerContext->conditionIndex =
                    conditionIndex;
                auto callback =
                    [handlerContext](
                        ::Aero::DependencyObject& object,
                        const Meta::
                            DependencyPropertyChangedEventArgs&
                                args) noexcept {
                        handlerContext->Invoke(
                            object, args);
                    };
                Meta::DependencyPropertyChangedEventHandler
                    handler(callback);
                Base::Result<void> subscribed =
                    dependencySource->AddValueChangedHandlerChecked(
                        condition.property, handler);
                if (!subscribed) {
                    FreeObject(
                        *allocator,
                        Base::MemoryTag::Ui,
                        handlerContext);
                    return subscribed.GetStatus();
                }
                DataTemplateTriggerSubscription record;
                record.source = dependencySource.Get();
                record.property = condition.property;
                record.handler = handler;
                record.context = handlerContext;
                Base::Result<void> retained =
                    dataTemplateTriggerSubscriptions.
                        PushBack(std::move(record));
                if (!retained) {
                    static_cast<void>(
                        dependencySource->RemoveValueChangedHandler(
                            condition.property, handler));
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

    Base::Result<bool> StartEventTrigger(
        MediaAnimation::EventTrigger& trigger,
        Base::Object& defaultSource,
        Aero::FrameworkElement& actionOwner,
        const Aero::NameScope* names) noexcept {
        const Base::StringView routedEvent =
            trigger.GetRoutedEvent();
        Base::Object* eventSource =
            trigger.GetSourceName().Empty()
            ? &defaultSource
            : names != nullptr
                ? names->Find(trigger.GetSourceName())
                : loadedDocument.names.Find(
                      trigger.GetSourceName());
        if (eventSource == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "EventTrigger SourceName was not found");
        }
        // Microsoft.Xaml.Behaviors EventTrigger defaults EventName to
        // Loaded. Several reference samples intentionally omit EventName
        // to request that startup behavior.
        Base::StringView eventName = routedEvent.Empty()
            ? Base::StringView("Loaded")
            : routedEvent;
        std::uint32_t dot = UINT32_MAX;
        for (std::uint32_t index = 0U;
             index < eventName.SizeBytes(); ++index) {
            if (eventName[index] == '.') dot = index;
        }
        Base::StringView eventOwnerName;
        if (dot != UINT32_MAX) {
            eventOwnerName = eventName.Substr(0U, dot);
            eventName = eventName.Substr(
                dot + 1U,
                eventName.SizeBytes() - dot - 1U);
        }
        if (eventName == Base::StringView("Loaded")) {
            for (const Base::Ref<MediaAnimation::TriggerAction>& action :
                 trigger.GetActions()) {
                if (!action) continue;
                Base::Result<void> executed =
                    ExecuteAnimationAction(
                        *action, actionOwner, nullptr, names);
                if (!executed) return executed.GetStatus();
            }
            return true;
        }
        // WPF's GotFocus is the logical-focus counterpart of Aero's
        // keyboard-focus event. Preserve the authored behavior trigger while
        // routing it through the focus event exposed by the runtime.
        if (eventName == Base::StringView("GotFocus")) {
            eventName = Base::StringView("GotKeyboardFocus");
        }

        const bool uiSource = metadata->Types().IsDerivedFrom(
            eventSource->RuntimeType(), Aero::UIElement::StaticTypeId());
        const bool contentSource = metadata->Types().IsDerivedFrom(
            eventSource->RuntimeType(), Aero::ContentElement::StaticTypeId());
        if (!uiSource && !contentSource) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "EventTrigger source does not support routed events");
        }
        const Meta::EventInfo* event = nullptr;
        if (!eventOwnerName.Empty()) {
            Base::StringView ownerName = eventOwnerName;
            for (std::uint32_t index = 0U;
                 index < ownerName.SizeBytes(); ++index) {
                if (ownerName[index] == ':') {
                    ownerName = ownerName.Substr(
                        index + 1U,
                        ownerName.SizeBytes() - index - 1U);
                }
            }
            for (const Meta::TypeInfo& type :
                 metadata->Types().Types()) {
                if (type.Name() != ownerName) continue;
                event = metadata->Types().FindEvent(
                    type.Id(), eventName, true);
                if (event != nullptr) break;
            }
        } else {
            event = metadata->Types().FindEvent(
                eventSource->RuntimeType(), eventName, true);
        }
        if (event == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "EventTrigger RoutedEvent was not found on its source");
        }
        const Aero::RoutedEventHandle eventHandle{event->Id()};
        AnimationEventState* eventContext = nullptr;
        Base::Result<void> created = AllocateObject(
            *allocator, Base::MemoryTag::Ui, eventContext);
        if (!created) return created.GetStatus();
        eventContext->runtime = this;
        eventContext->trigger = &trigger;
        eventContext->owner = &actionOwner;
        eventContext->names = names;
        auto callback = [eventContext](
            Base::Object* sender,
            Aero::RoutedEventArgs& args) noexcept {
            eventContext->Invoke(sender, args);
        };
        Aero::RoutedEventHandler handler(callback);
        Base::Result<void> subscribed = uiSource
            ? static_cast<Aero::UIElement*>(eventSource)->AddHandlerChecked(
                  eventHandle, handler)
            : static_cast<Aero::ContentElement*>(eventSource)->AddHandlerChecked(
                  eventHandle, handler);
        if (!subscribed) {
            FreeObject(
                *allocator, Base::MemoryTag::Ui, eventContext);
            return subscribed.GetStatus();
        }
        AnimationEventSubscription subscription;
        subscription.source = eventSource;
        subscription.visualOwner = &actionOwner;
        subscription.event = eventHandle;
        subscription.handler = handler;
        subscription.context = eventContext;
        subscription.contentSource = contentSource;
        Base::Result<void> retained =
            animationEventSubscriptions.PushBack(
                std::move(subscription));
        if (!retained) {
            if (contentSource) {
                static_cast<void>(
                    static_cast<Aero::ContentElement*>(eventSource)
                        ->RemoveHandler(eventHandle, handler));
            } else {
                static_cast<void>(
                    static_cast<Aero::UIElement*>(eventSource)
                        ->RemoveHandler(eventHandle, handler));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui, eventContext);
            return retained.GetStatus();
        }
        return true;
    }

    Base::Result<std::uint32_t> StartContentElementAnimations(
        Aero::FrameworkContentElement& content,
        Aero::FrameworkElement& actionOwner,
        const Aero::NameScope* names) noexcept {
        std::uint32_t count = 0U;
        for (const Base::Ref<Base::Object>& authored :
             Aero::ElementPrivate::AuthoredTriggers(
                 content)) {
            if (!authored || authored->RuntimeType() !=
                    MediaAnimation::EventTrigger::StaticTypeId()) {
                continue;
            }
            Base::Result<bool> started = StartEventTrigger(
                static_cast<MediaAnimation::EventTrigger&>(*authored),
                content,
                actionOwner,
                names);
            if (!started) return started.GetStatus();
            if (started.Value()) ++count;
        }
        if (metadata->Types().IsDerivedFrom(
                content.RuntimeType(),
                Documents::Span::StaticTypeId())) {
            const Documents::InlineCollectionView inlines =
                static_cast<const Documents::Span&>(content).GetInlines();
            for (std::uint32_t index = 0U;
                 index < inlines.GetCount(); ++index) {
                const Documents::Inline* child = inlines.GetItem(index);
                if (child == nullptr) continue;
                Base::Result<std::uint32_t> nested =
                    StartContentElementAnimations(
                        const_cast<Documents::Inline&>(*child),
                        actionOwner,
                        names);
                if (!nested) return nested.GetStatus();
                if (count > UINT32_MAX - nested.Value()) {
                    return Base::Status::Failure(
                        Base::ErrorCode::OutOfRange,
                        "Content trigger count overflow");
                }
                count += nested.Value();
            }
        }
        return count;
    }

    Base::Result<Base::Ref<Interactivity::Behavior>>
    CloneBehaviorPrototype(
        const Interactivity::Behavior& prototype) noexcept {
        if (metadata == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Behavior metadata is unavailable");
        }
        Base::Result<Base::Ref<Base::Object>> created =
            metadata->CreateObject(prototype.RuntimeType());
        if (!created) return created.GetStatus();
        if (!created.Value() ||
            !metadata->Types().IsDerivedFrom(
                created.Value()->RuntimeType(),
                Interactivity::Behavior::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Behavior factory returned an incompatible object");
        }
        Base::Ref<Interactivity::Behavior> clone =
            Base::Ref<Interactivity::Behavior>::FromBorrowed(
                *static_cast<Interactivity::Behavior*>(
                    created.Value().Get()));
        for (const Meta::DependencyProperty& property :
             prototype.PropertyRegistry().Properties()) {
            if (property.MetadataFor(prototype.RuntimeType()) == nullptr ||
                property.MetadataFor(clone->RuntimeType()) == nullptr) {
                continue;
            }
            Meta::PropertyValue local =
                prototype.ReadLocalValue(property.Handle());
            if (local.IsUnset()) continue;
            Base::Result<void> copied = clone->SetValueChecked(
                property.Handle(), local);
            if (!copied) return copied.GetStatus();
        }
        Base::Result<void> bindingsCopied =
            prototype.CopyAuthoredBindingsTo(*clone);
        if (!bindingsCopied) return bindingsCopied.GetStatus();
        return clone;
    }

    Base::Object* ResolveBehaviorBindingSource(
        const Data::Binding& binding,
        Interactivity::Behavior& behavior,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        if (binding.GetSource()) return binding.GetSource().Get();
        if (!binding.GetElementName().Empty()) {
            Base::Object* source = owner.FindName(
                binding.GetElementName());
            if (source == nullptr && names != nullptr) {
                source = names->Find(binding.GetElementName());
            }
            if (source == nullptr) {
                source = loadedDocument.names.Find(
                    binding.GetElementName());
            }
            return source;
        }
        const Base::Ref<Data::RelativeSource> relative =
            binding.GetRelativeSource();
        if (!relative) return nullptr;
        if (relative->GetMode() == Data::RelativeSourceMode::Self) {
            return &behavior;
        }
        if (relative->GetMode() ==
            Data::RelativeSourceMode::TemplatedParent) {
            return owner.GetTemplatedParent();
        }
        if (relative->GetMode() !=
            Data::RelativeSourceMode::FindAncestor) {
            return nullptr;
        }
        Base::StringView ancestorName = relative->GetAncestorType();
        for (std::uint32_t index = 0U;
             index < ancestorName.SizeBytes(); ++index) {
            if (ancestorName[index] == ':') {
                ancestorName = ancestorName.Substr(
                    index + 1U,
                    ancestorName.SizeBytes() - index - 1U);
                break;
            }
        }
        std::uint32_t matched = 0U;
        Aero::Media::Visual* current = owner.GetLogicalParent();
        if (current == nullptr) current = owner.GetVisualParent();
        while (current != nullptr) {
            const Meta::TypeInfo* type =
                metadata->Types().FindType(current->RuntimeType());
            const bool matches = ancestorName.Empty() ||
                (type != nullptr && type->Name() == ancestorName);
            if (matches && ++matched == relative->GetAncestorLevel()) {
                return current;
            }
            Aero::Media::Visual* next = current->GetLogicalParent();
            if (next == nullptr) next = current->GetVisualParent();
            current = next;
        }
        return nullptr;
    }

    Base::Object* ResolveAuthoredBindingSource(
        const Data::Binding& binding,
        Aero::FrameworkElement& owner,
        Aero::Controls::DataTemplateTriggerState*
            dataTemplateContext,
        const Aero::NameScope* names,
        Base::Object* self) noexcept {
        if (binding.GetSource()) {
            return binding.GetSource().Get();
        }
        if (!binding.GetElementName().Empty()) {
            Base::Object* source = dataTemplateContext != nullptr
                ? dataTemplateContext->FindName(binding.GetElementName())
                : nullptr;
            if (source == nullptr) {
                source = owner.FindName(binding.GetElementName());
            }
            if (source == nullptr && names != nullptr) {
                source = names->Find(binding.GetElementName());
            }
            if (source == nullptr) {
                source = loadedDocument.names.Find(
                    binding.GetElementName());
            }
            return source;
        }

        const Base::Ref<Data::RelativeSource> relative =
            binding.GetRelativeSource();
        if (relative) {
            if (relative->GetMode() ==
                Data::RelativeSourceMode::Self) {
                return self != nullptr
                    ? self
                    : static_cast<Base::Object*>(&owner);
            }
            if (relative->GetMode() ==
                Data::RelativeSourceMode::TemplatedParent) {
                return owner.GetTemplatedParent();
            }
            if (relative->GetMode() !=
                Data::RelativeSourceMode::FindAncestor) {
                return nullptr;
            }
            Base::StringView ancestorName =
                relative->GetAncestorType();
            for (std::uint32_t index = 0U;
                 index < ancestorName.SizeBytes(); ++index) {
                if (ancestorName[index] != ':') continue;
                ancestorName = ancestorName.Substr(
                    index + 1U,
                    ancestorName.SizeBytes() - index - 1U);
                break;
            }
            std::uint32_t matched = 0U;
            Aero::Media::Visual* current = owner.GetLogicalParent();
            if (current == nullptr) {
                current = owner.GetVisualParent();
            }
            while (current != nullptr) {
                const Meta::TypeInfo* type =
                    metadata != nullptr
                    ? metadata->Types().FindType(
                          current->RuntimeType())
                    : nullptr;
                const bool matches = ancestorName.Empty() ||
                    (type != nullptr &&
                     type->Name() == ancestorName);
                if (matches &&
                    ++matched == relative->GetAncestorLevel()) {
                    return current;
                }
                Aero::Media::Visual* next = current->GetLogicalParent();
                if (next == nullptr) {
                    next = current->GetVisualParent();
                }
                current = next;
            }
            return nullptr;
        }

        Meta::PropertyValue dataContext = owner.GetDataContext();
        if (dataContext.Kind() != Meta::ValueKind::Object ||
            dataContext.IsNullObject() ||
            !dataContext.AsObject()) {
            return nullptr;
        }
        return dataContext.AsObject().Get();
    }

    Base::Result<Meta::PropertyValue> EvaluateAuthoredBinding(
        const Data::Binding& binding,
        Aero::FrameworkElement& owner,
        Aero::Controls::DataTemplateTriggerState*
            dataTemplateContext,
        const Aero::NameScope* names,
        Base::Object* self) noexcept {
        if (metadata == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Authored Binding metadata is unavailable");
        }
        Base::Object* source = ResolveAuthoredBindingSource(
            binding,
            owner,
            dataTemplateContext,
            names,
            self);
        if (source == nullptr) {
            if (!binding.GetFallbackValue().IsUnset()) {
                return binding.GetFallbackValue();
            }
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Authored Binding source was not found");
        }

        Base::Result<Meta::PropertyValue> value =
            Meta::PropertyValue::FromObject(
                source->RuntimeType(),
                Base::Ref<Base::Object>::FromBorrowed(*source));
        const Base::StringView path = binding.GetPath().GetPath();
        if (!path.Empty()) {
            Meta::BindingPathCompileError pathError;
            Base::Result<Meta::BindingPathPlan> plan =
                Meta::BindingPathPlan::Compile(
                    *metadata,
                    source->RuntimeType(),
                    path,
                    &pathError);
            if (plan) {
                value = plan.Value().Get(*metadata, *source);
            } else {
                value = plan.GetStatus();
            }
        }
        if (!value) {
            if (!binding.GetFallbackValue().IsUnset()) {
                return binding.GetFallbackValue();
            }
            return value.GetStatus();
        }

        Meta::PropertyValue resolved = value.Value();
        if (resolved.Kind() == Meta::ValueKind::Object &&
            !resolved.IsNullObject() && resolved.AsObject() &&
            resolved.AsObject()->RuntimeType() ==
                Controls::BoxedItemValue::StaticTypeId()) {
            resolved = static_cast<const Controls::BoxedItemValue&>(
                *resolved.AsObject()).Value();
        }
        if (resolved.IsNullObject() &&
            !binding.GetTargetNullValue().IsUnset()) {
            resolved = binding.GetTargetNullValue();
        }
        if (binding.GetConverter()) {
            Base::Result<Meta::PropertyValue> converted =
                binding.GetConverter()->Convert(
                    resolved,
                    binding.GetConverterParameter());
            if (!converted) {
                if (!binding.GetFallbackValue().IsUnset()) {
                    return binding.GetFallbackValue();
                }
                return converted.GetStatus();
            }
            resolved = std::move(converted).Value();
        }
        return resolved;
    }

    Base::Result<void> ExecuteTriggerActions(
        Base::Span<const Base::Ref<Base::Object>> actions,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        for (const Base::Ref<Base::Object>& authored : actions) {
            if (!authored || metadata == nullptr ||
                !metadata->Types().IsDerivedFrom(
                    authored->RuntimeType(),
                    MediaAnimation::TriggerAction::StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "Interaction Trigger contains an invalid action");
            }
            Base::Result<void> executed = ExecuteAnimationAction(
                static_cast<MediaAnimation::TriggerAction&>(*authored),
                owner,
                nullptr,
                names);
            if (!executed) return executed.GetStatus();
        }
        return {};
    }

    Base::Result<void> ExecuteTriggerActions(
        Base::Span<const Base::Ref<MediaAnimation::TriggerAction>> actions,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        for (const Base::Ref<MediaAnimation::TriggerAction>& action :
             actions) {
            if (!action) continue;
            Base::Result<void> executed = ExecuteAnimationAction(
                *action, owner, nullptr, names);
            if (!executed) return executed.GetStatus();
        }
        return {};
    }

    struct InteractionTriggerProperty {
        Base::Object* source = nullptr;
        ::Aero::DependencyObject* dependencySource = nullptr;
        Meta::DependencyPropertyHandle dependencyProperty;
        Meta::MemberId metadataProperty = Meta::InvalidMemberId;
    };

    Base::Result<InteractionTriggerProperty>
    ResolveInteractionTriggerProperty(
        const Data::Binding& binding,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        Base::Object* sourceObject = ResolveAuthoredBindingSource(
            binding, owner, nullptr, names, nullptr);
        if (sourceObject == nullptr || metadata == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Interaction Trigger Binding source was not found");
        }
        const Base::StringView path = binding.GetPath().GetPath();
        if (path.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Interaction Trigger Binding requires a property path");
        }

        InteractionTriggerProperty resolved;
        resolved.source = sourceObject;
        if (metadata->Types().IsDerivedFrom(
                sourceObject->RuntimeType(),
                ::Aero::DependencyObject::StaticTypeId())) {
            resolved.dependencySource =
                static_cast<::Aero::DependencyObject*>(sourceObject);
            const Meta::DependencyProperty* property =
                Aero::MetadataPrivate::DependencyProperties(
                    *metadata).Find(sourceObject->RuntimeType(), path);
            if (property == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Interaction Trigger Binding property was not found");
            }
            resolved.dependencyProperty = property->Handle();
            return resolved;
        }

        Base::StringView rootPath = path;
        for (std::uint32_t index = 0U; index < path.SizeBytes(); ++index) {
            if (path[index] == '.') {
                rootPath = path.Substr(0U, index);
                break;
            }
        }
        const Meta::PropertyInfo* property = metadata->Types().FindProperty(
            sourceObject->RuntimeType(), rootPath, true);
        if (property == nullptr ||
            !metadata->CanReadProperty(property->Id())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Interaction Trigger Binding property was not found");
        }
        resolved.metadataProperty = property->Id();
        return resolved;
    }

    Base::Result<bool> EvaluateInteractionDataTrigger(
        InteractionDataTriggerState& state) noexcept {
        if (state.trigger == nullptr || state.owner == nullptr ||
            !state.trigger->GetBinding()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Interaction DataTrigger state is invalid");
        }
        Base::Result<Meta::PropertyValue> actual =
            EvaluateAuthoredBinding(
                *state.trigger->GetBinding(),
                *state.owner,
                nullptr,
                state.names,
                nullptr);
        if (!actual) return actual.GetStatus();
        Base::Result<bool> matches = DataTemplateTriggerValuesMatch(
            actual.Value(), state.trigger->GetAuthoredValue());
        if (!matches) return matches.GetStatus();
        const bool active = matches.Value();
        if (active == state.active) return false;
        Base::Result<bool> allowed = ConditionBehaviorsAllowExecution(
            state.trigger->GetBehaviors(), *state.owner, state.names);
        if (!allowed) return allowed.GetStatus();
        if (allowed.Value()) {
            Base::Result<void> executed = ExecuteTriggerActions(
                active
                    ? state.trigger->GetEnterActions()
                    : state.trigger->GetExitActions(),
                *state.owner,
                state.names);
            if (!executed) return executed.GetStatus();
        }
        state.active = active;
        return true;
    }

    Base::Result<bool> StartPropertyChangedTrigger(
        MediaAnimation::PropertyChangedTrigger& trigger,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        if (!trigger.GetBinding()) return false;
        for (const PropertyChangedTriggerSubscription& existing :
             propertyChangedTriggerSubscriptions) {
            if (existing.owner == &owner && existing.context != nullptr &&
                existing.context->trigger == &trigger) {
                return false;
            }
        }
Base::Result<InteractionTriggerProperty> property =
            ResolveInteractionTriggerProperty(
                *trigger.GetBinding(), owner, names);
        if (property.GetStatus().code == Base::ErrorCode::NotFound) {
            return false;
        }
        if (!property) return property.GetStatus();
        PropertyChangedTriggerState* context = nullptr;
        Base::Result<void> allocated = AllocateObject(
            *allocator, Base::MemoryTag::Ui, context);
        if (!allocated) return allocated.GetStatus();
        context->runtime = this;
        context->trigger = &trigger;
        context->owner = &owner;
        context->names = names;
        context->metadataProperty = property.Value().metadataProperty;
        Meta::DependencyPropertyChangedEventHandler handler(
            [context](
                ::Aero::DependencyObject& object,
                const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
                    context->Invoke(object, args);
                });
        std::uint64_t metadataSubscription = 0U;
        Base::Result<void> subscribed;
        if (property.Value().dependencySource != nullptr) {
            subscribed = property.Value().dependencySource
                ->AddValueChangedHandlerChecked(
                    property.Value().dependencyProperty, handler);
        } else {
            Base::Result<std::uint64_t> notification =
                metadata->SubscribePropertyChanged(
                    *property.Value().source,
                    &PropertyChangedTriggerState::MetadataInvoke,
                    context);
            if (notification) {
                metadataSubscription = notification.Value();
            } else {
                subscribed = notification.GetStatus();
            }
        }
        if (!subscribed) {
            FreeObject(
                *allocator, Base::MemoryTag::Ui, context);
            return subscribed.GetStatus();
        }
        PropertyChangedTriggerSubscription subscription;
        subscription.owner = &owner;
        subscription.source = property.Value().dependencySource;
        subscription.metadataSource = property.Value().dependencySource == nullptr
            ? property.Value().source : nullptr;
        subscription.property = property.Value().dependencyProperty;
        subscription.metadataSubscription = metadataSubscription;
        subscription.handler = handler;
        subscription.context = context;
        Base::Result<void> retained =
            propertyChangedTriggerSubscriptions.PushBack(
                std::move(subscription));
        if (!retained) {
            if (property.Value().dependencySource != nullptr) {
                static_cast<void>(property.Value().dependencySource
                    ->RemoveValueChangedHandler(
                        property.Value().dependencyProperty, handler));
            } else if (metadataSubscription != 0U) {
                static_cast<void>(metadata->UnsubscribePropertyChanged(
                    *property.Value().source, metadataSubscription));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui, context);
            return retained.GetStatus();
        }
        return true;
    }

    Base::Result<bool> StartInteractionDataTrigger(
        Aero::DataTrigger& trigger,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        if (!trigger.GetBinding()) return false;
        for (const InteractionDataTriggerSubscription& existing :
             interactionDataTriggerSubscriptions) {
            if (existing.owner == &owner && existing.context != nullptr &&
                existing.context->trigger == &trigger) {
                return false;
            }
        }
        Base::Result<InteractionTriggerProperty> property =
            ResolveInteractionTriggerProperty(
                *trigger.GetBinding(), owner, names);
        if (property.GetStatus().code == Base::ErrorCode::NotFound) {
            return false;
        }
        if (!property) return property.GetStatus();
        InteractionDataTriggerState* context = nullptr;
        Base::Result<void> allocated = AllocateObject(
            *allocator, Base::MemoryTag::Ui, context);
        if (!allocated) return allocated.GetStatus();
        context->runtime = this;
        context->trigger = &trigger;
        context->owner = &owner;
        context->names = names;
        context->metadataProperty = property.Value().metadataProperty;
        Meta::DependencyPropertyChangedEventHandler handler(
            [context](
                ::Aero::DependencyObject& object,
                const Meta::DependencyPropertyChangedEventArgs& args) noexcept {
                    context->Invoke(object, args);
                });
        std::uint64_t metadataSubscription = 0U;
        Base::Result<void> subscribed;
        if (property.Value().dependencySource != nullptr) {
            subscribed = property.Value().dependencySource
                ->AddValueChangedHandlerChecked(
                    property.Value().dependencyProperty, handler);
        } else {
            Base::Result<std::uint64_t> notification =
                metadata->SubscribePropertyChanged(
                    *property.Value().source,
                    &InteractionDataTriggerState::MetadataInvoke,
                    context);
            if (notification) {
                metadataSubscription = notification.Value();
            } else {
                subscribed = notification.GetStatus();
            }
        }
        if (!subscribed) {
            FreeObject(
                *allocator, Base::MemoryTag::Ui, context);
            return subscribed.GetStatus();
        }
        InteractionDataTriggerSubscription subscription;
        subscription.owner = &owner;
        subscription.source = property.Value().dependencySource;
        subscription.metadataSource = property.Value().dependencySource == nullptr
            ? property.Value().source : nullptr;
        subscription.property = property.Value().dependencyProperty;
        subscription.metadataSubscription = metadataSubscription;
        subscription.handler = handler;
        subscription.context = context;
        Base::Result<void> retained =
            interactionDataTriggerSubscriptions.PushBack(
                std::move(subscription));
        if (!retained) {
            if (property.Value().dependencySource != nullptr) {
                static_cast<void>(property.Value().dependencySource
                    ->RemoveValueChangedHandler(
                        property.Value().dependencyProperty, handler));
            } else if (metadataSubscription != 0U) {
                static_cast<void>(metadata->UnsubscribePropertyChanged(
                    *property.Value().source, metadataSubscription));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui, context);
            return retained.GetStatus();
        }
        Base::Result<bool> evaluated =
            EvaluateInteractionDataTrigger(*context);
        if (!evaluated) return evaluated.GetStatus();
        return true;
    }

    static std::uint32_t KeyCodeFromName(
        Base::StringView key) noexcept {
        if (Base::Detail::ValueConversion::EqualsAsciiInsensitive(
                key, "Enter") ||
            Base::Detail::ValueConversion::EqualsAsciiInsensitive(
                key, "Return")) {
            return Input::KeyboardKeyEnter;
        }
        if (Base::Detail::ValueConversion::EqualsAsciiInsensitive(
                key, "Space")) {
            return Input::KeyboardKeySpace;
        }
        if (Base::Detail::ValueConversion::EqualsAsciiInsensitive(
                key, "Escape")) {
            return Input::KeyboardKeyEscape;
        }
        if (Base::Detail::ValueConversion::EqualsAsciiInsensitive(
                key, "Tab")) {
            return Input::KeyboardKeyTab;
        }
        if (Base::Detail::ValueConversion::EqualsAsciiInsensitive(
                key, "Left")) {
            return Input::KeyboardKeyLeft;
        }
        if (Base::Detail::ValueConversion::EqualsAsciiInsensitive(
                key, "Right")) {
            return Input::KeyboardKeyRight;
        }
        if (Base::Detail::ValueConversion::EqualsAsciiInsensitive(
                key, "Up")) {
            return Input::KeyboardKeyUp;
        }
        if (Base::Detail::ValueConversion::EqualsAsciiInsensitive(
                key, "Down")) {
            return Input::KeyboardKeyDown;
        }
        return 0U;
    }

    Base::Result<bool> StartKeyTrigger(
        MediaAnimation::KeyTrigger& trigger,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names) noexcept {
        Aero::UIElement* source = owner.AsUIElement();
        if (source == nullptr) return false;
        if (KeyCodeFromName(trigger.GetKey()) == 0U) {
            return Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "KeyTrigger Key is not supported");
        }
        for (const KeyTriggerSubscription& existing :
             keyTriggerSubscriptions) {
            if (existing.owner == &owner && existing.context != nullptr &&
                existing.context->trigger == &trigger) {
                return false;
            }
        }
        KeyTriggerState* context = nullptr;
        Base::Result<void> allocated = AllocateObject(
            *allocator, Base::MemoryTag::Ui, context);
        if (!allocated) return allocated.GetStatus();
        context->runtime = this;
        context->trigger = &trigger;
        context->owner = &owner;
        context->names = names;
        Aero::KeyEventHandler handler(
            [context](Base::Object* sender, Aero::KeyEventArgs& args) noexcept {
                context->Invoke(sender, args);
            });
        Base::Result<void> subscribed = source->AddHandlerChecked(
            Aero::UIElement::KeyDownEvent.Handle(), handler);
        if (!subscribed) {
            FreeObject(
                *allocator, Base::MemoryTag::Ui, context);
            return subscribed.GetStatus();
        }
        Base::Result<void> retained = keyTriggerSubscriptions.PushBack({
            &owner, source, handler, context});
        if (!retained) {
            static_cast<void>(source->RemoveHandler(
                Aero::UIElement::KeyDownEvent.Handle(), handler));
            FreeObject(
                *allocator, Base::MemoryTag::Ui, context);
            return retained.GetStatus();
        }
        return true;
    }

    Base::Result<void> AttachBehavior(
        const Interactivity::Behavior& prototype,
        Aero::FrameworkElement& owner,
        const Aero::NameScope* names,
        bool clonePrototype) noexcept {
        for (const AttachedBehaviorInstance& existing :
             attachedBehaviorInstances) {
            if (existing.target == &owner &&
                existing.prototype == &prototype) {
                return {};
            }
        }
        Base::Ref<Interactivity::Behavior> instance;
        if (clonePrototype) {
            Base::Result<Base::Ref<Interactivity::Behavior>> cloned =
                CloneBehaviorPrototype(prototype);
            if (!cloned) return cloned.GetStatus();
            instance = std::move(cloned).Value();
        } else {
            instance = Base::Ref<Interactivity::Behavior>::TryFromBorrowed(
                const_cast<Interactivity::Behavior&>(prototype));
            if (!instance) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidState,
                    "Direct Behavior instance cannot be retained");
            }
        }
        AttachedBehaviorInstance record;
        record.target = &owner;
        record.prototype = &prototype;
        record.instance = std::move(instance);

        for (const Interactivity::Behavior::AuthoredBinding& authored :
             record.instance->GetAuthoredBindings()) {
            if (!authored.binding) continue;
            Base::Object* source = ResolveBehaviorBindingSource(
                *authored.binding, *record.instance, owner, names);
            if ((!authored.binding->GetElementName().Empty() ||
                 authored.binding->GetSource() ||
                 authored.binding->GetRelativeSource()) &&
                source == nullptr) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Behavior Binding source was not found");
            }
            Data::MetadataBindingDescriptor descriptor;
            descriptor.metadata = metadata;
            descriptor.source = source;
            descriptor.target = record.instance.Get();
            descriptor.targetProperty = authored.property;
            descriptor.dataContextProperty =
                FrameworkElement::DataContextProperty.Handle();
            descriptor.dataContextOwner = &owner;
            descriptor.path = authored.binding->GetPath().GetPath();
            descriptor.stringFormat =
                authored.binding->GetStringFormat();
            descriptor.bindsToSource = descriptor.path.Empty();
            descriptor.mode = authored.binding->GetMode() ==
                    Data::BindingMode::Default
                ? Data::BindingMode::OneWay
                : authored.binding->GetMode();
            descriptor.updateSourceTrigger =
                authored.binding->GetUpdateSourceTrigger() ==
                    Meta::UpdateSourceTrigger::Default
                ? Meta::UpdateSourceTrigger::PropertyChanged
                : authored.binding->GetUpdateSourceTrigger();
            descriptor.fallbackValue =
                authored.binding->GetFallbackValue();
            descriptor.targetNullValue =
                authored.binding->GetTargetNullValue();
            Base::Result<Data::BindingHandle> attached =
                bindings->Attach(descriptor);
            if (!attached) {
                for (const Data::BindingHandle handle : record.bindings) {
                    static_cast<void>(bindings->Detach(handle));
                }
                return attached.GetStatus();
            }
            Base::Result<void> retained = record.bindings.PushBack(
                attached.Value());
            if (!retained) {
                static_cast<void>(bindings->Detach(attached.Value()));
                for (const Data::BindingHandle handle : record.bindings) {
                    static_cast<void>(bindings->Detach(handle));
                }
                return retained.GetStatus();
            }
        }
        Base::Result<void> attached = record.instance->Attach(owner);
        if (!attached) {
            for (const Data::BindingHandle handle : record.bindings) {
                static_cast<void>(bindings->Detach(handle));
            }
            return attached.GetStatus();
        }
        Base::Result<void> retained = attachedBehaviorInstances.PushBack(
            std::move(record));
        if (!retained) {
            record.instance->Detach();
            for (const Data::BindingHandle handle : record.bindings) {
                static_cast<void>(bindings->Detach(handle));
            }
            return retained.GetStatus();
        }
        return {};
    }

    Base::Result<std::uint32_t> StartLoadedAnimations(
        Aero::Media::Visual* visual,
        const Aero::NameScope* names = nullptr) noexcept {
        if (visual == nullptr) return std::uint32_t{0U};
        std::uint32_t count = 0U;
        Aero::FrameworkElement* element =
            visual->AsFrameworkElement();
        if (element != nullptr) {
            for (const Base::Ref<Base::Object>& authoredBehavior :
                 Aero::ElementPrivate::AuthoredBehaviors(
                     *element)) {
                if (!authoredBehavior ||
                    !metadata->Types().IsDerivedFrom(
                        authoredBehavior->RuntimeType(),
                        Interactivity::Behavior::StaticTypeId())) {
                    continue;
                }
                Base::Result<void> attached = AttachBehavior(
                    static_cast<const Interactivity::Behavior&>(
                        *authoredBehavior),
                    *element,
                    names,
                    false);
                if (!attached) return attached.GetStatus();
            }
            for (const Base::Ref<Base::Object>& behaviorPrototype :
                 Aero::ElementPrivate::StyleBehaviorPrototypes(
                     *element)) {
                if (!behaviorPrototype ||
                    !metadata->Types().IsDerivedFrom(
                        behaviorPrototype->RuntimeType(),
                        Interactivity::Behavior::StaticTypeId())) {
                    continue;
                }
                Base::Result<void> attached = AttachBehavior(
                    static_cast<const Interactivity::Behavior&>(
                        *behaviorPrototype),
                    *element,
                    names,
                    true);
                if (!attached) return attached.GetStatus();
            }
            if (input != nullptr &&
                metadata->Types().IsDerivedFrom(
                    element->RuntimeType(),
                    Controls::Grid::StaticTypeId())) {
                auto& grid = static_cast<Controls::Grid&>(*element);
                for (const Base::Ref<Input::KeyBinding>& binding :
                     grid.GetInputBindings()) {
                    if (!binding) continue;
                    Base::Result<Input::InputBindingHandle> added =
                        input->AddInputBinding(*element, binding);
                    if (!added) return added.GetStatus();
                }
            }
            for (const Base::Ref<Base::Object>& authored :
                 Aero::ElementPrivate::AuthoredTriggers(*element)) {
                if (!authored) {
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::Controls::DataTemplateTriggerState::
                            StaticTypeId()) {
                    Base::Result<std::uint32_t> started =
                        StartDataTemplateTriggers(
                            static_cast<
                                Aero::Controls::DataTemplateTriggerState&>(
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
                            PushBack({
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
                if (authored->RuntimeType() ==
                    MediaAnimation::PropertyChangedTrigger::
                        StaticTypeId()) {
                    Base::Result<bool> started =
                        StartPropertyChangedTrigger(
                            static_cast<
                                MediaAnimation::PropertyChangedTrigger&>(
                                    *authored),
                            *element,
                            names);
                    if (!started) return started.GetStatus();
                    if (started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    MediaAnimation::KeyTrigger::StaticTypeId()) {
                    Base::Result<bool> started = StartKeyTrigger(
                        static_cast<MediaAnimation::KeyTrigger&>(
                            *authored),
                        *element,
                        names);
                    if (!started) return started.GetStatus();
                    if (started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::DataTrigger::StaticTypeId()) {
                    Base::Result<bool> started =
                        StartInteractionDataTrigger(
                            static_cast<Aero::DataTrigger&>(*authored),
                            *element,
                            names);
                    if (!started) return started.GetStatus();
                    if (started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() !=
                    MediaAnimation::EventTrigger::StaticTypeId()) {
                    continue;
                }
                Base::Result<bool> started = StartEventTrigger(
                    static_cast<MediaAnimation::EventTrigger&>(*authored),
                    *element,
                    *element,
                    names);
                if (!started) return started.GetStatus();
                if (started.Value()) ++count;
            }
            for (const Base::Ref<Base::Object>& authored :
                 Aero::ElementPrivate::StyleTriggerPrototypes(
                     *element)) {
                if (!authored) continue;
                if (authored->RuntimeType() ==
                    MediaAnimation::StoryboardCompletedTrigger::StaticTypeId()) {
                    Base::Result<void> retained =
                        storyboardCompletedSubscriptions.PushBack({
                            static_cast<MediaAnimation::StoryboardCompletedTrigger*>(
                                authored.Get()),
                            element,
                            names});
                    if (!retained) return retained.GetStatus();
                    continue;
                }
                if (authored->RuntimeType() ==
                    MediaAnimation::PropertyChangedTrigger::StaticTypeId()) {
                    Base::Result<bool> started = StartPropertyChangedTrigger(
                        static_cast<MediaAnimation::PropertyChangedTrigger&>(
                            *authored),
                        *element,
                        names);
                    if (!started) return started.GetStatus();
                    if (started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    MediaAnimation::KeyTrigger::StaticTypeId()) {
                    Base::Result<bool> started = StartKeyTrigger(
                        static_cast<MediaAnimation::KeyTrigger&>(*authored),
                        *element,
                        names);
                    if (!started) return started.GetStatus();
                    if (started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    Aero::DataTrigger::StaticTypeId()) {
                    Base::Result<bool> started =
                        StartInteractionDataTrigger(
                            static_cast<Aero::DataTrigger&>(*authored),
                            *element,
                            names);
                    if (!started) return started.GetStatus();
                    if (started.Value()) ++count;
                    continue;
                }
                if (authored->RuntimeType() ==
                    MediaAnimation::EventTrigger::StaticTypeId()) {
                    Base::Result<bool> started = StartEventTrigger(
                        static_cast<MediaAnimation::EventTrigger&>(*authored),
                        *element,
                        *element,
                        names);
                    if (!started) return started.GetStatus();
                    if (started.Value()) ++count;
                }
            }
            if (styles != nullptr) {
                const Aero::Style* applied = styles->AppliedStyle(*element);
                if (applied != nullptr) {
                    for (const Base::Ref<Aero::TriggerBase>& authored :
                         applied->GetAuthoredTriggers()) {
                        if (!authored ||
                            !metadata->Types().IsDerivedFrom(
                                authored->RuntimeType(),
                                MediaAnimation::EventTrigger::StaticTypeId())) {
                            continue;
                        }
                        Base::Result<bool> started = StartEventTrigger(
                            static_cast<MediaAnimation::EventTrigger&>(
                                *authored),
                            *element,
                            *element,
                            names);
                        if (!started) return started.GetStatus();
                        if (started.Value()) ++count;
                    }
                }
            }
            if (metadata->Types().IsDerivedFrom(
                    element->RuntimeType(),
                    Controls::TextBlock::StaticTypeId())) {
                const Documents::InlineCollectionView inlines =
                    static_cast<const Controls::TextBlock&>(*element)
                        .GetInlines();
                for (std::uint32_t index = 0U;
                     index < inlines.GetCount(); ++index) {
                    const Documents::Inline* inlineValue =
                        inlines.GetItem(index);
                    if (inlineValue == nullptr) continue;
                    Base::Result<std::uint32_t> started =
                        StartContentElementAnimations(
                            const_cast<Documents::Inline&>(*inlineValue),
                            *element,
                            names);
                    if (!started) return started.GetStatus();
                    if (count > UINT32_MAX - started.Value()) {
                        return Base::Status::Failure(
                            Base::ErrorCode::OutOfRange,
                            "Inline trigger count overflow");
                    }
                    count += started.Value();
                }
            }
        }
        for (Aero::Media::Visual* child :
             Aero::ElementPrivate::VisualChildren(*visual)) {
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
        Aero::Media::Visual* node,
        const Aero::Media::Visual& fragmentRoot) const noexcept {
        while (node != nullptr) {
            if (node == &fragmentRoot) return true;
            node = node->GetLogicalParent() != nullptr
                ? node->GetLogicalParent()
                : node->GetVisualParent();
        }
        return false;
    }

    void ClearDataTemplateTriggerProviders(
        Aero::Controls::DataTemplateTriggerState& context) noexcept {
        if (values != nullptr) {
            for (Aero::Controls::DataTemplatePropertyTrigger& trigger :
                 context.triggers) {
                for (Aero::Controls::DataTemplateTriggerSetter& setter :
                     trigger.setters) {
                    Base::Ref<DependencyObject> target =
                        setter.target.Lock();
                    if (!target || !setter.token.IsValid()) continue;
                    static_cast<void>(
                        values->ClearProviderContribution(
                            *target,
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
        Aero::Media::Visual& visual) noexcept {
        Aero::FrameworkElement* element =
            visual.AsFrameworkElement();
        if (element != nullptr) {
            for (const Base::Ref<Base::Object>& authored :
                 Aero::ElementPrivate::AuthoredTriggers(*element)) {
                if (authored && authored->RuntimeType() ==
                    Aero::Controls::DataTemplateTriggerState::StaticTypeId()) {
                    ClearDataTemplateTriggerProviders(
                        static_cast<Aero::Controls::DataTemplateTriggerState&>(
                            *authored));
                }
            }
        }
        for (Aero::Media::Visual* child : Aero::ElementPrivate::VisualChildren(visual)) {
            if (child != nullptr) {
                ClearDataTemplateTriggerProvidersInSubtree(*child);
            }
        }
    }

    void DetachBehaviorsInSubtree(Aero::Media::Visual& visual) noexcept {
        for (std::uint32_t index = 0U;
             index < attachedBehaviorInstances.Size();) {
            AttachedBehaviorInstance& record =
                attachedBehaviorInstances[index];
            if (record.target == nullptr ||
                !IsInVisualSubtree(record.target, visual)) {
                ++index;
                continue;
            }
            for (const Data::BindingHandle handle : record.bindings) {
                if (bindings != nullptr) {
                    static_cast<void>(bindings->Detach(handle));
                }
            }
            if (record.instance) record.instance->Detach();
            if (index + 1U != attachedBehaviorInstances.Size()) {
                attachedBehaviorInstances[index] =
                    std::move(attachedBehaviorInstances.Back());
            }
            attachedBehaviorInstances.PopBack();
        }
    }

    void ClearAnimationSubscriptionsFor(
        Aero::Media::Visual& fragmentRoot) noexcept {
        DetachBehaviorsInSubtree(fragmentRoot);
        ClearDataTemplateTriggerProvidersInSubtree(fragmentRoot);
        for (std::uint32_t index = 0U;
             index < dataTemplateTriggerSubscriptions.Size();) {
            DataTemplateTriggerSubscription& subscription =
                dataTemplateTriggerSubscriptions[index];
            const bool sourceMatches =
                subscription.source != nullptr &&
                metadata->Types().IsDerivedFrom(
                    subscription.source->RuntimeType(),
                    Aero::Media::Visual::StaticTypeId()) &&
                IsInVisualSubtree(
                    static_cast<Aero::Media::Visual*>(
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
             index < propertyChangedTriggerSubscriptions.Size();) {
            PropertyChangedTriggerSubscription& subscription =
                propertyChangedTriggerSubscriptions[index];
            if (subscription.owner == nullptr ||
                !IsInVisualSubtree(subscription.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            if (subscription.source != nullptr) {
                static_cast<void>(
                    subscription.source->RemoveValueChangedHandler(
                        subscription.property,
                        subscription.handler));
            } else if (subscription.metadataSource != nullptr &&
                       subscription.metadataSubscription != 0U &&
                       metadata != nullptr) {
                static_cast<void>(metadata->UnsubscribePropertyChanged(
                    *subscription.metadataSource,
                    subscription.metadataSubscription));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui,
                subscription.context);
            if (index + 1U !=
                propertyChangedTriggerSubscriptions.Size()) {
                propertyChangedTriggerSubscriptions[index] =
                    std::move(
                        propertyChangedTriggerSubscriptions.Back());
            }
            propertyChangedTriggerSubscriptions.PopBack();
        }
        for (std::uint32_t index = 0U;
             index < interactionDataTriggerSubscriptions.Size();) {
            InteractionDataTriggerSubscription& subscription =
                interactionDataTriggerSubscriptions[index];
            if (subscription.owner == nullptr ||
                !IsInVisualSubtree(subscription.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            if (subscription.source != nullptr) {
                static_cast<void>(
                    subscription.source->RemoveValueChangedHandler(
                        subscription.property,
                        subscription.handler));
            } else if (subscription.metadataSource != nullptr &&
                       subscription.metadataSubscription != 0U &&
                       metadata != nullptr) {
                static_cast<void>(metadata->UnsubscribePropertyChanged(
                    *subscription.metadataSource,
                    subscription.metadataSubscription));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui,
                subscription.context);
            if (index + 1U !=
                interactionDataTriggerSubscriptions.Size()) {
                interactionDataTriggerSubscriptions[index] =
                    std::move(
                        interactionDataTriggerSubscriptions.Back());
            }
            interactionDataTriggerSubscriptions.PopBack();
        }
        for (std::uint32_t index = 0U;
             index < keyTriggerSubscriptions.Size();) {
            KeyTriggerSubscription& subscription =
                keyTriggerSubscriptions[index];
            if (subscription.owner == nullptr ||
                !IsInVisualSubtree(subscription.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            if (subscription.source != nullptr) {
                static_cast<void>(subscription.source->RemoveHandler(
                    Aero::UIElement::KeyDownEvent.Handle(),
                    subscription.handler));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui,
                subscription.context);
            if (index + 1U != keyTriggerSubscriptions.Size()) {
                keyTriggerSubscriptions[index] =
                    std::move(keyTriggerSubscriptions.Back());
            }
            keyTriggerSubscriptions.PopBack();
        }
        for (std::uint32_t index = 0U;
             index < animationEventSubscriptions.Size();) {
            AnimationEventSubscription& subscription =
                animationEventSubscriptions[index];
            if (subscription.visualOwner == nullptr ||
                !IsInVisualSubtree(
                    subscription.visualOwner, fragmentRoot)) {
                ++index;
                continue;
            }
            if (subscription.source != nullptr) {
                if (subscription.contentSource) {
                    static_cast<void>(
                        static_cast<Aero::ContentElement*>(subscription.source)
                            ->RemoveHandler(
                                subscription.event,
                                subscription.handler));
                } else {
                    static_cast<void>(
                        static_cast<Aero::UIElement*>(subscription.source)
                            ->RemoveHandler(
                                subscription.event,
                                subscription.handler));
                }
            }
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
                for (Aero::Media::Animation::Runtime::AnimationHandle handle : session.handles) {
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
             index < storyboardCompletionSessions.Size();) {
            StoryboardCompletionSession& session =
                storyboardCompletionSessions[index];
            if (session.owner == nullptr ||
                !IsInVisualSubtree(session.owner, fragmentRoot)) {
                ++index;
                continue;
            }
            if (animations != nullptr) {
                for (Aero::Media::Animation::Runtime::AnimationHandle handle :
                     session.handles) {
                    static_cast<void>(animations->Remove(handle));
                }
            }
            for (std::uint32_t next = index + 1U;
                 next < storyboardCompletionSessions.Size(); ++next) {
                storyboardCompletionSessions[next - 1U] =
                    std::move(storyboardCompletionSessions[next]);
            }
            storyboardCompletionSessions.PopBack();
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
        for (PropertyChangedTriggerSubscription& subscription :
             propertyChangedTriggerSubscriptions) {
            if (subscription.source != nullptr) {
                static_cast<void>(
                    subscription.source->RemoveValueChangedHandler(
                        subscription.property,
                        subscription.handler));
            } else if (subscription.metadataSource != nullptr &&
                       subscription.metadataSubscription != 0U &&
                       metadata != nullptr) {
                static_cast<void>(metadata->UnsubscribePropertyChanged(
                    *subscription.metadataSource,
                    subscription.metadataSubscription));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui,
                subscription.context);
        }
        propertyChangedTriggerSubscriptions.Clear();
        for (InteractionDataTriggerSubscription& subscription :
             interactionDataTriggerSubscriptions) {
            if (subscription.source != nullptr) {
                static_cast<void>(
                    subscription.source->RemoveValueChangedHandler(
                        subscription.property,
                        subscription.handler));
            } else if (subscription.metadataSource != nullptr &&
                       subscription.metadataSubscription != 0U &&
                       metadata != nullptr) {
                static_cast<void>(metadata->UnsubscribePropertyChanged(
                    *subscription.metadataSource,
                    subscription.metadataSubscription));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui,
                subscription.context);
        }
        interactionDataTriggerSubscriptions.Clear();
        for (KeyTriggerSubscription& subscription :
             keyTriggerSubscriptions) {
            if (subscription.source != nullptr) {
                static_cast<void>(subscription.source->RemoveHandler(
                    Aero::UIElement::KeyDownEvent.Handle(),
                    subscription.handler));
            }
            FreeObject(
                *allocator, Base::MemoryTag::Ui,
                subscription.context);
        }
        keyTriggerSubscriptions.Clear();
        for (AnimationEventSubscription& subscription :
             animationEventSubscriptions) {
            if (subscription.source != nullptr) {
                if (subscription.contentSource) {
                    static_cast<void>(
                        static_cast<Aero::ContentElement*>(subscription.source)
                            ->RemoveHandler(
                                subscription.event,
                                subscription.handler));
                } else {
                    static_cast<void>(
                        static_cast<Aero::UIElement*>(subscription.source)
                            ->RemoveHandler(
                                subscription.event,
                                subscription.handler));
                }
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
        Aero::Media::Visual* node) noexcept {
        if (node == nullptr) return;
        if (metadata->Types().IsDerivedFrom(
                node->RuntimeType(),
                Controls::Control::StaticTypeId())) {
            ::Aero::Controls::ControlBehavior::SetVisualStateManager(*static_cast<Controls::Control*>(node), nullptr);
        }
        for (Aero::Media::Visual* child :
             Aero::ElementPrivate::VisualChildren(*node)) {
            ClearElementEvents(child);
        }
    }

    void BeginDestroyInteractions() noexcept {
        if (Aero::Media::Visual* rootVisual = RootVisual()) {
            DetachBehaviorsInSubtree(*rootVisual);
        }
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
        FreeObject(*allocator, Base::MemoryTag::Ui, controlBehaviors);
        elementHost.controlBehaviors = nullptr;
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
        Aero::Media::Visual* rootVisual = RootVisual();
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
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, controlBehaviors,
                *allocator, *metadata, *tree, *events, *input,
                visualStates, options.clipboard,
                options.attachControlInteractions,
                options.attachTextEditing);
            if (!status) return status.GetStatus();
            status = controlBehaviors->Initialize();
            if (!status) return status.GetStatus();
            elementHost.controlBehaviors = controlBehaviors;
        }
        status = VisitAndAttach(*rootVisual);
        if (!status) {
            return status.GetStatus();
        }
        return {};
    }

    void Shutdown() noexcept {
        audio.Shutdown();
        BeginDestroyInteractions();
        DetachUi();
        FinishDestroyInteractions();
        static_cast<void>(UnmountAllFragments());
        VisitTextElements(RootVisual(), nullptr);
        VisitPaths(RootVisual(), nullptr);
        elementHost.textLayout = nullptr;
        elementHost.meshResources = nullptr;
        DestroyUiEngines();
        if (images != nullptr) {
            images->Shutdown(GetImageResources());
        }
        if (tree != nullptr) {
            tree->SetLifecycleHandler(nullptr);
        }
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


        FreeObject(*allocator, Base::MemoryTag::Ui, input);
        FreeObject(*allocator, Base::MemoryTag::Ui, events);
        if (bindings != nullptr) bindings->Shutdown();
        FreeObject(*allocator, Base::MemoryTag::Ui, bindings);
        FreeObject(*allocator, Base::MemoryTag::Ui, renderer);
        FreeObject(*allocator, Base::MemoryTag::Ui, layout);
        FreeObject(*allocator, Base::MemoryTag::Ui, tree);
        FreeObject(*allocator, Base::MemoryTag::Ui, text);
        FreeObject(*allocator, Base::MemoryTag::Ui, images);
        FreeObject(*allocator, Base::MemoryTag::Ui, animations);
        FreeObject(*allocator, Base::MemoryTag::Ui, values);
        FreeObject(*allocator, Base::MemoryTag::Ui, objectFactory);
        schema = nullptr;
        metadata = nullptr;
        device.Reset();
        initialized = false;
    }

    Base::Result<void> Initialize(
        const ViewOptions& requested) noexcept {
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
        if (!gui) {
            return ViewInvalidState("View has no Gui provider state");
        }
        const GuiState& guiState =
            static_cast<const GuiState&>(*gui);
        seenTextureProviderChange = guiState.textureChangeGeneration;
        seenFontProviderChange = guiState.fontChangeGeneration;

        Base::Result<void> status;

        Base::Result<Base::Ref<RenderDevice>>
            headless =
                ::Aero::Render::CreateHeadlessRenderDevice(
                    allocator);
        if (!headless) {
            terminal = true;
            return headless.GetStatus();
        }
        device = std::move(headless).Value();
        deviceGeneration = device->Generation();

        Base::Result<Base::Ref<Markup::EffectLifetime>> lifetime =
            Base::MakeRefWithAllocator<Markup::EffectLifetime>(
                *allocator);
        status = lifetime
            ? Base::Result<void>()
            : Base::Result<void>(lifetime.GetStatus());
        if (status) effectLifetime = std::move(lifetime).Value();

        if (!status || schemaBundle == nullptr ||
            !schemaBundle->IsFrozen()) {
            terminal = true;
            return status
                ? ViewInvalidState(
                      "Gui schema is not initialized")
                : status.GetStatus();
        }
        metadata = &schemaBundle->Metadata();
        schema = &schemaBundle->Schema();

        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, objectFactory, *dispatcher,
                ::Aero::MetadataPrivate::
                    DependencyProperties(*metadata),
                *metadata);
        }
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, values, *dispatcher,
                ::Aero::MetadataPrivate::
                    DependencyProperties(*metadata));
        }
        if (status) status = values->Initialize();
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, animations, *dispatcher, *values, allocator);
        }
        if (status) status = animations->Initialize();
        if (status) {
            animations->SetAutomaticTickingEnabled(
                options.automaticAnimationClock);
        }
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, tree, *dispatcher, *values);
        }
        if (status) status = tree->Initialize();
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, layout, *dispatcher);
        }
        if (status) status = layout->Initialize();
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, renderer, *dispatcher);
        }
        if (status) status = renderer->Initialize();
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, images, allocator);
        }
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, text, allocator);
        }
        if (status) {
            status = text->Initialize(
                *device, nullptr, options.text);
        }
        if (status) {
            tree->SetLifecycleHandler(
                &TextLifecycleHook, this);
        }
        if (status) {
            status = AllocateObject(
                *allocator,
                Base::MemoryTag::Ui,
                bindings,
                *dispatcher,
                metadata);
        }
        if (status) status = bindings->Initialize();
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, events,
                ::Aero::MetadataPrivate::
                    RoutedEventState(*metadata));
        }
        if (status) {
            status = AllocateObject(*allocator, Base::MemoryTag::Ui, input, *tree, *events);
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
        Markup::XamlDocument document,
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
                target.AddMerged(dictionary);
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
        Diagnostics::IDiagnosticSink* diagnostics,
        bool merge = false) noexcept {
        if (!initialized) {
            return AeroNotInitialized(
                "View must be initialized before loading resources");
        }
        if (mounted || root || loadedDocument.root) {
            return ViewInvalidState(
                "View resource layers must be loaded before a document");
        }
        Base::Result<Markup::XamlReaderSettings> loadOptions =
            XamlSettings();
        if (!loadOptions) {
            return loadOptions.GetStatus();
        }
        if (xamlRuntime == nullptr) {
            return AeroNotInitialized(
                "Gui XAML runtime is unavailable");
        }
        Base::Result<Markup::XamlDocument> loaded =
            xamlRuntime->Load(
            xamlRuntime->Providers(),
            &loadContext,
            allocator,
            uri, loadOptions.Value(), diagnostics);
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
            return AeroNotInitialized(
                "View must be initialized before loading resources");
        }
        if (mounted || root || loadedDocument.root) {
            return ViewInvalidState(
                "View resource layers must be loaded before a document");
        }
        Base::Result<Markup::XamlReaderSettings> loadOptions =
            XamlSettings();
        if (!loadOptions) return loadOptions.GetStatus();
        if (xamlRuntime == nullptr) {
            return AeroNotInitialized(
                "Gui XAML runtime is unavailable");
        }
        Base::Result<Markup::XamlDocument> loaded =
            xamlRuntime->LoadCompiled(
                xamlRuntime->Providers(), &loadContext, allocator,
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
                Aero::Media::Visual::StaticTypeId())) {
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
            return AeroNotInitialized(
                "View must be initialized before mounting");
        }
        if (mounted || root) {
            return ViewInvalidState(
                "View already has a mounted root");
        }
        const bool needsViewport =
            viewport.logicalSize.width != availableSize.width ||
            viewport.logicalSize.height != availableSize.height ||
            (availableSize.width > 0.0 && viewport.pixelWidth == 0U) ||
            (availableSize.height > 0.0 && viewport.pixelHeight == 0U);
        if (needsViewport) {
            Base::Result<ViewViewport> nextViewport =
                MakeLogicalViewport(availableSize, viewport.dpiScale);
            if (!nextViewport) return nextViewport.GetStatus();
            Base::Result<void> viewportApplied =
                ApplyViewport(nextViewport.Value());
            if (!viewportApplied) return viewportApplied.GetStatus();
        }
        Base::Result<void> validRoot = ValidateDocumentRoot(requestedRoot);
        if (!validRoot) return validRoot.GetStatus();
        if (loadedDocument.root &&
            loadedDocument.root.Get() != requestedRoot.Get()) {
            return ViewInvalidState(
                "Mounted root does not match the staged XAML document");
        }
        Base::Result<Aero::Media::Visual*> rootVisual =
            ResolveVisual(*requestedRoot, requestedRoot->RuntimeType());
        if (!rootVisual) return rootVisual.GetStatus();
        Base::Result<Aero::UIElement*> rootLayout =
            ResolveUIElement(*requestedRoot, requestedRoot->RuntimeType());
        if (!rootLayout) return rootLayout.GetStatus();
        Base::Result<void> rootTracked =
            loadedDocument.visualContent.AddNode(*rootVisual.Value());
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
        Markup::EffectRuntimeServices runtimeServices;
        runtimeServices.effectiveValues = values;
        runtimeServices.bindings = bindings;
        runtimeServices.fallbackResources = &dynamicResourceEnvironment;
        runtimeServices.lifetime = effectLifetime;
        Base::Result<void> bound = loadedDocument.effects.Bind(runtimeServices);
        if (!bound) {
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return bound.GetStatus();
        }
        Base::Result<void> effects = loadedDocument.effects.Commit();
        if (!effects) {
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return effects.GetStatus();
        }
        Base::Result<std::uint32_t> initialBindings =
            bindings->Flush();
        if (!initialBindings) {
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return initialBindings.GetStatus();
        }
        deferGeneratedActivation = true;
        Base::Result<void> uiApplied =
            ApplyUi(*rootVisual.Value());
        if (!uiApplied) {
            deferGeneratedActivation = false;
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
            deferGeneratedActivation = false;
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
            deferGeneratedActivation = false;
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
            deferGeneratedActivation = false;
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
        Base::Result<void> itemGeneratorsAttached =
            AttachPendingItemGenerators(*rootVisual.Value());
        if (!itemGeneratorsAttached) {
            deferGeneratedActivation = false;
            BeginDestroyInteractions();
            DetachUi();
            FinishDestroyInteractions();
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return itemGeneratorsAttached.GetStatus();
        }
        Base::Result<std::uint32_t> settledBindings =
            bindings->Flush();
        deferGeneratedActivation = false;
        if (!settledBindings) {
            BeginDestroyInteractions();
            DetachUi();
            FinishDestroyInteractions();
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return settledBindings.GetStatus();
        }
        Base::Result<void> generated = FlushGeneratedVisuals();
        if (!generated) {
            BeginDestroyInteractions();
            DetachUi();
            FinishDestroyInteractions();
            static_cast<void>(DetachVisualGraph({
                loadedDocument.visualContent.mountEdges.Data(),
                loadedDocument.visualContent.mountEdges.Size()}));
            mounted = false;
            root.Reset();
            ClearLoadedDocument();
            return generated.GetStatus();
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
        Base::Result<Aero::Media::Visual*> rootVisual =
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
        for (const Aero::Markup::VisualEdge& edge :
             fragment.document.visualContent.mountEdges) {
            if (edge.state.IsAttached()) ++remaining;
        }
        while (remaining > 0U) {
            bool progressed = false;
            for (Aero::Markup::VisualEdge& edge :
                 fragment.document.visualContent.mountEdges) {
                if (!edge.state.IsAttached()) continue;
                bool hasMountedChild = false;
                for (const Aero::Markup::VisualEdge& candidate :
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
            fragment.host->SetContent(nullptr);
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

void Media::CompositionTarget::AddRendering(
    View& view,
    const RenderingEventHandler& handler) noexcept {
    if (view.state_ != nullptr && !handler.Empty()) {
        view.state_->renderingHandlers.Add(handler);
    }
}

bool Media::CompositionTarget::RemoveRendering(
    View& view,
    const RenderingEventHandler& handler) noexcept {
    return view.state_ != nullptr &&
        view.state_->renderingHandlers.Remove(handler);
}

void Media::CompositionTarget::RaiseRendering(View& view) noexcept {
    if (view.state_ != nullptr &&
        !view.state_->renderingHandlers.Empty()) {
        view.state_->renderingHandlers.Invoke();
    }
    RenderingEventHandler& legacy =
        LegacyCompositionRenderingHandlers();
    if (!legacy.Empty()) legacy.Invoke();
}

void ViewState::StyleDataTriggerHandlerState::Invoke(
    ::Aero::DependencyObject&,
    const Meta::DependencyPropertyChangedEventArgs&) noexcept {
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

void ViewState::
DataTemplateTriggerHandlerState::Invoke(
    ::Aero::DependencyObject&,
    const Meta::DependencyPropertyChangedEventArgs&)
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

void ViewState::PropertyChangedTriggerState::Invoke(
    ::Aero::DependencyObject&,
    const Meta::DependencyPropertyChangedEventArgs&) noexcept {
    if (runtime == nullptr || trigger == nullptr || owner == nullptr ||
        !runtime->animationEventStatus.IsOk()) {
        return;
    }
    Base::Result<void> executed = runtime->ExecuteTriggerActions(
        trigger->GetActions(), *owner, names);
    if (!executed) {
        runtime->animationEventStatus = executed.GetStatus();
    }
}

void ViewState::PropertyChangedTriggerState::MetadataInvoke(
    Base::Object&,
    Meta::MemberId property,
    void* context) noexcept {
    auto* state = static_cast<PropertyChangedTriggerState*>(context);
    if (state == nullptr || (state->metadataProperty != Meta::InvalidMemberId &&
        property != state->metadataProperty)) {
        return;
    }
    if (state->runtime == nullptr || state->trigger == nullptr ||
        state->owner == nullptr ||
        !state->runtime->animationEventStatus.IsOk()) {
        return;
    }
    Base::Result<void> executed = state->runtime->ExecuteTriggerActions(
        state->trigger->GetActions(), *state->owner, state->names);
    if (!executed) {
        state->runtime->animationEventStatus = executed.GetStatus();
    }
}

void ViewState::InteractionDataTriggerState::Invoke(
    ::Aero::DependencyObject&,
    const Meta::DependencyPropertyChangedEventArgs&) noexcept {
    if (runtime == nullptr || trigger == nullptr || owner == nullptr ||
        !runtime->animationEventStatus.IsOk()) {
        return;
    }
    Base::Result<bool> evaluated =
        runtime->EvaluateInteractionDataTrigger(*this);
    if (!evaluated) {
        runtime->animationEventStatus = evaluated.GetStatus();
    }
}

void ViewState::InteractionDataTriggerState::MetadataInvoke(
    Base::Object&,
    Meta::MemberId property,
    void* context) noexcept {
    auto* state = static_cast<InteractionDataTriggerState*>(context);
    if (state == nullptr || (state->metadataProperty != Meta::InvalidMemberId &&
        property != state->metadataProperty)) {
        return;
    }
    if (state->runtime == nullptr || state->trigger == nullptr ||
        state->owner == nullptr ||
        !state->runtime->animationEventStatus.IsOk()) {
        return;
    }
    Base::Result<bool> evaluated =
        state->runtime->EvaluateInteractionDataTrigger(*state);
    if (!evaluated) {
        state->runtime->animationEventStatus = evaluated.GetStatus();
    }
}

void ViewState::KeyTriggerState::Invoke(
    Base::Object*,
    Aero::KeyEventArgs& args) noexcept {
    if (runtime == nullptr || trigger == nullptr || owner == nullptr ||
        !runtime->animationEventStatus.IsOk() ||
        args.GetAction() != Input::KeyboardAction::Down ||
        args.GetKey() != ViewState::KeyCodeFromName(trigger->GetKey())) {
        return;
    }
    if (trigger->GetActiveOnFocus()) {
        Aero::UIElement* expected = owner->AsUIElement();
        if (expected == nullptr || runtime->input == nullptr ||
            runtime->input->GetFocusedElement() != expected) {
            return;
        }
    }
    Base::Result<void> executed = runtime->ExecuteTriggerActions(
        trigger->GetActions(), *owner, names);
    if (!executed) {
        runtime->animationEventStatus = executed.GetStatus();
    }
}

Base::Result<void>
ViewState::ExecuteAnimationAction(
    MediaAnimation::TriggerAction& action,
    Aero::FrameworkElement& owner,
    Aero::Controls::DataTemplateTriggerState*
        dataTemplateContext,
    const Aero::NameScope* names) noexcept {
    const Meta::TypeId type =
        action.RuntimeType();
    if (type ==
        MediaAnimation::ChangePropertyAction::StaticTypeId()) {
        auto& change =
            static_cast<MediaAnimation::ChangePropertyAction&>(
                action);
        Base::Object* targetObject =
            change.GetTargetName().Empty()
            ? static_cast<Base::Object*>(&owner)
            : dataTemplateContext != nullptr
                ? dataTemplateContext->FindName(
                      change.GetTargetName())
                : names != nullptr
                    ? names->Find(change.GetTargetName())
                    : loadedDocument.names.Find(
                          change.GetTargetName());
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
                target, change.GetPropertyName());
        if (!resolved) return resolved.GetStatus();

        ::Aero::DependencyObject& propertyTarget =
            *resolved.Value().target;
        const Meta::DependencyPropertyHandle propertyHandle =
            resolved.Value().property;
        const Meta::DependencyProperty* property =
            ::Aero::MetadataPrivate::
                DependencyProperties(*metadata)
                    .Find(propertyHandle);
        if (property == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "ChangePropertyAction property metadata was not found");
        }

        Meta::PropertyValue value = change.GetValue();
        Base::Ref<Data::Binding> valueBinding =
            change.GetValueBinding();
        if (valueBinding) {
            Base::Result<Meta::PropertyValue> evaluated =
                EvaluateAuthoredBinding(
                    *valueBinding,
                    owner,
                    dataTemplateContext,
                    names,
                    &action);
            if (!evaluated) return evaluated.GetStatus();
            value = std::move(evaluated).Value();
        }
        if (value.IsNullObject() &&
            propertyHandle ==
                Controls::Primitives::ToggleButton::
                    IsCheckedProperty.Handle() &&
            metadata->Types().IsDerivedFrom(
                propertyTarget.RuntimeType(),
                Controls::Primitives::ToggleButton::
                    StaticTypeId())) {
            static_cast<Controls::Primitives::ToggleButton&>(
                propertyTarget).SetIsChecked(Nullable<bool>{});
            return {};
        }
        Base::Result<Meta::PropertyValue> coerced =
            Data::CoerceBindingTargetValue(
                metadata,
                *property,
                std::move(value));
        if (!coerced) return coerced.GetStatus();
        propertyTarget.SetCurrentValue(
            propertyHandle,
            std::move(coerced).Value());
        return {};
    }

    if (type ==
        MediaAnimation::InvokeCommandAction::StaticTypeId()) {
        auto& invoke =
            static_cast<MediaAnimation::InvokeCommandAction&>(action);
        Base::Ref<Input::ICommand> command = invoke.GetCommand();
        if (!command && invoke.GetCommandBinding()) {
            Base::Result<Meta::PropertyValue> evaluated =
                EvaluateAuthoredBinding(
                    *invoke.GetCommandBinding(),
                    owner,
                    dataTemplateContext,
                    names,
                    &action);
            if (!evaluated) return evaluated.GetStatus();
            if (evaluated.Value().Kind() != Meta::ValueKind::Object ||
                evaluated.Value().IsNullObject() ||
                !evaluated.Value().AsObject() ||
                !metadata->Types().IsDerivedFrom(
                    evaluated.Value().AsObject()->RuntimeType(),
                    Input::ICommand::StaticTypeId())) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "InvokeCommandAction Binding did not return ICommand");
            }
            command = Base::Ref<Input::ICommand>::FromBorrowed(
                *static_cast<Input::ICommand*>(
                    evaluated.Value().AsObject().Get()));
        }
        if (!command) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "InvokeCommandAction Command is unavailable");
        }

        Meta::PropertyValue parameter = invoke.GetCommandParameter();
        if (invoke.GetCommandParameterBinding()) {
            Base::Result<Meta::PropertyValue> evaluated =
                EvaluateAuthoredBinding(
                    *invoke.GetCommandParameterBinding(),
                    owner,
                    dataTemplateContext,
                    names,
                    &action);
            if (!evaluated) return evaluated.GetStatus();
            parameter = std::move(evaluated).Value();
        }
        if (parameter.IsUnset()) {
            parameter = Meta::PropertyValue::NullObject(
                Meta::TypeOf<Base::Object>());
        }
        Aero::UIElement* target = owner.AsUIElement();
        if (target == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "InvokeCommandAction owner is not a UIElement");
        }
        Base::Result<bool> canExecute = input != nullptr
            ? input->CanExecute(*command, parameter, *target)
            : command->CanExecute(parameter, target);
        if (!canExecute) return canExecute.GetStatus();
        if (!canExecute.Value()) return {};
        if (input != nullptr) {
            Base::Result<bool> executed =
                input->Execute(*command, parameter, *target);
            return executed
                ? Base::Result<void>()
                : Base::Result<void>(executed.GetStatus());
        }
        command->Execute(parameter, target);
        return {};
    }

    if (type == MediaAnimation::SetFocusAction::StaticTypeId()) {
        auto& setFocus = static_cast<MediaAnimation::SetFocusAction&>(action);
        if (!setFocus.GetEngage() || input == nullptr) return {};
        Base::Object* targetObject = setFocus.GetTargetName().Empty()
            ? static_cast<Base::Object*>(&owner)
            : dataTemplateContext != nullptr
                ? dataTemplateContext->FindName(setFocus.GetTargetName())
                : names != nullptr
                    ? names->Find(setFocus.GetTargetName())
                    : loadedDocument.names.Find(setFocus.GetTargetName());
        Aero::UIElement* target =
            targetObject != nullptr && metadata->Types().IsDerivedFrom(
                targetObject->RuntimeType(), Aero::UIElement::StaticTypeId())
            ? static_cast<Aero::UIElement*>(targetObject)
            : nullptr;
        if (target == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "SetFocusAction target is unavailable");
        }
        if (!target->GetIsLoaded()) {
            return QueueFocus(*target);
        }
        if (!target->GetIsEnabled()) return {};
        Base::Result<bool> focused = input->SetFocus(target);
        return focused
            ? Base::Result<void>()
            : Base::Result<void>(focused.GetStatus());
    }

    if (type == MediaAnimation::SelectAction::StaticTypeId()) {
        if (metadata->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Controls::ListBoxItem::StaticTypeId())) {
            static_cast<Controls::ListBoxItem&>(owner)
                .SetIsSelected(true);
            return {};
        }
        if (metadata->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Controls::TabItem::StaticTypeId())) {
            static_cast<Controls::TabItem&>(owner)
                .SetIsSelected(true);
            return {};
        }
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "SelectAction owner is not a selectable item container");
    }

    if (type == MediaAnimation::SelectAllAction::StaticTypeId()) {
        if (metadata->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Controls::TextBox::StaticTypeId())) {
            return static_cast<Controls::TextBox&>(owner)
                .SelectAll();
        }
        if (metadata->Types().IsDerivedFrom(
                owner.RuntimeType(),
                Controls::PasswordBox::StaticTypeId())) {
            return static_cast<Controls::PasswordBox&>(owner)
                .SelectAll();
        }
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "SelectAllAction owner is not a text editor");
    }

    if (type == MediaAnimation::PlaySoundAction::StaticTypeId()) {
        auto& playSound =
            static_cast<MediaAnimation::PlaySoundAction&>(action);
        if (!playSound.GetIsEnabled() ||
            playSound.GetSource().Empty()) {
            return {};
        }
        const double volume = playSound.GetVolume();
        if (!std::isfinite(volume) ||
            volume < 0.0 || volume > 1.0) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "PlaySoundAction Volume must be between zero and one");
        }
        Base::Result<void> initialized = audio.Initialize();
        if (!initialized &&
            (initialized.GetStatus().code ==
                 Base::ErrorCode::Unsupported ||
             initialized.GetStatus().code ==
                 Base::ErrorCode::InvalidState)) {
            // Audio is optional for headless and provider-free hosts.
            return {};
        }
        if (!initialized) return initialized.GetStatus();
        audio.SetEffectsVolume(
            static_cast<float>(volume));
        Base::Result<void> played =
            audio.PlayEffect(playSound.GetSource());
        if (!played &&
            (played.GetStatus().code ==
                 Base::ErrorCode::InvalidState ||
             played.GetStatus().code ==
                 Base::ErrorCode::NotFound)) {
            // A missing device or authored file must not poison the UI
            // trigger pipeline.
            return {};
        }
        return played;
    }

    if (type == MediaAnimation::RemoveElementAction::StaticTypeId()) {
        auto& remove = static_cast<MediaAnimation::RemoveElementAction&>(action);
        Base::Object* targetObject = static_cast<Base::Object*>(&owner);
        Base::Ref<Data::Binding> targetBinding =
            remove.GetTargetObject();
        if (targetBinding) {
            const Base::Ref<Data::RelativeSource> relative = targetBinding->GetRelativeSource();
            if (!relative || relative->GetMode() != Data::RelativeSourceMode::FindAncestor ||
                relative->GetAncestorType() != Base::StringView("ContextMenu") ||
                targetBinding->GetPath().GetPath() != Base::StringView("PlacementTarget")) {
                return Base::Status::Failure(
                    Base::ErrorCode::Unsupported,
                    "RemoveElementAction TargetObject binding is not supported");
            }
            Aero::Media::Visual* current = &owner;
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
                !contextMenu->GetPlacementTarget()) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "RemoveElementAction ContextMenu PlacementTarget was not found");
            }
            targetObject = contextMenu->GetPlacementTarget().Get();
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
        Aero::Media::Visual* current = target.GetLogicalParent() != nullptr
            ? target.GetLogicalParent() : target.GetVisualParent();
        while (current != nullptr) {
            if (metadata->Types().IsDerivedFrom(
                    current->RuntimeType(),
                    Controls::ItemsControl::StaticTypeId())) {
                auto& items = static_cast<Controls::ItemsControl&>(*current);
                std::uint32_t index = UINT32_MAX;
                for (std::uint32_t candidate = 0U;
                     candidate < items.GetCount(); ++candidate) {
                    Base::Ref<Base::Object> item = items.GetItem(candidate);
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
        if (!begin.GetStoryboard()) return {};
        if (!begin.GetName().Empty()) {
            for (std::uint32_t index = 0U;
                 index < storyboardSessions.Size();
                 ++index) {
                StoryboardSession& existing =
                    storyboardSessions[index];
                if (existing.name.View() != begin.GetName()) {
                    continue;
                }
                CancelStoryboardCompletionSessions(
                    existing.handles.AsSpan());
                for (Aero::Media::Animation::Runtime::AnimationHandle handle :
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
        completion.storyboard = begin.GetStoryboard();
        completion.owner = &owner;
        Base::Result<std::uint32_t> started =
            BeginTimeline(
                *begin.GetStoryboard(),
                owner, names, nullptr,
                &completion.handles,
                dataTemplateContext);
        if (!started) {
            for (Aero::Media::Animation::Runtime::AnimationHandle handle :
                 completion.handles) {
                static_cast<void>(
                    animations->Remove(handle));
            }
            return started.GetStatus();
        }
        StoryboardSession namedSession(allocator);
        if (!begin.GetName().Empty()) {
            namedSession.owner = &owner;
            Base::Result<void> named =
                namedSession.name.Assign(begin.GetName());
            if (named) {
                named = namedSession.handles.Append(
                    completion.handles.AsSpan());
            }
            if (!named) {
                for (Aero::Media::Animation::Runtime::AnimationHandle handle :
                     completion.handles) {
                    static_cast<void>(
                        animations->Remove(handle));
                }
                return named.GetStatus();
            }
        }
        Base::Result<void> retained =
            storyboardCompletionSessions.PushBack(
                std::move(completion));
        if (!retained) {
            for (Aero::Media::Animation::Runtime::AnimationHandle handle :
                 completion.handles) {
                static_cast<void>(
                    animations->Remove(handle));
            }
            return retained.GetStatus();
        }
        if (!begin.GetName().Empty()) {
            retained = storyboardSessions.PushBack(
                std::move(namedSession));
            if (!retained) {
                for (Aero::Media::Animation::Runtime::AnimationHandle handle :
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
        if (!control.GetStoryboard()) return {};
        if (control.GetControlOption() == MediaAnimation::ControlStoryboardAction::Option::Play) {
            MediaAnimation::BeginStoryboard begin;
            begin.SetStoryboard(control.GetStoryboard());
            return ExecuteAnimationAction(
                begin, owner, dataTemplateContext, names);
        }
        bool found = false;
        for (StoryboardCompletionSession& session : storyboardCompletionSessions) {
            if (session.owner != &owner || session.storyboard.Get() != control.GetStoryboard().Get()) continue;
            found = true;
            for (Aero::Media::Animation::Runtime::AnimationHandle handle : session.handles) {
                Base::Result<void> result;
                if (control.GetControlOption() == MediaAnimation::ControlStoryboardAction::Option::Stop) result = animations->Stop(handle);
                else if (control.GetControlOption() == MediaAnimation::ControlStoryboardAction::Option::Pause) result = animations->Pause(handle);
                else if (control.GetControlOption() == MediaAnimation::ControlStoryboardAction::Option::Resume) result = animations->Resume(handle);
                else return Base::Status::Failure(Base::ErrorCode::Unsupported, "ControlStoryboardAction option is not implemented");
                if (!result) return result.GetStatus();
            }
        }
        return found ? Base::Result<void>{} : Base::Status::Failure(
            Base::ErrorCode::NotFound, "ControlStoryboardAction storyboard was not started");
    }

    if (type == MediaAnimation::PlayMediaAction::StaticTypeId() ||
        type == MediaAnimation::PauseMediaAction::StaticTypeId() ||
        type == MediaAnimation::StopMediaAction::StaticTypeId()) {
        Base::StringView targetName = type ==
                MediaAnimation::PlayMediaAction::StaticTypeId()
            ? static_cast<MediaAnimation::PlayMediaAction&>(action)
                  .GetTargetName()
            : type == MediaAnimation::PauseMediaAction::StaticTypeId()
                ? static_cast<MediaAnimation::PauseMediaAction&>(action)
                      .GetTargetName()
                : static_cast<MediaAnimation::StopMediaAction&>(action)
                      .GetTargetName();
        Base::Object* targetObject = targetName.Empty()
            ? static_cast<Base::Object*>(&owner)
            : names != nullptr
                ? names->Find(targetName)
                : loadedDocument.names.Find(targetName);
        if (targetObject == nullptr ||
            !metadata->Types().IsDerivedFrom(
                targetObject->RuntimeType(),
                Aero::Media::MediaElement::StaticTypeId())) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "MediaAction TargetName did not resolve to a MediaElement");
        }
        auto& media = static_cast<Aero::Media::MediaElement&>(
            *targetObject);
        if (type == MediaAnimation::PlayMediaAction::StaticTypeId()) {
            media.Play();
        } else if (type ==
            MediaAnimation::PauseMediaAction::StaticTypeId()) {
            media.Pause();
        } else {
            media.Stop();
        }
        return {};
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
                control.GetBeginStoryboardName()) {
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
    for (Aero::Media::Animation::Runtime::AnimationHandle handle :
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
                    GetOffsetMicroseconds());
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

void ViewState::
CancelStoryboardCompletionSessions(
    Base::Span<const Aero::Media::Animation::Runtime::AnimationHandle>
        handles) noexcept {
    for (std::uint32_t index = 0U;
         index < storyboardCompletionSessions.Size();) {
        bool matches = false;
        for (Aero::Media::Animation::Runtime::AnimationHandle sessionHandle :
             storyboardCompletionSessions[index].handles) {
            for (Aero::Media::Animation::Runtime::AnimationHandle handle :
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
ViewState::ProcessStoryboardCompletions() noexcept {
    std::uint32_t actionCount = 0U;
    std::uint32_t index = 0U;
    while (index < storyboardCompletionSessions.Size()) {
        StoryboardCompletionSession& session =
            storyboardCompletionSessions[index];
        bool completed = true;
        for (Aero::Media::Animation::Runtime::AnimationHandle handle :
             session.handles) {
            const Aero::Media::Animation::Runtime::AnimationState state =
                animations->State(handle);
            if (state ==
                    Aero::Media::Animation::Runtime::AnimationState::Active ||
                state ==
                    Aero::Media::Animation::Runtime::AnimationState::Paused) {
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
                subscription.trigger->GetStoryboard().Get() !=
                    storyboard.Get()) {
                continue;
            }
            Base::Result<bool> allowed = ConditionBehaviorsAllowExecution(
                subscription.trigger->GetBehaviors(),
                *subscription.owner,
                subscription.names);
            if (!allowed) return allowed.GetStatus();
            if (!allowed.Value()) continue;
            for (const Base::Ref<
                     MediaAnimation::TriggerAction>& action :
                 subscription.trigger->GetActions()) {
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

struct ViewFrameResult {
    struct Layout {
        std::uint64_t passVersion = 0U;
        std::uint32_t measuredCount = 0U;
        std::uint32_t arrangedCount = 0U;
        std::uint32_t pendingMeasureCount = 0U;
        std::uint32_t pendingArrangeCount = 0U;
    };
    struct Render {
        std::uint64_t snapshotVersion = 0U;
        std::uint32_t nodeCount = 0U;
        std::uint32_t commandCount = 0U;
        std::uint32_t glyphCommandCount = 0U;
        std::uint32_t dirtyCount = 0U;
        std::uint64_t snapshotHash = 0U;
        std::uint32_t drawPacketCount = 0U;
        std::uint32_t batchCount = 0U;
        std::uint32_t drawCallCount = 0U;
        std::uint32_t mergedPacketCount = 0U;
        std::uint32_t barrierCount = 0U;
        std::uint32_t instanceCount = 0U;
        std::uint32_t stateBindingCount = 0U;
        bool batchingEnabled = true;
    };

    std::uint64_t frameNumber = 0U;
    std::uint32_t callbackCount = 0U;
    Layout layout;
    Render render;
};

namespace {

Base::Status ViewApiInvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status ViewNotInitialized(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotInitialized, message);
}

Base::Result<void> LoadViewResources(
    ViewState& state,
    ResourceLayer layer,
    Base::StringView uri,
    ResourceLoadMode mode = ResourceLoadMode::Replace,
    Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
Base::Result<void> LoadViewCompiledResources(
    ViewState& state,
    ResourceLayer layer,
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    ResourceLoadMode mode = ResourceLoadMode::Replace) noexcept;
void SetViewResourceDictionary(
    ViewState& state,
    ResourceLayer layer,
    Aero::ResourceDictionary& dictionary,
    ResourceLoadMode mode) noexcept;
Base::Result<void> LoadViewBuiltInTheme(
    ViewState& state,
    BuiltInTheme theme) noexcept;
Base::Result<void> MountViewContent(
    ViewState& state,
    Base::Ref<Base::Object> root,
    Aero::Size availableSize) noexcept;
Base::Result<void> MountViewDocument(
    ViewState& state,
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept;
Base::Result<void> ReplaceViewDocument(
    ViewState& state,
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept;
Base::Result<std::uint32_t> AdvanceViewClocks(
    ViewState& state,
    std::uint32_t elapsedMilliseconds) noexcept;

} // namespace

View::View(
    ConstructionToken,
    Gui& gui,
    Base::IAllocator* allocator) noexcept {
    Base::IAllocator* selected = allocator != nullptr
        ? allocator
        : &Base::GetDefaultAllocator();
    void* stateMemory = selected->Allocate({
        sizeof(ViewState), alignof(ViewState), Base::MemoryTag::Markup});
    if (stateMemory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(ViewState), alignof(ViewState), Base::MemoryTag::Markup);
    }
    state_ = new (stateMemory) ViewState(
        *this, gui, *selected, gui.state_);
}

View::~View() noexcept {
    if (state_ == nullptr) return;
    if (!state_->terminal) {
        state_->Shutdown();
        state_->terminal = true;
    }
    state_->publicRenderer.Shutdown();
    Base::IAllocator* allocator = state_->allocator;
    state_->~ViewState();
    allocator->Deallocate(
        state_, sizeof(ViewState), alignof(ViewState), Base::MemoryTag::Markup);
    state_ = nullptr;
}

Base::Result<void> View::Initialize(
    const ViewOptions& options) noexcept {
    if (state_ == nullptr || !state_->gui) {
        return ViewApiInvalidState("View has no Gui state");
    }
    const GuiState& guiState =
        static_cast<const GuiState&>(*state_->gui);
    if (!guiState.initialized) {
        return ViewNotInitialized(
            "Gui must be initialized before creating a View");
    }
    Base::Result<void> initialized = state_->Initialize(options);
    if (!initialized) return initialized.GetStatus();
    if (options.applicationResources != nullptr) {
        SetViewResourceDictionary(
            *state_,
            ResourceLayer::Application,
            *options.applicationResources,
            ResourceLoadMode::Replace);
    }
    return options.loadBuiltInTheme
        ? LoadViewBuiltInTheme(*state_, options.builtInTheme)
        : Base::Result<void>();
}

namespace {

Base::Result<void> LoadViewResources(
    ViewState& state,
    ResourceLayer layer,
    Base::StringView uri,
    ResourceLoadMode mode,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    ViewState* state_ = &state;
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
LoadViewCompiledResources(
    ViewState& state,
    ResourceLayer layer,
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    ResourceLoadMode mode) noexcept {
    ViewState* state_ = &state;
    Base::Result<Aero::ResourceDictionary*> target =
        state_->ResolveResourceLayer(layer);
    if (!target) return target.GetStatus();
    return state_->LoadCompiledResourceLayer(
        bytes,
        originUri,
        *target.Value(),
        mode == ResourceLoadMode::Merge);
}

void SetViewResourceDictionary(
    ViewState& state,
    ResourceLayer layer,
    Aero::ResourceDictionary& dictionary,
    ResourceLoadMode mode) noexcept {
    ViewState* state_ = &state;
    if (state_ == nullptr || !state_->initialized) {
        return;
    }
    if (state_->mounted || state_->root ||
        state_->loadedDocument.root) {
        return;
    }
    Base::Result<Aero::ResourceDictionary*> target =
        state_->ResolveResourceLayer(layer);
    if (!target) return;
    Base::Result<Aero::ResourceDictionary> shared =
        dictionary.Share();
    if (!shared) return;
    if (mode == ResourceLoadMode::Merge) {
        Base::Result<void> merged =
            target.Value()->AddMerged(shared.Value());
        if (!merged) return;
        (void)state_->RebuildDynamicResourceEnvironment();
        return;
    }

    Aero::ResourceDictionary previous =
        std::move(*target.Value());
    *target.Value() = std::move(shared).Value();
    Base::Result<void> rebuilt =
        state_->RebuildDynamicResourceEnvironment();
    if (rebuilt) return;
    *target.Value() = std::move(previous);
    Base::Result<void> restored =
        state_->RebuildDynamicResourceEnvironment();
    (void)restored;
}

Base::Result<void> LoadViewBuiltInTheme(
    ViewState& state,
    BuiltInTheme theme) noexcept {
    ViewState* state_ = &state;
    if (theme != BuiltInTheme::Light &&
        theme != BuiltInTheme::Dark) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Built-in theme value is invalid");
    }
    const std::uint8_t* paletteBytes =
        theme == BuiltInTheme::Light
        ? Aero::AeroThemeLightCompiled
        : Aero::AeroThemeDarkCompiled;
    const std::uint32_t paletteSize =
        theme == BuiltInTheme::Light
        ? Aero::AeroThemeLightCompiledSize
        : Aero::AeroThemeDarkCompiledSize;
    Base::Result<Base::ResourceUri> paletteUri =
        ::Aero::BuiltInThemeUri(
            theme == BuiltInTheme::Light
            ? Base::StringView("Light.xaml")
            : Base::StringView("Dark.xaml"));
    if (!paletteUri) return paletteUri.GetStatus();
    Base::Result<Base::ResourceUri> genericUri =
        ::Aero::BuiltInThemeUri(
            Base::StringView("Generic.xaml"));
    if (!genericUri) return genericUri.GetStatus();

    Aero::ResourceDictionary previous =
        std::move(state_->themeResources);
    Base::Result<void> loaded = paletteSize != 0U
        ? LoadViewCompiledResources(state,
              ResourceLayer::Theme,
              {paletteBytes, paletteSize},
              paletteUri.Value())
        : LoadViewResources(state,
              ResourceLayer::Theme,
              paletteUri.Value().Canonical());
    if (loaded) {
        loaded = Aero::AeroThemeGenericCompiledSize != 0U
            ? LoadViewCompiledResources(state,
                  ResourceLayer::Theme,
                  {Aero::AeroThemeGenericCompiled,
                   Aero::AeroThemeGenericCompiledSize},
                  genericUri.Value(),
                  ResourceLoadMode::Merge)
            : LoadViewResources(state,
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

} // namespace

Base::Result<void> View::SetContent(
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept {
    if (state_ == nullptr || !state_->initialized) {
        return ViewNotInitialized(
            "View must be initialized before SetContent");
    }
    return state_->mounted
        ? ReplaceViewDocument(
              *state_, std::move(document), availableSize)
        : MountViewDocument(
              *state_, std::move(document), availableSize);
}


Base::Result<void> View::SetContent(
    Base::Ref<FrameworkElement> root,
    Aero::Size availableSize) noexcept {
    if (state_ == nullptr || !state_->initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "View must be initialized before SetContent");
    }
    if (root) {
        Markup::XamlDocument document;
        Base::Result<bool> pending =
            GetGui().TakeLoadedDocument(*root, document);
        if (!pending) return pending.GetStatus();
        if (pending.Value()) {
            return SetContent(std::move(document), availableSize);
        }
    }
    if (state_->mounted) {
        Base::Result<void> unmounted = state_->UnmountRoot();
        if (!unmounted) return unmounted.GetStatus();
    }
    return MountViewContent(
        *state_,
        Base::Ref<Base::Object>(std::move(root)),
        availableSize);
}

Base::Result<void> View::SetContent(
    Base::Ref<FrameworkElement> root) noexcept {
    const Aero::Size availableSize = state_ != nullptr
        ? state_->viewport.logicalSize
        : Aero::Size{};
    return SetContent(std::move(root), availableSize);
}

Base::Result<void> View::SetContent(
    Base::Ref<FrameworkElement> root,
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept {
    if (state_ == nullptr || !state_->initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "View must be initialized before SetContent");
    }
    if (!root) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View host root must not be null");
    }
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View cannot mount an empty UI document");
    }
    if (state_->mounted) {
        Base::Result<void> unmounted = state_->UnmountRoot();
        if (!unmounted) return unmounted.GetStatus();
    }

    Markup::LoaderResult next = Aero::Markup::TakeXamlDocument(document);
    if (!next.root) {
        next.Clear();
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View host document has no root object");
    }
    Base::Result<Aero::UIElement*> documentRoot =
        state_->ResolveUIElement(
            *next.root, next.root->RuntimeType());
    if (!documentRoot) {
        next.Clear();
        return Base::Result<void>(documentRoot.GetStatus());
    }
    Base::Result<Aero::UIElement*> hostRoot =
        state_->ResolveUIElement(*root, root->RuntimeType());
    if (!hostRoot) {
        next.Clear();
        return Base::Result<void>(hostRoot.GetStatus());
    }

    // Attach the loaded document root (for example a UserControl) as a visual
    // child of the host root (for example the wrapping Window). The host is
    // kept as the view root so the app-facing Window remains the mounted root.
    Aero::Markup::VisualEdge edge;
    edge.parent = hostRoot.Value();
    edge.child = documentRoot.Value();
    Base::Result<void> pushed =
        next.visualContent.mountEdges.PushBack(std::move(edge));
    if (!pushed) {
        next.Clear();
        return pushed.GetStatus();
    }

    Base::Result<void> assigned =
        ::Aero::Controls::ControlPrivate::SetOwnedContent(
            *static_cast<Controls::ContentControl*>(hostRoot.Value()),
            next.root,
            *documentRoot.Value());
    if (!assigned) {
        next.Clear();
        return assigned.GetStatus();
    }

    next.root = std::move(root);
    state_->loadedDocument = std::move(next);
    return state_->MountRoot(
        state_->loadedDocument.root, availableSize);
}

namespace {

Base::Result<void> MountViewContent(
    ViewState& state,
    Base::Ref<Base::Object> root,
    Aero::Size availableSize) noexcept {
    return state.MountRoot(
        std::move(root), availableSize);
}

Base::Result<void> MountViewDocument(
    ViewState& state,
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept {
    Base::Result<void> ready = state.BeginDocumentLoad();
    if (!ready) return ready.GetStatus();
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View cannot mount an empty UI document");
    }
    Base::Result<void> valid = state.ValidateDocumentRoot(document.Root());
    if (!valid) return valid.GetStatus();
    state.loadedDocument =
        Aero::Markup::TakeXamlDocument(document);
    return state.MountRoot(
        state.loadedDocument.root, availableSize);
}

Base::Result<void> ReplaceViewDocument(
    ViewState& state,
    Markup::XamlDocument&& document,
    Aero::Size availableSize) noexcept {
    ViewState* state_ = &state;
    if (state_ == nullptr || !state_->initialized || !state_->mounted) {
        return ViewApiInvalidState(
            "View document replacement requires a mounted view");
    }
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View cannot replace a document with an empty document");
    }
    Base::Result<void> valid = state_->ValidateDocumentRoot(document.Root());
    if (!valid) return valid.GetStatus();

    Markup::LoaderResult next =
        Aero::Markup::TakeXamlDocument(document);
    if (!next.root ||
        !state_->metadata->Types().IsDerivedFrom(
            next.root->RuntimeType(),
            Aero::Media::Visual::StaticTypeId())) {
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

Base::Result<void> MountViewFragment(
    ViewState& state,
    Controls::ContentControl& host,
    Markup::XamlDocument&& document) noexcept {
    ViewState* state_ = &state;
    if (state_ == nullptr || !state_->initialized || !state_->mounted ||
        state_->tree == nullptr || state_->layout == nullptr) {
        return ViewApiInvalidState(
            "content fragment mounting requires a mounted View");
    }
    if (!document.IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "content fragment document must not be empty");
    }
    if (Aero::ElementPrivate::Tree(host) != state_->tree) {
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
    } else if (::Aero::Controls::ControlPrivate::ContentElement(host) != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
              "content fragment host already owns non-fragment content");
    }
    Base::Result<void> capacity = state_->fragmentMounts.Reserve(
        state_->fragmentMounts.Size() + 1U);
    if (!capacity) return capacity.GetStatus();

    ::Aero::ViewState::FragmentMount fragment;
    fragment.host = &host;
    fragment.document = Aero::Markup::TakeXamlDocument(document);
    const Aero::NameScope* previousActiveNames =
        state_->activeFragmentNames;
    state_->activeFragmentNames = &fragment.document.names;
    const auto restoreActiveNames = [&]() noexcept {
        state_->activeFragmentNames = previousActiveNames;
    };
    Base::Result<Aero::Media::Visual*> rootVisual =
        state_->ResolveVisual(
            *fragment.document.root,
            fragment.document.root->RuntimeType());
    Base::Result<Aero::UIElement*> rootElement =
        state_->ResolveUIElement(
            *fragment.document.root,
            fragment.document.root->RuntimeType());
    if (!rootVisual || !rootElement) {
        restoreActiveNames();
        fragment.document.Clear();
        return !rootVisual
            ? Base::Result<void>(rootVisual.GetStatus())
            : Base::Result<void>(rootElement.GetStatus());
    }
    Base::Result<void> tracked =
        fragment.document.visualContent.AddNode(*rootVisual.Value());
    if (!tracked) {
        restoreActiveNames();
        fragment.document.Clear();
        return tracked.GetStatus();
    }
    Base::Result<void> assigned = ::Aero::Controls::ControlPrivate::SetOwnedContent(host,
        fragment.document.root, *rootElement.Value());
    if (!assigned) {
        restoreActiveNames();
        fragment.document.Clear();
        return assigned.GetStatus();
    }

    ElementTree& context = *state_->tree;
    Base::Result<Aero::ElementAttachment> rootMounted =
        context.AttachElement(host, *rootVisual.Value());
    if (!rootMounted) {
        restoreActiveNames();
        static_cast<void>(host.SetContent(nullptr));
        fragment.document.Clear();
        return rootMounted.GetStatus();
    }
    fragment.rootEdge = std::move(rootMounted).Value();

    const auto detachFailedFragment = [&]() noexcept {
        restoreActiveNames();
        static_cast<void>(state_->DetachFragment(fragment));
    };
    const auto attachEdges = [&](bool deferred) noexcept
        -> Base::Result<void> {
        std::uint32_t attached = 0U;
        for (const Aero::Markup::VisualEdge& edge :
             fragment.document.visualContent.mountEdges) {
            if (edge.state.logicalAttached) ++attached;
        }
        while (attached < fragment.document.visualContent.mountEdges.Size()) {
            bool progressed = false;
            for (Aero::Markup::VisualEdge& edge :
                 fragment.document.visualContent.mountEdges) {
                if (edge.state.logicalAttached || edge.parent == nullptr ||
                    edge.child == nullptr ||
                    Aero::ElementPrivate::Tree(*edge.parent) != state_->tree ||
                    (deferred && Aero::ElementPrivate::Tree(*edge.child) == state_->tree)) {
                    continue;
                }
                Base::Result<Aero::ElementAttachment> mounted =
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
    Markup::EffectRuntimeServices runtimeServices;
    runtimeServices.effectiveValues = state_->values;
    runtimeServices.bindings = state_->bindings;
    runtimeServices.fallbackResources = &state_->dynamicResourceEnvironment;
    runtimeServices.lifetime = state_->effectLifetime;
    Base::Result<void> boundEffects = fragment.document.effects.Bind(runtimeServices);
    if (!boundEffects) {
        detachFailedFragment();
        return boundEffects.GetStatus();
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
    restoreActiveNames();
    Base::Result<void> retained =
        state_->fragmentMounts.PushBack(std::move(fragment));
    if (!retained) {
        static_cast<void>(state_->DetachFragment(fragment));
        return retained.GetStatus();
    }
    return {};
}

Base::Result<void> UnmountViewFragment(
    ViewState& state,
    Controls::ContentControl& host) noexcept {
    ViewState* state_ = &state;
    if (state_ == nullptr || !state_->initialized || !state_->mounted) {
        return ViewApiInvalidState(
            "content fragment unmounting requires a mounted View");
    }
    for (std::uint32_t index = 0U;
         index < state_->fragmentMounts.Size(); ++index) {
        if (state_->fragmentMounts[index].host == &host) {
            return state_->UnmountFragmentAt(index);
        }
    }
    return ::Aero::Controls::ControlPrivate::ContentElement(host) == nullptr
        ? Base::Result<void>()
        : Base::Result<void>(Base::Status::Failure(
              Base::ErrorCode::InvalidState,
              "content host does not contain a mounted XAML fragment"));
}

} // namespace

Base::Result<void> Markup::XamlReader::MountFragment(
    View& view,
    Controls::ContentControl& host,
    Markup::XamlDocument&& document) noexcept {
    if (gui_ == nullptr || &view.GetGui() != gui_ || view.state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML fragment View does not belong to this reader");
    }
    return MountViewFragment(
        *view.state_, host, std::move(document));
}

Base::Result<void> Markup::XamlReader::UnmountFragment(
    View& view,
    Controls::ContentControl& host) noexcept {
    if (gui_ == nullptr || &view.GetGui() != gui_ || view.state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML fragment View does not belong to this reader");
    }
    return UnmountViewFragment(*view.state_, host);
}

Base::Result<void> View::SetViewport(
    const ViewViewport& viewport) noexcept {
    if (state_ == nullptr || !state_->initialized) {
        return ViewNotInitialized(
            "View must be initialized before setting its viewport");
    }
    Base::Result<void> valid = ValidateViewport(viewport);
    if (!valid) return valid.GetStatus();
    return state_->ApplyViewport(viewport);
}

void View::SetSize(
    Aero::Size availableSize) noexcept {
    if (state_ == nullptr || !state_->initialized) return;
    Base::Result<ViewViewport> viewport =
        Aero::MakeLogicalViewport(
            availableSize,
            state_->viewport.dpiScale);
    if (!viewport) return;
    static_cast<void>(SetViewport(viewport.Value()));
}

void View::SetSize(
    std::uint32_t width,
    std::uint32_t height) noexcept {
    SetSize(Aero::Size{
        static_cast<double>(width),
        static_cast<double>(height)});
}

void View::SetScale(double scale) noexcept {
    if (state_ == nullptr || !state_->initialized ||
        !std::isfinite(scale) || scale <= 0.0) {
        return;
    }
    Base::Result<ViewViewport> viewport =
        Aero::MakeLogicalViewport(
            state_->viewport.logicalSize, scale);
    if (!viewport) return;
    static_cast<void>(SetViewport(viewport.Value()));
}

bool View::Update(double timeInSeconds) noexcept {
    if (!active_ || state_ == nullptr) return false;
    if (!std::isfinite(timeInSeconds) || timeInSeconds < 0.0 ||
        (hasUpdateTime_ && timeInSeconds < updateTimeSeconds_)) {
        state_->ReportUpdateFailure(Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View update time must be finite, nonnegative and monotonic"));
        return false;
    }
    const ::Aero::Render::RenderFrame* before =
        ViewState::CurrentFrame(*this);
    const std::uint64_t beforeVersion =
        before != nullptr ? before->Version() : 0U;
    double elapsedSeconds = 0.0;
    if (hasUpdateTime_) {
        elapsedSeconds = timeInSeconds - updateTimeSeconds_;
    }
    updateTimeSeconds_ = timeInSeconds;
    hasUpdateTime_ = true;
    const double elapsedMilliseconds = std::min(
        elapsedSeconds * 1000.0,
        static_cast<double>(UINT32_MAX));
    const std::uint32_t elapsed =
        static_cast<std::uint32_t>(elapsedMilliseconds);
    if (elapsed != 0U) {
        Base::Result<std::uint32_t> advanced =
            AdvanceViewClocks(*state_, elapsed);
        if (!advanced) {
            state_->ReportUpdateFailure(advanced.GetStatus());
            return false;
        }
    }
    Base::Result<std::uint32_t> frame = state_->ExecuteFrame(*this);
    if (!frame) {
        state_->ReportUpdateFailure(frame.GetStatus());
        return false;
    }
    state_->ClearUpdateFailure();
    const ::Aero::Render::RenderFrame* after =
        ViewState::CurrentFrame(*this);
    return after != nullptr && after->Version() != 0U &&
        after->Version() != beforeVersion;
}

void View::Activate() noexcept {
    active_ = true;
    hasUpdateTime_ = false;
}

void View::Deactivate() noexcept {
    active_ = false;
    hasUpdateTime_ = false;
}

Base::Result<std::uint32_t> ViewState::ExecuteFrame(
    View& view) noexcept {
    ViewState* state_ = this;
    if (!state_->initialized) {
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
    GuiState& guiState = static_cast<GuiState&>(*state_->gui);
    const bool fontProviderChanged =
        guiState.fontChangeGeneration != state_->seenFontProviderChange;
    if (fontProviderChanged) {
        state_->seenFontProviderChange = guiState.fontChangeGeneration;
    }
    if (state_->images != nullptr &&
        guiState.textureChangeGeneration !=
            state_->seenTextureProviderChange) {
        if (guiState.textureChangesLost) {
            state_->images->Invalidate({}, state_->GetImageResources());
        } else {
            for (const XamlProviderChangeRecord& change :
                 guiState.textureChanges) {
                if (change.generation <=
                        state_->seenTextureProviderChange) {
                    continue;
                }
                state_->images->Invalidate(
                    change.uri,
                    state_->GetImageResources());
            }
        }
        state_->seenTextureProviderChange =
            guiState.textureChangeGeneration;
    }
    if (state_->device) {
        const Base::Status deviceStatus =
            ::Aero::Render::RenderDeviceBase::FrameStatus(
                *state_->device);
        if (!deviceStatus.IsOk()) {
            return deviceStatus;
        }
        const std::uint64_t generation =
            state_->device->Generation();
        if (generation !=
            state_->deviceGeneration) {
            deviceGenerationChanged = true;
            Aero::Media::Visual* rootVisual =
                state_->RootVisual();
            if (rootVisual != nullptr) {
                Base::Result<void> invalidated =
                    state_->renderer->Invalidate(
                        *rootVisual,
                        Aero::Render::RenderInvalidation::All);
                if (!invalidated) {
                    return invalidated.GetStatus();
                }
            }
            state_->VisitPaths(
                rootVisual,
                state_->GetMeshResources(),
                true);
            state_->elementHost.meshResources =
                state_->GetMeshResources();
            state_->deviceGeneration = generation;
        }
    }
    if (state_->text != nullptr) {
        Base::Result<bool> synchronized =
            state_->text->SynchronizeBackend(
                *state_->device,
                state_->publicRenderer.Resources().text,
                deviceGenerationChanged || fontProviderChanged);
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
                state_->xamlRuntime->Providers(),
                guiState.textureProvider.Get(),
                state_->GetImageResources(),
                deviceGenerationChanged);
        if (!synchronized) {
            return synchronized.GetStatus();
        }
        if (synchronized.Value()) {
            Aero::Media::Visual* rootVisual =
                state_->RootVisual();
            if (rootVisual != nullptr) {
                Base::Result<void> invalidated =
                    state_->renderer->Invalidate(
                        *rootVisual,
                        Aero::Render::RenderInvalidation::All);
                if (!invalidated) {
                    return invalidated.GetStatus();
                }
            }
        }
    }
    const ::Aero::Threading::DispatcherFramePhase phases[] = {
        ::Aero::Threading::DispatcherFramePhase::BeginFrame,
        ::Aero::Threading::DispatcherFramePhase::Input,
        ::Aero::Threading::DispatcherFramePhase::PropertyChanges,
        ::Aero::Threading::DispatcherFramePhase::DataBind,
        ::Aero::Threading::DispatcherFramePhase::Animation,
        ::Aero::Threading::DispatcherFramePhase::Lifecycle,
        ::Aero::Threading::DispatcherFramePhase::Layout,
        ::Aero::Threading::DispatcherFramePhase::RenderCommit,
        ::Aero::Threading::DispatcherFramePhase::EndFrame};
    ViewFrameResult result;
    for (::Aero::Threading::DispatcherFramePhase phase : phases) {
        if (phase ==
                ::Aero::Threading::DispatcherFramePhase::Layout &&
            state_->HasAttachedRoot()) {
            Base::Result<void> completed =
                state_->CompleteVisualEdges({
                    state_->loadedDocument.visualContent.mountEdges.Data(),
                    state_->loadedDocument.visualContent.mountEdges.Size()});
            if (!completed) return completed.GetStatus();
        }
        if (phase ==
            ::Aero::Threading::DispatcherFramePhase::
                RenderCommit) {
            ::Aero::Media::CompositionTarget::RaiseRendering(view);
            Base::Result<void> overlays =
                state_->SynchronizeOverlays();
            if (!overlays) {
                return overlays.GetStatus();
            }
        }
        Base::Result<std::uint32_t> ran =
            state_->dispatcher->RunFramePhase(phase);
        if (!ran) return ran.GetStatus();
        if (phase ==
                ::Aero::Threading::DispatcherFramePhase::Lifecycle) {
            Base::Result<std::uint32_t> focused =
                state_->ProcessPendingFocus();
            if (!focused) return focused.GetStatus();
            if (result.callbackCount >
                UINT32_MAX - focused.Value()) {
                return Base::Status::Failure(
                    Base::ErrorCode::OutOfRange,
                    "View callback count overflow");
            }
            result.callbackCount += focused.Value();
        }
        if (phase ==
                ::Aero::Threading::DispatcherFramePhase::DataBind) {
            Base::Result<void> generatedVisualsFlushed =
                state_->FlushGeneratedVisuals();
            if (!generatedVisualsFlushed) {
                return generatedVisualsFlushed.GetStatus();
            }
        }
        if (phase == ::Aero::Threading::DispatcherFramePhase::Layout &&
            !state_->layout->LastFlushStatus().IsOk()) {
            return state_->layout->LastFlushStatus();
        }
        if (phase ==
                ::Aero::Threading::DispatcherFramePhase::Layout &&
            state_->layout->Diagnostics().arrangedCount != 0U) {
            for (auto& behavior : state_->attachedBehaviorInstances) {
                if (behavior.instance) {
                    behavior.instance->NotifyLayoutUpdated();
                }
            }
            Aero::Media::Visual* rootVisual =
                state_->RootVisual();
            if (rootVisual != nullptr &&
                state_->renderer != nullptr) {
                Base::Result<void> invalidated =
                    state_->renderer->Invalidate(
                        *rootVisual,
                        Aero::Render::RenderInvalidation::All);
                if (!invalidated) {
                    return invalidated.GetStatus();
                }
            }
        }
        if (phase == ::Aero::Threading::DispatcherFramePhase::Animation &&
            state_->animations != nullptr) {
            const Base::Status animationStatus =
                state_->animations->LastTickStatus();
            if (!animationStatus.IsOk()) {
                return animationStatus;
            }
            const auto animationDiagnostics =
                state_->animations->Diagnostics();
            if (animationDiagnostics.appliedValueCount != 0U) {
                Aero::Media::Visual* rootVisual = state_->RootVisual();
                if (rootVisual != nullptr &&
                    state_->renderer != nullptr) {
                    Base::Result<void> invalidated =
                        state_->renderer->Invalidate(
                            *rootVisual,
                            Aero::Render::RenderInvalidation::All);
                    if (!invalidated) return invalidated.GetStatus();
                }
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
        if (phase == ::Aero::Threading::DispatcherFramePhase::Lifecycle &&
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
            ::Aero::Threading::DispatcherFramePhase::RenderCommit) {
            const Base::Status committed =
                state_->renderer->LastCommitStatus();
            if (!committed.IsOk()) return committed;
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
    const ::Aero::Render::RenderDiagnostics render =
        state_->renderer->Diagnostics();
    result.render.snapshotVersion = render.commitVersion;
    result.render.nodeCount = render.nodeCount;
    result.render.commandCount = render.commandCount;
    result.render.glyphCommandCount =
        render.glyphCommandCount;
    result.render.dirtyCount = render.dirtyCount;
    result.render.snapshotHash = render.frameHash;
    if (state_->device) {
        const Diagnostics::RenderFrameStatistics deviceStatistics =
            Diagnostics::GetLastRenderFrameStatistics(*state_->device);
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
    return result.callbackCount;
}

namespace {

Base::Result<Input::PointerDispatchResult> DispatchPointer(
    ViewState& state,
    const Input::PointerInput& input) noexcept {
    if (!state.mounted || state.input == nullptr) {
        return ViewNotInitialized(
            "Pointer input requires a mounted View");
    }
    Base::Result<
        Input::PointerDispatchResult>
        dispatched =
            state.input->DispatchPointer(input);
    if (!dispatched) {
        return dispatched.GetStatus();
    }
    Aero::UIElement* target =
        dispatched.Value().hit.target;
    Base::Result<void> dismissed =
        state.DismissOverlaysForPointer(
            input, target);
    if (!dismissed) {
        return dismissed.GetStatus();
    }
    Base::Result<void> toolTip =
        state.UpdateToolTipForPointer(
            input, target);
    if (!toolTip) {
        return toolTip.GetStatus();
    }
    Base::Result<void> contextMenu =
        state.OpenContextMenuForPointer(
            input, target);
    if (!contextMenu) {
        return contextMenu.GetStatus();
    }
    return dispatched;
}

Base::Result<Input::KeyboardDispatchResult>
DispatchKeyboard(
    ViewState& state,
    const Input::KeyboardInput& input) noexcept {
    if (!state.mounted || state.input == nullptr) {
        return ViewNotInitialized(
            "Keyboard input requires a mounted View");
    }
    if (input.action ==
            Input::KeyboardAction::Down &&
        input.key ==
            Input::KeyboardKeyEscape &&
        state.input->IsDragging()) {
        return state.input->DispatchKeyboard(input);
    }
    if (input.action ==
            Input::KeyboardAction::Down &&
        input.key ==
            Input::KeyboardKeyEscape) {
        Base::Result<bool> dismissed =
            state.DismissTopOverlayForEscape();
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
    return state.input->DispatchKeyboard(input);
}

Base::Result<Input::TextInputDispatchResult>
DispatchText(
    ViewState& state,
    const Input::TextInput& input) noexcept {
    if (!state.mounted || state.input == nullptr) {
        return ViewNotInitialized(
            "Text input requires a mounted View");
    }
    return state.input->DispatchText(input);
}

} // namespace

bool View::MouseMove(int x, int y) noexcept {
    if (!active_) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Move;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    Base::Result<Input::PointerDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchPointer(*state_, input)
        : Base::Result<Input::PointerDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

bool View::MouseButtonDown(
    int x,
    int y,
    Input::MouseButton button) noexcept {
    if (!active_) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Down;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    input.changedButton = button;
    Base::Result<Input::PointerDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchPointer(*state_, input)
        : Base::Result<Input::PointerDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

bool View::MouseButtonUp(
    int x,
    int y,
    Input::MouseButton button) noexcept {
    if (!active_) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Up;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    input.changedButton = button;
    Base::Result<Input::PointerDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchPointer(*state_, input)
        : Base::Result<Input::PointerDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

bool View::MouseDoubleClick(
    int x,
    int y,
    Input::MouseButton button) noexcept {
    if (!active_) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Down;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    input.changedButton = button;
    input.clickCount = 2U;
    Base::Result<Input::PointerDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchPointer(*state_, input)
        : Base::Result<Input::PointerDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

bool View::MouseWheel(
    int x,
    int y,
    int delta) noexcept {
    if (!active_) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Wheel;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    input.wheelDeltaY = static_cast<double>(delta);
    Base::Result<Input::PointerDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchPointer(*state_, input)
        : Base::Result<Input::PointerDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

bool View::MouseHWheel(
    int x,
    int y,
    int delta) noexcept {
    if (!active_) return false;
    Input::PointerInput input;
    input.pointerId = 0U;
    input.action = Input::PointerAction::Wheel;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    input.wheelDeltaX = static_cast<double>(delta);
    Base::Result<Input::PointerDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchPointer(*state_, input)
        : Base::Result<Input::PointerDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

bool View::KeyDown(Input::Key key) noexcept {
    if (!active_) return false;
    Input::KeyboardInput input;
    input.action = Input::KeyboardAction::Down;
    input.key = static_cast<std::uint32_t>(key);
    Base::Result<Input::KeyboardDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchKeyboard(*state_, input)
        : Base::Result<Input::KeyboardDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

bool View::KeyUp(Input::Key key) noexcept {
    if (!active_) return false;
    Input::KeyboardInput input;
    input.action = Input::KeyboardAction::Up;
    input.key = static_cast<std::uint32_t>(key);
    Base::Result<Input::KeyboardDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchKeyboard(*state_, input)
        : Base::Result<Input::KeyboardDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

bool View::Char(std::uint32_t codePoint) noexcept {
    if (!active_ || codePoint > 0x10FFFFU ||
        (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
        return false;
    }
    char text[4]{};
    std::uint32_t length = 0U;
    if (codePoint <= 0x7FU) {
        text[length++] = static_cast<char>(codePoint);
    } else if (codePoint <= 0x7FFU) {
        text[length++] = static_cast<char>(0xC0U | (codePoint >> 6U));
        text[length++] = static_cast<char>(0x80U | (codePoint & 0x3FU));
    } else if (codePoint <= 0xFFFFU) {
        text[length++] = static_cast<char>(0xE0U | (codePoint >> 12U));
        text[length++] = static_cast<char>(
            0x80U | ((codePoint >> 6U) & 0x3FU));
        text[length++] = static_cast<char>(0x80U | (codePoint & 0x3FU));
    } else {
        text[length++] = static_cast<char>(0xF0U | (codePoint >> 18U));
        text[length++] = static_cast<char>(
            0x80U | ((codePoint >> 12U) & 0x3FU));
        text[length++] = static_cast<char>(
            0x80U | ((codePoint >> 6U) & 0x3FU));
        text[length++] = static_cast<char>(0x80U | (codePoint & 0x3FU));
    }
    Base::Result<Input::TextInputDispatchResult> dispatched =
        state_ != nullptr
        ? DispatchText(
              *state_, {Base::StringView(text, length)})
        : Base::Result<Input::TextInputDispatchResult>(
              ViewNotInitialized("View has no implementation"));
    return dispatched && dispatched.Value().routed;
}

namespace {

bool DispatchTouch(
    ViewState* state,
    Input::PointerAction action,
    int x,
    int y,
    std::uint64_t id) noexcept {
    if (id >= static_cast<std::uint64_t>(UINT32_MAX)) return false;
    Input::PointerInput input;
    input.pointerId = static_cast<std::uint32_t>(id) + 1U;
    input.action = action;
    input.position = {
        static_cast<double>(x),
        static_cast<double>(y)};
    if (state == nullptr) return false;
    Base::Result<Input::PointerDispatchResult> dispatched =
        DispatchPointer(*state, input);
    return dispatched && dispatched.Value().routed;
}

} // namespace

bool View::TouchDown(
    int x,
    int y,
    std::uint64_t id) noexcept {
    return active_ && DispatchTouch(
        state_, Input::PointerAction::Down, x, y, id);
}

bool View::TouchMove(
    int x,
    int y,
    std::uint64_t id) noexcept {
    return active_ && DispatchTouch(
        state_, Input::PointerAction::Move, x, y, id);
}

bool View::TouchUp(
    int x,
    int y,
    std::uint64_t id) noexcept {
    return active_ && DispatchTouch(
        state_, Input::PointerAction::Up, x, y, id);
}

namespace {

Base::Result<std::uint32_t> AdvanceViewClocks(
    ViewState& state,
    std::uint32_t elapsedMilliseconds) noexcept {
    ViewState* state_ = &state;
    if (!state_->mounted || state_->animations == nullptr) {
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
            static_cast<Aero::Media::Animation::AnimationTime>(
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

[[maybe_unused]] Base::Result<std::uint32_t> AdvanceViewAnimations(
    ViewState& state,
    std::uint32_t elapsedMilliseconds) noexcept {
    ViewState* state_ = &state;
    if (!state_->mounted || state_->animations == nullptr) {
        return ViewNotInitialized(
            "Animation timing requires a mounted View");
    }
    Base::Result<std::uint32_t> advanced =
        state_->animations->AdvanceBy(
        static_cast<Aero::Media::Animation::AnimationTime>(
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

} // namespace

ViewRenderer::ViewRenderer(
    View& view,
    Base::IAllocator& allocator) noexcept
    : allocator_(&allocator), view_(&view) {}

ViewRenderer::~ViewRenderer() noexcept {
    Shutdown();
}

Base::Result<void> ViewRenderer::Init(
    Base::Ref<RenderDevice> device) noexcept {
    if (view_ == nullptr ||
        view_->state_ == nullptr ||
        !view_->state_->initialized) {
        return ViewNotInitialized(
            "Renderer requires an initialized View");
    }
    if (!device) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Renderer requires a RenderDevice");
    }
    if (initialized_) {
        return device_.Get() == device.Get()
            ? Base::Result<void>()
            : Base::Result<void>(Base::Status::Failure(
                  Base::ErrorCode::AlreadyExists,
                  "Renderer is already initialized"));
    }

    Base::Status deviceStatus =
        Render::RenderDeviceBase::FrameStatus(*device);
    if (!deviceStatus.IsOk()) {
        return deviceStatus;
    }

    auto& data = *view_->state_;
    Base::Ref<RenderDevice> previous =
        data.device;
    const bool changingDevice =
        previous.Get() != device.Get();
    if (changingDevice && previous) {
        Base::Result<void> idle =
            previous->WaitIdle();
        if (!idle) return idle.GetStatus();

        Aero::Render::ImageResources*
            previousImages = data.GetImageResources();
        if (data.images != nullptr) {
            data.images->ReleaseBackendResources(
                previousImages);
        }
        data.VisitTextElements(
            data.RootVisual(), nullptr);
        if (data.text != nullptr) {
            Base::Result<bool> detached =
                data.text->SynchronizeBackend(
                    *previous, nullptr, true);
            if (!detached) return detached.GetStatus();
        }
        data.VisitPaths(
            data.RootVisual(), nullptr);
        data.elementHost.meshResources = nullptr;
        ShutdownRenderResources();
    }

    if (!frameEncoder_.has_value()) {
        Render::RenderDeviceBase* backend =
            Render::RenderDeviceBase::From(*device);
        if (backend == nullptr) {
            return ViewNotInitialized(
                "Renderer requires a native RenderDevice state");
        }
        Base::Result<void> prepared = InitializeRenderResources(
            *backend,
            Render::RenderDeviceBase::BackendGeneration(*device));
        if (!prepared) {
            ShutdownRenderResources();
            return prepared.GetStatus();
        }
    }

    Base::Result<void> status;
    if (data.text != nullptr) {
        Base::Result<bool> synchronized =
            data.text->SynchronizeBackend(
                *device,
                Resources().text,
                true);
        if (!synchronized) {
            status = synchronized.GetStatus();
        }
    }
    data.device = device;
    data.deviceGeneration =
        device->Generation();

    if (data.text != nullptr) {
        data.VisitTextElements(
            data.RootVisual(),
            data.text->Layout(),
            true);
    }
    if (status && data.images != nullptr) {
        Base::Result<bool> synchronized =
            data.images->Synchronize(
                data.RootVisual(),
                data.loadedDocument.canonicalUri,
                data.xamlRuntime->Providers(),
                static_cast<GuiState&>(*data.gui).
                    textureProvider.Get(),
                data.GetImageResources(),
                true);
        if (!synchronized) {
            status = synchronized.GetStatus();
        }
    }
    if (status) {
        data.VisitPaths(
            data.RootVisual(),
            data.GetMeshResources(),
            true);
        data.elementHost.meshResources =
            data.GetMeshResources();
        Aero::Media::Visual* rootVisual =
            data.RootVisual();
        if (rootVisual != nullptr) {
            status = data.renderer->Invalidate(
                *rootVisual,
                Aero::Render::RenderInvalidation::All);
        }
    }
    if (!status) {
        return status.GetStatus();
    }

    device_ = std::move(device);
    updatedVersion_ = 0U;
    renderedVersion_ = 0U;
    offscreenReady_ = false;
    initialized_ = true;
    return {};
}

void ViewRenderer::Shutdown() noexcept {
    if (device_) {
        static_cast<void>(device_->WaitIdle());
    }
    if (frameEncoder_.has_value() && view_ != nullptr &&
        view_->state_ != nullptr) {
        ViewState& data = *view_->state_;
        if (data.images != nullptr) {
            data.images->ReleaseBackendResources(
                Resources().images);
        }
        if (data.text != nullptr && device_) {
            static_cast<void>(data.text->SynchronizeBackend(
                *device_, nullptr, true));
        }
        data.VisitTextElements(data.RootVisual(), nullptr);
        data.VisitPaths(data.RootVisual(), nullptr);
        data.elementHost.meshResources = nullptr;
    }
    ShutdownRenderResources();
    device_.Reset();
    updatedVersion_ = 0U;
    renderedVersion_ = 0U;
    offscreenReady_ = false;
    initialized_ = false;
}

bool ViewRenderer::IsInitialized() const noexcept {
    return initialized_;
}

bool ViewRenderer::UpdateRenderTree() noexcept {
    if (!initialized_ || !device_ ||
        view_ == nullptr || view_->state_ == nullptr ||
        !view_->state_->initialized) {
        if (view_ != nullptr && view_->state_ != nullptr) {
            view_->state_->ReportRendererFailure(ViewNotInitialized(
                "Renderer must be initialized before UpdateRenderTree"));
        }
        return false;
    }

    Base::Status deviceStatus =
        Render::RenderDeviceBase::FrameStatus(*device_);
    if (!deviceStatus.IsOk()) {
        view_->state_->ReportRendererFailure(deviceStatus);
        return false;
    }

    auto& data = *view_->state_;
    if (data.renderer == nullptr) {
        data.ReportRendererFailure(ViewNotInitialized(
            "View render tree is unavailable"));
        return false;
    }
    const ::Aero::Render::RenderFrame& frame =
        data.renderer->CurrentFrame();
    if (frame.Version() == 0U) {
        data.ClearRendererFailure();
        return false;
    }
    Base::Result<void> valid =
        ::Aero::Render::ValidateRenderFrame(frame);
    if (!valid) {
        data.ReportRendererFailure(valid.GetStatus());
        return false;
    }

    const bool changed =
        frame.Version() != updatedVersion_;
    if (changed) {
        updatedVersion_ = frame.Version();
        offscreenReady_ = false;
    }
    data.ClearRendererFailure();
    return changed;
}

bool ViewRenderer::RenderOffscreen() noexcept {
    if (!initialized_ || !device_ || !frameEncoder_.has_value() ||
        view_ == nullptr || view_->state_ == nullptr) {
        if (view_ != nullptr && view_->state_ != nullptr) {
            view_->state_->ReportRendererFailure(ViewNotInitialized(
                "Renderer must be initialized before RenderOffscreen"));
        }
        return false;
    }

    const ::Aero::Render::RenderFrame& frame =
        view_->state_->renderer->CurrentFrame();
    if (frame.Version() == 0U) {
        offscreenReady_ = true;
        view_->state_->ClearRendererFailure();
        return true;
    }
    if (frame.PixelWidth() == 0U || frame.PixelHeight() == 0U) {
        offscreenReady_ = true;
        view_->state_->ClearRendererFailure();
        return true;
    }
    if (frame.Version() != updatedVersion_) {
        view_->state_->ReportRendererFailure(ViewApiInvalidState(
            "UpdateRenderTree must run before RenderOffscreen"));
        return false;
    }

    Base::Result<::Aero::Graphics::FenceValue> submitted =
        RenderOffscreenFrame(frame);
    if (!submitted) {
        Render::RenderDeviceBase::RefreshHealth(*device_);
        view_->state_->ReportRendererFailure(submitted.GetStatus());
        return false;
    }
    offscreenReady_ = true;
    view_->state_->ClearRendererFailure();
    return true;
}

void ViewRenderer::Render(
    RenderTarget& target) noexcept {
    if (!initialized_ || !device_ || !frameEncoder_.has_value() ||
        view_ == nullptr || view_->state_ == nullptr) {
        if (view_ != nullptr && view_->state_ != nullptr) {
            view_->state_->ReportRendererFailure(ViewNotInitialized(
                "Renderer must be initialized before Render"));
        }
        return;
    }

    Base::Ref<RenderDevice> surfaceDevice = target.GetDevice();
    if (!surfaceDevice || surfaceDevice.Get() != device_.Get()) {
        view_->state_->ReportRendererFailure(ViewApiInvalidState(
            "RenderTarget must belong to the renderer RenderDevice"));
        return;
    }

    const ::Aero::Render::RenderFrame& frame =
        view_->state_->renderer->CurrentFrame();
    if (frame.Version() == 0U) {
        view_->state_->ClearRendererFailure();
        return;
    }
    if (frame.Version() != updatedVersion_) {
        view_->state_->ReportRendererFailure(ViewApiInvalidState(
            "UpdateRenderTree must run before Render"));
        return;
    }
    if (!offscreenReady_) {
        view_->state_->ReportRendererFailure(ViewApiInvalidState(
            "RenderOffscreen must run before Render"));
        return;
    }
    if (frame.PixelWidth() == 0U || frame.PixelHeight() == 0U) {
        renderedVersion_ = frame.Version();
        offscreenReady_ = false;
        view_->state_->ClearRendererFailure();
        return;
    }

    Base::Result<void> submitted =
        Render::RenderTargetServices::Render(
            target, *this, frame);
    if (!submitted) {
        view_->state_->ReportRendererFailure(submitted.GetStatus());
        return;
    }

    renderedVersion_ = frame.Version();
    offscreenReady_ = false;
    view_->state_->ClearRendererFailure();
}

IRenderer& View::GetRenderer() noexcept {
    return state_->publicRenderer;
}

const IRenderer& View::GetRenderer() const noexcept {
    return state_->publicRenderer;
}

FrameworkElement* View::GetContent() noexcept {
    return state_ != nullptr && state_->RootVisual() != nullptr
        ? state_->RootVisual()->AsFrameworkElement()
        : nullptr;
}

const FrameworkElement* View::GetContent() const noexcept {
    return state_ != nullptr && state_->RootVisual() != nullptr
        ? state_->RootVisual()->AsFrameworkElement()
        : nullptr;
}

Gui& View::GetGui() noexcept {
    return *state_->guiOwner;
}

const Gui& View::GetGui() const noexcept {
    return *state_->guiOwner;
}

const ::Aero::Render::RenderFrame* ViewState::CurrentFrame(
    const View& view) noexcept {
    return view.state_ != nullptr && view.state_->renderer != nullptr
        ? &view.state_->renderer->CurrentFrame()
        : nullptr;
}

const ::Aero::Render::RenderFrame* CurrentFrameForConformance(
    const View& view) noexcept {
    return ViewState::CurrentFrame(view);
}

} // namespace Aero
