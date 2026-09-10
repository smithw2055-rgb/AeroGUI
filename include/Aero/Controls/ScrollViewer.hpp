#pragma once

#include <Aero/Controls/ScrollContentPresenter.hpp>
#include <Aero/Events/ControlEventArgs.hpp>
#include <Aero/Input.hpp>

namespace Aero::Controls::Primitives {
class ScrollBar;
}

namespace Aero::Controls {
using ::Aero::Meta::TypeId;
enum class ScrollBarVisibility : std::uint8_t {
    Disabled = 0U,
    Auto,
    Hidden,
    Visible,
};

// Mirrors WPF's gesture-direction policy. Pointer/touch routing can use this
// value without changing the established mouse-wheel scroll behavior.
enum class PanningMode : std::uint8_t {
    None = 0U,
    HorizontalOnly,
    VerticalOnly,
    Both,
    HorizontalFirst,
    VerticalFirst,
};

class AERO_GUI_API ScrollViewer
    : public ScrollContentPresenter {
    AERO_DECLARE_TYPE(ScrollViewer, ScrollContentPresenter)
public:

    ScrollViewer() noexcept;
    ~ScrollViewer() override;

    inline static constexpr RoutedEvent<ScrollChangedEventArgs> ScrollChangedEvent{"ScrollChanged"};
    UIElement::Event<ScrollChangedEventArgs>
        ScrollChanged() noexcept {
        return GetEvent(ScrollChangedEvent);
    }

    double GetHorizontalOffset() const noexcept;
    double GetVerticalOffset() const noexcept;
    double GetExtentWidth() const noexcept;
    double GetExtentHeight() const noexcept;
    double GetViewportWidth() const noexcept;
    double GetViewportHeight() const noexcept;
    double GetScrollableWidth() const noexcept;
    double GetScrollableHeight() const noexcept;
    ScrollBarVisibility
    GetHorizontalScrollBarVisibility() const noexcept;
    ScrollBarVisibility
    GetVerticalScrollBarVisibility() const noexcept;
    Visibility
    GetComputedHorizontalScrollBarVisibility() const noexcept;
    Visibility
    GetComputedVerticalScrollBarVisibility() const noexcept;

    void SetCanHorizontallyScroll(bool value) noexcept;
    void SetCanVerticallyScroll(bool value) noexcept;
    void SetCanContentScroll(bool value) noexcept;
    void SetHorizontalScrollBarVisibility(ScrollBarVisibility value) noexcept;
    void SetVerticalScrollBarVisibility(ScrollBarVisibility value) noexcept;
    PanningMode GetPanningMode() const noexcept;
    void SetPanningMode(PanningMode value) noexcept;
    void SetHorizontalOffset(double value) noexcept override;
    void SetVerticalOffset(double value) noexcept override;
    Result<bool> LineHorizontal(
        double direction) noexcept override;
    Result<bool> LineVertical(
        double direction) noexcept override;
    Result<bool> PageHorizontal(
        double direction) noexcept override;
    Result<bool> PageVertical(
        double direction) noexcept override;

    static ScrollBarVisibility
    GetHorizontalScrollBarVisibility(
        const DependencyObject& element) noexcept;
    static ScrollBarVisibility
    GetVerticalScrollBarVisibility(
        const DependencyObject& element) noexcept;
    static void SetHorizontalScrollBarVisibility(DependencyObject& element, ScrollBarVisibility value) noexcept;
    static void SetVerticalScrollBarVisibility(DependencyObject& element, ScrollBarVisibility value) noexcept;

    AERO_READONLY_PROPERTY(double, HorizontalOffset);
    AERO_READONLY_PROPERTY(double, VerticalOffset);
    AERO_READONLY_PROPERTY(double, ExtentWidth);
    AERO_READONLY_PROPERTY(double, ExtentHeight);
    AERO_READONLY_PROPERTY(double, ViewportWidth);
    AERO_READONLY_PROPERTY(double, ViewportHeight);
    AERO_READONLY_PROPERTY(double, ScrollableWidth);
    AERO_READONLY_PROPERTY(double, ScrollableHeight);
    AERO_READONLY_PROPERTY(Visibility, ComputedHorizontalScrollBarVisibility);
    AERO_READONLY_PROPERTY(Visibility, ComputedVerticalScrollBarVisibility);
    AERO_ATTACHED_PROPERTY(ScrollBarVisibility, HorizontalScrollBarVisibility);
    AERO_ATTACHED_PROPERTY(ScrollBarVisibility, VerticalScrollBarVisibility);
    AERO_DEPENDENCY_PROPERTY(bool, CanHorizontallyScroll);
    AERO_DEPENDENCY_PROPERTY(bool, CanVerticallyScroll);
    AERO_ATTACHED_PROPERTY(bool, CanContentScroll);
    AERO_ATTACHED_PROPERTY(PanningMode, PanningMode);

protected:
    void OnApplyTemplate() noexcept override;
    Size MeasureOverride(
        Size availableSize) noexcept override;
    void OnScrollDataChanged(
        const ScrollData& oldData,
        const ScrollData& newData,
        ScrollInputKind kind) noexcept override;
    bool GetAllowsHorizontalScroll() const noexcept override;
    bool GetAllowsVerticalScroll() const noexcept override;
    bool GetUsesContentScrolling() const noexcept override;
    void OnTemplateDetached() noexcept override;
    void OnMouseWheel(MouseWheelEventArgs& args) override;

private:
    friend class ScrollContentPresenter;
    ScrollContentPresenter* contentPresenter_ = nullptr;
    void AdoptPresenterData(
        ScrollContentPresenter& presenter,
        const ScrollData& data,
        ScrollInputKind kind) noexcept;
    void UpdateComputedScrollBarVisibility(
        const ScrollData& data) noexcept;
    void AttachScrollBars() noexcept;
    void DetachScrollBars() noexcept;
    void OnScrollBarValueChanged(
        DependencyObject& sender,
        const DependencyPropertyChangedEventArgs& args) noexcept;

    Primitives::ScrollBar* verticalScrollBar_ = nullptr;
    Primitives::ScrollBar* horizontalScrollBar_ = nullptr;
    bool synchronizingScrollBars_ = false;
    DependencyPropertyChangedEventHandler scrollBarValueChangedHandler_;
};
} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::ScrollBarVisibility)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::PanningMode)
