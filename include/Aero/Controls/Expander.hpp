#pragma once

#include <Aero/Controls/HeaderedContentControl.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Controls {
namespace Primitives { class ToggleButton; }
using ::Aero::Meta::TypeId;
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::DependencyPropertyChangedEventHandler;
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
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

    Expander() noexcept;
    ~Expander() override;

    bool GetIsExpanded() const noexcept;
    void SetIsExpanded(bool value) noexcept;
    ExpandDirection GetDirection() const noexcept;
    void SetDirection(ExpandDirection value) noexcept;

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
    AERO_DEPENDENCY_PROPERTY(bool, IsExpanded);
    AERO_DEPENDENCY_PROPERTY(ExpandDirection, ExpandDirection);

protected:
    virtual void OnExpanded();
    virtual void OnCollapsed();
    void OnPropertyChanged(
        const DependencyPropertyChangedEventArgs& args) noexcept override;
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;

private:
    DependencyPropertyChangedEventHandler
        headerCheckedHandler_;
    Primitives::ToggleButton* headerToggle_ = nullptr;
    bool synchronizingHeader_ = false;
    void BindHeaderToggle() noexcept;
    void UnbindHeaderToggle() noexcept;
    void OnHeaderCheckedChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
};

} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::ExpandDirection)
