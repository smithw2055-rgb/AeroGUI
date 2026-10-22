#pragma once

#include <Aero/Base/Vector.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/Controls/Control.hpp>
#include <Aero/Events.hpp>
#include <Aero/Input.hpp>

namespace Aero::Controls::Primitives { class ScrollBar; }

namespace Aero::Controls {

using Size = ::Aero::Base::Size;
using ::Aero::Meta::DependencyPropertyChangedEventArgs;
using ::Aero::Meta::TypeId;

// ============================================================================
// IScrollInfo
// ============================================================================

class AERO_GUI_API IScrollInfo {
public:
    virtual ~IScrollInfo() = default;
    virtual ScrollData GetData() const noexcept = 0;
    virtual void SetViewport(Size viewport) noexcept = 0;
    virtual void SetHorizontalOffset(double value) noexcept = 0;
    virtual void SetVerticalOffset(double value) noexcept = 0;
    virtual Result<bool> LineHorizontal(double direction) noexcept = 0;
    virtual Result<bool> LineVertical(double direction) noexcept = 0;
    virtual Result<bool> PageHorizontal(double direction) noexcept = 0;
    virtual Result<bool> PageVertical(double direction) noexcept = 0;

    virtual Result<bool> LineUp() noexcept { return LineVertical(-1.0); }
    virtual Result<bool> LineDown() noexcept { return LineVertical(1.0); }
    virtual Result<bool> LineLeft() noexcept { return LineHorizontal(-1.0); }
    virtual Result<bool> LineRight() noexcept { return LineHorizontal(1.0); }
    virtual Result<bool> PageUp() noexcept { return PageVertical(-1.0); }
    virtual Result<bool> PageDown() noexcept { return PageVertical(1.0); }
    virtual Result<bool> MouseWheelUp() noexcept { return PageVertical(-1.0); }
    virtual Result<bool> MouseWheelDown() noexcept { return PageVertical(1.0); }
    virtual Result<bool> MouseWheelLeft() noexcept { return PageHorizontal(-1.0); }
    virtual Result<bool> MouseWheelRight() noexcept { return PageHorizontal(1.0); }
};

// ============================================================================
// ScrollContentPresenter
// ============================================================================

class AERO_GUI_API ScrollContentPresenter : public ContentPresenter,
      public IScrollInfo {
    AERO_DECLARE_TYPE(ScrollContentPresenter, ContentPresenter)

public:
    ScrollContentPresenter() noexcept;
    ~ScrollContentPresenter() override = default;

    UIElement* GetContentElement() const noexcept { return GetContent(); }

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
    double desiredVerticalOffset_ = 0.0;
    double desiredHorizontalOffset_ = 0.0;

    Result<bool> SyncLogicalData(ScrollInputKind kind) noexcept;
    IScrollInfo* ActiveContentScrollInfo() const noexcept;
};

// ============================================================================
// ScrollBarVisibility & PanningMode
// ============================================================================

enum class ScrollBarVisibility : std::uint8_t {
    Disabled = 0U,
    Auto,
    Hidden,
    Visible,
};

enum class PanningMode : std::uint8_t {
    None = 0U,
    HorizontalOnly,
    VerticalOnly,
    Both,
    HorizontalFirst,
    VerticalFirst,
};

// ============================================================================
// ScrollViewer
// ============================================================================

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
    ScrollBarVisibility GetHorizontalScrollBarVisibility() const noexcept;
    static ScrollBarVisibility GetHorizontalScrollBarVisibility(const DependencyObject& element) noexcept;
    void SetHorizontalScrollBarVisibility(ScrollBarVisibility value) noexcept;
    static void SetHorizontalScrollBarVisibility(DependencyObject& element, ScrollBarVisibility value) noexcept;
    ScrollBarVisibility GetVerticalScrollBarVisibility() const noexcept;
    static ScrollBarVisibility GetVerticalScrollBarVisibility(const DependencyObject& element) noexcept;
    void SetVerticalScrollBarVisibility(ScrollBarVisibility value) noexcept;
    static void SetVerticalScrollBarVisibility(DependencyObject& element, ScrollBarVisibility value) noexcept;
    Visibility GetComputedHorizontalScrollBarVisibility() const noexcept;
    Visibility GetComputedVerticalScrollBarVisibility() const noexcept;
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
    void OnContentChanged(const Value& oldContent, const Value& newContent) override;
    void OnVisualParentChanged(::Aero::Media::Visual* oldParent) noexcept override;
    Size MeasureOverride(Size availableSize) noexcept override;
    Size ArrangeOverride(Size finalSize) noexcept override;
    std::uint32_t GetVisualChildrenCount() const noexcept override;
    ::Aero::Media::Visual* GetVisualChild(std::uint32_t index) const noexcept override;
    void OnTemplateDetached() noexcept override;
    void OnMouseWheel(MouseWheelEventArgs& args);
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

    void EnsureCommands() noexcept;
    void UnregisterCommands() noexcept;

    void OnLineUpCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    void OnLineDownCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    void OnLineLeftCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    void OnLineRightCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    void OnPageUpCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    void OnPageDownCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    void OnPageLeftCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    void OnPageRightCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    void OnScrollToTopCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    void OnScrollToBottomCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    void OnScrollToLeftEndCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    void OnScrollToRightEndCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;

    ExecutedRoutedEventHandler lineUpHandler_;
    ExecutedRoutedEventHandler lineDownHandler_;
    ExecutedRoutedEventHandler lineLeftHandler_;
    ExecutedRoutedEventHandler lineRightHandler_;
    ExecutedRoutedEventHandler pageUpHandler_;
    ExecutedRoutedEventHandler pageDownHandler_;
    ExecutedRoutedEventHandler pageLeftHandler_;
    ExecutedRoutedEventHandler pageRightHandler_;
    ExecutedRoutedEventHandler scrollToTopHandler_;
    ExecutedRoutedEventHandler scrollToBottomHandler_;
    ExecutedRoutedEventHandler scrollToLeftEndHandler_;
    ExecutedRoutedEventHandler scrollToRightEndHandler_;
    Base::Vector<Input::CommandBindingHandle> commandHandles_;

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
