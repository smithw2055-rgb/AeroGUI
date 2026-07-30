#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Utf8.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Presentation/InputValues.hpp>
#include <Aero/Presentation/Layout.hpp>
#include <Aero/Presentation/ObjectTree.hpp>

namespace Aero::Presentation {

using namespace Aero::Core;

class CommandManager;

// UI-thread visual-tree hit testing. The last visual child is frontmost.
// Callers register the C++ cast for each concrete layout type; this avoids
// RTTI and keeps the Core runtime usable with /GR- builds.
class AERO_API HitTestManager final {
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

using PointerStateChangedHandler =
    Base::Delegate<void(UIElement&)>;
using PointerCaptureChangedHandler =
    Base::Delegate<void(
        std::uint32_t, UIElement*, bool)>;

class AERO_API PointerInputManager final {
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

enum class FocusNavigationDirection : std::uint8_t {
    Next,
    Previous,
};

enum class KeyboardNavigationMode : std::uint8_t {
    Continue = 0U,
    Once,
    Cycle,
    None,
    Contained,
    Local
};

class AERO_API KeyboardNavigation final
    : public Base::Object {
    AERO_DECLARE_TYPE(
        KeyboardNavigation,
        Base::Object)
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    inline static constexpr Members::AttachedProperty<
        KeyboardNavigationMode>
        DirectionalNavigationProperty{
            "DirectionalNavigation"};

    inline static constexpr Members::AttachedProperty<
        KeyboardNavigationMode>
        TabNavigationProperty{
            "TabNavigation"};
    inline static constexpr Members::AttachedProperty<
        std::uint32_t>
        TabIndexProperty{"TabIndex"};
};

class AERO_API FocusManager final {
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

// UI-thread keyboard router. It delivers KeyDown/KeyUp to the current focus
// target and uses the same routed-event snapshot semantics as pointer input.
class AERO_API KeyboardInputManager final {
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

class AERO_API TextInputManager final {
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

} // namespace Aero::Presentation

namespace Aero::Core {

template<>
struct MetaTypeTraits<
    Presentation::KeyboardNavigationMode> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId(
            "KeyboardNavigationMode");
    }
    static constexpr Base::StringView
    Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "KeyboardNavigationMode";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core
