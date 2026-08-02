#include "gui/MetadataInternal.hpp"
#include <Aero/Controls/Common.hpp>
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

Meta::Value ToolBar::GetHeader()
    const noexcept {
    return GetValueOr(
        HeaderProperty,
        Meta::Value::NullObject(
            Meta::TypeOf<Base::Object>()));
}

void ToolBar::SetHeader(
    const Meta::Value& value) noexcept {
    SetValue(HeaderProperty, value);
}

Base::Ref<DataTemplate>
ToolBar::GetHeaderTemplate() const noexcept {
    return GetValueOr(
        HeaderTemplateProperty,
        Base::Ref<DataTemplate>{});
}

void ToolBar::SetHeaderTemplate(
    Base::Ref<DataTemplate> value) noexcept {
    SetValue(
        HeaderTemplateProperty, std::move(value));
}

Orientation ToolBar::GetOrientation()
    const noexcept {
    return GetValueOr(
        OrientationProperty,
        Orientation::Horizontal);
}

void ToolBar::SetOrientation(
    Orientation value) noexcept {
    SetValue(
        OrientationProperty, value);
}

std::uint32_t ToolBar::GetOverflowCapacity()
    const noexcept {
    return GetValueOr(
        OverflowCapacityProperty,
        UINT32_MAX);
}

void
ToolBar::SetOverflowCapacity(
    std::uint32_t value) noexcept {
    SetValue(
        OverflowCapacityProperty, value);
}

bool ToolBar::GetIsOverflowOpen() const noexcept {
    return GetValueOr(IsOverflowOpenProperty, false);
}

void ToolBar::SetIsOverflowOpen(
    bool value) noexcept {
    SetValue(IsOverflowOpenProperty, value);
}

bool ToolBar::GetHasOverflowItems() const noexcept {
    return GetValueOr(
        HasOverflowItemsProperty, false);
}

std::uint32_t ToolBar::GetOverflowItemCount()
    const noexcept {
    return GetValueOr(
        OverflowItemCountProperty,
        std::uint32_t{0U});
}

void
ToolBar::OnApplyTemplate() noexcept {
    ItemsControl::OnApplyTemplate();
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
        return;
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
        return;
    }
    static_cast<void>(SynchronizeToolBar());
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
        const Meta::Value headerValue = GetHeader();
        headerText_->SetText(
            headerValue.Kind() == Meta::ValueKind::String
            ? headerValue.AsString()
            : Base::StringView{});
    }
    Panel* host = GetItemsHost();
    if (host != nullptr &&
        PropertyRegistry().Types().IsDerivedFrom(
            host->RuntimeType(),
            StackPanel::StaticTypeId())) {
        static_cast<StackPanel*>(host)->
            SetOrientation(GetOrientation());
    }
    const std::uint32_t capacity =
        GetOverflowCapacity();
    const std::uint32_t count =
        GetCount();
    const std::uint32_t overflowCount =
        count > capacity
        ? count - capacity
        : 0U;
    SetValue(
        HasOverflowItemsProperty,
        overflowCount != 0U);
    SetValue(
        OverflowItemCountProperty,
        overflowCount);
    if (host != nullptr) {
        const Base::Span<Visual* const> children =
            Aero::Internal::ElementPrivate::VisualChildren(*host);
        for (std::uint32_t index = 0U;
             index < children.Size();
             ++index) {
            UIElement* child =
                children[index] != nullptr
                ? children[index]->AsUIElement()
                : nullptr;
            if (child == nullptr) continue;
            child->SetVisibility(
                index < capacity
                ? Visibility::Visible
                : Visibility::Collapsed);
        }
    }
    if (overflowGlyph_ != nullptr) {
        overflowGlyph_->SetText(
            overflowCount != 0U
            ? Base::StringView("...")
            : Base::StringView(""));
    }
    return {};
}

Base::Result<Base::Ref<FrameworkElement>>
StatusBar::CreateContainer(
    const Base::Ref<Base::Object>&) noexcept {
    Base::Result<Base::Ref<StatusBarItem>>
        made =
            Base::MakeRef<StatusBarItem>();
    if (!made) return made.GetStatus();
    return Base::Ref<FrameworkElement>(
        std::move(made).Value());
}

std::uint32_t ToolTip::GetInitialShowDelay()
    const noexcept {
    return GetValueOr(
        InitialShowDelayProperty,
        std::uint32_t{500U});
}

void
ToolTip::SetInitialShowDelay(
    std::uint32_t value) noexcept {
    SetValue(
        InitialShowDelayProperty, value);
}

std::uint32_t ToolTip::GetShowDuration()
    const noexcept {
    return GetValueOr(
        ShowDurationProperty,
        std::uint32_t{5000U});
}

void ToolTip::SetShowDuration(
    std::uint32_t value) noexcept {
    SetValue(
        ShowDurationProperty, value);
}

Base::Ref<ToolTip> ToolTipService::GetToolTip(
    const DependencyObject& target) noexcept {
    return target.GetValueOr(
        ToolTipProperty,
        Base::Ref<ToolTip>{});
}

void ToolTipService::SetToolTip(
    DependencyObject& target,
    Base::Ref<ToolTip> value) noexcept {
    target.SetValue(
        ToolTipProperty,
        std::move(value));
}

std::uint32_t ToolTipService::GetInitialShowDelay(
    const DependencyObject& target) noexcept {
    return target.GetValueOr(
        InitialShowDelayProperty,
        std::uint32_t{500U});
}

void
ToolTipService::SetInitialShowDelay(
    DependencyObject& target,
    std::uint32_t value) noexcept {
    target.SetValue(
        InitialShowDelayProperty, value);
}

std::uint32_t ToolTipService::GetShowDuration(
    const DependencyObject& target) noexcept {
    return target.GetValueOr(
        ShowDurationProperty,
        std::uint32_t{5000U});
}

void ToolTipService::SetShowDuration(
    DependencyObject& target,
    std::uint32_t value) noexcept {
    target.SetValue(
        ShowDurationProperty, value);
}

} // namespace Aero::Controls
