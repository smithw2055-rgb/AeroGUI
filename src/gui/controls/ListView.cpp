#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "gui/meta/TypeRegistryDetail.hpp"
#include <Aero/Controls.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/DataTemplate.hpp>
#include <Aero/Style.hpp>
#include <Aero/Data/Binding.hpp>
#include "gui/meta/ValueConversion.hpp"

#include <cmath>
#include <utility>

namespace Aero::Controls {

Value GridViewColumn::GetHeader()
    const noexcept {
    return GetValue(HeaderProperty);
}

void GridViewColumn::SetHeader(
    Value value) noexcept {
    SetValue(HeaderProperty, std::move(value));
}

void GridViewColumn::SetHeader(
    Base::StringView value) noexcept {
    Base::Result<Value> boxed = Value::TryFromString(
        Meta::TypeOf<Base::String>(), value);
    if (!boxed) { AERO_ASSERT(false); return; }
    SetHeader(std::move(boxed).Value());
}

double GridViewColumn::GetWidth()
    const noexcept {
    return GetValue(WidthProperty);
}

void GridViewColumn::SetWidth(
    double value) noexcept {
    if (!std::isfinite(value) ||
        value < 0.0) {
        return;
    }
    SetValue(WidthProperty, value);
}

Base::Ref<DataTemplate>
GridViewColumn::GetCellTemplate() const noexcept {
    return GetValue(CellTemplateProperty);
}

void
GridViewColumn::SetCellTemplate(
    Base::Ref<DataTemplate> value) noexcept {
    SetValue(
        CellTemplateProperty,
        std::move(value));
}

Base::Ref<DataTemplate>
GridViewColumn::GetHeaderTemplate() const noexcept {
    return GetValue(HeaderTemplateProperty);
}

void
GridViewColumn::SetHeaderTemplate(
    Base::Ref<DataTemplate> value) noexcept {
    SetValue(
        HeaderTemplateProperty,
        std::move(value));
}

Base::StringView
GridViewColumn::GetDisplayMemberPath()
    const noexcept {
    return GetValue(DisplayMemberPathProperty);
}

void
GridViewColumn::SetDisplayMemberPath(
    Base::StringView value) noexcept {
    SetValue(
        DisplayMemberPathProperty, value);
}

Base::Ref<Data::Binding>
GridViewColumn::GetDisplayMemberBinding() const noexcept {
    return GetValue(DisplayMemberBindingProperty);
}

void
GridViewColumn::SetDisplayMemberBinding(
    Base::Ref<Data::Binding> value) noexcept {
    SetValue(
        DisplayMemberBindingProperty, std::move(value));
}

void GridView::AddColumn(
    Base::Ref<GridViewColumn> column)
    noexcept {
    if (!column) { AERO_ASSERT(false); return; }
    columns_.PushBack(
        std::move(column));
}

Base::Ref<GridView>
ListView::GetView() const noexcept {
    return GetValue(ViewProperty);
}

void ListView::SetView(
    Base::Ref<GridView> value) noexcept {
    SetValue(ViewProperty, std::move(value));
    SynchronizeColumnHeaders();
}

void
ListView::OnApplyTemplate() noexcept {
    ListBox::OnApplyTemplate();
    DependencyObject* headers =
        GetTemplateChild("ColumnHeaders");
    columnHeaders_ =
        headers != nullptr &&
        AeroGuiInternal::PropertyRegistry(*this).Types().IsDerivedFrom(
            headers->RuntimeType(),
            TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(headers)
        : nullptr;
    if (columnHeaders_ == nullptr) {
        return;
    }
    SynchronizeColumnHeaders();
}

void ListView::OnTemplateDetached() noexcept {
    columnHeaders_ = nullptr;
    ListBox::OnTemplateDetached();
}

void
ListView::SynchronizeColumnHeaders() noexcept {
    if (columnHeaders_ == nullptr) return;
    Base::String text;
    Base::Ref<GridView> view = GetView();
    if (view) {
        for (const Base::Ref<GridViewColumn>&
             column : view->GetColumns()) {
            if (!column) continue;
            const Value header = column->GetHeader();
            Base::Result<void> appended = text.Append(
                header.Kind() == ValueKind::String
                ? header.AsString()
                : Base::StringView{});
            if (!appended) {
                return;
            }
            const std::uint32_t headerCharacters =
                header.Kind() == ValueKind::String
                ? header.AsString().SizeBytes()
                : 0U;
            const std::uint32_t columnCharacters =
                column->GetWidth() > 0.0
                ? static_cast<std::uint32_t>(
                      std::max(
                          1.0,
                          std::floor(
                              column->GetWidth() /
                              8.0)))
                : headerCharacters + 2U;
            const std::uint32_t padding =
                std::max(
                    std::uint32_t{2U},
                    columnCharacters >
                            headerCharacters
                        ? columnCharacters -
                            headerCharacters
                        : std::uint32_t{2U});
            for (std::uint32_t index = 0U;
                 index < padding;
                 ++index) {
                appended = text.Append(
                    Base::StringView(" "));
                if (!appended) {
                    return;
                }
            }
        }
    }
    columnHeaders_->SetText(text.View());
}

Base::Result<Base::Ref<FrameworkElement>>
ListView::GetContainerForItemOverride() const noexcept {
    Base::Result<Base::Ref<ListViewItem>>
        made =
            Base::MakeRef<ListViewItem>();
    if (!made) return made.GetStatus();
    return Base::Ref<FrameworkElement>(
        std::move(made).Value());
}

namespace {

void AddGridViewColumn(
    Base::Object& owner,
    const Base::Ref<Base::Object>& item,
    void*) noexcept {
    if (!item ||
        item->RuntimeType() !=
            GridViewColumn::StaticTypeId()) {
        return;
    }
    (void)static_cast<GridView&>(
        owner).AddColumn(
            Base::Ref<GridViewColumn>::
                FromBorrowed(
                    static_cast<GridViewColumn&>(
                        *item)));
}

void ClearGridViewColumns(
    Base::Object& owner,
    void*) noexcept {
    static_cast<GridView&>(
        owner).ClearColumns();
}

} // namespace

void GridViewColumnHeader::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<GridViewColumnHeader>(context)
        .Property(GridViewColumnHeader::RoleProperty, GridViewColumnHeaderRole::Normal)
        .Factory();
}

void GridViewColumn::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<GridViewColumn>(context)
        .Property(GridViewColumn::HeaderProperty, Value::NullObject(TypeOf<Base::Object>()))
        .Property(GridViewColumn::WidthProperty, 100.0, FrameworkPropertyMetadataOptions::None, &Base::Validate::NonNegative<double>)
        .Property(GridViewColumn::CellTemplateProperty, Base::Ref<DataTemplate>{})
        .Property(GridViewColumn::HeaderTemplateProperty, Base::Ref<DataTemplate>{})
        .Property(GridViewColumn::DisplayMemberPathProperty, Base::String{})
        .Property(GridViewColumn::DisplayMemberBindingProperty, Base::Ref<Data::Binding>{})
        .Property(GridViewColumn::HeaderContainerStyleProperty, Base::Ref<Style>{})
        .Factory();
}

void GridView::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<GridView>(context)
        .Property<bool, &GridView::GetAllowsColumnReorder, &GridView::SetAllowsColumnReorder>("AllowsColumnReorder")
        .Property<Base::Ref<Style>, &GridView::GetColumnHeaderContainerStyle, &GridView::SetColumnHeaderContainerStyle>("ColumnHeaderContainerStyle", PropertyFlags::Structural)
        .Property<Base::Ref<Base::Object>, &GridView::GetColumnHeaderContextMenu, &GridView::SetColumnHeaderContextMenu>("ColumnHeaderContextMenu", PropertyFlags::Structural)
        .Property<Base::Ref<Base::Object>, &GridView::GetColumnHeaderTemplate, &GridView::SetColumnHeaderTemplate>("ColumnHeaderTemplate", PropertyFlags::Structural)
        .Property<Base::Ref<Base::Object>, &GridView::GetColumnHeaderTemplateSelector, &GridView::SetColumnHeaderTemplateSelector>("ColumnHeaderTemplateSelector", PropertyFlags::Structural)
        .Property<Base::Ref<Base::Object>, &GridView::GetColumnHeaderToolTip, &GridView::SetColumnHeaderToolTip>("ColumnHeaderToolTip", PropertyFlags::Structural)
        .Property<Base::Ref<Base::Object>, &GridView::GetColumnsObject, &GridView::SetColumnsObject>("Columns", PropertyFlags::Structural)
        .Content<Base::Object>("ColumnItems", ContentKind::Collection, &AddGridViewColumn, &ClearGridViewColumns)
        .Factory();
}

void GridViewHeaderRowPresenter::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<GridViewHeaderRowPresenter>(context)
        .Property(GridViewHeaderRowPresenter::AllowsColumnReorderProperty, false)
        .Property(GridViewHeaderRowPresenter::ColumnHeaderContainerStyleProperty, Base::Ref<Base::Object>{})
        .Property(GridViewHeaderRowPresenter::ColumnHeaderContextMenuProperty, Base::Ref<Base::Object>{})
        .Property(GridViewHeaderRowPresenter::ColumnHeaderTemplateProperty, Base::Ref<Base::Object>{})
        .Property(GridViewHeaderRowPresenter::ColumnHeaderTemplateSelectorProperty, Base::Ref<Base::Object>{})
        .Property(GridViewHeaderRowPresenter::ColumnHeaderToolTipProperty, Base::Ref<Base::Object>{})
        .Property(GridViewHeaderRowPresenter::ColumnsProperty, Base::Ref<Base::Object>{})
        .Factory();
}

void GridViewRowPresenter::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<GridViewRowPresenter>(context)
        .Property(GridViewRowPresenter::ColumnsProperty, Base::Ref<Base::Object>{})
        .Property(GridViewRowPresenter::ContentProperty, Base::Ref<Base::Object>{})
        .Factory();
}

void ListView::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<ListView>(context)
        .Property(ListView::ViewProperty, Base::Ref<GridView>{}, AffectsMeasure)
        .Factory();
}

void ListViewItem::RegisterMetadata(::Aero::Meta::Registration& context) noexcept {
    using namespace Aero::Meta;
    Register<ListViewItem>(context)
        .Override(Aero::UIElement::IsTabStopProperty, true, FrameworkPropertyMetadataOptions::None)
        .Factory();
}

} // namespace Aero::Controls
