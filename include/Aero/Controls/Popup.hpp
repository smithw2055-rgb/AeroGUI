#pragma once

#include <Aero/Controls/Control.hpp>

namespace Aero::Controls {

namespace Primitives {
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

    UIElement* GetChild() const noexcept { return GetContentElement(); }
    void SetChild(UIElement* child) noexcept { SetContent(child); }
    void SetChild(Ref<UIElement> child) noexcept { SetContent(std::move(child)); }
    void SetChild(std::nullptr_t) noexcept { SetContent(static_cast<UIElement*>(nullptr)); }
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
    Ref<UIElement> GetPlacementTarget() const noexcept;
    void SetPlacementTarget(Ref<UIElement> value) noexcept;
    PopupAnimation GetPopupAnimation() const noexcept;
    void SetPopupAnimation(PopupAnimation value) noexcept;
    bool GetAllowsTransparency() const noexcept;
    void SetAllowsTransparency(bool value) noexcept;

    inline static constexpr RoutedEvent<RoutedEventArgs> OpenedEvent{"Opened"};
    inline static constexpr RoutedEvent<RoutedEventArgs> ClosedEvent{"Closed"};
    UIElement::Event<RoutedEventArgs> Opened() noexcept { return GetEvent(OpenedEvent); }
    UIElement::Event<RoutedEventArgs> Closed() noexcept { return GetEvent(ClosedEvent); }

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
    explicit Popup(TypeId runtimeType) noexcept;

    virtual void OnOpened(RoutedEventArgs& e);
    virtual void OnClosed(RoutedEventArgs& e);
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;

private:
    Size popupDesiredSize_;
};

} // namespace Primitives

using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::TypeId;

class AERO_GUI_API ToolTip : public ContentControl {
    AERO_DECLARE_TYPE(ToolTip, ContentControl)

public:
    ToolTip() noexcept : ContentControl(StaticTypeId()) {}
    ~ToolTip() override = default;

    std::uint32_t GetInitialShowDelay() const noexcept;
    void SetInitialShowDelay(std::uint32_t value) noexcept;
    std::uint32_t GetShowDuration() const noexcept;
    void SetShowDuration(std::uint32_t value) noexcept;
    bool GetIsOpen() const noexcept;
    void SetIsOpen(bool value) noexcept;
    Primitives::PlacementMode GetPlacement() const noexcept;
    void SetPlacement(Primitives::PlacementMode value) noexcept;
    double GetHorizontalOffset() const noexcept;
    void SetHorizontalOffset(double value) noexcept;
    double GetVerticalOffset() const noexcept;
    void SetVerticalOffset(double value) noexcept;
    Ref<UIElement> GetPlacementTarget() const noexcept;
    void SetPlacementTarget(Ref<UIElement> value) noexcept;

    AERO_DEPENDENCY_PROPERTY(std::uint32_t, InitialShowDelay);
    AERO_DEPENDENCY_PROPERTY(std::uint32_t, ShowDuration);
    AERO_DEPENDENCY_PROPERTY(bool, IsOpen);
    AERO_DEPENDENCY_PROPERTY(Primitives::PlacementMode, Placement);
    AERO_DEPENDENCY_PROPERTY(double, HorizontalOffset);
    AERO_DEPENDENCY_PROPERTY(double, VerticalOffset);
    AERO_DEPENDENCY_PROPERTY(Ref<UIElement>, PlacementTarget);

protected:
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
};

class AERO_GUI_API ToolTipService : public Base::Object {
    AERO_DECLARE_TYPE(ToolTipService, Base::Object)

public:
    TypeId RuntimeType() const noexcept override { return StaticTypeId(); }
    static Ref<ToolTip> GetToolTip(const DependencyObject& target) noexcept;
    static void SetToolTip(DependencyObject& target, Ref<ToolTip> value) noexcept;
    static std::uint32_t GetInitialShowDelay(const DependencyObject& target) noexcept;
    static void SetInitialShowDelay(DependencyObject& target, std::uint32_t value) noexcept;
    static std::uint32_t GetShowDuration(const DependencyObject& target) noexcept;
    static void SetShowDuration(DependencyObject& target, std::uint32_t value) noexcept;

    AERO_ATTACHED_PROPERTY(Ref<ToolTip>, ToolTip);
    AERO_ATTACHED_PROPERTY(std::uint32_t, InitialShowDelay);
    AERO_ATTACHED_PROPERTY(std::uint32_t, ShowDuration);
};

} // namespace Aero::Controls

AERO_DECLARE_TYPE_ENUM(Aero::Controls::Primitives::PlacementMode)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::Primitives::PopupAnimation)
