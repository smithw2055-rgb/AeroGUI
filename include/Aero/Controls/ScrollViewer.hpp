#pragma once

#include <Aero/Controls/ContentControl.hpp>
#include <Aero/Controls/IScrollInfo.hpp>
#include <Aero/Events/ControlEventArgs.hpp>
#include <Aero/Input.hpp>

namespace Aero::Controls::Primitives { class ScrollBar; }

namespace Aero::Controls {
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::TypeId;

class AERO_GUI_API ScrollContentPresenter : public ContentControl,
      public IScrollInfo {
    AERO_DECLARE_TYPE(ScrollContentPresenter, ContentControl)

public:
    ScrollContentPresenter() noexcept;
    ~ScrollContentPresenter() override = default;

    ScrollData GetData() const noexcept override;
    IScrollInfo* GetContentScrollInfo() const noexcept { return contentScrollInfo_; }
    void SetContentScrollInfo(IScrollInfo* value) noexcept;

    bool GetCanHorizontallyScroll() const noexcept;
    bool GetCanVerticallyScroll() const noexcept;
    bool GetCanContentScroll() const noexcept;
    void SetCanHorizontallyScroll(bool value) noexcept;
    void SetCanVerticallyScroll(bool value) noexcept;
    void SetCanContentScroll(bool value) noexcept;
    AERO_DEPENDENCY_PROPERTY(bool, CanContentScroll);

    void SetViewport(Size viewport) noexcept override;
    void SetHorizontalOffset(double value) noexcept override;
    void SetVerticalOffset(double value) noexcept override;
    Result<bool> LineHorizontal(double direction) noexcept override;
    Result<bool> LineVertical(double direction) noexcept override;
    Result<bool> PageHorizontal(double direction) noexcept override;
    Result<bool> PageVertical(double direction) noexcept override;
    Result<bool> ApplyScrollDelta(double deltaX, double deltaY, ScrollInputKind kind) noexcept;

    double GetLineScrollAmount() const noexcept { return lineScrollAmount_; }
    void SetLineScrollAmount(double value) noexcept;

protected:
    explicit ScrollContentPresenter(TypeId runtimeType) noexcept;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
    virtual void OnScrollDataChanged(const ScrollData& oldData, const ScrollData& newData,
        ScrollInputKind kind) noexcept;
    virtual bool GetAllowsHorizontalScroll() const noexcept;
    virtual bool GetAllowsVerticalScroll() const noexcept;
    virtual bool GetUsesContentScrolling() const noexcept;
    Result<bool> UpdateData(ScrollData value, ScrollInputKind kind, bool invalidateArrange) noexcept;

private:
    ScrollData data_;
    IScrollInfo* contentScrollInfo_ = nullptr;
    double lineScrollAmount_ = 16.0;
    bool canHorizontallyScroll_ = false;
    bool canVerticallyScroll_ = true;
    ScrollInputKind pendingInputKind_ = ScrollInputKind::Line;

    Result<bool> SyncLogicalData(ScrollInputKind kind) noexcept;
    IScrollInfo* ActiveContentScrollInfo() const noexcept;
};

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

class AERO_GUI_API ScrollViewer : public ContentControl {
    AERO_DECLARE_TYPE(ScrollViewer, ContentControl)

public:
    ScrollViewer() noexcept;
    ~ScrollViewer() override;

    Result<bool> LineHorizontal(double direction) noexcept;
    Result<bool> LineVertical(double direction) noexcept;
    Result<bool> PageHorizontal(double direction) noexcept;
    Result<bool> PageVertical(double direction) noexcept;

    double GetHorizontalOffset() const noexcept;
    void SetHorizontalOffset(double value) noexcept;
    double GetVerticalOffset() const noexcept;
    void SetVerticalOffset(double value) noexcept;
    double GetExtentWidth() const noexcept;
    double GetExtentHeight() const noexcept;
    double GetViewportWidth() const noexcept;
    double GetViewportHeight() const noexcept;
    double GetScrollableWidth() const noexcept;
    double GetScrollableHeight() const noexcept;
    ScrollBarVisibility
    GetHorizontalScrollBarVisibility() const noexcept;
    static ScrollBarVisibility
    GetHorizontalScrollBarVisibility(const DependencyObject& element) noexcept;
    void SetHorizontalScrollBarVisibility(ScrollBarVisibility value) noexcept;
    static void SetHorizontalScrollBarVisibility(DependencyObject& element, ScrollBarVisibility value) noexcept;
    ScrollBarVisibility
    GetVerticalScrollBarVisibility() const noexcept;
    static ScrollBarVisibility
    GetVerticalScrollBarVisibility(const DependencyObject& element) noexcept;
    void SetVerticalScrollBarVisibility(ScrollBarVisibility value) noexcept;
    static void SetVerticalScrollBarVisibility(DependencyObject& element, ScrollBarVisibility value) noexcept;
    Visibility
    GetComputedHorizontalScrollBarVisibility() const noexcept;
    Visibility
    GetComputedVerticalScrollBarVisibility() const noexcept;
    void SetCanHorizontallyScroll(bool value) noexcept;
    void SetCanVerticallyScroll(bool value) noexcept;
    void SetCanContentScroll(bool value) noexcept;
    PanningMode GetPanningMode() const noexcept;
    void SetPanningMode(PanningMode value) noexcept;
    IScrollInfo* GetContentScrollInfo() const noexcept;
    void SetContentScrollInfo(IScrollInfo* value) noexcept;

    inline static constexpr RoutedEvent<ScrollChangedEventArgs> ScrollChangedEvent{"ScrollChanged"};
    UIElement::Event<ScrollChangedEventArgs> ScrollChanged() noexcept { return GetEvent(ScrollChangedEvent); }

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
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
    std::uint32_t GetVisualChildrenCount() const noexcept override;
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override;
    void OnTemplateDetached() noexcept override;
    void OnMouseWheel(MouseWheelEventArgs& args);
    // Replaces OnScrollViewerVisibilityChanged delegate (setter re-entry).
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;

private:
    friend class ScrollContentPresenter;

    void EnsureFallbackPresenter() noexcept;
    void AdoptPresenterData(ScrollContentPresenter& presenter, const ScrollData& data, ScrollInputKind kind) noexcept;
    void OnScrollDataChanged(const ScrollData& oldData, const ScrollData& newData, ScrollInputKind kind) noexcept;
    void UpdateComputedScrollBarVisibility(const ScrollData& data) noexcept;
    void AttachScrollBars() noexcept;
    void DetachScrollBars() noexcept;
    void OnScrollBarValueChanged(DependencyObject& sender, const DependencyPropertyChangedEventArgs& args) noexcept;

    ScrollContentPresenter* contentPresenter_ = nullptr;
    Ref<ScrollContentPresenter> ownedPresenter_;
    IScrollInfo* pendingScrollInfo_ = nullptr;
    ScrollData lastData_{};
    Primitives::ScrollBar* verticalScrollBar_ = nullptr;
    Primitives::ScrollBar* horizontalScrollBar_ = nullptr;
    bool synchronizingScrollBars_ = false;
    DependencyPropertyChangedEventHandler scrollBarValueChangedHandler_;
};
} // namespace Aero::Controls
AERO_DECLARE_TYPE_ENUM(Aero::Controls::ScrollBarVisibility)
AERO_DECLARE_TYPE_ENUM(Aero::Controls::PanningMode)
