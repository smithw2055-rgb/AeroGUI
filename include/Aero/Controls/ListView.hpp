#pragma once

#include <Aero/Controls/Selection.hpp>
#include <Aero/Presentation/Binding.hpp>

namespace Aero::Controls {

enum class GridViewColumnHeaderRole : std::uint8_t {
    Normal = 0U,
    Floating,
    Padding
};

// Standard GridView header container. The presenter owns header generation
// and interaction; this control supplies the WPF-visible Role state used by
// the default templates.
class AERO_API GridViewColumnHeader final
    : public ContentControl {
    AERO_DECLARE_TYPE(GridViewColumnHeader, ContentControl)
public:
    GridViewColumnHeader() noexcept
        : ContentControl(StaticTypeId()) {}

    GridViewColumnHeaderRole Role() const noexcept {
        return GetValueOr(
            RoleProperty, GridViewColumnHeaderRole::Normal);
    }
    Base::Result<void> SetRole(
        GridViewColumnHeaderRole value) noexcept {
        return SetValue(RoleProperty, value);
    }

    inline static constexpr Members::Property<
        GridViewColumnHeaderRole>
        RoleProperty{"Role"};
};

class AERO_API GridViewColumn final
    : public DependencyObject {
    AERO_DECLARE_TYPE(GridViewColumn, DependencyObject)
public:
    GridViewColumn() noexcept
        : DependencyObject(StaticTypeId()) {}
    Base::StringView Header() const noexcept;
    Base::Result<void> SetHeader(
        Base::StringView value) noexcept;
    double Width() const noexcept;
    Base::Result<void> SetWidth(
        double value) noexcept;
    Base::Ref<DataTemplate>
        CellTemplate() const noexcept;
    Base::Result<void> SetCellTemplate(
        Base::Ref<DataTemplate> value) noexcept;
    Base::Ref<DataTemplate>
        HeaderTemplate() const noexcept;
    Base::Result<void> SetHeaderTemplate(
        Base::Ref<DataTemplate> value) noexcept;
    Base::StringView DisplayMemberPath()
        const noexcept;
    Base::Result<void> SetDisplayMemberPath(
        Base::StringView value) noexcept;
    Base::Ref<Presentation::BindingSpec>
        DisplayMemberBinding() const noexcept;
    Base::Result<void> SetDisplayMemberBinding(
        Base::Ref<Presentation::BindingSpec> value) noexcept;
    Base::Ref<Style> HeaderContainerStyle() const noexcept {
        return GetValueOr(
            HeaderContainerStyleProperty,
            Base::Ref<Style>{});
    }
    Base::Result<void> SetHeaderContainerStyle(
        Base::Ref<Style> value) noexcept {
        return SetValue(
            HeaderContainerStyleProperty, std::move(value));
    }

    inline static constexpr Members::Property<
        Base::String> HeaderProperty{"Header"};
    inline static constexpr Members::Property<double>
        WidthProperty{"Width"};
    inline static constexpr Members::Property<
        Base::Ref<DataTemplate>>
        CellTemplateProperty{"CellTemplate"};
    inline static constexpr Members::Property<
        Base::Ref<DataTemplate>>
        HeaderTemplateProperty{"HeaderTemplate"};
    inline static constexpr Members::Property<
        Base::String>
        DisplayMemberPathProperty{
            "DisplayMemberPath"};
    inline static constexpr Members::Property<
        Base::Ref<Presentation::BindingSpec>>
        DisplayMemberBindingProperty{
            "DisplayMemberBinding"};
    inline static constexpr Members::Property<Base::Ref<Style>>
        HeaderContainerStyleProperty{"HeaderContainerStyle"};
};

class AERO_API GridView final
    : public Base::Object {
    AERO_DECLARE_TYPE(GridView, Base::Object)
public:
    GridView() noexcept = default;
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    Base::Span<const Base::Ref<GridViewColumn>>
        Columns() const noexcept {
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
    Base::Ref<Style> ColumnHeaderContainerStyle() const noexcept {
        return columnHeaderContainerStyle_;
    }
    Base::Result<void> SetColumnHeaderContainerStyle(
        Base::Ref<Style> value) noexcept {
        columnHeaderContainerStyle_ = std::move(value);
        return {};
    }

private:
    Base::Vector<Base::Ref<GridViewColumn>>
        columns_;
    Base::Ref<Style> columnHeaderContainerStyle_;
};

// Hosts GridView column headers inside the ListView ScrollViewer template.
// The column collection is normally supplied by a template binding from the
// owning ListView's GridView and is consumed by the view implementation.
class AERO_API GridViewHeaderRowPresenter final
    : public Presentation::FrameworkElement {
    AERO_DECLARE_TYPE(
        GridViewHeaderRowPresenter,
        Presentation::FrameworkElement)
public:
    GridViewHeaderRowPresenter() noexcept
        : FrameworkElement(StaticTypeId()) {}

    bool AllowsColumnReorder() const noexcept {
        return GetValueOr(AllowsColumnReorderProperty, false);
    }
    Base::Result<void> SetAllowsColumnReorder(bool value) noexcept {
        return SetValue(AllowsColumnReorderProperty, value);
    }

    inline static constexpr Members::Property<bool>
        AllowsColumnReorderProperty{"AllowsColumnReorder"};
    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        ColumnHeaderContainerStyleProperty{
            "ColumnHeaderContainerStyle"};
    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        ColumnHeaderContextMenuProperty{
            "ColumnHeaderContextMenu"};
    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        ColumnHeaderTemplateProperty{
            "ColumnHeaderTemplate"};
    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        ColumnHeaderTemplateSelectorProperty{
            "ColumnHeaderTemplateSelector"};
    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        ColumnHeaderToolTipProperty{"ColumnHeaderToolTip"};
    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        ColumnsProperty{"Columns"};
};

// The row counterpart to GridViewHeaderRowPresenter. It is instantiated by
// ListViewItem templates and receives the active GridView columns/content
// during ListView container realization.
class AERO_API GridViewRowPresenter final
    : public Presentation::FrameworkElement {
    AERO_DECLARE_TYPE(
        GridViewRowPresenter,
        Presentation::FrameworkElement)
public:
    GridViewRowPresenter() noexcept
        : FrameworkElement(StaticTypeId()) {}

    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        ColumnsProperty{"Columns"};
    inline static constexpr Members::Property<
        Base::Ref<Base::Object>>
        ContentProperty{"Content"};
};

class AERO_API ListViewItem final
    : public ListBoxItem {
    AERO_DECLARE_TYPE(ListViewItem, ListBoxItem)
public:
    ListViewItem() noexcept
        : ListBoxItem(StaticTypeId()) {}
    ~ListViewItem() override = default;
};

class AERO_API ListView final
    : public ListBox {
    AERO_DECLARE_TYPE(ListView, ListBox)
public:
    ListView() noexcept
        : ListBox(StaticTypeId()) {}
    ~ListView() override = default;

    Base::Ref<GridView> View() const noexcept;
    Base::Result<void> SetView(
        Base::Ref<GridView> value) noexcept;

    inline static constexpr Members::Property<
        Base::Ref<GridView>>
        ViewProperty{"View"};

protected:
    Base::Result<void>
        OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    Base::Result<Base::Ref<ItemContainer>>
        CreateContainer(
            const Base::Ref<Base::Object>& item)
            noexcept override;

private:
    TextBlock* columnHeaders_ = nullptr;
    Base::Result<void>
        SynchronizeColumnHeaders() noexcept;
};

} // namespace Aero::Controls

namespace Aero::Core {

template<>
struct MetaTypeTraits<Controls::GridViewColumnHeaderRole> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("GridViewColumnHeaderRole");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "GridViewColumnHeaderRole";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core
