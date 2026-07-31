#pragma once

#include "ThemeStyleRegistry.hpp"

#include "../runtime/RuntimeFwd.hpp"
#include "../core/property/PropertyProviderSession.hpp"
#include "../data/BindingRuntime.hpp"

// Private runtime declarations extracted from public authoring headers.
// These services are owned by View/runtime composition and are not part
// of the normal WPF control-authoring surface.
#include "ObjectTree.hpp"
#include <Aero/Input.hpp>
#include <Aero/Layout.hpp>
#include <Aero/Data.hpp>
#include "../media/AnimationRuntimeTypes.hpp"
#include <Aero/Styling.hpp>

namespace Aero::Detail {

using namespace Aero::Core;
using namespace Aero::Input;
using namespace Aero::Data;
using namespace Aero::Detail::Animation;

class AERO_API UiRuntimeAccess::RoutedEventManager final {
public:
    explicit RoutedEventManager(void* eventState) noexcept;
    ~RoutedEventManager() noexcept;

    template<class TArgs>
    Base::Result<void> RegisterClassHandler(
        RoutedEventHandle event,
        TypeId classType,
        const Base::Delegate<void(Base::Object*, const TArgs&)>& handler,
        bool handledEventsToo = false) noexcept;

    Base::Result<void> RaiseEvent(
        UIElement& source,
        RoutedEventHandle event,
        RoutedEventArgs* args = nullptr) noexcept;

private:
    struct ClassHandlerRecord final {
        RoutedEventHandle event;
        TypeId classType = InvalidTypeId;
        Aero::Detail::RoutedHandlerStorage handler;
        std::uint64_t sequence = 0U;
        bool handledEventsToo = false;
    };

    void* eventState_ = nullptr;
    Base::Vector<ClassHandlerRecord> classHandlers_;
    std::uint64_t nextClassSequence_ = 1U;
    std::uint32_t raiseDepth_ = 0U;

    Base::Result<void> BuildRoute(
        Visual& source,
        RoutingStrategy strategy,
        Base::Vector<Aero::Detail::VisualLease>& route) noexcept;
    void InvokeNode(Visual& node, RoutedEventArgs& args) noexcept;
    void CleanupClassHandlers() noexcept;
    Base::Result<void> ValidateClassHandler(
        RoutedEventHandle event,
        TypeId classType,
        TypeId eventArgsType) const noexcept;
};
class AERO_API UiRuntimeAccess::CommandManager final {
public:
    explicit CommandManager(ObjectTree& tree) noexcept;

    Base::Result<CommandBindingHandle> TryAddBinding(
        UIElement& owner,
        const CommandBinding& binding) noexcept;
    Base::Result<bool> RemoveBinding(
        CommandBindingHandle handle) noexcept;
    Base::Result<InputBindingHandle> TryAddInputBinding(
        UIElement& owner,
        Base::Ref<KeyBinding> binding) noexcept;

    Base::Result<bool> CanExecute(
        ICommand& command,
        const Core::Value& parameter,
        UIElement& target) noexcept;
    Base::Result<bool> Execute(
        ICommand& command,
        const Core::Value& parameter,
        UIElement& target) noexcept;
    Base::Result<bool> CanExecute(
        RoutedCommand& command,
        const Core::Value& parameter,
        UIElement& target) noexcept;
    Base::Result<bool> Execute(
        RoutedCommand& command,
        const Core::Value& parameter,
        UIElement& target) noexcept;
    Base::Result<bool> ProcessInput(
        UIElement& target,
        const KeyboardInput& input) noexcept;

    Base::Result<void> TryAddRequerySuggested(
        const RequerySuggestedHandler& handler) noexcept;
    bool RemoveRequerySuggested(
        const RequerySuggestedHandler& handler) noexcept;
    void InvalidateRequerySuggested() const noexcept;

private:
    struct BindingRecord final {
        CommandBindingHandle handle;
        VisualHandle owner;
        CommandBinding binding;
    };
    struct RouteBinding final {
        VisualHandle owner;
        CommandBinding binding;
    };
    struct InputBindingRecord final {
        InputBindingHandle handle;
        VisualHandle owner;
        Base::Ref<KeyBinding> binding;
    };

    ObjectTree* tree_ = nullptr;
    Base::Vector<BindingRecord> bindings_;
    Base::Vector<InputBindingRecord> inputBindings_;
    std::uint64_t nextBinding_ = 1U;
    std::uint64_t nextInputBinding_ = 1U;
    RequerySuggestedHandler requerySuggested_;

    Base::Result<void> VerifyTarget(UIElement& target) const noexcept;
    Base::Result<void> SnapshotRoute(
        UIElement& target,
        RoutedCommand* command,
        Base::Vector<RouteBinding>& route) noexcept;
    void PruneStaleBindings() noexcept;
    void PruneStaleInputBindings() noexcept;
};
class AERO_API UiRuntimeAccess::HitTestManager final {
public:
    HitTestManager() noexcept = default;
    Base::Result<void> SetOverlays(
        Base::Span<UIElement* const> overlays,
        Base::Span<const Point> origins) noexcept;
    void ClearOverlays() noexcept {
        overlays_.Clear();
    }
    Base::Result<HitTestResult> HitTest(
        Visual& root, Point position) const noexcept;
    // Converts a position expressed in root coordinates to target-local
    // coordinates. Unlike HitTest(), capture routing intentionally does not
    // test visibility, clipping, or bounds.
    Base::Result<HitTestResult> RootToLocal(
        Visual& root, Visual& target, Point position) const noexcept;

private:
    struct OverlayRecord final {
        UIElement* element = nullptr;
        Point origin;
    };
    Base::Vector<OverlayRecord> overlays_;
    static UIElement* AsUIElement(Visual& node) noexcept {
        return node.AsUIElement();
    }
    Base::Result<HitTestResult> HitTestElement(
        UIElement& element, Point position) const noexcept;
    bool IsOverlay(
        const UIElement& element) const noexcept;
};
class AERO_API UiRuntimeAccess::PointerInputManager final {
public:
    PointerInputManager(HitTestManager& hitTests, RoutedEventManager& events,
        Visual& root) noexcept;

    Base::Result<PointerDispatchResult> Dispatch(
        const PointerInput& input) noexcept;
    Base::Result<void> CapturePointer(
        std::uint32_t pointerId, UIElement& target) noexcept;
    Base::Result<bool> ReleasePointer(
        std::uint32_t pointerId) noexcept;
    UIElement* CapturedNode(
        std::uint32_t pointerId) noexcept;
    Base::Result<void> TryAddStateChanged(
        const PointerStateChangedHandler& handler) noexcept {
        return stateChanged_.TryAdd(handler);
    }
    bool RemoveStateChanged(
        const PointerStateChangedHandler& handler) noexcept {
        return stateChanged_.Remove(handler);
    }
    Base::Result<void> TryAddCaptureChanged(
        const PointerCaptureChangedHandler& handler) noexcept {
        return captureChanged_.TryAdd(handler);
    }
    bool RemoveCaptureChanged(
        const PointerCaptureChangedHandler& handler) noexcept {
        return captureChanged_.Remove(handler);
    }

private:
    struct PointerCapture final {
        std::uint32_t pointerId = 0U;
        VisualHandle target;
    };
    struct PointerState final {
        std::uint32_t pointerId = 0U;
        VisualHandle hover;
        VisualHandle pressed;
    };

    HitTestManager* hitTests_ = nullptr;
    RoutedEventManager* events_ = nullptr;
    Visual* root_ = nullptr;
    Base::Vector<PointerCapture> captures_;
    Base::Vector<PointerState> states_;
    PointerStateChangedHandler stateChanged_;
    PointerCaptureChangedHandler captureChanged_;

    std::uint32_t FindCapture(
        std::uint32_t pointerId) const noexcept;
    void RemoveCaptureAt(std::uint32_t index) noexcept;
    std::uint32_t FindState(std::uint32_t pointerId) const noexcept;
    Base::Result<void> UpdateHover(
        std::uint32_t pointerId, UIElement* target) noexcept;
    Base::Result<void> UpdatePressed(
        std::uint32_t pointerId, UIElement* target) noexcept;
    bool HasHover(VisualHandle target,
        std::uint32_t ignoredIndex) const noexcept;
    bool HasPressed(VisualHandle target,
        std::uint32_t ignoredIndex) const noexcept;
};
class AERO_API UiRuntimeAccess::FocusManager final {
public:
    FocusManager(ObjectTree& tree, RoutedEventManager& events) noexcept;

    UIElement* FocusedNode() noexcept;
    UIElement* FocusedElement(UIElement& scope) noexcept;
    Base::Result<bool> SetFocus(UIElement* node) noexcept;
    Base::Result<bool> ClearFocus() noexcept;
    Base::Result<bool> MoveFocus(
        FocusNavigationDirection direction,
        bool wrap = true) noexcept;

private:
    struct ScopeFocus final {
        VisualHandle scope;
        VisualHandle focused;
    };
    struct FocusCandidate final {
        UIElement* element = nullptr;
        std::uint32_t tabIndex = 0U;
        std::uint32_t order = 0U;
    };

    ObjectTree* tree_ = nullptr;
    RoutedEventManager* events_ = nullptr;
    VisualHandle focused_;
    Base::Vector<ScopeFocus> scopeFocus_;

    UIElement* FindNavigationScope(UIElement* node) noexcept;
    Base::Result<void> RememberFocus(UIElement& node) noexcept;
    Base::Result<void> CollectCandidates(
        Visual& parent,
        Base::Vector<FocusCandidate>& candidates,
        std::uint32_t& order) noexcept;
};
class AERO_API UiRuntimeAccess::KeyboardInputManager final {
public:
    KeyboardInputManager(FocusManager& focus, RoutedEventManager& events,
        ObjectTree& tree) noexcept;
    KeyboardInputManager(FocusManager& focus, RoutedEventManager& events,
        ObjectTree& tree, CommandManager* commands) noexcept;

    void SetCommandManager(CommandManager* commands) noexcept {
        commands_ = commands;
    }

    Base::Result<KeyboardDispatchResult> Dispatch(
        const KeyboardInput& input) noexcept;

private:
    FocusManager* focus_ = nullptr;
    RoutedEventManager* events_ = nullptr;
    ObjectTree* tree_ = nullptr;
    CommandManager* commands_ = nullptr;
};
class AERO_API UiRuntimeAccess::TextInputManager final {
public:
    TextInputManager(FocusManager& focus, RoutedEventManager& events,
        ObjectTree& tree) noexcept;

    Base::Result<TextInputDispatchResult> Dispatch(
        const TextInput& input) noexcept;

private:
    FocusManager* focus_ = nullptr;
    RoutedEventManager* events_ = nullptr;
    ObjectTree* tree_ = nullptr;
};
class AERO_API UiRuntimeAccess::LayoutManager final {
public:
    explicit LayoutManager(Dispatcher& dispatcher) noexcept;
    ~LayoutManager() noexcept;
    LayoutManager(const LayoutManager&) = delete;
    LayoutManager& operator=(const LayoutManager&) = delete;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> Attach(UIElement& parent, UIElement& child) noexcept;
    Base::Result<void> Detach(UIElement& parent, UIElement& child) noexcept;
    Base::Result<void> SetRoot(UIElement* root, Size availableSize) noexcept;
    Base::Result<void> InvalidateMeasure(UIElement& element) noexcept;
    Base::Result<void> InvalidateArrange(UIElement& element) noexcept;
    Base::Result<std::uint32_t> Flush() noexcept;
    LayoutDiagnostics Diagnostics() const noexcept;
    std::uint64_t PassVersion() const noexcept { return passVersion_; }
    Base::Status LastFlushStatus() const noexcept {
        return lastFlushStatus_;
    }

private:
    friend class Aero::UIElement;
    Dispatcher* dispatcher_ = nullptr;
    UIElement* root_ = nullptr;
    Size rootAvailableSize_;
    Base::Vector<Aero::Detail::VisualLease> measureQueue_;
    Base::Vector<Aero::Detail::VisualLease> arrangeQueue_;
    DispatcherFrameHookHandle phaseHook_;
    std::uint64_t passVersion_ = 0U;
    std::uint32_t measuredCount_ = 0U;
    std::uint32_t arrangedCount_ = 0U;
    Base::Status lastFlushStatus_;
    bool flushing_ = false;

    Base::Result<void> VerifyElement(const UIElement& element) const noexcept;
    Base::Result<void> QueueMeasure(UIElement& element) noexcept;
    Base::Result<void> QueueArrange(UIElement& element) noexcept;
    void RemoveQueued(UIElement& element) noexcept;
    Base::Result<void> MeasureElement(UIElement& element, Size constraint) noexcept;
    Base::Result<void> ArrangeElement(UIElement& element, Rect slot) noexcept;
    static void LayoutHook(void* context) noexcept;
};
class AERO_API UiRuntimeAccess::BindingManager final {
public:
    explicit BindingManager(Dispatcher& dispatcher) noexcept;
    ~BindingManager() noexcept;

    BindingManager(const BindingManager&) = delete;
    BindingManager& operator=(const BindingManager&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    Base::Result<BindingHandle> Attach(
        const BindingDescriptor& descriptor) noexcept;
    Base::Result<BindingHandle> Attach(
        const MetadataBindingDescriptor& descriptor) noexcept;
    // Deferred templates are cloned before their visual roots are mounted.
    // Queueing preserves the declaration until the target acquires its
    // Dispatcher; RuntimeUiServices activates it while walking the
    // newly mounted instance.
    Base::Result<void> QueueDeferred(
        const MetadataBindingDescriptor& descriptor) noexcept;
    Base::Result<std::uint32_t> ActivateDeferred(
        DependencyObject& target) noexcept;
    template<
        class TSourceOwner,
        class TValue,
        class TTargetOwner>
    Base::Result<BindingHandle> Attach(
        DependencyObject& source,
        const Core::DependencyPropertyRef<
            TSourceOwner, TValue>& sourceProperty,
        DependencyObject& target,
        const Core::DependencyPropertyRef<
            TTargetOwner, TValue>& targetProperty,
        BindingMode mode = BindingMode::OneWay) noexcept {
        BindingDescriptor descriptor;
        descriptor.source = &source;
        descriptor.sourceProperty =
            sourceProperty.Handle();
        descriptor.target = &target;
        descriptor.targetProperty =
            targetProperty.Handle();
        descriptor.mode = mode;
        return Attach(descriptor);
    }
    template<
        class TSourceOwner,
        class TValue,
        class TTargetOwner>
    Base::Result<BindingHandle> Attach(
        DependencyObject& source,
        const Core::ReadOnlyPropertyRef<
            TSourceOwner, TValue>& sourceProperty,
        DependencyObject& target,
        const Core::DependencyPropertyRef<
            TTargetOwner, TValue>& targetProperty,
        BindingMode mode = BindingMode::OneWay) noexcept {
        BindingDescriptor descriptor;
        descriptor.source = &source;
        descriptor.sourceProperty =
            sourceProperty.Handle();
        descriptor.target = &target;
        descriptor.targetProperty =
            targetProperty.Handle();
        descriptor.mode = mode;
        return Attach(descriptor);
    }
    Base::Result<bool> Detach(BindingHandle handle) noexcept;
    Base::Result<bool> UpdateSource(BindingHandle handle) noexcept;

    // Removes every binding whose source or target is object. Tree/object
    // ownership code uses this before destroying a DependencyObject.
    Base::Result<std::uint32_t> DetachObject(
        DependencyObject& object) noexcept;

    // Flush is also exposed for deterministic headless tests. Normal hosts run
    // it through the DataBind frame phase registered by Initialize().
    Base::Result<std::uint32_t> Flush() noexcept;
    Base::Result<std::uint32_t>
    InspectBindings(
        const DependencyObject& object,
        Base::Vector<BindingInspection>&
            output) const noexcept;

    bool IsInitialized() const noexcept {
        return hook_.IsValid();
    }
    bool IsFlushing() const noexcept {
        return flushing_;
    }
    std::uint32_t BindingCount() const noexcept {
        return bindings_.Size();
    }
    Base::Status LastError() const noexcept {
        return lastError_;
    }

private:
    enum class BindingSourceKind : std::uint8_t {
        DependencyProperty = 0U,
        MetadataPath,
        MetadataObject,
        DataContext
    };

    struct BindingRecord final {
        BindingHandle handle;
        BindingDescriptor descriptor;
        BindingSourceKind sourceKind =
            BindingSourceKind::DependencyProperty;
        MetadataRuntime* metadata = nullptr;
        Base::Object* metadataSource = nullptr;
        DependencyPropertyHandle dataContextProperty;
        Base::String path;
        Base::String stringFormat;
        bool bindsToSource = false;
        BindingPathPlan pathPlan;
        std::uint64_t notificationSubscription = 0U;
        PropertyValue lastSourceValue;
        PropertyValue lastTargetValue;
        BindingDiagnosticStage conversionFailureStage =
            BindingDiagnosticStage::Convert;
        bool applied = false;
        bool sourceDirty = true;
        bool targetDirty = true;
        bool forceSourceUpdate = false;
    };

    struct DeferredBindingRecord final {
        MetadataRuntime* metadata = nullptr;
        Base::Object* source = nullptr;
        DependencyObject* target = nullptr;
        DependencyPropertyHandle targetProperty;
        DependencyPropertyHandle dataContextProperty;
        Base::String path;
        Base::String stringFormat;
        bool bindsToSource = false;
        BindingMode mode = BindingMode::OneWay;
        UpdateSourceTrigger updateSourceTrigger =
            UpdateSourceTrigger::PropertyChanged;
        BindingConvertCallback convert = nullptr;
        BindingConvertCallback convertBack = nullptr;
        BindingValidateCallback validate = nullptr;
        BindingValidateCallback validateBack = nullptr;
        void* conversionContext = nullptr;
        PropertyValue fallbackValue;
        PropertyValue targetNullValue;
        BindingDiagnosticCallback diagnostic = nullptr;
        void* diagnosticContext = nullptr;
    };

    Dispatcher* dispatcher_ = nullptr;
    Base::Vector<BindingRecord> bindings_;
    Base::Vector<DeferredBindingRecord> deferredBindings_;
    DispatcherFrameHookHandle hook_;
    std::uint64_t nextHandle_ = 1U;
    bool flushing_ = false;
    Base::Status lastError_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;

    static void DataBindHook(void* context) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void OnMetadataPropertyChanged(
        Base::Object& object,
        MemberId property) noexcept;
    static void MetadataPropertyChanged(
        Base::Object& object,
        MemberId property,
        void* context) noexcept;
    Base::Result<void> VerifyDescriptor(
        const BindingDescriptor& descriptor) const noexcept;
    Base::Result<void> VerifyDescriptor(
        const MetadataBindingDescriptor& descriptor) const noexcept;
    Base::Result<void> ResolveMetadataSource(
        BindingRecord& record) noexcept;
    Base::Result<PropertyValue> ReadSource(
        BindingRecord& record) noexcept;
    Base::Result<void> WriteSource(
        BindingRecord& record,
        const PropertyValue& value) noexcept;
    Base::Result<PropertyValue> ConvertForTarget(
        BindingRecord& record,
        const PropertyValue& value) noexcept;
    Base::Result<PropertyValue> ConvertForSource(
        BindingRecord& record,
        const PropertyValue& value) noexcept;
    void ReportDiagnostic(
        BindingRecord& record,
        BindingDiagnosticStage stage,
        Base::Status status) noexcept;
    Base::Result<void> SubscribeMetadataSource(
        BindingRecord& record) noexcept;
    void ReleaseMetadataSource(BindingRecord& record) noexcept;
    void RemoveAt(std::uint32_t index) noexcept;
};
class AERO_API UiRuntimeAccess::AnimationManager final {
public:
    AnimationManager(
        Core::Dispatcher& dispatcher,
        Core::EffectiveValueEngine& values,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~AnimationManager() noexcept;

    AnimationManager(const AnimationManager&) = delete;
    AnimationManager& operator=(const AnimationManager&) = delete;

    Base::Result<void> Initialize() noexcept;
    void Shutdown() noexcept;

    Base::Result<AnimationHandle> Begin(
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        const DoubleAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        const ColorAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        const PointAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        const RectAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        const ThicknessAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        const DoubleKeyFrameAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        const ColorKeyFrameAnimation& animation) noexcept;
    Base::Result<AnimationHandle> Begin(
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        const DiscreteAnimation& animation) noexcept;

    Base::Result<void> Pause(AnimationHandle handle) noexcept;
    Base::Result<void> Resume(AnimationHandle handle) noexcept;
    Base::Result<void> Seek(
        AnimationHandle handle,
        AnimationTime offsetMicroseconds) noexcept;
    Base::Result<void> Stop(AnimationHandle handle) noexcept;
    Base::Result<void> Remove(AnimationHandle handle) noexcept;
    Base::Result<std::uint32_t> RemoveTarget(
        Core::DependencyObject& target) noexcept;
    Base::Result<void> RemoveAll() noexcept;

    Base::Result<std::uint32_t> Tick(
        AnimationTime nowMicroseconds) noexcept;
    // Samples newly-created automatic timelines at t=0 so the first submitted
    // frame has the authored initial key frame.
    Base::Result<std::uint32_t> ApplyPendingInitialValues() noexcept;
    // Called after a frame containing those initial values is submitted. The
    // automatic clock starts here rather than at storyboard construction.
    void CommitPendingInitialValues() noexcept;
    Base::Result<std::uint32_t> AdvanceBy(
        AnimationTime elapsedMicroseconds) noexcept;

    AnimationState State(AnimationHandle handle) const noexcept;
    AnimationDiagnostics Diagnostics() const noexcept;
    Base::Status LastTickStatus() const noexcept {
        return lastTickStatus_;
    }
    bool IsInitialized() const noexcept {
        return frameHook_.IsValid();
    }
    void SetAutomaticTickingEnabled(bool enabled) noexcept {
        automaticTickingEnabled_ = enabled;
    }
    bool AutomaticTickingEnabled() const noexcept {
        return automaticTickingEnabled_;
    }

    static double Ease(
        double progress,
        const EasingFunction& easing) noexcept;

private:
    struct Track;

    Core::Dispatcher* dispatcher_ = nullptr;
    Core::EffectiveValueEngine* values_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Track* tracks_ = nullptr;
    std::uint32_t trackCount_ = 0U;
    std::uint32_t trackCapacity_ = 0U;
    Core::DispatcherFrameHookHandle frameHook_;
    AnimationTime currentTimeMicroseconds_ = 0U;
    std::uint64_t nextHandle_ = 1U;
    AnimationDiagnostics diagnostics_;
    Base::Status lastTickStatus_;
    bool ticking_ = false;
    bool automaticTickingEnabled_ = true;

    Base::Result<Track*> AddTrack() noexcept;
    Track* FindTrack(AnimationHandle handle) noexcept;
    const Track* FindTrack(AnimationHandle handle) const noexcept;
    Base::Result<void> ClearTrackValue(Track& track) noexcept;
    Base::Result<bool> ApplyTrack(
        Track& track,
        AnimationTime nowMicroseconds) noexcept;
    void CompactStopped() noexcept;
    void ReleaseTracks() noexcept;

    static void AnimationFrameHook(void* context) noexcept;
};
class AERO_API UiRuntimeAccess::StyleManager final {
public:
    using TriggerActionHandler = Base::Result<void>(*)(
        DependencyObject& owner,
        Base::Span<const Base::Ref<Base::Object>> actions,
        void* context) noexcept;

    explicit StyleManager(
        EffectiveValueEngine& values,
        DependencyPropertyRegistry& properties) noexcept
        : providerSession_(values),
          values_(&providerSession_),
          properties_(&properties),
          applications_(),
          propertyChangedHandler_(
              this, &StyleManager::OnPropertyChanged) {}
    ~StyleManager() noexcept;

    Base::Result<void> Apply(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> Clear(
        DependencyObject& object,
        const Style& style) noexcept;
    // Tree/object ownership code calls this before destroying an object.
    Base::Result<bool> DetachObject(
        DependencyObject& object) noexcept;
    const Style* AppliedStyle(
        const DependencyObject& object)
        const noexcept;
    void SetTriggerActionHandler(
        TriggerActionHandler handler,
        void* context) noexcept {
        triggerActionHandler_ = handler;
        triggerActionContext_ = context;
    }
    const Base::Status& LastActionStatus() const noexcept {
        return lastActionStatus_;
    }

private:
    Core::Detail::StyleProviderSession providerSession_;
    Core::Detail::StyleProviderSession* values_ = nullptr;
    DependencyPropertyRegistry* properties_ = nullptr;
    struct Application final {
        DependencyObject* object = nullptr;
        const Style* style = nullptr;
        Base::Vector<std::uint8_t> triggerStates;
    };
    Base::Vector<Application> applications_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    Dispatcher* dispatcher_ = nullptr;
    DispatcherFrameHookHandle triggerPhaseHook_;
    Base::Vector<DependencyObject*>
        pendingTriggerEvaluations_;
    TriggerActionHandler triggerActionHandler_ = nullptr;
    void* triggerActionContext_ = nullptr;
    Base::Status lastActionStatus_;

    Base::Result<void> VerifyTarget(
        const DependencyObject& object,
        const Style& style) const noexcept;
    std::uint32_t FindApplication(
        const DependencyObject& object) const noexcept;
    Base::Result<void> ClearSetters(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> SubscribeTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
    void UnsubscribeTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> EvaluateTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> ExecuteTriggerActions(
        DependencyObject& object,
        Base::Span<const Base::Ref<Base::Object>>
            actions) noexcept;
    Base::Result<void> EnsureTriggerPhaseHook(
        DependencyObject& object) noexcept;
    Base::Result<void> QueueTriggerEvaluation(
        DependencyObject& object) noexcept;
    void RemovePendingTriggerEvaluation(
        DependencyObject& object) noexcept;
    Base::Result<std::uint32_t>
        FlushPendingTriggerEvaluations() noexcept;
    static void TriggerPhaseHook(void* context) noexcept;
    Base::Result<void> ClearTriggerSetters(
        DependencyObject& object,
        const Style& style) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
};
class AERO_API UiRuntimeAccess::ThemeStyleManager final {
public:
    ThemeStyleManager(
        EffectiveValueEngine& values,
        const Aero::Detail::ThemeStyleRegistry& registry) noexcept
        : providerSession_(values),
          values_(&providerSession_),
          registry_(&registry) {}

    Base::Result<bool> ApplyDefault(
        DependencyObject& object) noexcept;
    Base::Result<bool> Clear(
        DependencyObject& object) noexcept;

private:
    struct Application final {
        DependencyObject* object = nullptr;
        const Style* style = nullptr;
    };
    Core::Detail::ThemeStyleProviderSession providerSession_;
    Core::Detail::ThemeStyleProviderSession* values_ = nullptr;
    const Aero::Detail::ThemeStyleRegistry* registry_ = nullptr;
    Base::Vector<Application> applications_;

    std::uint32_t FindApplication(
        const DependencyObject& object) const noexcept;
    Base::Result<void> ClearSetters(
        DependencyObject& object,
        const Style& style) noexcept;
};

template<class TArgs>
Base::Result<void> RoutedEventManager::RegisterClassHandler(
    RoutedEventHandle event,
    TypeId classType,
    const Base::Delegate<void(Base::Object*, const TArgs&)>& handler,
    bool handledEventsToo) noexcept {
    static_assert(std::is_base_of<RoutedEventArgs, TArgs>::value,
        "Routed event arguments must derive from RoutedEventArgs");
    if (raiseDepth_ != 0U) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Cannot mutate class handlers during routed event dispatch");
    }
    if (handler.Empty()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Class handler registration is invalid");
    }
    Base::Result<void> valid = ValidateClassHandler(
        event, classType, TArgs::StaticTypeId());
    if (!valid) return valid.GetStatus();
    ClassHandlerRecord value;
    value.event = event;
    value.classType = classType;
    value.handler = Aero::Detail::RoutedHandlerStorage(handler);
    value.handledEventsToo = handledEventsToo;
    value.sequence = nextClassSequence_++;
    return classHandlers_.TryPushBack(std::move(value));
}

} // namespace Aero::Detail
