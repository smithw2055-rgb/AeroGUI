#pragma once

#include <Aero/Controls/Base.hpp>

namespace Aero::Controls::Detail {

class PanelAccess final {
public:
    static std::uint32_t Count(const Panel& panel) noexcept { return panel.ChildCountCore(); }
    static Base::Ref<Base::Object> At(const Panel& panel, std::uint32_t index) noexcept { return panel.ChildAtCore(index); }
    static Base::Result<void> Add(Panel& panel, const Base::Ref<Base::Object>& owner, UIElement& child) noexcept { return panel.AddChildCore(owner, child); }
    static Base::Result<bool> Remove(Panel& panel, UIElement& child) noexcept { return panel.RemoveChildCore(child); }
    static Base::Result<void> Clear(Panel& panel) noexcept { return panel.ClearChildrenCore(); }
};

class DecoratorAccess final {
public:
    static const Base::Ref<Base::Object>& OwnedChild(const Decorator& decorator) noexcept { return decorator.ownedChild_; }
    static Base::Result<void> SetOwnedChild(Decorator& decorator, const Base::Ref<Base::Object>& owner, UIElement& child) noexcept { return decorator.SetOwnedChild(owner, child); }
};

} // namespace Aero::Controls::Detail
