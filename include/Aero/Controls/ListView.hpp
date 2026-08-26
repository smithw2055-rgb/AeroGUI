#pragma once

#include <Aero/Controls/ListBox.hpp>
#include <Aero/Controls/ListBoxItem.hpp>
#include <Aero/Controls/TextBlock.hpp>
#include <Aero/Data/Binding.hpp>
#include <Aero/Style.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;
enum class GridViewColumnHeaderRole : std::uint8_t {
    Normal = 0U,
    Floating,
    Padding
};

// Standard GridView header container. The presenter owns header generation
// and interaction; this control supplies the WPF-visible Role state used by
// the default templates.
class AERO_GUI_API GridViewColumnHeader
    : public ContentControl {
    AERO_DECLARE_TYPE(GridViewColumnHeader, ContentControl)
public:
    GridViewColumnHeader() noexcept
        : ContentControl(StaticTypeId()) {}

    GridViewColumnHeaderRole GetRole() const noexcept {
        return GetValueOr(
            RoleProperty, GridViewColumnHeaderRole::Normal);
    }
    void SetRole(
        GridViewColumnHeaderRole value) noexcept {
        SetValue(RoleProperty, value);
    }

    inline static constexpr DependencyProperty<GridViewColumnHeaderRole> RoleProperty{"Role"};
};

class AERO_GUI_API GridViewColumn
    : public DependencyObject {
    AERO_DECLARE_TYPE(GridViewColumn, DependencyObject)
public:
    GridViewColumn() noexcept
        : DependencyObject(StaticTypeId()) {}
    Value GetHeader() const noexcept;
    void SetHeader(Value value) noexcept;
    Result<void> SetHeader(StringView value) noexcept;
    double GetWidth() const noexcept;
    void SetWidth(
        double value) noexcept;
    Ref<DataTemplate>
        GetCellTemplate() const noexcept;
    void SetCellTemplate(
        Ref<DataTemplate> value) noexcept;
    Ref<DataTemplate>
        GetHeaderTemplate() const noexcept;
    void SetHeaderTemplate(
        Ref<DataTemplate> value) noexcept;
    StringView GetDisplayMemberPath()
        const noexcept;
    void SetDisplayMemberPath(
        StringView value) noexcept;
    Ref<Aero::Data::Binding>
        GetDisplayMemberBinding() const noexcept;
    void SetDisplayMemberBinding(
        Ref<Aero::Data::Binding> value) noexcept;
    Ref<Style> GetHeaderContainerStyle() const noexcept {
        return GetValueOr(
            HeaderContainerStyleProperty,
            Ref<Style>{});
    }
    void SetHeaderContainerStyle(
        Ref<Style> value) noexcept {
        SetValue(HeaderContainerStyleProperty, std::move(value));
    }

    inline static constexpr DependencyProperty<Value> HeaderProperty{"Header"};
    inline static constexpr DependencyProperty<double> WidthProperty{"Width"};
    inline static constexpr DependencyProperty<Ref<DataTemplate>> CellTemplateProperty{"CellTemplate"};
    inline static constexpr DependencyProperty<Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};
    inline static constexpr DependencyProperty<String> DisplayMemberPathProperty{"DisplayMemberPath"};
    inline static constexpr DependencyProperty<Ref<Aero::Data::Binding>> DisplayMemberBindingProperty{"DisplayMemberBinding"};
    inline static constexpr DependencyProperty<Ref<Style>> HeaderContainerStyleProperty{"HeaderContainerStyle"};
};

class AERO_GUI_API GridView
    : public Base::Object {
    AERO_DECLARE_TYPE(GridView, Base::Object)
public:
    GridView() noexcept = default;
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Span<const Ref<GridViewColumn>>
        GetColumns() const noexcept {
        return {
            columns_.Data(),
            columns_.Size()};
    }
    Result<void> AddColumn(
        Ref<GridViewColumn> column)
        noexcept;
    void ClearColumns() noexcept {
        columns_.Clear();
    }
    bool GetAllowsColumnReorder() const noexcept {
        return allowsColumnReorder_;
    }
    void SetAllowsColumnReorder(bool value) noexcept {
        allowsColumnReorder_ = value;
    }
    Ref<Style> GetColumnHeaderContainerStyle() const noexcept {
        return columnHeaderContainerStyle_;
    }
    void SetColumnHeaderContainerStyle(
        Ref<Style> value) noexcept {
        columnHeaderContainerStyle_ = std::move(value);
    }
    Ref<Base::Object> GetColumnHeaderContextMenu() const noexcept {
        return columnHeaderContextMenu_;
    }
    void SetColumnHeaderContextMenu(Ref<Base::Object> value) noexcept {
        columnHeaderContextMenu_ = std::move(value);
    }
    Ref<Base::Object> GetColumnHeaderTemplate() const noexcept {
        return columnHeaderTemplate_;
    }
    void SetColumnHeaderTemplate(Ref<Base::Object> value) noexcept {
        columnHeaderTemplate_ = std::move(value);
    }
    Ref<Base::Object> GetColumnHeaderTemplateSelector() const noexcept {
        return columnHeaderTemplateSelector_;
    }
    void SetColumnHeaderTemplateSelector(Ref<Base::Object> value) noexcept {
        columnHeaderTemplateSelector_ = std::move(value);
    }
    Ref<Base::Object> GetColumnHeaderToolTip() const noexcept {
        return columnHeaderToolTip_;
    }
    void SetColumnHeaderToolTip(Ref<Base::Object> value) noexcept {
        columnHeaderToolTip_ = std::move(value);
    }
    Ref<Base::Object> GetColumnsObject() const noexcept {
        return Ref<Base::Object>::TryFromBorrowed(
            *const_cast<GridView*>(this));
    }
    void SetColumnsObject(Ref<Base::Object>) noexcept {}

private:
    Base::Vector<Ref<GridViewColumn>>
        columns_;
    bool allowsColumnReorder_ = false;
    Ref<Style> columnHeaderContainerStyle_;
    Ref<Base::Object> columnHeaderContextMenu_;
    Ref<Base::Object> columnHeaderTemplate_;
    Ref<Base::Object> columnHeaderTemplateSelector_;
    Ref<Base::Object> columnHeaderToolTip_;
};

// Hosts GridView column headers inside the ListView ScrollViewer template.
// The column collection is normally supplied by a template binding from the
// owning ListView's GridView and is consumed by the view implementation.
class AERO_GUI_API GridViewHeaderRowPresenter
    : public Aero::FrameworkElement {
    AERO_DECLARE_TYPE(
        GridViewHeaderRowPresenter,
        Aero::FrameworkElement)
public:
    GridViewHeaderRowPresenter() noexcept
        : FrameworkElement(StaticTypeId()) {}

    bool GetAllowsColumnReorder() const noexcept {
        return GetValueOr(AllowsColumnReorderProperty, false);
    }
    void SetAllowsColumnReorder(bool value) noexcept {
        SetValue(AllowsColumnReorderProperty, value);
    }

    inline static constexpr DependencyProperty<bool> AllowsColumnReorderProperty{"AllowsColumnReorder"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ColumnHeaderContainerStyleProperty{"ColumnHeaderContainerStyle"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ColumnHeaderContextMenuProperty{"ColumnHeaderContextMenu"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ColumnHeaderTemplateProperty{"ColumnHeaderTemplate"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ColumnHeaderTemplateSelectorProperty{"ColumnHeaderTemplateSelector"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ColumnHeaderToolTipProperty{"ColumnHeaderToolTip"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ColumnsProperty{"Columns"};
};

// The row counterpart to GridViewHeaderRowPresenter. It is instantiated by
// ListViewItem templates and receives the active GridView columns/content
// during ListView container realization.
class AERO_GUI_API GridViewRowPresenter
    : public Aero::FrameworkElement {
    AERO_DECLARE_TYPE(
        GridViewRowPresenter,
        Aero::FrameworkElement)
public:
    GridViewRowPresenter() noexcept
        : FrameworkElement(StaticTypeId()) {}

    inline static constexpr DependencyProperty<Ref<Base::Object>> ColumnsProperty{"Columns"};
    inline static constexpr DependencyProperty<Ref<Base::Object>> ContentProperty{"Content"};
};

class AERO_GUI_API ListViewItem
    : public ListBoxItem {
    AERO_DECLARE_TYPE(ListViewItem, ListBoxItem)
public:
    ListViewItem() noexcept
        : ListBoxItem(StaticTypeId()) {}
    ~ListViewItem() override = default;
};

class AERO_GUI_API ListView
    : public ListBox {
    AERO_DECLARE_TYPE(ListView, ListBox)
public:
    ListView() noexcept
        : ListBox(StaticTypeId()) {}
    ~ListView() override = default;

    Ref<GridView> GetView() const noexcept;
    void SetView(
        Ref<GridView> value) noexcept;

    inline static constexpr DependencyProperty<Ref<GridView>> ViewProperty{"View"};

protected:
    void
        OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    Result<Ref<FrameworkElement>>
        CreateContainer(
            const Ref<Base::Object>& item)
            noexcept override;

private:
    TextBlock* columnHeaders_ = nullptr;
    Result<void>
        SynchronizeColumnHeaders() noexcept;
};
} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::GridViewColumnHeaderRole)
