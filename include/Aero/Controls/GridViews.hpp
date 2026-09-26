#pragma once

// Desktop GridView column and presenter family.
#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Style.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/FrameworkElement.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;

enum class GridViewColumnHeaderRole : std::uint8_t {
    Normal = 0U,
    Floating,
    Padding
};

class AERO_GUI_API GridViewColumnHeader : public ContentControl {
    AERO_DECLARE_TYPE(GridViewColumnHeader, ContentControl)
public:
    GridViewColumnHeader() noexcept : ContentControl(StaticTypeId()) {}

    GridViewColumnHeaderRole GetRole() const noexcept { return GetValue(RoleProperty); }
    void SetRole(GridViewColumnHeaderRole value) noexcept { SetValue(RoleProperty, value); }

    AERO_DEPENDENCY_PROPERTY(GridViewColumnHeaderRole, Role);
};

class AERO_GUI_API GridViewColumn : public DependencyObject {
    AERO_DECLARE_TYPE(GridViewColumn, DependencyObject)
public:
    GridViewColumn() noexcept : DependencyObject(StaticTypeId()) {}
    Value GetHeader() const noexcept;
    void SetHeader(Value value) noexcept;
    void SetHeader(StringView value) noexcept;
    double GetWidth() const noexcept;
    void SetWidth(double value) noexcept;
    Ref<DataTemplate> GetCellTemplate() const noexcept;
    void SetCellTemplate(Ref<DataTemplate> value) noexcept;
    Ref<DataTemplate> GetHeaderTemplate() const noexcept;
    void SetHeaderTemplate(Ref<DataTemplate> value) noexcept;
    StringView GetDisplayMemberPath() const noexcept;
    void SetDisplayMemberPath(StringView value) noexcept;
    Ref<Aero::Data::Binding> GetDisplayMemberBinding() const noexcept;
    void SetDisplayMemberBinding(Ref<Aero::Data::Binding> value) noexcept;
    Ref<Style> GetHeaderContainerStyle() const noexcept { return GetValue(HeaderContainerStyleProperty); }
    void SetHeaderContainerStyle(Ref<Style> value) noexcept {
        SetValue(HeaderContainerStyleProperty, std::move(value));
    }

    AERO_DEPENDENCY_PROPERTY(Value, Header);
    AERO_DEPENDENCY_PROPERTY(double, Width);
    AERO_DEPENDENCY_PROPERTY(Ref<DataTemplate>, CellTemplate);
    AERO_DEPENDENCY_PROPERTY(Ref<DataTemplate>, HeaderTemplate);
    AERO_DEPENDENCY_PROPERTY(String, DisplayMemberPath);
    AERO_DEPENDENCY_PROPERTY(Ref<Aero::Data::Binding>, DisplayMemberBinding);
    AERO_DEPENDENCY_PROPERTY(Ref<Style>, HeaderContainerStyle);
};

class AERO_GUI_API ViewBase : public Base::Object {
    AERO_DECLARE_TYPE(ViewBase, Base::Object)
public:
    ViewBase() noexcept = default;
    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }

protected:
    explicit ViewBase(TypeId) noexcept {}
};

class AERO_GUI_API GridView : public ViewBase {
    AERO_DECLARE_TYPE(GridView, ViewBase)
public:
    GridView() noexcept = default;
    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Span<const Ref<GridViewColumn>> GetColumns() const noexcept {
        return {
            columns_.Data(),
            columns_.Size()};
    }
    void AddColumn(Ref<GridViewColumn> column) noexcept;
    void ClearColumns() noexcept { columns_.Clear(); }
    bool GetAllowsColumnReorder() const noexcept { return allowsColumnReorder_; }
    void SetAllowsColumnReorder(bool value) noexcept { allowsColumnReorder_ = value; }
    Ref<Style> GetColumnHeaderContainerStyle() const noexcept { return columnHeaderContainerStyle_; }
    void SetColumnHeaderContainerStyle(Ref<Style> value) noexcept { columnHeaderContainerStyle_ = std::move(value); }
    Ref<Base::Object> GetColumnHeaderContextMenu() const noexcept { return columnHeaderContextMenu_; }
    void SetColumnHeaderContextMenu(Ref<Base::Object> value) noexcept { columnHeaderContextMenu_ = std::move(value); }
    Ref<Base::Object> GetColumnHeaderTemplate() const noexcept { return columnHeaderTemplate_; }
    void SetColumnHeaderTemplate(Ref<Base::Object> value) noexcept { columnHeaderTemplate_ = std::move(value); }
    Ref<Base::Object> GetColumnHeaderTemplateSelector() const noexcept { return columnHeaderTemplateSelector_; }
    void SetColumnHeaderTemplateSelector(Ref<Base::Object> value) noexcept {
        columnHeaderTemplateSelector_ = std::move(value);
    }
    Ref<Base::Object> GetColumnHeaderToolTip() const noexcept { return columnHeaderToolTip_; }
    void SetColumnHeaderToolTip(Ref<Base::Object> value) noexcept { columnHeaderToolTip_ = std::move(value); }
    Ref<Base::Object> GetColumnsObject() const noexcept {
        return Ref<Base::Object>::TryFromBorrowed(
            *const_cast<GridView*>(this));
    }
    void SetColumnsObject(Ref<Base::Object>) noexcept {}

private:
    Base::Vector<Ref<GridViewColumn>> columns_;
    bool allowsColumnReorder_ = false;
    Ref<Style> columnHeaderContainerStyle_;
    Ref<Base::Object> columnHeaderContextMenu_;
    Ref<Base::Object> columnHeaderTemplate_;
    Ref<Base::Object> columnHeaderTemplateSelector_;
    Ref<Base::Object> columnHeaderToolTip_;
};

class AERO_GUI_API GridViewHeaderRowPresenter : public Aero::FrameworkElement {
    AERO_DECLARE_TYPE(GridViewHeaderRowPresenter, Aero::FrameworkElement)
public:
    GridViewHeaderRowPresenter() noexcept : FrameworkElement(StaticTypeId()) {}

    bool GetAllowsColumnReorder() const noexcept { return GetValue(AllowsColumnReorderProperty); }
    void SetAllowsColumnReorder(bool value) noexcept { SetValue(AllowsColumnReorderProperty, value); }

    AERO_DEPENDENCY_PROPERTY(bool, AllowsColumnReorder);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderContainerStyle);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderContextMenu);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderTemplate);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderTemplateSelector);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderToolTip);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, Columns);
};

class AERO_GUI_API GridViewRowPresenter : public Aero::FrameworkElement {
    AERO_DECLARE_TYPE(GridViewRowPresenter, Aero::FrameworkElement)
public:
    GridViewRowPresenter() noexcept : FrameworkElement(StaticTypeId()) {}

    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, Columns);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, Content);
};
} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::GridViewColumnHeaderRole)
