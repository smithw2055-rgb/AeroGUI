#pragma once

#include <Aero/FrameworkElement.hpp>
#include <Aero/Media/Brushes.hpp>
#include <Aero/Layout.hpp>
#include <utility>

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
    std::uint32_t ChildCountCore() const noexcept { return ownedChildren_.Size(); }
    Ref<Base::Object> ChildAtCore(std::uint32_t index) const noexcept {
        return index < ownedChildren_.Size() ? ownedChildren_[index] : Ref<Base::Object>{};
    }
    void AddChildCore(const Ref<Base::Object>& childObject, UIElement& child) noexcept;
    Result<bool> RemoveChildCore(UIElement& child) noexcept;

    Ref<Aero::Media::Brush> GetBackground() const noexcept { return GetValue(BackgroundProperty); }
    void SetBackground(Ref<Aero::Media::Brush> value) noexcept { SetValue(BackgroundProperty, std::move(value)); }
    bool HitTestCore() const noexcept override { return GetBackground() != nullptr; }
    Ref<Base::Object> GetChildrenObject() const noexcept;
    void SetChildrenObject(Ref<Base::Object>) noexcept {}
    UIElementCollection& GetChildren() noexcept { return children_; }
    const UIElementCollection& GetChildren() const noexcept { return children_; }
    void ClearChildrenCore() noexcept;
    void InvalidateZOrder() noexcept { zOrderDirty_ = true; }

    AERO_DEPENDENCY_PROPERTY(Ref<Aero::Media::Brush>, Background);
    AERO_DEPENDENCY_PROPERTY(bool, IsItemsHost);
    AERO_ATTACHED_PROPERTY(std::int32_t, ZIndex);

protected:
    explicit Panel(TypeId runtimeType) noexcept : FrameworkElement(runtimeType), children_(*this), ownedChildren_() {}
    ~Panel() override;

    std::uint32_t GetVisualChildrenCount() const noexcept override;
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override;
    std::uint32_t GetLayoutChildrenCount() const noexcept override;
    UIElement* GetLayoutChild(std::uint32_t index) const noexcept override;
    void OnRender(::Aero::Media::DrawingContext& context) noexcept override;

private:
    friend class UIElementCollection;

    UIElementCollection children_;
    Base::Vector<Ref<Base::Object>> ownedChildren_;
    mutable Base::Ref<Base::Object> childrenView_;
    mutable Base::Vector<UIElement*> zOrderCache_;
    mutable bool zOrderDirty_ = true;
    void NotifyChildrenViewReset() noexcept;
};

class AERO_GUI_API StackPanel : public Panel {
    AERO_DECLARE_TYPE(StackPanel, Panel)

public:
    StackPanel() noexcept;
    explicit StackPanel(Orientation orientation) noexcept;
    Orientation GetOrientation() const noexcept;
    void SetOrientation(Orientation value) noexcept;
    AERO_DEPENDENCY_PROPERTY(Orientation, Orientation);

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

class AERO_GUI_API Canvas : public Panel {
    AERO_DECLARE_TYPE(Canvas, Panel)

public:
    Canvas() noexcept;
    void SetChildPosition(UIElement& child, Point position) noexcept;
    Point GetChildPosition(const UIElement& child) const noexcept;
    AERO_ATTACHED_PROPERTY(double, Left);
    AERO_ATTACHED_PROPERTY(double, Top);
    AERO_ATTACHED_PROPERTY(double, Right);
    AERO_ATTACHED_PROPERTY(double, Bottom);

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

class AERO_GUI_API DockPanel : public Panel {
    AERO_DECLARE_TYPE(DockPanel, Panel)

public:
    DockPanel() noexcept : Panel(StaticTypeId()) {}
    bool GetLastChildFill() const noexcept;
    void SetLastChildFill(bool value) noexcept;
    void SetChildDock(UIElement& child, Dock value) noexcept;
    Dock GetChildDock(const UIElement& child) const noexcept;
    AERO_DEPENDENCY_PROPERTY(bool, LastChildFill);
    AERO_ATTACHED_PROPERTY(Dock, Dock);

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

class AERO_GUI_API WrapPanel : public Panel {
    AERO_DECLARE_TYPE(WrapPanel, Panel)

public:
    WrapPanel() noexcept : Panel(StaticTypeId()) {}
    Orientation GetOrientation() const noexcept;
    void SetOrientation(Orientation value) noexcept;
    double GetItemWidth() const noexcept;
    double GetItemHeight() const noexcept;
    void SetItemWidth(double value) noexcept;
    void SetItemHeight(double value) noexcept;
    AERO_DEPENDENCY_PROPERTY(Orientation, Orientation);
    AERO_DEPENDENCY_PROPERTY(double, ItemWidth);
    AERO_DEPENDENCY_PROPERTY(double, ItemHeight);

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

class AERO_GUI_API UniformGrid : public Panel {
    AERO_DECLARE_TYPE(UniformGrid, Panel)

public:
    UniformGrid() noexcept : Panel(StaticTypeId()) {}
    std::uint32_t GetRows() const noexcept;
    std::uint32_t GetColumns() const noexcept;
    std::uint32_t GetFirstColumn() const noexcept;
    void SetRows(std::uint32_t value) noexcept;
    void SetColumns(std::uint32_t value) noexcept;
    void SetFirstColumn(std::uint32_t value) noexcept;
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, Rows);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, Columns);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, FirstColumn);

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;

private:
    void ResolveDimensions(std::uint32_t childCount, std::uint32_t& rows, std::uint32_t& columns) const noexcept;
};

} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::Orientation)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::Dock)
