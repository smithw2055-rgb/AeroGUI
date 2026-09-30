#pragma once

// Simple panels. Panel itself stays in Panel.hpp.
#include <Aero/Controls/Panel.hpp>

namespace Aero::Controls {

using ::Aero::Meta::TypeId;

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
    // Zero selects the child's desired dimension.
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
