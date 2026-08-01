#pragma once

#include "gui/events/EventRouter.hpp"
#include "RuntimeTypes.hpp"
#include "gui/tree/ObjectTree.hpp"

#include <Aero/Input.hpp>
#include <Aero/Layout.hpp>

namespace Aero::Detail {

using namespace Aero::Core;
using namespace Aero::Input;

class AERO_API CommandState final {
public:
    CommandState(ObjectTree& tree, EventRouter& events) noexcept;

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

    ObjectTree* tree_ = nullptr;
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
    FocusState(ObjectTree& tree, EventRouter& events) noexcept;

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
        ObjectTree& tree) noexcept;
    KeyboardState(FocusState& focus, EventRouter& events,
        ObjectTree& tree, CommandState* commands) noexcept;

    void SetCommandState(CommandState* commands) noexcept {
        commands_ = commands;
    }

    Base::Result<KeyboardDispatchResult> Dispatch(
        const KeyboardInput& input) noexcept;

private:
    FocusState* focus_ = nullptr;
    EventRouter* events_ = nullptr;
    ObjectTree* tree_ = nullptr;
    CommandState* commands_ = nullptr;
};
class AERO_API TextInputState final {
public:
    TextInputState(FocusState& focus, EventRouter& events,
        ObjectTree& tree) noexcept;

    Base::Result<TextInputDispatchResult> Dispatch(
        const TextInput& input) noexcept;

private:
    FocusState* focus_ = nullptr;
    EventRouter* events_ = nullptr;
    ObjectTree* tree_ = nullptr;
};

} // namespace Aero::Detail
