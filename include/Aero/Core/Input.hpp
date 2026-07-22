#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
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
    AERO_NODISCARD bool HasTarget() const noexcept { return target != nullptr; }
};

// UI-thread visual-tree hit testing. The last visual child is frontmost.
// Callers register the C++ cast for each concrete layout type; this avoids
// RTTI and keeps the Core runtime usable with /GR- builds.
class AERO_API HitTestManager final {
public:
    explicit HitTestManager(
        TypeRegistry& types,
        Base::IAllocator* allocator = nullptr) noexcept;

    AERO_NODISCARD Base::Result<void> TryRegisterType(
        const HitTestTypeRegistration& registration) noexcept;
    AERO_NODISCARD Base::Result<HitTestResult> HitTest(
        TreeNode& root, Point position) const noexcept;

private:
    TypeRegistry* types_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<HitTestTypeRegistration> typesByRuntimeType_;

    AERO_NODISCARD const HitTestTypeRegistration* FindRegistration(
        TypeId type) const noexcept;
    AERO_NODISCARD LayoutElement* AsLayoutElement(TreeNode& node) const noexcept;
    AERO_NODISCARD Base::Result<HitTestResult> HitTestElement(
        LayoutElement& element, Point position) const noexcept;
};

struct PointerInput final {
    std::uint32_t pointerId = 0U;
    PointerAction action = PointerAction::Move;
    Point position;
};

struct PointerRouteEvents final {
    RoutedEventHandle moved;
    RoutedEventHandle pressed;
    RoutedEventHandle released;
};

struct PointerDispatchResult final {
    HitTestResult hit;
    bool routed = false;
};

class AERO_API PointerInputManager final {
public:
    PointerInputManager(HitTestManager& hitTests, RoutedEventRegistry& events,
        TreeNode& root, PointerRouteEvents routedEvents) noexcept;

    AERO_NODISCARD Base::Result<PointerDispatchResult> Dispatch(
        const PointerInput& input) noexcept;

private:
    HitTestManager* hitTests_ = nullptr;
    RoutedEventRegistry* events_ = nullptr;
    TreeNode* root_ = nullptr;
    PointerRouteEvents routedEvents_;
};

} // namespace Aero::Core
