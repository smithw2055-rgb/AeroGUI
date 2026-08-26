#pragma once

#include <Aero/Controls/Primitives/RangeBase.hpp>
#include <Aero/Controls/Primitives/Track.hpp>

namespace Aero::Controls::Primitives {

class AERO_GUI_API ScrollBar : public RangeBase {
    AERO_DECLARE_TYPE(ScrollBar, RangeBase)
public:
    ScrollBar() noexcept;
    ~ScrollBar() override;

    Orientation GetOrientation() const noexcept;
    double GetViewportSize() const noexcept;
    double GetSmallChange() const noexcept;
    double GetLargeChange() const noexcept;
    void SetOrientation(
        Orientation value) noexcept;
    void SetViewportSize(
        double value) noexcept;
    void SetSmallChange(
        double value) noexcept;
    void SetLargeChange(
        double value) noexcept;
    Result<bool> LineDecrement() noexcept;
    Result<bool> LineIncrement() noexcept;
    Result<bool> PageDecrement() noexcept;
    Result<bool> PageIncrement() noexcept;
    Result<bool> DragThumb(
        double thumbOffset,
        double trackLength,
        double minimumThumbLength = 8.0) noexcept;

    inline static constexpr DependencyProperty<Orientation> OrientationProperty{"Orientation"};
    inline static constexpr DependencyProperty<double> ViewportSizeProperty{"ViewportSize"};
    inline static constexpr DependencyProperty<double> SmallChangeProperty{"SmallChange"};
    inline static constexpr DependencyProperty<double> LargeChangeProperty{"LargeChange"};

protected:
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;

private:
    Track* track_ = nullptr;
    DependencyPropertyChangedEventHandler
        trackPropertyChangedHandler_;
    void OnTrackPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void SynchronizeTrack() noexcept;
};

} // namespace Aero::Controls::Primitives
