#pragma once

#include <Aero/Visual.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>

#include <cstdint>

namespace Aero {

struct VisualHandle final {
    std::uint32_t index = UINT32_MAX;
    std::uint32_t generation = 0U;

    constexpr bool IsValid() const noexcept {
        return index != UINT32_MAX && generation != 0U;
    }
};

struct ObjectTreeLifecycleEvent final {
    Visual* node = nullptr;
    bool loaded = false;
    std::uint64_t treeVersion = 0U;
};

using ObjectTreeLifecycleHandler = void (*)(const ObjectTreeLifecycleEvent& event, void* context) noexcept;

} // namespace Aero

namespace Aero::Detail {

class VisualLifetime final : public Base::Object {
public:
    explicit VisualLifetime(Visual& node) noexcept : node_(&node) {}
    ~VisualLifetime() override = default;

    Visual* Node() const noexcept { return node_; }
    void Invalidate() noexcept { node_ = nullptr; }

private:
    Visual* node_ = nullptr;
};

struct VisualLease final {
    Base::Ref<Visual> strong;
    Base::Ref<VisualLifetime> lifetime;

    static Base::Result<VisualLease> Acquire(Visual& node) noexcept;

    Visual* Resolve() const noexcept {
        return strong ? strong.Get() : (lifetime ? lifetime->Node() : nullptr);
    }
};

class VisualAccess final {
public:
    static ObjectTree* Tree(const Visual& visual) noexcept { return visual.tree_; }
    static Visual* LogicalParent(const Visual& visual) noexcept { return visual.logicalParent_; }
    static Visual* VisualParent(const Visual& visual) noexcept { return visual.visualParent_; }
    static Base::Span<Visual* const> LogicalChildren(const Visual& visual) noexcept { return {visual.logicalChildren_.Data(), visual.logicalChildren_.Size()}; }
    static Base::Span<Visual* const> VisualChildren(const Visual& visual) noexcept { return {visual.visualChildren_.Data(), visual.visualChildren_.Size()}; }
    static bool IsLoaded(const Visual& visual) noexcept { return visual.loaded_; }
    static VisualHandle Handle(const Visual& visual) noexcept { return {visual.handleIndex_, visual.handleGeneration_}; }
    static void SetHandle(Visual& visual, VisualHandle handle) noexcept { visual.handleIndex_ = handle.index; visual.handleGeneration_ = handle.generation; }
    static UIElement* AsUIElement(Visual& visual) noexcept { return visual.AsUIElement(); }
    static const UIElement* AsUIElement(const Visual& visual) noexcept { return visual.AsUIElement(); }
    static FrameworkElement* AsFrameworkElement(Visual& visual) noexcept { return visual.AsFrameworkElement(); }
    static const FrameworkElement* AsFrameworkElement(const Visual& visual) noexcept { return visual.AsFrameworkElement(); }
    static Base::Result<Base::Ref<Base::Object>> AcquireLifetime(Visual& visual) noexcept { return visual.AcquireLifetime(); }
};

} // namespace Aero::Detail
