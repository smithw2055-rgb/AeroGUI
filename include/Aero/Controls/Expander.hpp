#pragma once

#include <Aero/Controls/HeaderedContentControl.hpp>

namespace Aero::Controls {
using ::Aero::Meta::TypeId;
enum class ExpandDirection : std::uint8_t {
    Down = 0U,
    Up,
    Left,
    Right,
};

class AERO_GUI_API Expander
    : public HeaderedContentControl {
    AERO_DECLARE_TYPE(
        Expander,
        HeaderedContentControl)
public:
    Expander() noexcept;
    ~Expander() override;

    bool GetIsExpanded() const noexcept;
    void SetIsExpanded(
        bool value) noexcept;
    ExpandDirection GetDirection() const noexcept;
    void SetDirection(
        ExpandDirection value) noexcept;

    inline static constexpr RoutedEvent<RoutedEventArgs> ExpandedEvent{"Expanded"};
    inline static constexpr RoutedEvent<RoutedEventArgs> CollapsedEvent{"Collapsed"};
    UIElement::Event<RoutedEventArgs>
        Expanded() noexcept {
        return GetEvent(ExpandedEvent);
    }
    UIElement::Event<RoutedEventArgs>
        Collapsed() noexcept {
        return GetEvent(CollapsedEvent);
    }
    inline static constexpr DependencyProperty<bool> IsExpandedProperty{"IsExpanded"};
    inline static constexpr DependencyProperty<ExpandDirection> ExpandDirectionProperty{"ExpandDirection"};

protected:
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;

private:
    DependencyPropertyChangedEventHandler
        expandedChangedHandler_;
    void OnExpandedPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
};

} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::ExpandDirection)
