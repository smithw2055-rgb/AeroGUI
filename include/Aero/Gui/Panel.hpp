#pragma once

#include <Aero/Gui/FrameworkElement.hpp>
#include <Aero/Gui/Brush.hpp>
#include <Aero/Layout.hpp>
#include <utility>

namespace Aero::Controls {

using ::Aero::Meta::TypeId;

enum class Orientation : std::uint8_t { Horizontal = 0U, Vertical };
enum class Dock : std::uint8_t { Left = 0U, Top, Right, Bottom };

class AERO_API UIElementCollection {
public:
    std::uint32_t GetCount() const noexcept;
    bool GetIsEmpty() const noexcept { return GetCount() == 0U; }
    UIElement* GetItem(std::uint32_t index) const noexcept;
    Base::Result<void> Add(Base::Ref<UIElement> child) noexcept;
    Base::Result<void> Remove(UIElement& child) noexcept;
    void Clear() noexcept;

private:
    friend class Panel;
    explicit UIElementCollection(Panel& owner) noexcept : owner_(&owner) {}
    Panel* owner_ = nullptr;
};

class AERO_API Panel : public FrameworkElement {
    AERO_DECLARE_TYPE(Panel, FrameworkElement)
public:
    Base::Ref<Aero::Media::Brush> GetBackground() const noexcept {
        return GetValueOr(
            BackgroundProperty,
            Base::Ref<Aero::Media::Brush>{});
    }
    void SetBackground(
        Base::Ref<Aero::Media::Brush> value) noexcept {
        SetValue(BackgroundProperty, std::move(value));
    }
    inline static constexpr DependencyProperty<Base::Ref<Aero::Media::Brush>> BackgroundProperty{"Background"};
    inline static constexpr DependencyProperty<bool> IsItemsHostProperty{"IsItemsHost"};
    inline static constexpr AttachedProperty<std::int32_t> ZIndexProperty{"ZIndex"};
    UIElementCollection& GetChildren() noexcept { return children_; }
    const UIElementCollection& GetChildren() const noexcept { return children_; }
protected:
    explicit Panel(TypeId runtimeType) noexcept
        : FrameworkElement(runtimeType), children_(*this), ownedChildren_() {}
    ~Panel() override = default;
    void OnRender(
        ::Aero::Media::DrawingContext& context) noexcept override;
private:
    friend class UIElementCollection;
    friend struct ::Aero::Media::Visual::Impl;
    std::uint32_t ChildCountCore() const noexcept { return ownedChildren_.Size(); }
    Base::Ref<Base::Object> ChildAtCore(std::uint32_t index) const noexcept {
        return index < ownedChildren_.Size() ? ownedChildren_[index] : Base::Ref<Base::Object>{};
    }
    Base::Result<void> AddChildCore(const Base::Ref<Base::Object>& childObject, UIElement& child) noexcept;
    Base::Result<bool> RemoveChildCore(UIElement& child) noexcept;
    void ClearChildrenCore() noexcept;
    UIElementCollection children_;
    Base::Vector<Base::Ref<Base::Object>> ownedChildren_;
};

} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::Orientation)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::Dock)
