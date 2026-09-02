#pragma once

#include <Aero/Controls/Panel.hpp>
#include <Aero/Controls/Primitives/RangeBase.hpp>
#include <Aero/Media/DrawingContext.hpp>

namespace Aero::Controls {
namespace Primitives { class Track; }
class SliderBehavior;

enum class TickPlacement : std::uint8_t {
    None = 0U,
    TopLeft,
    BottomRight,
    Both
};

class AERO_GUI_API Slider : public Primitives::RangeBase {
    AERO_DECLARE_TYPE(Slider, Primitives::RangeBase)
public:

    Slider() noexcept;
    ~Slider() override;

    Orientation GetOrientation() const noexcept;
    double GetSmallChange() const noexcept;
    double GetLargeChange() const noexcept;
    TickPlacement GetTickPlacement() const noexcept;
    double GetTickFrequency() const noexcept;
    StringView GetTicks() const noexcept;
    bool GetIsSnapToTickEnabled() const noexcept;
    bool GetIsDirectionReversed() const noexcept;
    bool GetIsMoveToPointEnabled() const noexcept;
    void SetOrientation(
        Orientation value) noexcept;
    void SetSmallChange(
        double value) noexcept;
    void SetLargeChange(
        double value) noexcept;
    void SetTickPlacement(
        TickPlacement value) noexcept;
    void SetTickFrequency(
        double value) noexcept;
    void SetTicks(
        StringView value) noexcept;
    void SetIsSnapToTickEnabled(
        bool value) noexcept;
    void SetIsDirectionReversed(
        bool value) noexcept;
    void SetIsMoveToPointEnabled(
        bool value) noexcept;
    Result<bool> DecreaseSmall() noexcept;
    Result<bool> IncreaseSmall() noexcept;
    Result<bool> DecreaseLarge() noexcept;
    Result<bool> IncreaseLarge() noexcept;
    void SetValueFromPosition(
        double position,
        double trackLength) noexcept;
    void SetValueFromTrackPoint(
        Point local) noexcept;

    inline static constexpr DependencyProperty<Orientation> OrientationProperty{"Orientation"};
    inline static constexpr DependencyProperty<double> SmallChangeProperty{"SmallChange"};
    inline static constexpr DependencyProperty<double> LargeChangeProperty{"LargeChange"};
    inline static constexpr DependencyProperty<TickPlacement> TickPlacementProperty{"TickPlacement"};
    inline static constexpr DependencyProperty<double> TickFrequencyProperty{"TickFrequency"};
    inline static constexpr DependencyProperty<String> TicksProperty{"Ticks"};
    inline static constexpr DependencyProperty<bool> IsSnapToTickEnabledProperty{"IsSnapToTickEnabled"};
    inline static constexpr DependencyProperty<bool> IsDirectionReversedProperty{"IsDirectionReversed"};
    inline static constexpr DependencyProperty<bool> IsMoveToPointEnabledProperty{"IsMoveToPointEnabled"};

protected:
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;
    void OnRender(
        ::Aero::Media::DrawingContext& context) noexcept override;

private:
    friend class SliderBehavior;
    Primitives::Track* track_ = nullptr;
    DependencyPropertyChangedEventHandler trackPropertyChangedHandler_;
    void OnTrackPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void SynchronizeTrack() noexcept;
    double GetNormalizedValueForLayout() const noexcept;
    double GetSnapValue(double value) const noexcept;
};

} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::TickPlacement)
