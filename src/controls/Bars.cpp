#include <Aero/Controls/Standard.hpp>
#include "gui/ElementInternal.hpp"

#include <utility>

namespace Aero::Controls {

ToolBar::ToolBar() noexcept
    : ItemsControl(StaticTypeId()),
      headerChangedHandler_(
          this, &ToolBar::OnHeaderChanged) {
    static_cast<void>(TryAddValueChangedHandler(
        HeaderProperty,
        headerChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        OrientationProperty,
        headerChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        OverflowCapacityProperty,
        headerChangedHandler_));
    static_cast<void>(TryAddValueChangedHandler(
        IsOverflowOpenProperty,
        headerChangedHandler_));
}

ToolBar::~ToolBar() {
    static_cast<void>(RemoveValueChangedHandler(
        HeaderProperty,
        headerChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        OrientationProperty,
        headerChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        OverflowCapacityProperty,
        headerChangedHandler_));
    static_cast<void>(RemoveValueChangedHandler(
        IsOverflowOpenProperty,
        headerChangedHandler_));
}

Core::Value ToolBar::Header()
    const noexcept {
    return GetValueOr(
        HeaderProperty,
        Core::Value::NullObject(
            Core::TypeOf<Base::Object>()));
}

Base::Result<void> ToolBar::SetHeader(
    const Core::Value& value) noexcept {
    return SetValue(HeaderProperty, value);
}

Base::Ref<DataTemplate>
ToolBar::HeaderTemplate() const noexcept {
    return GetValueOr(
        HeaderTemplateProperty,
        Base::Ref<DataTemplate>{});
}

Base::Result<void> ToolBar::SetHeaderTemplate(
    Base::Ref<DataTemplate> value) noexcept {
    return SetValue(
        HeaderTemplateProperty, std::move(value));
}

Orientation ToolBar::GetOrientation()
    const noexcept {
    return GetValueOr(
        OrientationProperty,
        Orientation::Horizontal);
}

Base::Result<void> ToolBar::SetOrientation(
    Orientation value) noexcept {
    return SetValue(
        OrientationProperty, value);
}

std::uint32_t ToolBar::OverflowCapacity()
    const noexcept {
    return GetValueOr(
        OverflowCapacityProperty,
        UINT32_MAX);
}

Base::Result<void>
ToolBar::SetOverflowCapacity(
    std::uint32_t value) noexcept {
    return SetValue(
        OverflowCapacityProperty, value);
}

bool ToolBar::IsOverflowOpen() const noexcept {
    return GetValueOr(IsOverflowOpenProperty, false);
}

Base::Result<void> ToolBar::SetIsOverflowOpen(
    bool value) noexcept {
    return SetValue(IsOverflowOpenProperty, value);
}

bool ToolBar::HasOverflowItems() const noexcept {
    return GetValueOr(
        HasOverflowItemsProperty, false);
}

std::uint32_t ToolBar::OverflowItemCount()
    const noexcept {
    return GetValueOr(
        OverflowItemCountProperty,
        std::uint32_t{0U});
}

Base::Result<void>
ToolBar::OnApplyTemplate() noexcept {
    Base::Result<void> applied =
        ItemsControl::OnApplyTemplate();
    if (!applied) return applied.GetStatus();
    DependencyObject* header =
        GetTemplateChild("HeaderText");
    headerText_ =
        header != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            header->RuntimeType(),
            TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(header)
        : nullptr;
    if (headerText_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ToolBar template requires HeaderText");
    }
    DependencyObject* overflow =
        GetTemplateChild("OverflowGlyph");
    overflowGlyph_ =
        overflow != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            overflow->RuntimeType(),
            TextBlock::StaticTypeId())
        ? static_cast<TextBlock*>(overflow)
        : nullptr;
    if (overflowGlyph_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ToolBar template requires OverflowGlyph");
    }
    return SynchronizeToolBar();
}

void ToolBar::OnTemplateDetached() noexcept {
    headerText_ = nullptr;
    overflowGlyph_ = nullptr;
    ItemsControl::OnTemplateDetached();
}

void ToolBar::OnHeaderChanged(
    DependencyObject&,
    const DependencyPropertyChangedEventArgs&)
    noexcept {
    static_cast<void>(SynchronizeToolBar());
}

void ToolBar::OnContainersChanged() noexcept {
    static_cast<void>(SynchronizeToolBar());
}

Base::Result<void>
ToolBar::SynchronizeToolBar() noexcept {
    if (headerText_ != nullptr) {
        const Core::Value headerValue = Header();
        Base::Result<void> header =
            headerText_->SetText(
                headerValue.Kind() == Core::ValueKind::String
                ? headerValue.AsString()
                : Base::StringView{});
        if (!header) return header.GetStatus();
    }
    Panel* host = ItemsHost();
    if (host != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            host->RuntimeType(),
            StackPanel::StaticTypeId())) {
        Base::Result<void> oriented =
            static_cast<StackPanel*>(host)->
                SetOrientation(GetOrientation());
        if (!oriented) {
            return oriented.GetStatus();
        }
    }
    const std::uint32_t capacity =
        OverflowCapacity();
    const std::uint32_t count =
        ItemCount();
    const std::uint32_t overflowCount =
        count > capacity
        ? count - capacity
        : 0U;
    Base::Result<void> stored = SetValue(
        HasOverflowItemsProperty,
        overflowCount != 0U);
    if (!stored) return stored.GetStatus();
    stored = SetValue(
        OverflowItemCountProperty,
        overflowCount);
    if (!stored) return stored.GetStatus();
    if (host != nullptr) {
        const Base::Span<Visual* const> children =
            Aero::Detail::VisualAccess::VisualChildren(*host);
        for (std::uint32_t index = 0U;
             index < children.Size();
             ++index) {
            UIElement* child =
                children[index] != nullptr
                ? children[index]->AsUIElement()
                : nullptr;
            if (child == nullptr) continue;
            Base::Result<void> visible =
                child->SetVisibility(
                    index < capacity
                    ? Visibility::Visible
                    : Visibility::Collapsed);
            if (!visible) {
                return visible.GetStatus();
            }
        }
    }
    if (overflowGlyph_ != nullptr) {
        return overflowGlyph_->SetText(
            overflowCount != 0U
            ? Base::StringView("...")
            : Base::StringView(""));
    }
    return {};
}

Base::Result<Base::Ref<ItemContainer>>
StatusBar::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    Base::Result<Base::Ref<StatusBarItem>>
        made =
            Base::MakeRef<StatusBarItem>();
    if (!made) return made.GetStatus();
    return Base::Ref<ItemContainer>(
        std::move(made).Value());
}

std::uint32_t ToolTip::InitialShowDelay()
    const noexcept {
    return GetValueOr(
        InitialShowDelayProperty,
        std::uint32_t{500U});
}

Base::Result<void>
ToolTip::SetInitialShowDelay(
    std::uint32_t value) noexcept {
    return SetValue(
        InitialShowDelayProperty, value);
}

std::uint32_t ToolTip::ShowDuration()
    const noexcept {
    return GetValueOr(
        ShowDurationProperty,
        std::uint32_t{5000U});
}

Base::Result<void> ToolTip::SetShowDuration(
    std::uint32_t value) noexcept {
    return SetValue(
        ShowDurationProperty, value);
}

Base::Ref<ToolTip> ToolTipService::GetToolTip(
    const DependencyObject& target) noexcept {
    return target.GetValueOr(
        ToolTipProperty,
        Base::Ref<ToolTip>{});
}

Base::Result<void> ToolTipService::SetToolTip(
    DependencyObject& target,
    Base::Ref<ToolTip> value) noexcept {
    return target.SetValue(
        ToolTipProperty,
        std::move(value));
}

std::uint32_t ToolTipService::InitialShowDelay(
    const DependencyObject& target) noexcept {
    return target.GetValueOr(
        InitialShowDelayProperty,
        std::uint32_t{500U});
}

Base::Result<void>
ToolTipService::SetInitialShowDelay(
    DependencyObject& target,
    std::uint32_t value) noexcept {
    return target.SetValue(
        InitialShowDelayProperty, value);
}

std::uint32_t ToolTipService::ShowDuration(
    const DependencyObject& target) noexcept {
    return target.GetValueOr(
        ShowDurationProperty,
        std::uint32_t{5000U});
}

Base::Result<void> ToolTipService::SetShowDuration(
    DependencyObject& target,
    std::uint32_t value) noexcept {
    return target.SetValue(
        ShowDurationProperty, value);
}

} // namespace Aero::Controls
