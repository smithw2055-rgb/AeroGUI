#pragma once

#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Input.hpp>
#include <Aero/Events/ControlEventArgs.hpp>

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

class AERO_GUI_API IScrollInfo {
public:
    virtual ~IScrollInfo() = default;
    virtual ScrollData GetData() const noexcept = 0;
    virtual void SetViewport(
        Size viewport) noexcept = 0;
    virtual void SetHorizontalOffset(
        double value) noexcept = 0;
    virtual void SetVerticalOffset(
        double value) noexcept = 0;
    virtual Result<bool> LineHorizontal(
        double direction) noexcept = 0;
    virtual Result<bool> LineVertical(
        double direction) noexcept = 0;
    virtual Result<bool> PageHorizontal(
        double direction) noexcept = 0;
    virtual Result<bool> PageVertical(
        double direction) noexcept = 0;
};

class AERO_GUI_API ScrollContentPresenter
    : public ContentControl,
      public IScrollInfo {
    AERO_DECLARE_TYPE(ScrollContentPresenter, ContentControl)
public:
    ScrollContentPresenter() noexcept;
    ~ScrollContentPresenter() override = default;

    ScrollData GetData() const noexcept override;
    IScrollInfo* GetContentScrollInfo() const noexcept {
        return contentScrollInfo_;
    }
    void SetContentScrollInfo(
        IScrollInfo* value) noexcept;

    bool GetCanHorizontallyScroll() const noexcept;
    bool GetCanVerticallyScroll() const noexcept;
    bool GetCanContentScroll() const noexcept;
    void SetCanHorizontallyScroll(
        bool value) noexcept;
    void SetCanVerticallyScroll(
        bool value) noexcept;
    void SetCanContentScroll(
        bool value) noexcept;
    inline static constexpr DependencyProperty<bool> CanContentScrollProperty{"CanContentScroll"};

    void SetViewport(
        Size viewport) noexcept override;
    void SetHorizontalOffset(
        double value) noexcept override;
    void SetVerticalOffset(
        double value) noexcept override;
    Result<bool> LineHorizontal(
        double direction) noexcept override;
    Result<bool> LineVertical(
        double direction) noexcept override;
    Result<bool> PageHorizontal(
        double direction) noexcept override;
    Result<bool> PageVertical(
        double direction) noexcept override;
    Result<bool> ApplyScrollDelta(
        double deltaX,
        double deltaY,
        ScrollInputKind kind) noexcept;

    double GetLineScrollAmount() const noexcept {
        return lineScrollAmount_;
    }
    void SetLineScrollAmount(
        double value) noexcept;

protected:
    explicit ScrollContentPresenter(
        TypeId runtimeType) noexcept;
    Size MeasureOverride(
        Size availableSize) noexcept override;
    Size ArrangeOverride(
        Size finalSize) noexcept override;
    virtual void OnScrollDataChanged(
        const ScrollData& oldData,
        const ScrollData& newData,
        ScrollInputKind kind) noexcept;
    virtual bool GetAllowsHorizontalScroll() const noexcept;
    virtual bool GetAllowsVerticalScroll() const noexcept;
    virtual bool GetUsesContentScrolling() const noexcept;
    Result<bool> UpdateData(
        ScrollData value,
        ScrollInputKind kind,
        bool invalidateArrange) noexcept;

private:
    ScrollData data_;
    IScrollInfo* contentScrollInfo_ = nullptr;
    double lineScrollAmount_ = 16.0;
    bool canHorizontallyScroll_ = false;
    bool canVerticallyScroll_ = true;
    ScrollInputKind pendingInputKind_ = ScrollInputKind::Line;

    Result<bool> SyncLogicalData(
        ScrollInputKind kind) noexcept;
    IScrollInfo* ActiveContentScrollInfo() const noexcept;
};

class AERO_GUI_API ScrollViewer
    : public ScrollContentPresenter {
    AERO_DECLARE_TYPE(ScrollViewer, ScrollContentPresenter)
#if defined(AERO_GUI_IMPLEMENTATION)
public:
#else
private:
#endif
    struct Access;

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

    void SetCanHorizontallyScroll(
        bool value) noexcept;
    void SetCanVerticallyScroll(
        bool value) noexcept;
    void SetCanContentScroll(
        bool value) noexcept;
    void SetHorizontalScrollBarVisibility(
        ScrollBarVisibility value) noexcept;
    void SetVerticalScrollBarVisibility(
        ScrollBarVisibility value) noexcept;
    PanningMode GetPanningMode() const noexcept;
    void SetPanningMode(
        PanningMode value) noexcept;
    void SetHorizontalOffset(
        double value) noexcept override;
    void SetVerticalOffset(
        double value) noexcept override;
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
    static void SetHorizontalScrollBarVisibility(
        DependencyObject& element,
        ScrollBarVisibility value) noexcept;
    static void SetVerticalScrollBarVisibility(
        DependencyObject& element,
        ScrollBarVisibility value) noexcept;

    inline static constexpr ReadOnlyDependencyProperty<double> HorizontalOffsetProperty{"HorizontalOffset"};
    inline static constexpr ReadOnlyDependencyProperty<double> VerticalOffsetProperty{"VerticalOffset"};
    inline static constexpr ReadOnlyDependencyProperty<double> ExtentWidthProperty{"ExtentWidth"};
    inline static constexpr ReadOnlyDependencyProperty<double> ExtentHeightProperty{"ExtentHeight"};
    inline static constexpr ReadOnlyDependencyProperty<double> ViewportWidthProperty{"ViewportWidth"};
    inline static constexpr ReadOnlyDependencyProperty<double> ViewportHeightProperty{"ViewportHeight"};
    inline static constexpr ReadOnlyDependencyProperty<double> ScrollableWidthProperty{"ScrollableWidth"};
    inline static constexpr ReadOnlyDependencyProperty<double> ScrollableHeightProperty{"ScrollableHeight"};
    inline static constexpr ReadOnlyDependencyProperty<Visibility> ComputedHorizontalScrollBarVisibilityProperty{"ComputedHorizontalScrollBarVisibility"};
    inline static constexpr ReadOnlyDependencyProperty<Visibility> ComputedVerticalScrollBarVisibilityProperty{"ComputedVerticalScrollBarVisibility"};
    inline static constexpr AttachedProperty<ScrollBarVisibility> HorizontalScrollBarVisibilityProperty{"HorizontalScrollBarVisibility"};
    inline static constexpr AttachedProperty<ScrollBarVisibility> VerticalScrollBarVisibilityProperty{"VerticalScrollBarVisibility"};
    inline static constexpr DependencyProperty<bool> CanHorizontallyScrollProperty{"CanHorizontallyScroll"};
    inline static constexpr DependencyProperty<bool> CanVerticallyScrollProperty{"CanVerticallyScroll"};
    inline static constexpr AttachedProperty<bool> CanContentScrollProperty{"CanContentScroll"};
    inline static constexpr AttachedProperty<PanningMode> PanningModeProperty{"PanningMode"};

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

private:
    friend class ScrollContentPresenter;
    friend struct Access;
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
