#pragma once

#include <Aero/Controls/Selectors.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Controls/TextBlock.hpp>
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

class AERO_GUI_API ViewBase : public DependencyObject {
    AERO_DECLARE_TYPE(ViewBase, DependencyObject)

public:
    ViewBase() noexcept : DependencyObject(StaticTypeId()) {}
    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }

protected:
    explicit ViewBase(TypeId runtimeType) noexcept : DependencyObject(runtimeType) {}
};

class AERO_GUI_API GridView : public ViewBase {
    AERO_DECLARE_TYPE(GridView, ViewBase)

public:
    GridView() noexcept : ViewBase(StaticTypeId()) {}
    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    Span<const Ref<GridViewColumn>> GetColumns() const noexcept {
        return {
            columns_.Data(),
            columns_.Size()};
    }
    void AddColumn(Ref<GridViewColumn> column) noexcept;
    void ClearColumns() noexcept { columns_.Clear(); }
    bool GetAllowsColumnReorder() const noexcept { return GetValue(AllowsColumnReorderProperty); }
    void SetAllowsColumnReorder(bool value) noexcept { SetValue(AllowsColumnReorderProperty, value); }
    Ref<Style> GetColumnHeaderContainerStyle() const noexcept { return GetValue(ColumnHeaderContainerStyleProperty); }
    void SetColumnHeaderContainerStyle(Ref<Style> value) noexcept { SetValue(ColumnHeaderContainerStyleProperty, std::move(value)); }
    Ref<Base::Object> GetColumnHeaderContextMenu() const noexcept { return GetValue(ColumnHeaderContextMenuProperty); }
    void SetColumnHeaderContextMenu(Ref<Base::Object> value) noexcept { SetValue(ColumnHeaderContextMenuProperty, std::move(value)); }
    Ref<Base::Object> GetColumnHeaderTemplate() const noexcept { return GetValue(ColumnHeaderTemplateProperty); }
    void SetColumnHeaderTemplate(Ref<Base::Object> value) noexcept { SetValue(ColumnHeaderTemplateProperty, std::move(value)); }
    Ref<Base::Object> GetColumnHeaderTemplateSelector() const noexcept { return GetValue(ColumnHeaderTemplateSelectorProperty); }
    void SetColumnHeaderTemplateSelector(Ref<Base::Object> value) noexcept {
        SetValue(ColumnHeaderTemplateSelectorProperty, std::move(value));
    }
    Ref<Base::Object> GetColumnHeaderToolTip() const noexcept { return GetValue(ColumnHeaderToolTipProperty); }
    void SetColumnHeaderToolTip(Ref<Base::Object> value) noexcept { SetValue(ColumnHeaderToolTipProperty, std::move(value)); }
    Ref<Base::Object> GetColumnsObject() const noexcept {
        return Ref<Base::Object>::TryFromBorrowed(
            *const_cast<GridView*>(this));
    }
    void SetColumnsObject(Ref<Base::Object>) noexcept {}

    AERO_DEPENDENCY_PROPERTY(bool, AllowsColumnReorder);
    AERO_DEPENDENCY_PROPERTY(Ref<Style>, ColumnHeaderContainerStyle);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderContextMenu);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderTemplate);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderTemplateSelector);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderToolTip);

private:
    Base::Vector<Ref<GridViewColumn>> columns_;
};

namespace Primitives {

class AERO_GUI_API GridViewRowPresenterBase : public Aero::FrameworkElement {
    AERO_DECLARE_TYPE(GridViewRowPresenterBase, Aero::FrameworkElement)

public:
    ~GridViewRowPresenterBase() override;

    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, Columns);

protected:
    explicit GridViewRowPresenterBase(Meta::TypeId runtimeType) noexcept
        : FrameworkElement(runtimeType) {}

    std::uint32_t GetVisualChildrenCount() const noexcept override;
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override;
    std::uint32_t GetLayoutChildrenCount() const noexcept override;
    UIElement* GetLayoutChild(std::uint32_t index) const noexcept override;
    virtual void ClearCells() noexcept;

    Base::Vector<Ref<Base::Object>> ownedCells_;
};

} // namespace Primitives

class AERO_GUI_API GridViewHeaderRowPresenter : public Primitives::GridViewRowPresenterBase {
    AERO_DECLARE_TYPE(GridViewHeaderRowPresenter, Primitives::GridViewRowPresenterBase)

public:
    GridViewHeaderRowPresenter() noexcept : GridViewRowPresenterBase(StaticTypeId()) {}
    ~GridViewHeaderRowPresenter() override;

    bool GetAllowsColumnReorder() const noexcept { return GetValue(AllowsColumnReorderProperty); }
    void SetAllowsColumnReorder(bool value) noexcept { SetValue(AllowsColumnReorderProperty, value); }

    AERO_DEPENDENCY_PROPERTY(bool, AllowsColumnReorder);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderContainerStyle);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderContextMenu);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderTemplate);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderTemplateSelector);
    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, ColumnHeaderToolTip);

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;

private:
    void ClearCells() noexcept;
    void RebuildIfNeeded() noexcept;
    bool cellsDirty_ = true;
};

class AERO_GUI_API GridViewRowPresenter : public Primitives::GridViewRowPresenterBase {
    AERO_DECLARE_TYPE(GridViewRowPresenter, Primitives::GridViewRowPresenterBase)

public:
    GridViewRowPresenter() noexcept : GridViewRowPresenterBase(StaticTypeId()) {}
    ~GridViewRowPresenter() override;

    AERO_DEPENDENCY_PROPERTY(Ref<Base::Object>, Content);

protected:
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    void OnDataContextChanged(const Value& oldValue, const Value& newValue) noexcept override;

private:
    void ClearCells() noexcept;
    void RebuildIfNeeded() noexcept;
    Base::Ref<Base::Object> lastItem_;
    const GridView* lastView_ = nullptr;
    std::uint32_t lastColumnCount_ = 0U;
};

class AERO_GUI_API ListViewItem : public ListBoxItem {
    AERO_DECLARE_TYPE(ListViewItem, ListBoxItem)

public:
    ListViewItem() noexcept : ListBoxItem(StaticTypeId()) {}
    ~ListViewItem() override = default;
};

class AERO_GUI_API ListView : public ListBox {
    AERO_DECLARE_TYPE(ListView, ListBox)

public:
    ListView() noexcept : ListBox(StaticTypeId()) {}
    ~ListView() override = default;

    Ref<ViewBase> GetView() const noexcept;
    void SetView(Ref<ViewBase> value) noexcept;

    AERO_DEPENDENCY_PROPERTY(Ref<ViewBase>, View);

protected:
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    Ref<FrameworkElement> GetContainerForItemOverride() const noexcept override;

private:
    TextBlock* columnHeaders_ = nullptr;
    void SynchronizeColumnHeaders() noexcept;
};

} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::GridViewColumnHeaderRole)
