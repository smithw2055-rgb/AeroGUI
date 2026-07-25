#pragma once

#include <Aero/Controls/ControlPrimitives.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Presentation/Input.hpp>

namespace Aero::Controls {

enum class ScrollInputKind : std::uint8_t {
    Line = 0U,
    Page,
    Wheel,
    Thumb,
    Touch,
};

struct ScrollData final {
    double horizontalOffset = 0.0;
    double verticalOffset = 0.0;
    double extentWidth = 0.0;
    double extentHeight = 0.0;
    double viewportWidth = 0.0;
    double viewportHeight = 0.0;
};

class AERO_API IScrollInfo {
public:
    virtual ~IScrollInfo() = default;
    virtual ScrollData Data() const noexcept = 0;
    virtual Base::Result<bool> SetViewport(
        Size viewport) noexcept = 0;
    virtual Base::Result<bool> SetHorizontalOffset(
        double value) noexcept = 0;
    virtual Base::Result<bool> SetVerticalOffset(
        double value) noexcept = 0;
    virtual Base::Result<bool> LineHorizontal(
        double direction) noexcept = 0;
    virtual Base::Result<bool> LineVertical(
        double direction) noexcept = 0;
    virtual Base::Result<bool> PageHorizontal(
        double direction) noexcept = 0;
    virtual Base::Result<bool> PageVertical(
        double direction) noexcept = 0;
};

class ScrollInteractionManager;

struct ScrollChangedEventArgs final : RoutedEventArgs {
    AERO_TYPED_META(ScrollChangedEventArgs, RoutedEventArgs)
    ScrollChangedEventArgs() noexcept
        : RoutedEventArgs(StaticTypeId()) {}
    ScrollData oldData;
    ScrollData newData;
    ScrollInputKind inputKind = ScrollInputKind::Line;
};

using ScrollChangedEventHandler =
    Base::Delegate<void(
        Base::Object*, const ScrollChangedEventArgs&)>;

class AERO_API ScrollContentPresenter
    : public Decorator,
      public IScrollInfo {
    AERO_TYPED_META(ScrollContentPresenter, Decorator)
public:
    ScrollContentPresenter() noexcept;
    ~ScrollContentPresenter() override = default;

    ScrollData Data() const noexcept override;
    IScrollInfo* ContentScrollInfo() const noexcept {
        return contentScrollInfo_;
    }
    Base::Result<void> SetContentScrollInfo(
        IScrollInfo* value) noexcept;

    bool CanHorizontallyScroll() const noexcept;
    bool CanVerticallyScroll() const noexcept;
    bool CanContentScroll() const noexcept;
    Base::Result<void> SetCanHorizontallyScroll(
        bool value) noexcept;
    Base::Result<void> SetCanVerticallyScroll(
        bool value) noexcept;
    Base::Result<void> SetCanContentScroll(
        bool value) noexcept;

    Base::Result<bool> SetViewport(
        Size viewport) noexcept override;
    Base::Result<bool> SetHorizontalOffset(
        double value) noexcept override;
    Base::Result<bool> SetVerticalOffset(
        double value) noexcept override;
    Base::Result<bool> LineHorizontal(
        double direction) noexcept override;
    Base::Result<bool> LineVertical(
        double direction) noexcept override;
    Base::Result<bool> PageHorizontal(
        double direction) noexcept override;
    Base::Result<bool> PageVertical(
        double direction) noexcept override;
    Base::Result<bool> ApplyScrollDelta(
        double deltaX,
        double deltaY,
        ScrollInputKind kind) noexcept;

    double LineScrollAmount() const noexcept {
        return lineScrollAmount_;
    }
    Base::Result<void> SetLineScrollAmount(
        double value) noexcept;

protected:
    explicit ScrollContentPresenter(
        TypeId runtimeType) noexcept;
    Base::Result<Size> MeasureOverride(
        Size availableSize) noexcept override;
    Base::Result<Size> ArrangeOverride(
        Size finalSize) noexcept override;
    virtual void OnScrollDataChanged(
        const ScrollData& oldData,
        const ScrollData& newData,
        ScrollInputKind kind) noexcept;
    virtual bool AllowsHorizontalScroll() const noexcept;
    virtual bool AllowsVerticalScroll() const noexcept;
    virtual bool UsesContentScrolling() const noexcept;

private:
    ScrollData data_;
    IScrollInfo* contentScrollInfo_ = nullptr;
    double lineScrollAmount_ = 16.0;
    bool canHorizontallyScroll_ = true;
    bool canVerticallyScroll_ = true;
    bool canContentScroll_ = false;
    ScrollInputKind pendingInputKind_ = ScrollInputKind::Line;

    Base::Result<bool> UpdateData(
        ScrollData value,
        ScrollInputKind kind,
        bool invalidateArrange) noexcept;
    Base::Result<bool> SyncLogicalData(
        ScrollInputKind kind) noexcept;
    IScrollInfo* ActiveContentScrollInfo() const noexcept;
};

class AERO_API ScrollViewer final
    : public ScrollContentPresenter {
    AERO_TYPED_META(ScrollViewer, ScrollContentPresenter)
public:
    ScrollViewer() noexcept;
    ~ScrollViewer() override;

    inline static constexpr RoutedEventHandle
        ScrollChangedEvent = MakeRoutedEventHandle(
            StaticTypeIdValue_, "ScrollChanged");
    UIElement::RoutedEvent_<ScrollChangedEventHandler>
        ScrollChanged() noexcept {
        return {*this, ScrollChangedEvent};
    }

    double HorizontalOffset() const noexcept;
    double VerticalOffset() const noexcept;
    double ExtentWidth() const noexcept;
    double ExtentHeight() const noexcept;
    double ViewportWidth() const noexcept;
    double ViewportHeight() const noexcept;

    Base::Result<void> SetCanHorizontallyScroll(
        bool value) noexcept;
    Base::Result<void> SetCanVerticallyScroll(
        bool value) noexcept;
    Base::Result<void> SetCanContentScroll(
        bool value) noexcept;

    inline static constexpr DependencyPropertyHandle
        HorizontalOffsetProperty =
            MakeDependencyPropertyHandle(
                StaticTypeIdValue_, "HorizontalOffset");
    inline static constexpr DependencyPropertyHandle
        VerticalOffsetProperty =
            MakeDependencyPropertyHandle(
                StaticTypeIdValue_, "VerticalOffset");
    inline static constexpr DependencyPropertyHandle
        ExtentWidthProperty =
            MakeDependencyPropertyHandle(
                StaticTypeIdValue_, "ExtentWidth");
    inline static constexpr DependencyPropertyHandle
        ExtentHeightProperty =
            MakeDependencyPropertyHandle(
                StaticTypeIdValue_, "ExtentHeight");
    inline static constexpr DependencyPropertyHandle
        ViewportWidthProperty =
            MakeDependencyPropertyHandle(
                StaticTypeIdValue_, "ViewportWidth");
    inline static constexpr DependencyPropertyHandle
        ViewportHeightProperty =
            MakeDependencyPropertyHandle(
                StaticTypeIdValue_, "ViewportHeight");
    inline static constexpr DependencyPropertyHandle
        CanHorizontallyScrollProperty =
            MakeDependencyPropertyHandle(
                StaticTypeIdValue_, "CanHorizontallyScroll");
    inline static constexpr DependencyPropertyHandle
        CanVerticallyScrollProperty =
            MakeDependencyPropertyHandle(
                StaticTypeIdValue_, "CanVerticallyScroll");
    inline static constexpr DependencyPropertyHandle
        CanContentScrollProperty =
            MakeDependencyPropertyHandle(
                StaticTypeIdValue_, "CanContentScroll");

protected:
    void OnScrollDataChanged(
        const ScrollData& oldData,
        const ScrollData& newData,
        ScrollInputKind kind) noexcept override;
    bool AllowsHorizontalScroll() const noexcept override;
    bool AllowsVerticalScroll() const noexcept override;
    bool UsesContentScrolling() const noexcept override;

private:
    friend class ScrollInteractionManager;
    RoutedEventManager* events_ = nullptr;
    ScrollInteractionManager* interactions_ = nullptr;
};

struct ThumbDragDelta final {
    double horizontalChange = 0.0;
    double verticalChange = 0.0;
};

class AERO_API Thumb final : public Control {
    AERO_TYPED_META(Thumb, Control)
public:
    Thumb() noexcept : Control(StaticTypeId()) {}
    ~Thumb() override = default;

    bool IsDragging() const noexcept {
        return dragging_;
    }
    Base::Result<void> BeginDrag(
        std::uint32_t pointerId,
        Point position) noexcept;
    Base::Result<ThumbDragDelta> DragTo(
        std::uint32_t pointerId,
        Point position) noexcept;
    Base::Result<bool> EndDrag(
        std::uint32_t pointerId) noexcept;

private:
    std::uint32_t pointerId_ = 0U;
    Point lastPosition_;
    bool dragging_ = false;
};

class AERO_API Track final : public Control {
    AERO_TYPED_META(Track, Control)
public:
    Track() noexcept : Control(StaticTypeId()) {}
    ~Track() override = default;

    Orientation GetOrientation() const noexcept;
    double Minimum() const noexcept;
    double Maximum() const noexcept;
    double Value() const noexcept;
    double ViewportSize() const noexcept;
    Base::Result<void> SetOrientation(
        Orientation value) noexcept;
    Base::Result<void> SetRange(
        double minimum,
        double maximum) noexcept;
    Base::Result<void> SetValue(
        double value) noexcept;
    Base::Result<void> SetViewportSize(
        double value) noexcept;
    double ThumbLength(
        double trackLength,
        double minimumThumbLength = 8.0) const noexcept;
    double ThumbOffset(
        double trackLength,
        double minimumThumbLength = 8.0) const noexcept;
    Base::Result<double> ValueFromThumbOffset(
        double offset,
        double trackLength,
        double minimumThumbLength = 8.0) const noexcept;

    inline static constexpr DependencyPropertyHandle
        OrientationProperty = MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "Orientation");
    inline static constexpr DependencyPropertyHandle
        MinimumProperty = MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "Minimum");
    inline static constexpr DependencyPropertyHandle
        MaximumProperty = MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "Maximum");
    inline static constexpr DependencyPropertyHandle
        ValueProperty = MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "Value");
    inline static constexpr DependencyPropertyHandle
        ViewportSizeProperty = MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "ViewportSize");
};

class AERO_API ScrollBar final : public Control {
    AERO_TYPED_META(ScrollBar, Control)
public:
    ScrollBar() noexcept : Control(StaticTypeId()) {}
    ~ScrollBar() override = default;

    Orientation GetOrientation() const noexcept;
    double Minimum() const noexcept;
    double Maximum() const noexcept;
    double Value() const noexcept;
    double ViewportSize() const noexcept;
    double SmallChange() const noexcept;
    double LargeChange() const noexcept;
    Base::Result<void> SetOrientation(
        Orientation value) noexcept;
    Base::Result<void> SetRange(
        double minimum,
        double maximum) noexcept;
    Base::Result<bool> SetValue(
        double value) noexcept;
    Base::Result<void> SetViewportSize(
        double value) noexcept;
    Base::Result<void> SetSmallChange(
        double value) noexcept;
    Base::Result<void> SetLargeChange(
        double value) noexcept;
    Base::Result<bool> LineDecrement() noexcept;
    Base::Result<bool> LineIncrement() noexcept;
    Base::Result<bool> PageDecrement() noexcept;
    Base::Result<bool> PageIncrement() noexcept;
    Base::Result<bool> DragThumb(
        double thumbOffset,
        double trackLength,
        double minimumThumbLength = 8.0) noexcept;

    inline static constexpr DependencyPropertyHandle
        OrientationProperty = MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "Orientation");
    inline static constexpr DependencyPropertyHandle
        MinimumProperty = MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "Minimum");
    inline static constexpr DependencyPropertyHandle
        MaximumProperty = MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "Maximum");
    inline static constexpr DependencyPropertyHandle
        ValueProperty = MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "Value");
    inline static constexpr DependencyPropertyHandle
        ViewportSizeProperty = MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "ViewportSize");
    inline static constexpr DependencyPropertyHandle
        SmallChangeProperty = MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "SmallChange");
    inline static constexpr DependencyPropertyHandle
        LargeChangeProperty = MakeDependencyPropertyHandle(
            StaticTypeIdValue_, "LargeChange");
};

class AERO_API ScrollInteractionManager final {
public:
    ScrollInteractionManager(
        ObjectTree& tree,
        RoutedEventManager& events) noexcept;
    ~ScrollInteractionManager() noexcept;

    Base::Result<void> Attach(
        ScrollViewer& viewer) noexcept;
    Base::Result<bool> Detach(
        ScrollViewer& viewer) noexcept;

private:
    struct ViewerRecord final {
        ScrollViewer* viewer = nullptr;
        VisualHandle handle;
    };

    ObjectTree* tree_ = nullptr;
    RoutedEventManager* events_ = nullptr;
    Base::Vector<ViewerRecord> viewers_;
    MouseWheelEventHandler wheelHandler_;

    void OnMouseWheel(
        Base::Object* sender,
        const MouseWheelEventArgs& args) noexcept;
    std::uint32_t FindViewer(
        const ScrollViewer& viewer) const noexcept;
};

} // namespace Aero::Controls
