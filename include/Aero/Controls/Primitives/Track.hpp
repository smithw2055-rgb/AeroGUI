#pragma once

#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/Primitives/Thumb.hpp>
#include <Aero/Controls/Primitives/RepeatButton.hpp>

namespace Aero::Controls::Primitives {

class AERO_GUI_API Track : public Control {
    AERO_DECLARE_TYPE(Track, Control)
public:
    Track() noexcept : Control(StaticTypeId()) {}
    ~Track() override = default;
    using Control::GetValue;
    using Control::SetValue;

    Orientation GetOrientation() const noexcept;
    double GetMinimum() const noexcept;
    double GetMaximum() const noexcept;
    double GetValue() const noexcept;
    double GetViewportSize() const noexcept;
    bool GetIsDirectionReversed() const noexcept;
    Ref<RepeatButton>
    GetDecreaseRepeatButton() const noexcept {
        return decreaseRepeatButton_;
    }
    Ref<Thumb> GetThumbElement() const noexcept {
        return thumb_;
    }
    Ref<RepeatButton>
    GetIncreaseRepeatButton() const noexcept {
        return increaseRepeatButton_;
    }
    void SetOrientation(
        Orientation value) noexcept;
    void SetRange(
        double minimum,
        double maximum) noexcept;
    void SetValue(
        double value) noexcept;
    void SetViewportSize(
        double value) noexcept;
    void SetIsDirectionReversed(
        bool value) noexcept;
    void SetDecreaseRepeatButton(
        Ref<RepeatButton> value) noexcept;
    void SetThumb(
        Ref<Thumb> value) noexcept;
    void SetIncreaseRepeatButton(
        Ref<RepeatButton> value) noexcept;
    double GetThumbLength(
        double trackLength,
        double minimumThumbLength = 8.0) const noexcept;
    double GetThumbOffset(
        double trackLength,
        double minimumThumbLength = 8.0) const noexcept;
    Result<double> ValueFromThumbOffset(
        double offset,
        double trackLength,
        double minimumThumbLength = 8.0) const noexcept;

    // Control only exposes TemplateRoot. Track's Thumb / RepeatButtons are
    // assigned as structural properties; the first attached child would
    // otherwise steal TemplateRoot and hide the gold indicator.
    std::uint32_t GetVisualChildrenCount() const noexcept override {
        std::uint32_t count = 0U;
        if (decreaseRepeatButton_ &&
            decreaseRepeatButton_->GetVisualParent() == this) {
            ++count;
        }
        if (thumb_ && thumb_->GetVisualParent() == this) {
            ++count;
        }
        if (increaseRepeatButton_ &&
            increaseRepeatButton_->GetVisualParent() == this) {
            ++count;
        }
        return count;
    }
    ::Aero::Media::Visual* GetVisualChild(
        std::uint32_t index) const noexcept override {
        std::uint32_t current = 0U;
        const auto take =
            [&](::Aero::Media::Visual* child) noexcept
            -> ::Aero::Media::Visual* {
            if (child == nullptr ||
                child->GetVisualParent() != this) {
                return nullptr;
            }
            if (current == index) return child;
            ++current;
            return nullptr;
        };
        if (::Aero::Media::Visual* found =
                take(decreaseRepeatButton_.Get())) {
            return found;
        }
        if (::Aero::Media::Visual* found = take(thumb_.Get())) {
            return found;
        }
        return take(increaseRepeatButton_.Get());
    }

    inline static constexpr DependencyProperty<Orientation> OrientationProperty{"Orientation"};
    inline static constexpr DependencyProperty<double> MinimumProperty{"Minimum"};
    inline static constexpr DependencyProperty<double> MaximumProperty{"Maximum"};
    inline static constexpr DependencyProperty<double> ValueProperty{"Value"};
    inline static constexpr DependencyProperty<double> ViewportSizeProperty{"ViewportSize"};
    inline static constexpr DependencyProperty<bool> IsDirectionReversedProperty{"IsDirectionReversed"};

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;

private:
    Ref<RepeatButton> decreaseRepeatButton_;
    Ref<Thumb> thumb_;
    Ref<RepeatButton> increaseRepeatButton_;
};

} // namespace Aero::Controls::Primitives
