#pragma once

#include <Aero/Gui/ListBox.hpp>
#include <Aero/Gui/TextBlock.hpp>
#include <Aero/Gui/BindingBase.hpp>
#include <Aero/Gui/Style.hpp>

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
class AERO_API GridViewColumnHeader
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

class AERO_API GridViewColumn
    : public DependencyObject {
    AERO_DECLARE_TYPE(GridViewColumn, DependencyObject)
public:
    GridViewColumn() noexcept
        : DependencyObject(StaticTypeId()) {}
    Value GetHeader() const noexcept;
    void SetHeader(Value value) noexcept;
    Base::Result<void> SetHeader(Base::StringView value) noexcept;
    double GetWidth() const noexcept;
    void SetWidth(
        double value) noexcept;
    Base::Ref<DataTemplate>
        GetCellTemplate() const noexcept;
    void SetCellTemplate(
        Base::Ref<DataTemplate> value) noexcept;
    Base::Ref<DataTemplate>
        GetHeaderTemplate() const noexcept;
    void SetHeaderTemplate(
        Base::Ref<DataTemplate> value) noexcept;
    Base::StringView GetDisplayMemberPath()
        const noexcept;
    void SetDisplayMemberPath(
        Base::StringView value) noexcept;
    Base::Ref<Aero::Data::Binding>
        GetDisplayMemberBinding() const noexcept;
    void SetDisplayMemberBinding(
        Base::Ref<Aero::Data::Binding> value) noexcept;
    Base::Ref<Style> GetHeaderContainerStyle() const noexcept {
        return GetValueOr(
            HeaderContainerStyleProperty,
            Base::Ref<Style>{});
    }
    void SetHeaderContainerStyle(
        Base::Ref<Style> value) noexcept {
        SetValue(HeaderContainerStyleProperty, std::move(value));
    }

    inline static constexpr DependencyProperty<Value> HeaderProperty{"Header"};
    inline static constexpr DependencyProperty<double> WidthProperty{"Width"};
    inline static constexpr DependencyProperty<Base::Ref<DataTemplate>> CellTemplateProperty{"CellTemplate"};
    inline static constexpr DependencyProperty<Base::Ref<DataTemplate>> HeaderTemplateProperty{"HeaderTemplate"};
    inline static constexpr DependencyProperty<Base::String> DisplayMemberPathProperty{"DisplayMemberPath"};
    inline static constexpr DependencyProperty<Base::Ref<Aero::Data::Binding>> DisplayMemberBindingProperty{"DisplayMemberBinding"};
    inline static constexpr DependencyProperty<Base::Ref<Style>> HeaderContainerStyleProperty{"HeaderContainerStyle"};
};

class AERO_API GridView
    : public Base::Object {
    AERO_DECLARE_TYPE(GridView, Base::Object)
public:
    GridView() noexcept = default;
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Span<const Base::Ref<GridViewColumn>>
        GetColumns() const noexcept {
        return {
            columns_.Data(),
            columns_.Size()};
    }
    Base::Result<void> AddColumn(
        Base::Ref<GridViewColumn> column)
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
    Base::Ref<Style> GetColumnHeaderContainerStyle() const noexcept {
        return columnHeaderContainerStyle_;
    }
    void SetColumnHeaderContainerStyle(
        Base::Ref<Style> value) noexcept {
        columnHeaderContainerStyle_ = std::move(value);
    }
    Base::Ref<Base::Object> GetColumnHeaderContextMenu() const noexcept {
        return columnHeaderContextMenu_;
    }
    void SetColumnHeaderContextMenu(Base::Ref<Base::Object> value) noexcept {
        columnHeaderContextMenu_ = std::move(value);
    }
    Base::Ref<Base::Object> GetColumnHeaderTemplate() const noexcept {
        return columnHeaderTemplate_;
    }
    void SetColumnHeaderTemplate(Base::Ref<Base::Object> value) noexcept {
        columnHeaderTemplate_ = std::move(value);
    }
    Base::Ref<Base::Object> GetColumnHeaderTemplateSelector() const noexcept {
        return columnHeaderTemplateSelector_;
    }
    void SetColumnHeaderTemplateSelector(Base::Ref<Base::Object> value) noexcept {
        columnHeaderTemplateSelector_ = std::move(value);
    }
    Base::Ref<Base::Object> GetColumnHeaderToolTip() const noexcept {
        return columnHeaderToolTip_;
    }
    void SetColumnHeaderToolTip(Base::Ref<Base::Object> value) noexcept {
        columnHeaderToolTip_ = std::move(value);
    }
    Base::Ref<Base::Object> GetColumnsObject() const noexcept {
        return Base::Ref<Base::Object>::TryFromBorrowed(
            *const_cast<GridView*>(this));
    }
    void SetColumnsObject(Base::Ref<Base::Object>) noexcept {}

private:
    Base::Vector<Base::Ref<GridViewColumn>>
        columns_;
    bool allowsColumnReorder_ = false;
    Base::Ref<Style> columnHeaderContainerStyle_;
    Base::Ref<Base::Object> columnHeaderContextMenu_;
    Base::Ref<Base::Object> columnHeaderTemplate_;
    Base::Ref<Base::Object> columnHeaderTemplateSelector_;
    Base::Ref<Base::Object> columnHeaderToolTip_;
};

// Hosts GridView column headers inside the ListView ScrollViewer template.
// The column collection is normally supplied by a template binding from the
// owning ListView's GridView and is consumed by the view implementation.
class AERO_API GridViewHeaderRowPresenter
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
    inline static constexpr DependencyProperty<Base::Ref<Base::Object>> ColumnHeaderContainerStyleProperty{"ColumnHeaderContainerStyle"};
    inline static constexpr DependencyProperty<Base::Ref<Base::Object>> ColumnHeaderContextMenuProperty{"ColumnHeaderContextMenu"};
    inline static constexpr DependencyProperty<Base::Ref<Base::Object>> ColumnHeaderTemplateProperty{"ColumnHeaderTemplate"};
    inline static constexpr DependencyProperty<Base::Ref<Base::Object>> ColumnHeaderTemplateSelectorProperty{"ColumnHeaderTemplateSelector"};
    inline static constexpr DependencyProperty<Base::Ref<Base::Object>> ColumnHeaderToolTipProperty{"ColumnHeaderToolTip"};
    inline static constexpr DependencyProperty<Base::Ref<Base::Object>> ColumnsProperty{"Columns"};
};

// The row counterpart to GridViewHeaderRowPresenter. It is instantiated by
// ListViewItem templates and receives the active GridView columns/content
// during ListView container realization.
class AERO_API GridViewRowPresenter
    : public Aero::FrameworkElement {
    AERO_DECLARE_TYPE(
        GridViewRowPresenter,
        Aero::FrameworkElement)
public:
    GridViewRowPresenter() noexcept
        : FrameworkElement(StaticTypeId()) {}

    inline static constexpr DependencyProperty<Base::Ref<Base::Object>> ColumnsProperty{"Columns"};
    inline static constexpr DependencyProperty<Base::Ref<Base::Object>> ContentProperty{"Content"};
};

class AERO_API ListViewItem
    : public ListBoxItem {
    AERO_DECLARE_TYPE(ListViewItem, ListBoxItem)
public:
    ListViewItem() noexcept
        : ListBoxItem(StaticTypeId()) {}
    ~ListViewItem() override = default;
};

class AERO_API ListView
    : public ListBox {
    AERO_DECLARE_TYPE(ListView, ListBox)
public:
    ListView() noexcept
        : ListBox(StaticTypeId()) {}
    ~ListView() override = default;

    Base::Ref<GridView> GetView() const noexcept;
    void SetView(
        Base::Ref<GridView> value) noexcept;

    inline static constexpr DependencyProperty<Base::Ref<GridView>> ViewProperty{"View"};

protected:
    void
        OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    Base::Result<Base::Ref<FrameworkElement>>
        CreateContainer(
            const Base::Ref<Base::Object>& item)
            noexcept override;

private:
    TextBlock* columnHeaders_ = nullptr;
    Base::Result<void>
        SynchronizeColumnHeaders() noexcept;
};
} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::GridViewColumnHeaderRole)
