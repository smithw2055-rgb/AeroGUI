#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Utf8.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Layout.hpp>
#include <Aero/Core/ObjectTree.hpp>

namespace Aero::Core {

using HitTestAsLayoutElementCallback = LayoutElement* (*)(
    TreeNode& node, void* context) noexcept;

struct HitTestTypeRegistration final {
    TypeId type = InvalidTypeId;
    HitTestAsLayoutElementCallback cast = nullptr;
    void* context = nullptr;
};

struct HitTestResult final {
    LayoutElement* target = nullptr;
    Point position;
    bool HasTarget() const noexcept { return target != nullptr; }
};

// UI-thread visual-tree hit testing. The last visual child is frontmost.
// Callers register the C++ cast for each concrete layout type; this avoids
// RTTI and keeps the Core runtime usable with /GR- builds.
class AERO_API HitTestManager final {
public:
    explicit HitTestManager(
        TypeRegistry& types,
        Base::IAllocator* allocator = nullptr) noexcept;

    Base::Result<void> TryRegisterType(
        const HitTestTypeRegistration& registration) noexcept;
    Base::Result<HitTestResult> HitTest(
        TreeNode& root, Point position) const noexcept;
    // Converts a position expressed in root coordinates to target-local
    // coordinates. Unlike HitTest(), capture routing intentionally does not
    // test visibility, clipping, or bounds.
    Base::Result<HitTestResult> RootToLocal(
        TreeNode& root, TreeNode& target, Point position) const noexcept;

private:
    TypeRegistry* types_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<HitTestTypeRegistration> typesByRuntimeType_;

    const HitTestTypeRegistration* FindRegistration(
        TypeId type) const noexcept;
    LayoutElement* AsLayoutElement(TreeNode& node) const noexcept;
    Base::Result<HitTestResult> HitTestElement(
        LayoutElement& element, Point position) const noexcept;
};

struct PointerInput final {
    std::uint32_t pointerId = 0U;
    PointerAction action = PointerAction::Move;
    Point position;
    MouseButton changedButton = MouseButton::Left;
};

struct PointerDispatchResult final {
    HitTestResult hit;
    bool routed = false;
};

// The platform host normalizes native keyboard input into this small value
// type. key is a non-zero platform-neutral key identifier; text composition
// and IME remain a separate input path.
struct KeyboardInput final {
    KeyboardAction action = KeyboardAction::Down;
    std::uint32_t key = 0U;
    std::uint32_t modifiers = 0U;
    bool isRepeat = false;
};

struct KeyboardDispatchResult final {
    TreeNode* target = nullptr;
    bool routed = false;
};

// Text is delivered separately from physical/logical keyboard events. The
// UTF-8 view is borrowed for the duration of Dispatch() and handlers must not
// retain it; IME composition is intentionally a later, distinct protocol.
struct TextInput final {
    Base::StringView text;
};

struct TextInputDispatchResult final {
    TreeNode* target = nullptr;
    bool routed = false;
};

class AERO_API PointerInputManager final {
public:
    PointerInputManager(HitTestManager& hitTests, RoutedEventRegistry& events,
        TreeNode& root) noexcept;

    Base::Result<PointerDispatchResult> Dispatch(
        const PointerInput& input) noexcept;
    Base::Result<void> CapturePointer(
        std::uint32_t pointerId, TreeNode& target) noexcept;
    Base::Result<bool> ReleasePointer(
        std::uint32_t pointerId) noexcept;
    TreeNode* CapturedNode(
        std::uint32_t pointerId) noexcept;

private:
    struct PointerCapture final {
        std::uint32_t pointerId = 0U;
        TreeNodeHandle target;
    };

    HitTestManager* hitTests_ = nullptr;
    RoutedEventRegistry* events_ = nullptr;
    TreeNode* root_ = nullptr;
    Base::Vector<PointerCapture> captures_;

    std::uint32_t FindCapture(
        std::uint32_t pointerId) const noexcept;
    void RemoveCaptureAt(std::uint32_t index) noexcept;
};

class AERO_API FocusManager final {
public:
    FocusManager(ObjectTree& tree, RoutedEventRegistry& events) noexcept;

    TreeNode* FocusedNode() noexcept;
    Base::Result<bool> SetFocus(TreeNode* node) noexcept;
    Base::Result<bool> ClearFocus() noexcept;

private:
    ObjectTree* tree_ = nullptr;
    RoutedEventRegistry* events_ = nullptr;
    TreeNodeHandle focused_;
};

// UI-thread keyboard router. It delivers KeyDown/KeyUp to the current focus
// target and uses the same routed-event snapshot semantics as pointer input.
class AERO_API KeyboardInputManager final {
public:
    KeyboardInputManager(FocusManager& focus, RoutedEventRegistry& events,
        ObjectTree& tree) noexcept;

    Base::Result<KeyboardDispatchResult> Dispatch(
        const KeyboardInput& input) noexcept;

private:
    FocusManager* focus_ = nullptr;
    RoutedEventRegistry* events_ = nullptr;
    ObjectTree* tree_ = nullptr;
};

class AERO_API TextInputManager final {
public:
    TextInputManager(FocusManager& focus, RoutedEventRegistry& events,
        ObjectTree& tree) noexcept;

    Base::Result<TextInputDispatchResult> Dispatch(
        const TextInput& input) noexcept;

private:
    FocusManager* focus_ = nullptr;
    RoutedEventRegistry* events_ = nullptr;
    ObjectTree* tree_ = nullptr;
};

} // namespace Aero::Core
