#pragma once

#include <Aero/Controls/Primitives/RangeBase.hpp>
#include <Aero/Controls/Primitives/Track.hpp>
#include <Aero/Events/EventArgs.hpp>
#include <Aero/Events/CommandEventArgs.hpp>
#include <Aero/CommandBinding.hpp>
#include <Aero/Base/Vector.hpp>

namespace Aero::Controls::Primitives {

class AERO_GUI_API ScrollBar : public RangeBase {
    AERO_DECLARE_TYPE(ScrollBar, RangeBase)
public:
    ScrollBar() noexcept;
    ~ScrollBar() override;

    Orientation GetOrientation() const noexcept;
    double GetViewportSize() const noexcept;
    double GetSmallChange() const noexcept;
    double GetLargeChange() const noexcept;
    void SetOrientation(Orientation value) noexcept;
    void SetViewportSize(double value) noexcept;
    void SetSmallChange(double value) noexcept;
    void SetLargeChange(double value) noexcept;
    Result<bool> LineDecrement() noexcept;
    Result<bool> LineIncrement() noexcept;
    Result<bool> PageDecrement() noexcept;
    Result<bool> PageIncrement() noexcept;
    Result<bool> DragThumb(
        double thumbOffset,
        double trackLength,
        double minimumThumbLength = 8.0) noexcept;

    AERO_DEPENDENCY_PROPERTY(Orientation, Orientation);
    AERO_DEPENDENCY_PROPERTY(double, ViewportSize);
    AERO_DEPENDENCY_PROPERTY(double, SmallChange);
    AERO_DEPENDENCY_PROPERTY(double, LargeChange);

protected:
    void OnApplyTemplate() noexcept override;
    void OnTemplateDetached() noexcept override;
    void OnVisualParentChanged(Visual* oldParent) noexcept override;

    virtual void OnMouseLeftButtonDown(MouseButtonEventArgs& args);
    virtual void OnMouseLeftButtonUp(MouseButtonEventArgs& args);
    virtual void OnMouseMove(MouseEventArgs& args);
    virtual void OnKeyDown(KeyEventArgs& args);

private:
    Track* track_ = nullptr;
    DependencyPropertyChangedEventHandler
        trackPropertyChangedHandler_;
    std::uint32_t pointerId_ = 0U;
    bool dragging_ = false;
    Point dragOrigin_{};
    double dragStartValue_ = 0.0;

    MouseButtonEventHandler mouseDownHandler_;
    MouseEventHandler mouseMoveHandler_;
    MouseButtonEventHandler mouseUpHandler_;
    KeyEventHandler keyDownHandler_;

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
    ExecutedRoutedEventHandler scrollToHorizontalOffsetHandler_;
    ExecutedRoutedEventHandler scrollToVerticalOffsetHandler_;
    Base::Vector<Input::CommandBindingHandle> commandHandles_;

    void OnMouseDownHandler(Base::Object* sender, MouseButtonEventArgs& args) noexcept;
    void OnMouseMoveHandler(Base::Object* sender, MouseEventArgs& args) noexcept;
    void OnMouseUpHandler(Base::Object* sender, MouseButtonEventArgs& args) noexcept;
    void OnKeyDownHandler(Base::Object* sender, KeyEventArgs& args) noexcept;

    static void OnLineUpCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnLineDownCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnLineLeftCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnLineRightCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnPageUpCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnPageDownCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnPageLeftCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnPageRightCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnScrollToTopCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnScrollToBottomCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnScrollToLeftEndCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnScrollToRightEndCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnScrollToHorizontalOffsetCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;
    static void OnScrollToVerticalOffsetCommand(Base::Object* sender, ExecutedRoutedEventArgs& args) noexcept;

    void EnsureCommands() noexcept;
    void UnregisterCommands() noexcept;

    void OnTrackPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    void SynchronizeTrack() noexcept;
};

} // namespace Aero::Controls::Primitives
