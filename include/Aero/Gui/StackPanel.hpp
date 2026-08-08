#pragma once

#include <Aero/Gui/Panel.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

class AERO_GUI_API StackPanel : public Panel {
    AERO_DECLARE_TYPE(StackPanel, Panel)
public:
    StackPanel() noexcept;
    explicit StackPanel(Orientation orientation) noexcept;
    Orientation GetOrientation() const noexcept;
    void SetOrientation(Orientation value) noexcept;
    inline static constexpr DependencyProperty<Orientation> OrientationProperty{"Orientation"};
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
    void SetChildDock(
        UIElement& child, Dock value) noexcept;
    Dock GetChildDock(const UIElement& child) const noexcept;
    inline static constexpr DependencyProperty<bool> LastChildFillProperty{"LastChildFill"};
    inline static constexpr AttachedProperty<Dock> DockProperty{"Dock"};
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
    inline static constexpr DependencyProperty<Orientation> OrientationProperty{"Orientation"};
    // Zero selects the child's desired dimension.
    inline static constexpr DependencyProperty<double> ItemWidthProperty{"ItemWidth"};
    inline static constexpr DependencyProperty<double> ItemHeightProperty{"ItemHeight"};
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
    void SetFirstColumn(
        std::uint32_t value) noexcept;
    inline static constexpr DependencyProperty<std::uint32_t> RowsProperty{"Rows"};
    inline static constexpr DependencyProperty<std::uint32_t> ColumnsProperty{"Columns"};
    inline static constexpr DependencyProperty<std::uint32_t> FirstColumnProperty{"FirstColumn"};
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
private:
    void ResolveDimensions(
        std::uint32_t childCount,
        std::uint32_t& rows,
        std::uint32_t& columns) const noexcept;
};

class AERO_GUI_API Canvas : public Panel {
    AERO_DECLARE_TYPE(Canvas, Panel)
public:
    Canvas() noexcept;
    void SetChildPosition(UIElement& child, Point position) noexcept;
    Point GetChildPosition(const UIElement& child) const noexcept;
    inline static constexpr AttachedProperty<double> LeftProperty{"Left"};
    inline static constexpr AttachedProperty<double> TopProperty{"Top"};
    inline static constexpr AttachedProperty<double> RightProperty{"Right"};
    inline static constexpr AttachedProperty<double> BottomProperty{"Bottom"};
protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

} // namespace Aero::Controls
