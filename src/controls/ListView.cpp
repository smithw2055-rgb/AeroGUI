#include "gui/MetadataInternal.hpp"
#include <Aero/Controls/Items.hpp>

#include <cmath>
#include <utility>

namespace Aero::Controls {

Base::StringView GridViewColumn::GetHeader()
    const noexcept {
    return GetValueOr(
        HeaderProperty, Base::StringView{});
}

void GridViewColumn::SetHeader(
    Base::StringView value) noexcept {
    SetValue(HeaderProperty, value);
}

double GridViewColumn::GetWidth()
    const noexcept {
    return GetValueOr(
        WidthProperty, 100.0);
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
    return GetValueOr(
        CellTemplateProperty,
        Base::Ref<DataTemplate>{});
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
    return GetValueOr(
        HeaderTemplateProperty,
        Base::Ref<DataTemplate>{});
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
    return GetValueOr(
        DisplayMemberPathProperty,
        Base::StringView{});
}

void
GridViewColumn::SetDisplayMemberPath(
    Base::StringView value) noexcept {
    SetValue(
        DisplayMemberPathProperty, value);
}

Base::Ref<Data::Binding>
GridViewColumn::GetDisplayMemberBinding() const noexcept {
    return GetValueOr(
        DisplayMemberBindingProperty,
        Base::Ref<Data::Binding>{});
}

void
GridViewColumn::SetDisplayMemberBinding(
    Base::Ref<Data::Binding> value) noexcept {
    SetValue(
        DisplayMemberBindingProperty, std::move(value));
}

Base::Result<void> GridView::TryAddColumn(
    Base::Ref<GridViewColumn> column)
    noexcept {
    if (!column) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "GridView column is null");
    }
    return columns_.TryPushBack(
        std::move(column));
}

Base::Ref<GridView>
ListView::GetView() const noexcept {
    return GetValueOr(
        ViewProperty,
        Base::Ref<GridView>{});
}

void ListView::SetView(
    Base::Ref<GridView> value) noexcept {
    SetValue(ViewProperty, std::move(value));
    (void)SynchronizeColumnHeaders();
}

void
ListView::OnApplyTemplate() noexcept {
    ListBox::OnApplyTemplate();
    DependencyObject* headers =
        GetTemplateChild("ColumnHeaders");
    columnHeaders_ =
        headers != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            headers->RuntimeType(),
            TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(headers)
        : nullptr;
    if (columnHeaders_ == nullptr) {
        return;
    }
    static_cast<void>(SynchronizeColumnHeaders());
}

void ListView::OnTemplateDetached() noexcept {
    columnHeaders_ = nullptr;
    ListBox::OnTemplateDetached();
}

Base::Result<void>
ListView::SynchronizeColumnHeaders() noexcept {
    if (columnHeaders_ == nullptr) return {};
    Base::String text;
    Base::Ref<GridView> view = GetView();
    if (view) {
        for (const Base::Ref<GridViewColumn>&
             column : view->GetColumns()) {
            if (!column) continue;
            Base::Result<void> appended =
                text.TryAppend(column->GetHeader());
            if (!appended) {
                return appended.GetStatus();
            }
            const std::uint32_t headerCharacters =
                column->GetHeader().SizeBytes();
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
                appended = text.TryAppend(
                    Base::StringView(" "));
                if (!appended) {
                    return appended.GetStatus();
                }
            }
        }
    }
    columnHeaders_->SetText(text.View());
    return {};
}

Base::Result<Base::Ref<FrameworkElement>>
ListView::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    Base::Result<Base::Ref<ListViewItem>>
        made =
            Base::MakeRef<ListViewItem>();
    if (!made) return made.GetStatus();
    return Base::Ref<FrameworkElement>(
        std::move(made).Value());
}

} // namespace Aero::Controls
