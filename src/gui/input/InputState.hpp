#pragma once

// View-owned input, focus, capture and routed-command state.

#include <Aero/Base/Delegate.hpp>
#include <Aero/Media/Transform3D.hpp>
#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp" 
#include <cstdint>

namespace Aero { class UIElement; }

namespace Aero::Input {

struct CommandBindingHandle {
    std::uint64_t value = 0U;
    constexpr bool IsValid() const noexcept { return value != 0U; }
};

struct InputBindingHandle {
    std::uint64_t value = 0U;
    constexpr bool IsValid() const noexcept { return value != 0U; }
};

using RequerySuggestedHandler = Base::Delegate<void()>;
using PointerStateChangedHandler = Base::Delegate<void(UIElement&)>;
using PointerCaptureChangedHandler = Base::Delegate<void(std::uint32_t, UIElement*, bool)>;

} // namespace Aero::Input


#include <Aero/Input.hpp>
#include <Aero/ICommand.hpp>
#include <Aero/RoutedCommand.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/InputBinding.hpp>
#include <Aero/KeyBinding.hpp>
#include <Aero/KeyboardNavigation.hpp>
#include <Aero/FocusManager.hpp>
#include <Aero/Layout.hpp>

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using namespace Aero::Input;

class CommandState {
public:
    CommandState(ElementTree& tree, EventRouter& events) noexcept;

    Base::Result<CommandBindingHandle> AddBinding(
        UIElement& owner,
        const CommandBinding& binding) noexcept;
    Base::Result<bool> RemoveBinding(
        CommandBindingHandle handle) noexcept;
    Base::Result<InputBindingHandle> AddInputBinding(
        UIElement& owner,
        Base::Ref<InputBinding> binding) noexcept;

    Base::Result<bool> CanExecute(
        ICommand& command,
        const Meta::Value& parameter,
        UIElement& target) noexcept;
    Base::Result<bool> Execute(
        ICommand& command,
        const Meta::Value& parameter,
        UIElement& target) noexcept;
    Base::Result<bool> CanExecute(
        RoutedCommand& command,
        const Meta::Value& parameter,
        UIElement& target) noexcept;
    Base::Result<bool> Execute(
        RoutedCommand& command,
        const Meta::Value& parameter,
        UIElement& target) noexcept;
    Base::Result<bool> ProcessInput(
        UIElement& target,
        const KeyboardInput& input) noexcept;
    Base::Result<bool> ProcessInput(
        UIElement& target,
        const PointerInput& input) noexcept;

    void AddRequerySuggested(
        const RequerySuggestedHandler& handler) noexcept;
    bool RemoveRequerySuggested(
        const RequerySuggestedHandler& handler) noexcept;
    void InvalidateRequerySuggested() const noexcept;

private:
    struct BindingRecord {
        CommandBindingHandle handle;
        VisualHandle owner;
        CommandBinding binding;
    };
    struct InputBindingRecord {
        InputBindingHandle handle;
        VisualHandle owner;
        Base::Ref<InputBinding> binding;
    };

    ElementTree* tree_ = nullptr;
    EventRouter* events_ = nullptr;
    Base::Vector<BindingRecord> bindings_;
    Base::Vector<InputBindingRecord> inputBindings_;
    std::uint64_t nextBinding_ = 1U;
    std::uint64_t nextInputBinding_ = 1U;
    RequerySuggestedHandler requerySuggested_;

    Base::Result<void> VerifyTarget(UIElement& target) const noexcept;
    void PruneStaleBindings() noexcept;
    void PruneStaleInputBindings() noexcept;
};
class HitTestState {
public:
    HitTestState() noexcept = default;
    Base::Result<void> SetOverlays(
        Base::Span<UIElement* const> overlays,
        Base::Span<const Point> origins) noexcept;
    Base::Result<void> SetOverlays(
        Base::Span<UIElement* const> overlays,
        Base::Span<const Base::Transform2D> transforms) noexcept;
    void ClearOverlays() noexcept {
        overlays_.Clear();
    }
    Base::Result<HitTestResult> HitTest(
        ::Aero::Media::Visual& root, Point position) const noexcept;
    // Converts a position expressed in root coordinates to target-local
    // coordinates. Unlike HitTest(), capture routing intentionally does not
    // test visibility, clipping, or bounds.
    Base::Result<HitTestResult> RootToLocal(
        ::Aero::Media::Visual& root, ::Aero::Media::Visual& target, Point position) const noexcept;

private:
    struct OverlayRecord {
        UIElement* element = nullptr;
        Base::Transform2D transform;
    };
    Base::Vector<OverlayRecord> overlays_;
    Base::Result<HitTestResult> HitTestElement(
        UIElement& element,
        Point position,
        const Media::Transform3DContext& transform3D) const noexcept;
    bool IsOverlay(
        const UIElement& element) const noexcept;
};
class PointerStateMachine {
public:
    PointerStateMachine(HitTestState& hitTests, EventRouter& events) noexcept;
    void SetCommandState(CommandState* commands) noexcept {
        commands_ = commands;
    }

    void SetRoot(::Aero::Media::Visual* root) noexcept {
        if (root_ == root) return;
        root_ = root;
        captures_.Clear();
        states_.Clear();
    }

    Base::Result<PointerDispatchResult> Dispatch(
        const PointerInput& input) noexcept;
    Base::Result<void> CapturePointer(
        std::uint32_t pointerId, UIElement& target) noexcept;
    Base::Result<bool> ReleasePointer(
        std::uint32_t pointerId) noexcept;
    UIElement* CapturedNode(
        std::uint32_t pointerId) noexcept;
    void AddStateChanged(
        const PointerStateChangedHandler& handler) noexcept {
        stateChanged_.Add(handler);
    }
    bool RemoveStateChanged(
        const PointerStateChangedHandler& handler) noexcept {
        return stateChanged_.Remove(handler);
    }
    void AddCaptureChanged(
        const PointerCaptureChangedHandler& handler) noexcept {
        captureChanged_.Add(handler);
    }
    bool RemoveCaptureChanged(
        const PointerCaptureChangedHandler& handler) noexcept {
        return captureChanged_.Remove(handler);
    }

private:
    struct PointerCapture {
        std::uint32_t pointerId = 0U;
        VisualHandle target;
    };
    struct PointerState {
        std::uint32_t pointerId = 0U;
        VisualHandle hover;
        VisualHandle pressed;
    };

    HitTestState* hitTests_ = nullptr;
    EventRouter* events_ = nullptr;
    CommandState* commands_ = nullptr;
    ::Aero::Media::Visual* root_ = nullptr;
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

class DragDropState {
public:
    DragDropState(
        ElementTree& tree,
        EventRouter& events,
        HitTestState& hitTests) noexcept;

    void SetRoot(::Aero::Media::Visual* root) noexcept;
    Base::Result<void> Begin(
        UIElement& source,
        std::uint32_t pointerId,
        const Meta::Value& data,
        DragDropEffects allowedEffects) noexcept;
    Base::Result<bool> Cancel() noexcept;
    Base::Result<void> DispatchPointer(
        const PointerInput& input) noexcept;
    bool IsDragging() const noexcept { return active_; }
    bool IsSource(const UIElement& element) const noexcept;

private:
    ElementTree* tree_ = nullptr;
    EventRouter* events_ = nullptr;
    HitTestState* hitTests_ = nullptr;
    ::Aero::Media::Visual* root_ = nullptr;
    VisualHandle source_;
    VisualHandle target_;
    Meta::Value data_;
    DragDropEffects allowedEffects_ = DragDropEffects::None;
    DragDropEffects effects_ = DragDropEffects::None;
    std::uint32_t pointerId_ = 0U;
    bool active_ = false;

    UIElement* Resolve(VisualHandle handle) const noexcept;
    UIElement* FindDropTarget(UIElement* hit) const noexcept;
    Base::Result<void> UpdateTarget(
        UIElement* target, Base::Point rootPosition) noexcept;
    Base::Result<void> RaiseDragPair(
        UIElement& target,
        RoutedEventHandle previewEvent,
        RoutedEventHandle bubbleEvent,
        Base::Point rootPosition,
        DragDropEffects& effects) noexcept;
    Base::Result<void> RaiseFeedback() noexcept;
    Base::Result<void> Complete(
        DragDropEffects effects, bool canceled) noexcept;
    void Clear() noexcept;
};

class FocusState {
public:
    FocusState(ElementTree& tree, EventRouter& events) noexcept;

    UIElement* FocusedNode() noexcept;
    UIElement* FocusedElement(UIElement& scope) noexcept;
    Base::Result<bool> SetFocus(UIElement* node) noexcept;
    Base::Result<bool> ClearFocus() noexcept;
    Base::Result<bool> MoveFocus(
        FocusNavigationDirection direction,
        bool wrap = true) noexcept;

private:
    struct ScopeFocus {
        VisualHandle scope;
        VisualHandle focused;
    };
    struct FocusCandidate {
        UIElement* element = nullptr;
        std::uint32_t tabIndex = 0U;
        std::uint32_t order = 0U;
    };

    ElementTree* tree_ = nullptr;
    EventRouter* events_ = nullptr;
    VisualHandle focused_;
    Base::Vector<ScopeFocus> scopeFocus_;

    UIElement* FindNavigationScope(UIElement* node) noexcept;
    Base::Result<void> RememberFocus(UIElement& node) noexcept;
    Base::Result<void> CollectCandidates(
        ::Aero::Media::Visual& parent,
        Base::Vector<FocusCandidate>& candidates,
        std::uint32_t& order) noexcept;
};
class KeyboardState {
public:
    KeyboardState(FocusState& focus, EventRouter& events,
        ElementTree& tree) noexcept;
    KeyboardState(FocusState& focus, EventRouter& events,
        ElementTree& tree, CommandState* commands) noexcept;

    void SetCommandState(CommandState* commands) noexcept {
        commands_ = commands;
    }

    Base::Result<Input::KeyboardDispatchResult> Dispatch(
        const Input::KeyboardInput& input) noexcept;

private:
    FocusState* focus_ = nullptr;
    EventRouter* events_ = nullptr;
    ElementTree* tree_ = nullptr;
    CommandState* commands_ = nullptr;
};
class TextInputState {
public:
    TextInputState(FocusState& focus, EventRouter& events,
        ElementTree& tree) noexcept;

    Base::Result<Input::TextInputDispatchResult> Dispatch(
        const Input::TextInput& input) noexcept;

private:
    FocusState* focus_ = nullptr;
    EventRouter* events_ = nullptr;
    ElementTree* tree_ = nullptr;
};

// View-owned input coordinator. Consumers see one service; focus, hit testing,
// pointer capture, keyboard/text dispatch and routed commands remain private
// implementation components behind this facade.
class InputRouter {
public:
    InputRouter(ElementTree& tree, EventRouter& events) noexcept
        : commands_(tree, events),
          focus_(tree, events),
          pointer_(hitTests_, events),
          dragDrop_(tree, events, hitTests_),
          keyboard_(focus_, events, tree, &commands_),
          text_(focus_, events, tree) {
        pointer_.SetCommandState(&commands_);
    }

    void SetRoot(::Aero::Media::Visual* root) noexcept {
        pointer_.SetRoot(root);
        dragDrop_.SetRoot(root);
    }

    Base::Result<Input::PointerDispatchResult> DispatchPointer(
        const Input::PointerInput& input) noexcept {
        Base::Result<Input::PointerDispatchResult> routed =
            pointer_.Dispatch(input);
        if (!routed) return routed.GetStatus();
        Base::Result<void> dragged =
            dragDrop_.DispatchPointer(input);
        return dragged
            ? routed
            : Base::Result<Input::PointerDispatchResult>(
                  dragged.GetStatus());
    }
    Base::Result<Input::KeyboardDispatchResult> DispatchKeyboard(
        const Input::KeyboardInput& input) noexcept {
        if (dragDrop_.IsDragging() &&
            input.action == Input::KeyboardAction::Down &&
            input.key == Input::KeyboardKeyEscape) {
            Base::Result<bool> canceled = dragDrop_.Cancel();
            if (!canceled) return canceled.GetStatus();
            if (canceled.Value()) {
                Input::KeyboardDispatchResult result;
                result.routed = true;
                return result;
            }
        }
        return keyboard_.Dispatch(input);
    }
    Base::Result<Input::TextInputDispatchResult> DispatchText(const Input::TextInput& input) noexcept { return text_.Dispatch(input); }

    Base::Result<void> SetOverlays(Base::Span<UIElement* const> overlays, Base::Span<const Base::Point> origins) noexcept { return hitTests_.SetOverlays(overlays, origins); }
    Base::Result<void> SetOverlays(Base::Span<UIElement* const> overlays, Base::Span<const Base::Transform2D> transforms) noexcept { return hitTests_.SetOverlays(overlays, transforms); }
    void ClearOverlays() noexcept { hitTests_.ClearOverlays(); }
    Base::Result<Input::HitTestResult> HitTest(::Aero::Media::Visual& root, Base::Point position) const noexcept { return hitTests_.HitTest(root, position); }
    Base::Result<Input::HitTestResult> RootToLocal(::Aero::Media::Visual& root, ::Aero::Media::Visual& target, Base::Point position) const noexcept { return hitTests_.RootToLocal(root, target, position); }

    Base::Result<void> CapturePointer(std::uint32_t pointerId, UIElement& target) noexcept { return pointer_.CapturePointer(pointerId, target); }
    Base::Result<bool> ReleasePointer(std::uint32_t pointerId) noexcept { return pointer_.ReleasePointer(pointerId); }
    Base::Result<void> BeginDrag(
        UIElement& source,
        std::uint32_t pointerId,
        const Meta::Value& data,
        Input::DragDropEffects allowedEffects) noexcept {
        return dragDrop_.Begin(
            source, pointerId, data, allowedEffects);
    }
    Base::Result<bool> CancelDrag() noexcept {
        return dragDrop_.Cancel();
    }
    bool IsDragging() const noexcept {
        return dragDrop_.IsDragging();
    }
    bool IsDragSource(const UIElement& element) const noexcept {
        return dragDrop_.IsSource(element);
    }
    UIElement* GetCapturedPointer(std::uint32_t pointerId) noexcept { return pointer_.CapturedNode(pointerId); }
    void AddPointerStateChanged(const PointerStateChangedHandler& handler) noexcept { pointer_.AddStateChanged(handler); }
    bool RemovePointerStateChanged(const PointerStateChangedHandler& handler) noexcept { return pointer_.RemoveStateChanged(handler); }
    void AddPointerCaptureChanged(const PointerCaptureChangedHandler& handler) noexcept { pointer_.AddCaptureChanged(handler); }
    bool RemovePointerCaptureChanged(const PointerCaptureChangedHandler& handler) noexcept { return pointer_.RemoveCaptureChanged(handler); }

    UIElement* GetFocusedElement() noexcept { return focus_.FocusedNode(); }
    UIElement* GetFocusedElement(UIElement& scope) noexcept { return focus_.FocusedElement(scope); }
    Base::Result<bool> SetFocus(UIElement* element) noexcept { return focus_.SetFocus(element); }
    Base::Result<bool> ClearFocus() noexcept { return focus_.ClearFocus(); }
    Base::Result<bool> MoveFocus(FocusNavigationDirection direction, bool wrap = true) noexcept { return focus_.MoveFocus(direction, wrap); }

    Base::Result<CommandBindingHandle> AddCommandBinding(UIElement& owner, const CommandBinding& binding) noexcept { return commands_.AddBinding(owner, binding); }
    Base::Result<bool> RemoveCommandBinding(CommandBindingHandle handle) noexcept { return commands_.RemoveBinding(handle); }
    Base::Result<InputBindingHandle> AddInputBinding(UIElement& owner, Base::Ref<InputBinding> binding) noexcept { return commands_.AddInputBinding(owner, std::move(binding)); }
    void AddRequerySuggested(const RequerySuggestedHandler& handler) noexcept { commands_.AddRequerySuggested(handler); }
    bool RemoveRequerySuggested(const RequerySuggestedHandler& handler) noexcept { return commands_.RemoveRequerySuggested(handler); }
    void InvalidateRequerySuggested() const noexcept { commands_.InvalidateRequerySuggested(); }

    Base::Result<bool> CanExecute(ICommand& command, const Meta::Value& parameter, UIElement& target) noexcept { return commands_.CanExecute(command, parameter, target); }
    Base::Result<bool> CanExecute(RoutedCommand& command, const Meta::Value& parameter, UIElement& target) noexcept { return commands_.CanExecute(command, parameter, target); }
    Base::Result<bool> Execute(ICommand& command, const Meta::Value& parameter, UIElement& target) noexcept { return commands_.Execute(command, parameter, target); }
    Base::Result<bool> Execute(RoutedCommand& command, const Meta::Value& parameter, UIElement& target) noexcept { return commands_.Execute(command, parameter, target); }

private:
    CommandState commands_;
    HitTestState hitTests_;
    FocusState focus_;
    PointerStateMachine pointer_;
    DragDropState dragDrop_;
    KeyboardState keyboard_;
    TextInputState text_;
};

} // namespace Aero
