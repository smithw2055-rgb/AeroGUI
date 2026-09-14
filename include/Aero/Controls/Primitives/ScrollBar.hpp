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
    static void RegisterMetadata(::Aero::Meta::Registration& context) noexcept;

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

    void OnMouseLeftButtonDown(MouseButtonEventArgs& args) override;
    void OnMouseLeftButtonUp(MouseButtonEventArgs& args) override;
    void OnMouseMove(MouseEventArgs& args) override;
    void OnKeyDown(KeyEventArgs& args) override;
    void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept override;

private:
    Track* track_ = nullptr;
    std::uint32_t pointerId_ = 0U;
    bool dragging_ = false;
    Point dragOrigin_{};
    double dragStartValue_ = 0.0;

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

    void SynchronizeTrack() noexcept;
};

} // namespace Aero::Controls::Primitives
