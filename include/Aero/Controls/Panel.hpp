#pragma once

#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Layout.hpp>
#include <utility>

namespace Aero { class AeroGuiInternal; }
namespace Aero::Controls {

using ::Aero::Meta::TypeId;

class AERO_GUI_API Panel;

enum class Orientation : std::uint8_t { Horizontal = 0U, Vertical };
enum class Dock : std::uint8_t { Left = 0U, Top, Right, Bottom };

class AERO_GUI_API UIElementCollection {
public:
    std::uint32_t GetCount() const noexcept;
    bool GetIsEmpty() const noexcept { return GetCount() == 0U; }
    UIElement* GetItem(std::uint32_t index) const noexcept;
    void Add(Ref<UIElement> child) noexcept;
    void Remove(UIElement& child) noexcept;
    void Clear() noexcept;

private:
    friend class Panel;
    explicit UIElementCollection(Panel& owner) noexcept : owner_(&owner) {}
    Panel* owner_ = nullptr;
};

class AERO_GUI_API Panel : public FrameworkElement {
    AERO_DECLARE_TYPE(Panel, FrameworkElement)
public:
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    Ref<Aero::Media::Brush> GetBackground() const noexcept {
        return GetValue(BackgroundProperty);
    }
    void SetBackground(Ref<Aero::Media::Brush> value) noexcept {
        SetValue(BackgroundProperty, std::move(value));
    }
    AERO_DEPENDENCY_PROPERTY(Ref<Aero::Media::Brush>, Background);
    AERO_DEPENDENCY_PROPERTY(bool, IsItemsHost);
    AERO_ATTACHED_PROPERTY(std::int32_t, ZIndex);
    UIElementCollection& GetChildren() noexcept { return children_; }
    const UIElementCollection& GetChildren() const noexcept { return children_; }
protected:
    explicit Panel(TypeId runtimeType) noexcept
        : FrameworkElement(runtimeType), children_(*this), ownedChildren_() {}
    ~Panel() override = default;
    std::uint32_t GetVisualChildrenCount() const noexcept override;
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override;
    std::uint32_t GetLayoutChildrenCount() const noexcept override;
    UIElement* GetLayoutChild(std::uint32_t index) const noexcept override;
    void OnRender(
        ::Aero::Media::DrawingContext& context) noexcept override;
private:
    friend class UIElementCollection;
    friend class ::Aero::AeroGuiInternal;
    std::uint32_t ChildCountCore() const noexcept { return ownedChildren_.Size(); }
    Ref<Base::Object> ChildAtCore(std::uint32_t index) const noexcept {
        return index < ownedChildren_.Size() ? ownedChildren_[index] : Ref<Base::Object>{};
    }
    void AddChildCore(const Ref<Base::Object>& childObject, UIElement& child) noexcept;
    Result<bool> RemoveChildCore(UIElement& child) noexcept;
    void ClearChildrenCore() noexcept;
    UIElementCollection children_;
    Base::Vector<Ref<Base::Object>> ownedChildren_;
};

} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::Orientation)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::Dock)
