#pragma once

#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Controls/Popup.hpp>


namespace Aero::Controls {
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
