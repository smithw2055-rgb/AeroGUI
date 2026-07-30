#include <Aero/Controls/ListView.hpp>

#include <cmath>
#include <utility>

namespace Aero::Controls {

Base::StringView GridViewColumn::Header()
    const noexcept {
    return GetValueOr(
        HeaderProperty, Base::StringView{});
}

Base::Result<void> GridViewColumn::SetHeader(
    Base::StringView value) noexcept {
    return SetValue(HeaderProperty, value);
}

double GridViewColumn::Width()
    const noexcept {
    return GetValueOr(
        WidthProperty, 100.0);
}

Base::Result<void> GridViewColumn::SetWidth(
    double value) noexcept {
    if (!std::isfinite(value) ||
        value < 0.0) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "GridViewColumn Width must be finite and non-negative");
    }
    return SetValue(WidthProperty, value);
}

Base::Ref<DataTemplate>
GridViewColumn::CellTemplate() const noexcept {
    return GetValueOr(
        CellTemplateProperty,
        Base::Ref<DataTemplate>{});
}

Base::Result<void>
GridViewColumn::SetCellTemplate(
    Base::Ref<DataTemplate> value) noexcept {
    return SetValue(
        CellTemplateProperty,
        std::move(value));
}

Base::Ref<DataTemplate>
GridViewColumn::HeaderTemplate() const noexcept {
    return GetValueOr(
        HeaderTemplateProperty,
        Base::Ref<DataTemplate>{});
}

Base::Result<void>
GridViewColumn::SetHeaderTemplate(
    Base::Ref<DataTemplate> value) noexcept {
    return SetValue(
        HeaderTemplateProperty,
        std::move(value));
}

Base::StringView
GridViewColumn::DisplayMemberPath()
    const noexcept {
    return GetValueOr(
        DisplayMemberPathProperty,
        Base::StringView{});
}

Base::Result<void>
GridViewColumn::SetDisplayMemberPath(
    Base::StringView value) noexcept {
    return SetValue(
        DisplayMemberPathProperty, value);
}

Base::Ref<Presentation::BindingSpec>
GridViewColumn::DisplayMemberBinding() const noexcept {
    return GetValueOr(
        DisplayMemberBindingProperty,
        Base::Ref<Presentation::BindingSpec>{});
}

Base::Result<void>
GridViewColumn::SetDisplayMemberBinding(
    Base::Ref<Presentation::BindingSpec> value) noexcept {
    return SetValue(
        DisplayMemberBindingProperty, std::move(value));
}

Base::Result<void> GridView::AddColumn(
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
ListView::View() const noexcept {
    return GetValueOr(
        ViewProperty,
        Base::Ref<GridView>{});
}

Base::Result<void> ListView::SetView(
    Base::Ref<GridView> value) noexcept {
    Base::Result<void> stored = SetValue(
        ViewProperty, std::move(value));
    if (!stored) return stored.GetStatus();
    return SynchronizeColumnHeaders();
}

Base::Result<void>
ListView::OnApplyTemplate() noexcept {
    Base::Result<void> applied =
        ListBox::OnApplyTemplate();
    if (!applied) return applied.GetStatus();
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
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ListView template requires ColumnHeaders");
    }
    return SynchronizeColumnHeaders();
}

void ListView::OnTemplateDetached() noexcept {
    columnHeaders_ = nullptr;
    ListBox::OnTemplateDetached();
}

Base::Result<void>
ListView::SynchronizeColumnHeaders() noexcept {
    if (columnHeaders_ == nullptr) return {};
    Base::String text;
    Base::Ref<GridView> view = View();
    if (view) {
        for (const Base::Ref<GridViewColumn>&
             column : view->Columns()) {
            if (!column) continue;
            Base::Result<void> appended =
                text.TryAppend(column->Header());
            if (!appended) {
                return appended.GetStatus();
            }
            const std::uint32_t headerCharacters =
                column->Header().SizeBytes();
            const std::uint32_t columnCharacters =
                column->Width() > 0.0
                ? static_cast<std::uint32_t>(
                      std::max(
                          1.0,
                          std::floor(
                              column->Width() /
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
    return columnHeaders_->SetText(text.View());
}

Base::Result<Base::Ref<ItemContainer>>
ListView::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    Base::Result<Base::Ref<ListViewItem>>
        made =
            Base::MakeRef<ListViewItem>();
    if (!made) return made.GetStatus();
    return Base::Ref<ItemContainer>(
        std::move(made).Value());
}

} // namespace Aero::Controls
