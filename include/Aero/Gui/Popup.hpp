#pragma once

#include <Aero/Gui/ContentControl.hpp>

namespace Aero::Controls::Primitives {
using ::Aero::Meta::TypeId;
enum class PlacementMode : std::uint8_t {
    Bottom = 0U,
    Top,
    Left,
    Right,
    Center,
    Mouse,
};

enum class PopupAnimation : std::uint8_t {
    None = 0U,
    Fade,
    Slide,
    Scroll,
};

class AERO_GUI_API Popup : public ContentControl {
    AERO_DECLARE_TYPE(Popup, ContentControl)
public:
    Popup() noexcept;
    ~Popup() override;

    bool GetIsOpen() const noexcept;
    void SetIsOpen(bool value) noexcept;
    PlacementMode GetPlacement() const noexcept;
    void SetPlacement(
        PlacementMode value) noexcept;
    double GetHorizontalOffset() const noexcept;
    void SetHorizontalOffset(
        double value) noexcept;
    double GetVerticalOffset() const noexcept;
    void SetVerticalOffset(
        double value) noexcept;
    bool GetStaysOpen() const noexcept;
    void SetStaysOpen(
        bool value) noexcept;
    bool GetMatchPlacementTargetWidth() const noexcept;
    void SetMatchPlacementTargetWidth(
        bool value) noexcept;
    Base::Ref<UIElement>
        GetPlacementTarget() const noexcept;
    void SetPlacementTarget(
        Base::Ref<UIElement> value) noexcept;
    PopupAnimation GetPopupAnimation() const noexcept;
    void SetPopupAnimation(
        PopupAnimation value) noexcept;
    bool GetAllowsTransparency() const noexcept;
    void SetAllowsTransparency(
        bool value) noexcept;

    inline static constexpr RoutedEvent<RoutedEventArgs> OpenedEvent{"Opened"};
    inline static constexpr RoutedEvent<RoutedEventArgs> ClosedEvent{"Closed"};
    UIElement::Event<RoutedEventArgs>
        Opened() noexcept {
        return GetEvent(OpenedEvent);
    }
    UIElement::Event<RoutedEventArgs>
        Closed() noexcept {
        return GetEvent(ClosedEvent);
    }

    inline static constexpr DependencyProperty<bool> IsOpenProperty{"IsOpen"};
    inline static constexpr DependencyProperty<PlacementMode> PlacementProperty{"Placement"};
    inline static constexpr DependencyProperty<double> HorizontalOffsetProperty{"HorizontalOffset"};
    inline static constexpr DependencyProperty<double> VerticalOffsetProperty{"VerticalOffset"};
    inline static constexpr DependencyProperty<bool> StaysOpenProperty{"StaysOpen"};
    inline static constexpr DependencyProperty<bool> MatchPlacementTargetWidthProperty{"MatchPlacementTargetWidth"};
    inline static constexpr DependencyProperty<Base::Ref<UIElement>> PlacementTargetProperty{"PlacementTarget"};
    inline static constexpr DependencyProperty<PopupAnimation> PopupAnimationProperty{"PopupAnimation"};
    inline static constexpr DependencyProperty<bool> AllowsTransparencyProperty{"AllowsTransparency"};

protected:
    explicit Popup(TypeId runtimeType) noexcept;
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;

private:
    DependencyPropertyChangedEventHandler
        openChangedHandler_;
    Size popupDesiredSize_;
    void OnOpenPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
};
} // namespace Aero::Controls::Primitives
AERO_DECLARE_TYPE_ENUM(Aero::Controls::Primitives::PlacementMode)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::Primitives::PopupAnimation)
