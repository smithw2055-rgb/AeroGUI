#include "gui/controls/ScrollCommon.hpp"
#include "gui/meta/ValueConversion.hpp"
#include "gui/core/State.hpp"
#include "gui/input/InputState.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "render/DisplayList.hpp"
#include <Aero/Controls.hpp>
#include "gui/media/MediaState.hpp"
#include <Aero/Input/Mouse.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/Value.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include "ControlBehavior.hpp"
#include "gui/templates/TemplateState.hpp"

namespace Aero::Controls {
using namespace Primitives;
using namespace ::Aero::Render;

ScrollBehavior::ScrollBehavior(
    ElementTree& tree,
    EventRouter& events) noexcept
    : tree_(&tree),
      events_(&events),
      wheelHandler_(
          this,
          &ScrollBehavior::OnMouseWheel) {}

ScrollBehavior::~ScrollBehavior() noexcept {
    while (!viewers_.Empty()) {
        ScrollViewer* viewer =
            viewers_.Back().viewer;
        if (viewer != nullptr) {
            static_cast<void>(Detach(*viewer));
        } else {
            viewers_.PopBack();
        }
    }
}

std::uint32_t ScrollBehavior::FindViewer(
    const ScrollViewer& viewer) const noexcept {
    const VisualHandle handle = AeroGuiInternal::Handle(viewer);
    for (std::uint32_t index = 0U;
        index < viewers_.Size(); ++index) {
        if (viewers_[index].viewer == &viewer ||
            (viewers_[index].handle.index == handle.index &&
                viewers_[index].handle.generation ==
                    handle.generation)) {
            return index;
        }
    }
    return UINT32_MAX;
}

Base::Result<void> ScrollBehavior::Attach(
    ScrollViewer& viewer) noexcept {
    if (FindViewer(viewer) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "ScrollViewer is already attached");
    }
    if (viewer.GetTree() != tree_ ||
        !AeroGuiInternal::Handle(viewer).IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ScrollViewer must be loaded in the interaction tree");
    }
    viewer.AddHandler(
        UIElement::MouseWheelEvent,
        wheelHandler_);
    Base::Result<void> added =
        viewers_.PushBack(
            {&viewer, AeroGuiInternal::Handle(viewer)});
    if (!added) {
        static_cast<void>(viewer.RemoveHandler(
            UIElement::MouseWheelEvent,
            wheelHandler_));
        return added.GetStatus();
    }
    return {};
}

Base::Result<bool> ScrollBehavior::Detach(
    ScrollViewer& viewer) noexcept {
    const std::uint32_t index = FindViewer(viewer);
    if (index == UINT32_MAX) return false;
    static_cast<void>(viewer.RemoveHandler(
        UIElement::MouseWheelEvent,
        wheelHandler_));
    if (index + 1U != viewers_.Size()) {
        viewers_[index] = viewers_.Back();
    }
    viewers_.PopBack();
    return true;
}

void ScrollBehavior::OnMouseWheel(
    Base::Object* sender,
    MouseWheelEventArgs& args) noexcept {
    auto* viewer = static_cast<ScrollViewer*>(sender);
    if (viewer == nullptr ||
        FindViewer(*viewer) == UINT32_MAX) {
        return;
    }
    const double horizontal =
        -args.GetDeltaX() * viewer->GetLineScrollAmount();
    const double vertical =
        -args.GetDeltaY() * viewer->GetLineScrollAmount();
    Base::Result<bool> changed =
        viewer->ApplyScrollDelta(
            horizontal,
            vertical,
            ScrollInputKind::Wheel);
    if (changed && changed.Value()) {
        args.SetHandled(true);
    }
}

SliderBehavior::SliderBehavior(
    ElementTree& tree,
    EventRouter& events,
    InputRouter& input) noexcept
    : tree_(&tree),
      events_(&events),
      input_(&input),
      mouseDownHandler_(
          this,
          &SliderBehavior::OnMouseDown),
      mouseMoveHandler_(
          this,
          &SliderBehavior::OnMouseMove),
      mouseUpHandler_(
          this,
          &SliderBehavior::OnMouseUp),
      keyDownHandler_(
          this,
          &SliderBehavior::OnKeyDown),
      captureChangedHandler_(
          this,
          &SliderBehavior::OnCaptureChanged),
      decreaseSmallHandler_(
          this,
          &SliderBehavior::OnDecreaseSmallCommand),
      increaseSmallHandler_(
          this,
          &SliderBehavior::OnIncreaseSmallCommand),
      decreaseLargeHandler_(
          this,
          &SliderBehavior::OnDecreaseLargeCommand),
      increaseLargeHandler_(
          this,
          &SliderBehavior::OnIncreaseLargeCommand) {}

SliderBehavior::~SliderBehavior()
    noexcept {
    while (!sliders_.Empty()) {
        Slider* slider =
            Resolve(sliders_.Size() - 1U);
        if (slider == nullptr) {
            sliders_.PopBack();
            continue;
        }
        static_cast<void>(Detach(*slider));
    }
    static_cast<void>(
        input_->RemovePointerCaptureChanged(
            captureChangedHandler_));
}

std::uint32_t SliderBehavior::Find(
    const Slider& slider) const noexcept {
    for (std::uint32_t index = 0U;
         index < sliders_.Size(); ++index) {
        const VisualHandle current =
            AeroGuiInternal::Handle(slider);
        if (sliders_[index].handle.index ==
                current.index &&
            sliders_[index].handle.generation ==
                current.generation) {
            return index;
        }
    }
    return UINT32_MAX;
}

Slider* SliderBehavior::Resolve(
    std::uint32_t index) noexcept {
    if (index >= sliders_.Size()) return nullptr;
    ::Aero::Media::Visual* node =
        tree_->ResolveHandle(
            sliders_[index].handle);
    if (node == nullptr ||
        !node->PropertyRegistry().Types().
            IsDerivedFrom(
                node->RuntimeType(),
                Slider::StaticTypeId())) {
        return nullptr;
    }
    return static_cast<Slider*>(node);
}

void SliderBehavior::RemoveAt(
    std::uint32_t index) noexcept {
    if (index >= sliders_.Size()) return;
    if (index + 1U != sliders_.Size()) {
        sliders_[index] =
            sliders_.Back();
    }
    sliders_.PopBack();
}

Base::Result<void> SliderBehavior::Attach(
    Slider& slider) noexcept {
    if (Find(slider) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Slider is already attached");
    }
    if (slider.GetTree() != tree_ ||
        !AeroGuiInternal::Handle(slider).IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Slider must be loaded in the interaction tree");
    }
    if (sliders_.Empty()) {
        input_->AddPointerCaptureChanged(captureChangedHandler_);
    }
    slider.AddHandler(
        UIElement::MouseDownEvent,
        mouseDownHandler_);
    slider.AddHandler(
        UIElement::MouseMoveEvent,
        mouseMoveHandler_);
    slider.AddHandler(
        UIElement::MouseUpEvent,
        mouseUpHandler_);
    slider.AddHandler(
        UIElement::KeyDownEvent,
        keyDownHandler_);
    SliderRecord record;
    record.handle =
        AeroGuiInternal::Handle(slider);
    const auto addCommand =
        [this, &slider](
            Base::StringView name,
            const ExecutedRoutedEventHandler& handler,
            Input::CommandBindingHandle& output) noexcept
            -> Base::Result<void> {
        Base::Result<Base::Ref<Input::RoutedCommand>> command =
            Input::RoutedCommand::ResolveStatic(
                Slider::StaticTypeId(), name);
        if (!command) return command.GetStatus();
        Base::Result<Input::CommandBindingHandle> added =
            input_->AddCommandBinding(
                slider,
                Input::CommandBinding(
                    std::move(command).Value(), handler));
        if (!added) return added.GetStatus();
        output = added.Value();
        return {};
    };
    Base::Result<void> status = addCommand(
        "DecreaseSmall", decreaseSmallHandler_,
        record.decreaseSmallCommand);
    if (status) {
        status = addCommand(
            "IncreaseSmall", increaseSmallHandler_,
            record.increaseSmallCommand);
    }
    if (status) {
        status = addCommand(
            "DecreaseLarge", decreaseLargeHandler_,
            record.decreaseLargeCommand);
    }
    if (status) {
        status = addCommand(
            "IncreaseLarge", increaseLargeHandler_,
            record.increaseLargeCommand);
    }
    if (!status) {
        if (record.decreaseSmallCommand.IsValid()) {
            static_cast<void>(input_->RemoveCommandBinding(
                record.decreaseSmallCommand));
        }
        if (record.increaseSmallCommand.IsValid()) {
            static_cast<void>(input_->RemoveCommandBinding(
                record.increaseSmallCommand));
        }
        if (record.decreaseLargeCommand.IsValid()) {
            static_cast<void>(input_->RemoveCommandBinding(
                record.decreaseLargeCommand));
        }
        static_cast<void>(slider.RemoveHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_));
        static_cast<void>(slider.RemoveHandler(
            UIElement::MouseMoveEvent,
            mouseMoveHandler_));
        static_cast<void>(slider.RemoveHandler(
            UIElement::MouseUpEvent,
            mouseUpHandler_));
        static_cast<void>(slider.RemoveHandler(
            UIElement::KeyDownEvent,
            keyDownHandler_));
        if (sliders_.Empty()) {
            static_cast<void>(
                input_->RemovePointerCaptureChanged(
                    captureChangedHandler_));
        }
        return status.GetStatus();
    }
    Base::Result<void> appended =
        sliders_.PushBack(record);
    if (!appended) {
        static_cast<void>(input_->RemoveCommandBinding(
            record.decreaseSmallCommand));
        static_cast<void>(input_->RemoveCommandBinding(
            record.increaseSmallCommand));
        static_cast<void>(input_->RemoveCommandBinding(
            record.decreaseLargeCommand));
        static_cast<void>(input_->RemoveCommandBinding(
            record.increaseLargeCommand));
        static_cast<void>(slider.RemoveHandler(
            UIElement::MouseDownEvent,
            mouseDownHandler_));
        static_cast<void>(slider.RemoveHandler(
            UIElement::MouseMoveEvent,
            mouseMoveHandler_));
        static_cast<void>(slider.RemoveHandler(
            UIElement::MouseUpEvent,
            mouseUpHandler_));
        static_cast<void>(slider.RemoveHandler(
            UIElement::KeyDownEvent,
            keyDownHandler_));
        if (sliders_.Empty()) {
            static_cast<void>(
                input_->RemovePointerCaptureChanged(
                    captureChangedHandler_));
        }
        return appended.GetStatus();
    }
    return {};
}

Base::Result<bool> SliderBehavior::Detach(
    Slider& slider) noexcept {
    const std::uint32_t index = Find(slider);
    if (index == UINT32_MAX) return false;
    if (sliders_[index].dragging) {
        static_cast<void>(
            input_->ReleasePointer(
                sliders_[index].pointerId));
    }
    static_cast<void>(slider.RemoveHandler(
        UIElement::MouseDownEvent,
        mouseDownHandler_));
    static_cast<void>(slider.RemoveHandler(
        UIElement::MouseMoveEvent,
        mouseMoveHandler_));
    static_cast<void>(slider.RemoveHandler(
        UIElement::MouseUpEvent,
        mouseUpHandler_));
    static_cast<void>(slider.RemoveHandler(
        UIElement::KeyDownEvent,
        keyDownHandler_));
    static_cast<void>(input_->RemoveCommandBinding(
        sliders_[index].decreaseSmallCommand));
    static_cast<void>(input_->RemoveCommandBinding(
        sliders_[index].increaseSmallCommand));
    static_cast<void>(input_->RemoveCommandBinding(
        sliders_[index].decreaseLargeCommand));
    static_cast<void>(input_->RemoveCommandBinding(
        sliders_[index].increaseLargeCommand));
    RemoveAt(index);
    if (sliders_.Empty()) {
        static_cast<void>(
            input_->RemovePointerCaptureChanged(
                captureChangedHandler_));
    }
    return true;
}

void SliderBehavior::OnDecreaseSmallCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* slider = static_cast<Slider*>(sender);
    if (slider != nullptr) {
        static_cast<void>(slider->DecreaseSmall());
        args.SetHandled(true);
    }
}

void SliderBehavior::OnIncreaseSmallCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* slider = static_cast<Slider*>(sender);
    if (slider != nullptr) {
        static_cast<void>(slider->IncreaseSmall());
        args.SetHandled(true);
    }
}

void SliderBehavior::OnDecreaseLargeCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* slider = static_cast<Slider*>(sender);
    if (slider != nullptr) {
        static_cast<void>(slider->DecreaseLarge());
        args.SetHandled(true);
    }
}

void SliderBehavior::OnIncreaseLargeCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* slider = static_cast<Slider*>(sender);
    if (slider != nullptr) {
        static_cast<void>(slider->IncreaseLarge());
        args.SetHandled(true);
    }
}

Base::Result<void>
SliderBehavior::SetFromPoint(
    Slider& slider) noexcept {
    auto* track = slider.track_;
    if (track != nullptr) {
        slider.SetValueFromTrackPoint(
            Input::Mouse::GetPosition(track));
        return {};
    }
    const Point local = Input::Mouse::GetPosition(&slider);
    const bool horizontal =
        slider.GetOrientation() ==
        Orientation::Horizontal;
    slider.SetValueFromPosition(
        horizontal ? local.x : local.y,
        horizontal
            ? slider.GetRenderSize().width
            : slider.GetRenderSize().height);
    return {};
}

void SliderBehavior::OnMouseDown(
    Base::Object* sender,
    MouseButtonEventArgs& args) noexcept {
    auto& slider =
        *static_cast<Slider*>(sender);
    const std::uint32_t index =
        Find(slider);
    if (index == UINT32_MAX ||
        !slider.GetIsEnabled() ||
        args.GetChangedButton() !=
            MouseButton::Left) {
        return;
    }
    SliderRecord& record =
        sliders_[index];
    record.pointerId = args.GetPointerId();
    static_cast<void>(
        input_->SetFocus(&slider));
    auto* track = slider.track_;
    Thumb* thumb = track != nullptr
        ? track->GetThumbElement().Get()
        : nullptr;
    auto* source = ::Aero::TryCast<UIElement>(
        args.GetOriginalSource());
    const bool onThumb =
        thumb != nullptr &&
        source != nullptr &&
        (source == thumb || thumb->IsAncestorOf(*source));
    const bool onTrack =
        track != nullptr &&
        source != nullptr &&
        (source == track || track->IsAncestorOf(*source));
    const bool onSlider =
        source == nullptr ||
        source == &slider ||
        slider.IsAncestorOf(*source);
    const bool hasRepeatButtons =
        track != nullptr &&
        (track->GetDecreaseRepeatButton() ||
            track->GetIncreaseRepeatButton());
    // Templates without RepeatButtons (BlendTutorial) wrap PART_Track in a
    // Border. OriginalSource is then that Border, not the Track. Treat any
    // click on the slider as move-to-point so the thumb follows the pointer.
    const bool moveToPoint =
        slider.GetIsMoveToPointEnabled() ||
        ((onTrack || onSlider) && !hasRepeatButtons);
    record.dragging = onThumb || moveToPoint;
    if (!record.dragging && track != nullptr && onTrack) {
        const Point local = Input::Mouse::GetPosition(track);
        const bool horizontal =
            slider.GetOrientation() ==
            Orientation::Horizontal;
        const Size size = track->GetRenderSize();
        const double length =
            horizontal ? size.width : size.height;
        const double position =
            horizontal ? local.x : local.y;
        const double thumbOffset =
            track->GetThumbOffset(length);
        const double thumbLength =
            track->GetThumbLength(length);
        record.dragging =
            std::fabs(
                position - (thumbOffset + thumbLength * 0.5)) <=
            std::max(10.0, thumbLength);
    }
    if (record.dragging) {
        static_cast<void>(
            input_->CapturePointer(
                args.GetPointerId(), slider));
    }
    if (moveToPoint || (record.dragging && onThumb)) {
        static_cast<void>(SetFromPoint(slider));
    } else if (!record.dragging) {
        const Point local = track != nullptr
            ? Input::Mouse::GetPosition(track)
            : Input::Mouse::GetPosition(&slider);
        const bool horizontal =
            slider.GetOrientation() ==
            Orientation::Horizontal;
        const double position =
            horizontal ? local.x : local.y;
        const double length =
            horizontal
            ? (track != nullptr
                ? track->GetRenderSize().width
                : slider.GetRenderSize().width)
            : (track != nullptr
                ? track->GetRenderSize().height
                : slider.GetRenderSize().height);
        const bool after =
            position >= length * 0.5;
        const bool increase =
            slider.GetIsDirectionReversed()
            ? !after : after;
        static_cast<void>(
            increase
            ? slider.IncreaseLarge()
            : slider.DecreaseLarge());
    }
    args.SetHandled(true);
}

void SliderBehavior::OnMouseMove(
    Base::Object* sender,
    MouseEventArgs& args) noexcept {
    auto& slider =
        *static_cast<Slider*>(sender);
    const std::uint32_t index =
        Find(slider);
    if (index == UINT32_MAX ||
        !sliders_[index].dragging ||
        sliders_[index].pointerId !=
            args.GetPointerId()) {
        return;
    }
    static_cast<void>(
        SetFromPoint(slider));
    args.SetHandled(true);
}

void SliderBehavior::OnMouseUp(
    Base::Object* sender,
    MouseButtonEventArgs& args) noexcept {
    auto& slider =
        *static_cast<Slider*>(sender);
    const std::uint32_t index =
        Find(slider);
    if (index == UINT32_MAX ||
        args.GetChangedButton() !=
            MouseButton::Left ||
        !sliders_[index].dragging ||
        sliders_[index].pointerId !=
            args.GetPointerId()) {
        return;
    }
    static_cast<void>(
        SetFromPoint(slider));
    sliders_[index].dragging = false;
    static_cast<void>(
        input_->ReleasePointer(
            args.GetPointerId()));
    args.SetHandled(true);
}

void SliderBehavior::OnKeyDown(
    Base::Object* sender,
    KeyEventArgs& args) noexcept {
    auto& slider =
        *static_cast<Slider*>(sender);
    if (Find(slider) == UINT32_MAX ||
        !slider.GetIsEnabled()) {
        return;
    }
    bool changed = false;
    bool handled = true;
    const bool reversed =
        slider.GetIsDirectionReversed();
    if (args.GetKey() == KeyboardKeyHome) {
        const double oldValue = slider.GetValue();
        slider.SetValue(
            reversed
            ? slider.GetMaximum()
            : slider.GetMinimum());
        changed = !Same(oldValue, slider.GetValue());
    } else if (args.GetKey() == KeyboardKeyEnd) {
        const double oldValue = slider.GetValue();
        slider.SetValue(
            reversed
            ? slider.GetMinimum()
            : slider.GetMaximum());
        changed = !Same(oldValue, slider.GetValue());
    } else if (
        args.GetKey() == KeyboardKeyLeft ||
        args.GetKey() == KeyboardKeyDown) {
        Base::Result<bool> result = reversed
            ? slider.IncreaseSmall()
            : slider.DecreaseSmall();
        changed = result && result.Value();
    } else if (
        args.GetKey() == KeyboardKeyRight ||
        args.GetKey() == KeyboardKeyUp) {
        Base::Result<bool> result = reversed
            ? slider.DecreaseSmall()
            : slider.IncreaseSmall();
        changed = result && result.Value();
    } else {
        handled = false;
    }
    if (handled && changed) {
        args.SetHandled(true);
    }
}

void SliderBehavior::OnCaptureChanged(
    std::uint32_t pointerId,
    UIElement* target,
    bool captured) noexcept {
    if (captured) return;
    for (SliderRecord& record :
         sliders_) {
        if (!record.dragging ||
            record.pointerId != pointerId) {
            continue;
        }
        Slider* slider =
            static_cast<Slider*>(
                tree_->ResolveHandle(record.handle));
        if (target == nullptr ||
            target == slider) {
            record.dragging = false;
        }
    }
}

} // namespace Aero::Controls

namespace Aero::Controls {

ScrollBarBehavior::ScrollBarBehavior(
    ElementTree& tree,
    EventRouter& events,
    InputRouter& input,
    VisualStateManager* states) noexcept
    : tree_(&tree),
      events_(&events),
      input_(&input),
      states_(states),
      scrollBars_(&Base::GetDefaultAllocator()),
      thumbs_(&Base::GetDefaultAllocator()),
      mouseDownHandler_(this, &ScrollBarBehavior::OnMouseDown),
      mouseMoveHandler_(this, &ScrollBarBehavior::OnMouseMove),
      mouseUpHandler_(this, &ScrollBarBehavior::OnMouseUp),
      keyDownHandler_(this, &ScrollBarBehavior::OnKeyDown),
      captureChangedHandler_(this, &ScrollBarBehavior::OnCaptureChanged),
      pointerStateChangedHandler_(this, &ScrollBarBehavior::OnPointerStateChanged),
      lineUpHandler_(&ScrollBarBehavior::OnLineUpCommand),
      lineDownHandler_(&ScrollBarBehavior::OnLineDownCommand),
      lineLeftHandler_(&ScrollBarBehavior::OnLineLeftCommand),
      lineRightHandler_(&ScrollBarBehavior::OnLineRightCommand),
      pageUpHandler_(&ScrollBarBehavior::OnPageUpCommand),
      pageDownHandler_(&ScrollBarBehavior::OnPageDownCommand),
      pageLeftHandler_(&ScrollBarBehavior::OnPageLeftCommand),
      pageRightHandler_(&ScrollBarBehavior::OnPageRightCommand),
      scrollToTopHandler_(&ScrollBarBehavior::OnScrollToTopCommand),
      scrollToBottomHandler_(&ScrollBarBehavior::OnScrollToBottomCommand),
      scrollToLeftEndHandler_(&ScrollBarBehavior::OnScrollToLeftEndCommand),
      scrollToRightEndHandler_(&ScrollBarBehavior::OnScrollToRightEndCommand),
      scrollToHorizontalOffsetHandler_(&ScrollBarBehavior::OnScrollToHorizontalOffsetCommand),
      scrollToVerticalOffsetHandler_(&ScrollBarBehavior::OnScrollToVerticalOffsetCommand) {}

ScrollBarBehavior::~ScrollBarBehavior() {
    if (input_ != nullptr) {
        static_cast<void>(
            input_->RemovePointerStateChanged(
                pointerStateChangedHandler_));
    }
    while (!thumbs_.Empty()) {
        Thumb* thumb = ResolveThumb(thumbs_.Size() - 1U);
        if (thumb != nullptr) {
            static_cast<void>(DetachThumb(*thumb));
        } else {
            RemoveThumbAt(thumbs_.Size() - 1U);
        }
    }
    while (!scrollBars_.Empty()) {
        ScrollBar* scrollBar = Resolve(scrollBars_.Size() - 1U);
        if (scrollBar != nullptr) {
            static_cast<void>(Detach(*scrollBar));
        } else {
            RemoveAt(scrollBars_.Size() - 1U);
        }
    }
}

std::uint32_t ScrollBarBehavior::Find(
    const ScrollBar& scrollBar) const noexcept {
    const VisualHandle target =
        AeroGuiInternal::Handle(scrollBar);
    for (std::uint32_t index = 0U;
         index < scrollBars_.Size(); ++index) {
        if (scrollBars_[index].handle.index == target.index &&
            scrollBars_[index].handle.generation == target.generation) {
            return index;
        }
    }
    return UINT32_MAX;
}

ScrollBar* ScrollBarBehavior::Resolve(
    std::uint32_t index) noexcept {
    if (index >= scrollBars_.Size()) return nullptr;
    return static_cast<ScrollBar*>(
        tree_->ResolveHandle(scrollBars_[index].handle));
}

void ScrollBarBehavior::RemoveAt(
    std::uint32_t index) noexcept {
    if (index >= scrollBars_.Size()) return;
    if (index + 1U != scrollBars_.Size()) {
        scrollBars_[index] =
            std::move(scrollBars_.Back());
    }
    scrollBars_.PopBack();
}

std::uint32_t ScrollBarBehavior::FindThumb(
    const Thumb& thumb) const noexcept {
    const VisualHandle target =
        AeroGuiInternal::Handle(thumb);
    for (std::uint32_t index = 0U;
         index < thumbs_.Size(); ++index) {
        if (thumbs_[index].index == target.index &&
            thumbs_[index].generation == target.generation) {
            return index;
        }
    }
    return UINT32_MAX;
}

Thumb* ScrollBarBehavior::ResolveThumb(
    std::uint32_t index) noexcept {
    if (index >= thumbs_.Size()) return nullptr;
    return static_cast<Thumb*>(
        tree_->ResolveHandle(thumbs_[index]));
}

void ScrollBarBehavior::RemoveThumbAt(
    std::uint32_t index) noexcept {
    if (index >= thumbs_.Size()) return;
    if (index + 1U != thumbs_.Size()) {
        thumbs_[index] = thumbs_.Back();
    }
    thumbs_.PopBack();
}

void ScrollBarBehavior::SyncThumbVisualState(
    Thumb& thumb) noexcept {
    if (states_ == nullptr) return;
    Base::StringView common = "Normal";
    if (!thumb.GetIsEnabled()) {
        common = "Disabled";
    } else if (thumb.GetIsDragging()) {
        common = "Pressed";
    } else if (thumb.GetIsMouseOver()) {
        common = "MouseOver";
    }
    static_cast<void>(
        Aero::VisualStateManagerRuntime::GoToState(
            *states_,
            thumb,
            "CommonStates",
            common,
            true));
}

Base::Result<void> ScrollBarBehavior::AttachThumb(
    Thumb& thumb) noexcept {
    if (FindThumb(thumb) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "Thumb is already attached");
    }
    if (thumb.GetTree() != tree_ ||
        !AeroGuiInternal::Handle(thumb).IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Thumb must be loaded in the interaction tree");
    }
    if (thumbs_.Empty()) {
        input_->AddPointerStateChanged(pointerStateChangedHandler_);
    }
    Base::Result<void> added =
        thumbs_.PushBack(AeroGuiInternal::Handle(thumb));
    if (!added) {
        if (thumbs_.Empty()) {
            static_cast<void>(
                input_->RemovePointerStateChanged(
                    pointerStateChangedHandler_));
        }
        return added.GetStatus();
    }
    SyncThumbVisualState(thumb);
    return {};
}

Base::Result<bool> ScrollBarBehavior::DetachThumb(
    Thumb& thumb) noexcept {
    const std::uint32_t index = FindThumb(thumb);
    if (index == UINT32_MAX) return false;
    RemoveThumbAt(index);
    if (thumbs_.Empty()) {
        static_cast<void>(
            input_->RemovePointerStateChanged(
                pointerStateChangedHandler_));
    }
    return true;
}

void ScrollBarBehavior::OnPointerStateChanged(
    UIElement& element) noexcept {
    for (std::uint32_t index = 0U;
         index < thumbs_.Size(); ++index) {
        Thumb* thumb = ResolveThumb(index);
        if (thumb == nullptr) continue;
        if (thumb != &element &&
            !thumb->IsAncestorOf(element)) {
            continue;
        }
        SyncThumbVisualState(*thumb);
    }
}

Base::Result<void> ScrollBarBehavior::Attach(
    ScrollBar& scrollBar) noexcept {
    if (Find(scrollBar) != UINT32_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "ScrollBar is already attached");
    }
    if (scrollBar.GetTree() != tree_ ||
        !AeroGuiInternal::Handle(scrollBar).IsValid()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "ScrollBar must be loaded in the interaction tree");
    }
    if (scrollBars_.Empty()) {
        input_->AddPointerCaptureChanged(captureChangedHandler_);
    }
    scrollBar.AddHandler(
        UIElement::MouseDownEvent,
        mouseDownHandler_);
    scrollBar.AddHandler(
        UIElement::MouseMoveEvent,
        mouseMoveHandler_);
    scrollBar.AddHandler(
        UIElement::MouseUpEvent,
        mouseUpHandler_);
    scrollBar.AddHandler(
        UIElement::KeyDownEvent,
        keyDownHandler_);
    ScrollBarRecord record;
    record.handle =
        AeroGuiInternal::Handle(scrollBar);

    const auto addCommand =
        [this, &scrollBar, &record](
            Base::StringView name,
            const ExecutedRoutedEventHandler& handler) noexcept
            -> Base::Result<void> {
        Base::Result<Base::Ref<Input::RoutedCommand>> command =
            Input::RoutedCommand::ResolveStatic(
                ScrollBar::StaticTypeId(), name);
        if (!command) return command.GetStatus();
        Base::Result<Input::CommandBindingHandle> added =
            input_->AddCommandBinding(
                scrollBar,
                Input::CommandBinding(
                    std::move(command).Value(), handler));
        if (!added) return added.GetStatus();
        return record.commands.PushBack(added.Value());
    };

    static_cast<void>(addCommand("LineUpCommand", lineUpHandler_));
    static_cast<void>(addCommand("LineUp", lineUpHandler_));
    static_cast<void>(addCommand("LineDownCommand", lineDownHandler_));
    static_cast<void>(addCommand("LineDown", lineDownHandler_));
    static_cast<void>(addCommand("LineLeftCommand", lineLeftHandler_));
    static_cast<void>(addCommand("LineLeft", lineLeftHandler_));
    static_cast<void>(addCommand("LineRightCommand", lineRightHandler_));
    static_cast<void>(addCommand("LineRight", lineRightHandler_));
    static_cast<void>(addCommand("PageUpCommand", pageUpHandler_));
    static_cast<void>(addCommand("PageUp", pageUpHandler_));
    static_cast<void>(addCommand("PageDownCommand", pageDownHandler_));
    static_cast<void>(addCommand("PageDown", pageDownHandler_));
    static_cast<void>(addCommand("PageLeftCommand", pageLeftHandler_));
    static_cast<void>(addCommand("PageLeft", pageLeftHandler_));
    static_cast<void>(addCommand("PageRightCommand", pageRightHandler_));
    static_cast<void>(addCommand("PageRight", pageRightHandler_));
    static_cast<void>(addCommand("ScrollToTopCommand", scrollToTopHandler_));
    static_cast<void>(addCommand("ScrollToTop", scrollToTopHandler_));
    static_cast<void>(addCommand("ScrollToBottomCommand", scrollToBottomHandler_));
    static_cast<void>(addCommand("ScrollToBottom", scrollToBottomHandler_));
    static_cast<void>(addCommand("ScrollToLeftEndCommand", scrollToLeftEndHandler_));
    static_cast<void>(addCommand("ScrollToLeftEnd", scrollToLeftEndHandler_));
    static_cast<void>(addCommand("ScrollToRightEndCommand", scrollToRightEndHandler_));
    static_cast<void>(addCommand("ScrollToRightEnd", scrollToRightEndHandler_));
    static_cast<void>(addCommand("ScrollToHorizontalOffsetCommand", scrollToHorizontalOffsetHandler_));
    static_cast<void>(addCommand("ScrollToHorizontalOffset", scrollToHorizontalOffsetHandler_));
    static_cast<void>(addCommand("ScrollToVerticalOffsetCommand", scrollToVerticalOffsetHandler_));
    static_cast<void>(addCommand("ScrollToVerticalOffset", scrollToVerticalOffsetHandler_));

    return scrollBars_.PushBack(std::move(record));
}

Base::Result<bool> ScrollBarBehavior::Detach(
    ScrollBar& scrollBar) noexcept {
    const std::uint32_t index = Find(scrollBar);
    if (index == UINT32_MAX) return false;
    if (scrollBars_[index].dragging) {
        static_cast<void>(
            input_->ReleasePointer(
                scrollBars_[index].pointerId));
    }
    static_cast<void>(scrollBar.RemoveHandler(
        UIElement::MouseDownEvent,
        mouseDownHandler_));
    static_cast<void>(scrollBar.RemoveHandler(
        UIElement::MouseMoveEvent,
        mouseMoveHandler_));
    static_cast<void>(scrollBar.RemoveHandler(
        UIElement::MouseUpEvent,
        mouseUpHandler_));
    static_cast<void>(scrollBar.RemoveHandler(
        UIElement::KeyDownEvent,
        keyDownHandler_));
    for (Input::CommandBindingHandle handle : scrollBars_[index].commands) {
        static_cast<void>(input_->RemoveCommandBinding(handle));
    }
    RemoveAt(index);
    if (scrollBars_.Empty()) {
        static_cast<void>(
            input_->RemovePointerCaptureChanged(
                captureChangedHandler_));
    }
    return true;
}

void ScrollBarBehavior::OnLineUpCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->LineDecrement());
        args.SetHandled(true);
    }
}

void ScrollBarBehavior::OnLineDownCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->LineIncrement());
        args.SetHandled(true);
    }
}

void ScrollBarBehavior::OnLineLeftCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->LineDecrement());
        args.SetHandled(true);
    }
}

void ScrollBarBehavior::OnLineRightCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->LineIncrement());
        args.SetHandled(true);
    }
}

void ScrollBarBehavior::OnPageUpCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->PageDecrement());
        args.SetHandled(true);
    }
}

void ScrollBarBehavior::OnPageDownCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->PageIncrement());
        args.SetHandled(true);
    }
}

void ScrollBarBehavior::OnPageLeftCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->PageDecrement());
        args.SetHandled(true);
    }
}

void ScrollBarBehavior::OnPageRightCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        static_cast<void>(bar->PageIncrement());
        args.SetHandled(true);
    }
}

void ScrollBarBehavior::OnScrollToTopCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        bar->SetValue(bar->GetMinimum());
        args.SetHandled(true);
    }
}

void ScrollBarBehavior::OnScrollToBottomCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        bar->SetValue(bar->GetMaximum());
        args.SetHandled(true);
    }
}

void ScrollBarBehavior::OnScrollToLeftEndCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        bar->SetValue(bar->GetMinimum());
        args.SetHandled(true);
    }
}

void ScrollBarBehavior::OnScrollToRightEndCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        bar->SetValue(bar->GetMaximum());
        args.SetHandled(true);
    }
}

void ScrollBarBehavior::OnScrollToHorizontalOffsetCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        const double value =
            args.GetParameter().Kind() == Meta::ValueKind::Double
            ? args.GetParameter().AsDouble()
            : 0.0;
        bar->SetValue(
            std::clamp(
                value,
                bar->GetMinimum(),
                bar->GetMaximum()));
        args.SetHandled(true);
    }
}

void ScrollBarBehavior::OnScrollToVerticalOffsetCommand(
    Base::Object* sender,
    ExecutedRoutedEventArgs& args) noexcept {
    auto* bar = static_cast<ScrollBar*>(sender);
    if (bar != nullptr) {
        const double value =
            args.GetParameter().Kind() == Meta::ValueKind::Double
            ? args.GetParameter().AsDouble()
            : 0.0;
        bar->SetValue(
            std::clamp(
                value,
                bar->GetMinimum(),
                bar->GetMaximum()));
        args.SetHandled(true);
    }
}

void ScrollBarBehavior::OnMouseDown(
    Base::Object* sender,
    MouseButtonEventArgs& args) noexcept {
    auto& bar = *static_cast<ScrollBar*>(sender);
    const std::uint32_t index = Find(bar);
    if (index == UINT32_MAX ||
        args.GetHandled() ||
        !bar.GetIsEnabled() ||
        args.GetChangedButton() != MouseButton::Left) {
        return;
    }
    const Point local = args.GetPosition();
    const bool horizontal =
        bar.GetOrientation() == Orientation::Horizontal;
    const double position = horizontal ? local.x : local.y;
    const double length =
        horizontal
        ? bar.GetRenderSize().width
        : bar.GetRenderSize().height;
    const double range = bar.GetMaximum() - bar.GetMinimum();
    const double normalized =
        range > 0.0
        ? (bar.GetValue() - bar.GetMinimum()) / range
        : 0.0;
    const double thumbPosition =
        std::clamp(normalized, 0.0, 1.0) *
        std::max(0.0, length - 16.0);
    const bool isThumb =
        std::fabs(position - thumbPosition) <= 16.0;

    if (isThumb) {
        scrollBars_[index].dragging = true;
        scrollBars_[index].pointerId = args.GetPointerId();
        scrollBars_[index].dragOrigin = local;
        scrollBars_[index].dragStartValue = bar.GetValue();
        static_cast<void>(
            input_->CapturePointer(
                args.GetPointerId(), bar));
        args.SetHandled(true);
    } else if (position < thumbPosition) {
        static_cast<void>(bar.PageDecrement());
        args.SetHandled(true);
    } else {
        static_cast<void>(bar.PageIncrement());
        args.SetHandled(true);
    }
}

void ScrollBarBehavior::OnMouseMove(
    Base::Object* sender,
    MouseEventArgs& args) noexcept {
    auto& bar = *static_cast<ScrollBar*>(sender);
    const std::uint32_t index = Find(bar);
    if (index == UINT32_MAX ||
        !scrollBars_[index].dragging ||
        scrollBars_[index].pointerId != args.GetPointerId()) {
        return;
    }
    const Point current = args.GetPosition();
    const Point origin = scrollBars_[index].dragOrigin;
    const double range = bar.GetMaximum() - bar.GetMinimum();
    if (range <= 0.0) return;

    if (bar.GetOrientation() == Orientation::Vertical) {
        const double trackHeight = bar.GetRenderSize().height;
        const double available = std::max(1.0, trackHeight - 16.0);
        const double deltaY = current.y - origin.y;
        const double deltaVal = deltaY * range / available;
        bar.SetValue(
            std::clamp(
                scrollBars_[index].dragStartValue + deltaVal,
                bar.GetMinimum(),
                bar.GetMaximum()));
        args.SetHandled(true);
    } else {
        const double trackWidth = bar.GetRenderSize().width;
        const double available = std::max(1.0, trackWidth - 16.0);
        const double deltaX = current.x - origin.x;
        const double deltaVal = deltaX * range / available;
        bar.SetValue(
            std::clamp(
                scrollBars_[index].dragStartValue + deltaVal,
                bar.GetMinimum(),
                bar.GetMaximum()));
        args.SetHandled(true);
    }
}

void ScrollBarBehavior::OnMouseUp(
    Base::Object* sender,
    MouseButtonEventArgs& args) noexcept {
    auto& bar = *static_cast<ScrollBar*>(sender);
    const std::uint32_t index = Find(bar);
    if (index == UINT32_MAX ||
        !scrollBars_[index].dragging ||
        args.GetChangedButton() != MouseButton::Left) {
        return;
    }
    scrollBars_[index].dragging = false;
    static_cast<void>(
        input_->ReleasePointer(
            scrollBars_[index].pointerId));
    args.SetHandled(true);
}

void ScrollBarBehavior::OnKeyDown(
    Base::Object* sender,
    KeyEventArgs& args) noexcept {
    auto& bar = *static_cast<ScrollBar*>(sender);
    if (Find(bar) == UINT32_MAX || !bar.GetIsEnabled()) {
        return;
    }
    if (args.GetKey() == KeyboardKeyHome) {
        bar.SetValue(bar.GetMinimum());
        args.SetHandled(true);
    } else if (args.GetKey() == KeyboardKeyEnd) {
        bar.SetValue(bar.GetMaximum());
        args.SetHandled(true);
    } else if (
        args.GetKey() == KeyboardKeyUp ||
        args.GetKey() == KeyboardKeyLeft) {
        static_cast<void>(bar.LineDecrement());
        args.SetHandled(true);
    } else if (
        args.GetKey() == KeyboardKeyDown ||
        args.GetKey() == KeyboardKeyRight) {
        static_cast<void>(bar.LineIncrement());
        args.SetHandled(true);
    } else if (args.GetKey() == KeyboardKeyPageUp) {
        static_cast<void>(bar.PageDecrement());
        args.SetHandled(true);
    } else if (args.GetKey() == KeyboardKeyPageDown) {
        static_cast<void>(bar.PageIncrement());
        args.SetHandled(true);
    }
}

void ScrollBarBehavior::OnCaptureChanged(
    std::uint32_t pointerId,
    UIElement* target,
    bool captured) noexcept {
    if (captured) return;
    for (ScrollBarRecord& record : scrollBars_) {
        if (!record.dragging ||
            record.pointerId != pointerId) {
            continue;
        }
        ScrollBar* bar =
            static_cast<ScrollBar*>(
                tree_->ResolveHandle(record.handle));
        if (target == nullptr ||
            target == bar) {
            record.dragging = false;
        }
    }
}

} // namespace Aero::Controls
