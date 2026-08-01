#pragma once

// View-owned input, focus, capture and routed-command state.

#include <Aero/Base/Delegate.hpp>
#include <cstdint>

namespace Aero { class UIElement; }

namespace Aero::Input {

struct CommandBindingHandle final {
    std::uint64_t value = 0U;
    constexpr bool IsValid() const noexcept { return value != 0U; }
};

struct InputBindingHandle final {
    std::uint64_t value = 0U;
    constexpr bool IsValid() const noexcept { return value != 0U; }
};

using RequerySuggestedHandler = Base::Delegate<void()>;
using PointerStateChangedHandler = Base::Delegate<void(UIElement&)>;
using PointerCaptureChangedHandler = Base::Delegate<void(std::uint32_t, UIElement*, bool)>;

} // namespace Aero::Input

#include "gui/RoutedEventInternal.hpp"
#include "gui/ElementInternal.hpp"

#include <Aero/Input.hpp>
#include <Aero/Layout.hpp>

namespace Aero::Detail {

using namespace Aero::Core;
using namespace Aero::Input;

class AERO_API CommandState final {
public:
    CommandState(ElementTree& tree, EventRouter& events) noexcept;

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
    struct InputBindingRecord final {
        InputBindingHandle handle;
        VisualHandle owner;
        Base::Ref<KeyBinding> binding;
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
class AERO_API HitTestState final {
public:
    HitTestState() noexcept = default;
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
class AERO_API PointerStateMachine final {
public:
    PointerStateMachine(HitTestState& hitTests, EventRouter& events) noexcept;

    void SetRoot(Visual* root) noexcept {
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

    HitTestState* hitTests_ = nullptr;
    EventRouter* events_ = nullptr;
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
class AERO_API FocusState final {
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
    struct ScopeFocus final {
        VisualHandle scope;
        VisualHandle focused;
    };
    struct FocusCandidate final {
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
        Visual& parent,
        Base::Vector<FocusCandidate>& candidates,
        std::uint32_t& order) noexcept;
};
class AERO_API KeyboardState final {
public:
    KeyboardState(FocusState& focus, EventRouter& events,
        ElementTree& tree) noexcept;
    KeyboardState(FocusState& focus, EventRouter& events,
        ElementTree& tree, CommandState* commands) noexcept;

    void SetCommandState(CommandState* commands) noexcept {
        commands_ = commands;
    }

    Base::Result<KeyboardDispatchResult> Dispatch(
        const KeyboardInput& input) noexcept;

private:
    FocusState* focus_ = nullptr;
    EventRouter* events_ = nullptr;
    ElementTree* tree_ = nullptr;
    CommandState* commands_ = nullptr;
};
class AERO_API TextInputState final {
public:
    TextInputState(FocusState& focus, EventRouter& events,
        ElementTree& tree) noexcept;

    Base::Result<TextInputDispatchResult> Dispatch(
        const TextInput& input) noexcept;

private:
    FocusState* focus_ = nullptr;
    EventRouter* events_ = nullptr;
    ElementTree* tree_ = nullptr;
};

} // namespace Aero::Detail

namespace Aero::Detail {

// View-owned input coordinator. Consumers see one service; focus, hit testing,
// pointer capture, keyboard/text dispatch and routed commands remain private
// implementation components behind this facade.
class InputRouter final {
public:
    InputRouter(ElementTree& tree, EventRouter& events) noexcept
        : commands_(tree, events),
          focus_(tree, events),
          pointer_(hitTests_, events),
          keyboard_(focus_, events, tree, &commands_),
          text_(focus_, events, tree) {}

    void SetRoot(Visual* root) noexcept { pointer_.SetRoot(root); }

    Base::Result<PointerDispatchResult> DispatchPointer(const PointerInput& input) noexcept { return pointer_.Dispatch(input); }
    Base::Result<KeyboardDispatchResult> DispatchKeyboard(const KeyboardInput& input) noexcept { return keyboard_.Dispatch(input); }
    Base::Result<TextInputDispatchResult> DispatchText(const TextInput& input) noexcept { return text_.Dispatch(input); }

    Base::Result<void> SetOverlays(Base::Span<UIElement* const> overlays, Base::Span<const Point> origins) noexcept { return hitTests_.SetOverlays(overlays, origins); }
    void ClearOverlays() noexcept { hitTests_.ClearOverlays(); }
    Base::Result<HitTestResult> HitTest(Visual& root, Point position) const noexcept { return hitTests_.HitTest(root, position); }
    Base::Result<HitTestResult> RootToLocal(Visual& root, Visual& target, Point position) const noexcept { return hitTests_.RootToLocal(root, target, position); }

    Base::Result<void> CapturePointer(std::uint32_t pointerId, UIElement& target) noexcept { return pointer_.CapturePointer(pointerId, target); }
    Base::Result<bool> ReleasePointer(std::uint32_t pointerId) noexcept { return pointer_.ReleasePointer(pointerId); }
    UIElement* GetCapturedPointer(std::uint32_t pointerId) noexcept { return pointer_.CapturedNode(pointerId); }
    Base::Result<void> TryAddPointerStateChanged(const PointerStateChangedHandler& handler) noexcept { return pointer_.TryAddStateChanged(handler); }
    bool RemovePointerStateChanged(const PointerStateChangedHandler& handler) noexcept { return pointer_.RemoveStateChanged(handler); }
    Base::Result<void> TryAddPointerCaptureChanged(const PointerCaptureChangedHandler& handler) noexcept { return pointer_.TryAddCaptureChanged(handler); }
    bool RemovePointerCaptureChanged(const PointerCaptureChangedHandler& handler) noexcept { return pointer_.RemoveCaptureChanged(handler); }

    UIElement* GetFocusedElement() noexcept { return focus_.FocusedNode(); }
    UIElement* GetFocusedElement(UIElement& scope) noexcept { return focus_.FocusedElement(scope); }
    Base::Result<bool> SetFocus(UIElement* element) noexcept { return focus_.SetFocus(element); }
    Base::Result<bool> ClearFocus() noexcept { return focus_.ClearFocus(); }
    Base::Result<bool> MoveFocus(FocusNavigationDirection direction, bool wrap = true) noexcept { return focus_.MoveFocus(direction, wrap); }

    Base::Result<CommandBindingHandle> TryAddCommandBinding(UIElement& owner, const CommandBinding& binding) noexcept { return commands_.TryAddBinding(owner, binding); }
    Base::Result<bool> RemoveCommandBinding(CommandBindingHandle handle) noexcept { return commands_.RemoveBinding(handle); }
    Base::Result<InputBindingHandle> TryAddInputBinding(UIElement& owner, Base::Ref<KeyBinding> binding) noexcept { return commands_.TryAddInputBinding(owner, std::move(binding)); }
    Base::Result<void> TryAddRequerySuggested(const RequerySuggestedHandler& handler) noexcept { return commands_.TryAddRequerySuggested(handler); }
    bool RemoveRequerySuggested(const RequerySuggestedHandler& handler) noexcept { return commands_.RemoveRequerySuggested(handler); }
    void InvalidateRequerySuggested() const noexcept { commands_.InvalidateRequerySuggested(); }

    Base::Result<bool> CanExecute(ICommand& command, const Core::Value& parameter, UIElement& target) noexcept { return commands_.CanExecute(command, parameter, target); }
    Base::Result<bool> CanExecute(RoutedCommand& command, const Core::Value& parameter, UIElement& target) noexcept { return commands_.CanExecute(command, parameter, target); }
    Base::Result<bool> Execute(ICommand& command, const Core::Value& parameter, UIElement& target) noexcept { return commands_.Execute(command, parameter, target); }
    Base::Result<bool> Execute(RoutedCommand& command, const Core::Value& parameter, UIElement& target) noexcept { return commands_.Execute(command, parameter, target); }

private:
    CommandState commands_;
    HitTestState hitTests_;
    FocusState focus_;
    PointerStateMachine pointer_;
    KeyboardState keyboard_;
    TextInputState text_;
};

} // namespace Aero::Detail
