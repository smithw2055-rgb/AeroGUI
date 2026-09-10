#pragma once

#include <Aero/Controls/ContentControl.hpp>

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
    void SetPlacement(PlacementMode value) noexcept;
    double GetHorizontalOffset() const noexcept;
    void SetHorizontalOffset(double value) noexcept;
    double GetVerticalOffset() const noexcept;
    void SetVerticalOffset(double value) noexcept;
    bool GetStaysOpen() const noexcept;
    void SetStaysOpen(bool value) noexcept;
    bool GetMatchPlacementTargetWidth() const noexcept;
    void SetMatchPlacementTargetWidth(bool value) noexcept;
    Ref<UIElement>
        GetPlacementTarget() const noexcept;
    void SetPlacementTarget(Ref<UIElement> value) noexcept;
    PopupAnimation GetPopupAnimation() const noexcept;
    void SetPopupAnimation(PopupAnimation value) noexcept;
    bool GetAllowsTransparency() const noexcept;
    void SetAllowsTransparency(bool value) noexcept;

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

    AERO_DEPENDENCY_PROPERTY(bool, IsOpen);
    AERO_DEPENDENCY_PROPERTY(PlacementMode, Placement);
    AERO_DEPENDENCY_PROPERTY(double, HorizontalOffset);
    AERO_DEPENDENCY_PROPERTY(double, VerticalOffset);
    AERO_DEPENDENCY_PROPERTY(bool, StaysOpen);
    AERO_DEPENDENCY_PROPERTY(bool, MatchPlacementTargetWidth);
    AERO_DEPENDENCY_PROPERTY(Ref<UIElement>, PlacementTarget);
    AERO_DEPENDENCY_PROPERTY(PopupAnimation, PopupAnimation);
    AERO_DEPENDENCY_PROPERTY(bool, AllowsTransparency);

protected:
    virtual void OnOpened(RoutedEventArgs& e);
    virtual void OnClosed(RoutedEventArgs& e);
    void OnPropertyChanged(
        const DependencyPropertyChangedEventArgs& args) noexcept override;
    explicit Popup(TypeId runtimeType) noexcept;
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;

private:
    Size popupDesiredSize_;
};
} // namespace Aero::Controls::Primitives
AERO_DECLARE_TYPE_ENUM(Aero::Controls::Primitives::PlacementMode)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::Primitives::PopupAnimation)
