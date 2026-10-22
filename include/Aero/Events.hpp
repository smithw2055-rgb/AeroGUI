#pragma once

#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/RoutedEvent.hpp>
#include <Aero/Value.hpp>

#include <cstdint>
#include <utility>

namespace Aero {

class UIElement;

namespace Input {
class RoutedCommand;
enum class DragDropEffects : std::uint8_t;
enum class KeyboardAction : std::uint8_t;
}

// -----------------------------------------------------------------------------
// Generic Event Connector
// -----------------------------------------------------------------------------

template<class TSource, class TArgs>
class Event {
public:
    using Handler = Base::Delegate<void(Base::Object*, TArgs&)>;

    Event(TSource& source, RoutedEventHandle event) noexcept : source_(&source), event_(event) {}

    void Add(const Handler& handler, bool handledEventsToo = false) noexcept {
        source_->AddHandler(event_, handler, handledEventsToo);
    }

    void operator+=(const Handler& handler) noexcept { Add(handler); }

    bool Remove(const Handler& handler) noexcept {
        return source_->RemoveHandler(event_, handler);
    }

    void operator-=(const Handler& handler) noexcept {
        static_cast<void>(Remove(handler));
    }

private:
    TSource* source_ = nullptr;
    RoutedEventHandle event_;
};

// -----------------------------------------------------------------------------
// Core Event Arguments Hierarchy
// -----------------------------------------------------------------------------

struct EventArgs {
    AERO_DECLARE_TYPE(EventArgs, Meta::NoMetadataBase)

public:
    explicit constexpr EventArgs(Meta::TypeId type = StaticTypeId()) noexcept : eventArgsType_(type) {}

    constexpr Meta::TypeId GetEventArgsType() const noexcept { return eventArgsType_; }

private:
    Meta::TypeId eventArgsType_ = StaticTypeId();
};

struct RoutedEventArgs : EventArgs {
    AERO_DECLARE_TYPE(RoutedEventArgs, EventArgs)

public:
    explicit constexpr RoutedEventArgs(Meta::TypeId type = StaticTypeId()) noexcept : EventArgs(type) {}

    constexpr ::Aero::RoutedEvent GetRoutedEvent() const noexcept { return routedEvent_; }
    constexpr void SetRoutedEvent(::Aero::RoutedEvent value) noexcept { routedEvent_ = value; }
    Base::Object* GetSource() const noexcept { return source_; }
    void SetSource(Base::Object* value) noexcept { source_ = value; }
    Base::Object* GetOriginalSource() const noexcept { return originalSource_; }
    void SetOriginalSource(Base::Object* value) noexcept { originalSource_ = value; }
    constexpr bool GetHandled() const noexcept { return handled_; }
    constexpr void SetHandled(bool value) noexcept { handled_ = value; }

private:
    ::Aero::RoutedEvent routedEvent_{};
    Base::Object* source_ = nullptr;
    Base::Object* originalSource_ = nullptr;
    bool handled_ = false;
};

struct InputEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(InputEventArgs, RoutedEventArgs)

public:
    explicit constexpr InputEventArgs(Meta::TypeId type = StaticTypeId()) noexcept : RoutedEventArgs(type) {}

    constexpr std::uint32_t GetModifiers() const noexcept { return modifiers_; }
    constexpr void SetModifiers(std::uint32_t value) noexcept { modifiers_ = value; }

private:
    std::uint32_t modifiers_ = 0U;
};

struct MouseEventArgs : InputEventArgs {
    AERO_DECLARE_TYPE(MouseEventArgs, InputEventArgs)

public:
    explicit constexpr MouseEventArgs(Meta::TypeId type = StaticTypeId()) noexcept : InputEventArgs(type) {}

    constexpr std::uint32_t GetPointerId() const noexcept { return pointerId_; }
    constexpr void SetPointerId(std::uint32_t value) noexcept { pointerId_ = value; }
    constexpr Base::Point GetPosition() const noexcept { return position_; }
    constexpr void SetPosition(Base::Point value) noexcept { position_ = value; }

private:
    std::uint32_t pointerId_ = 0U;
    Base::Point position_;
};

struct MouseButtonEventArgs : MouseEventArgs {
    AERO_DECLARE_TYPE(MouseButtonEventArgs, MouseEventArgs)

public:
    constexpr MouseButtonEventArgs() noexcept : MouseEventArgs(StaticTypeId()) {}

    constexpr Input::MouseButton GetChangedButton() const noexcept { return changedButton_; }
    constexpr void SetChangedButton(Input::MouseButton value) noexcept { changedButton_ = value; }
    constexpr Input::MouseButtonState GetButtonState() const noexcept { return buttonState_; }
    constexpr void SetButtonState(Input::MouseButtonState value) noexcept { buttonState_ = value; }
    constexpr std::uint32_t GetClickCount() const noexcept { return clickCount_; }
    constexpr void SetClickCount(std::uint32_t value) noexcept { clickCount_ = value; }

private:
    Input::MouseButton changedButton_{};
    Input::MouseButtonState buttonState_{};
    std::uint32_t clickCount_ = 1U;
};

struct MouseWheelEventArgs : MouseEventArgs {
    AERO_DECLARE_TYPE(MouseWheelEventArgs, MouseEventArgs)

public:
    constexpr MouseWheelEventArgs() noexcept : MouseEventArgs(StaticTypeId()) {}

    constexpr double GetDeltaX() const noexcept { return deltaX_; }
    constexpr void SetDeltaX(double value) noexcept { deltaX_ = value; }
    constexpr double GetDeltaY() const noexcept { return deltaY_; }
    constexpr void SetDeltaY(double value) noexcept { deltaY_ = value; }

private:
    double deltaX_ = 0.0;
    double deltaY_ = 0.0;
};

struct DragEventArgs : MouseEventArgs {
    AERO_DECLARE_TYPE(DragEventArgs, MouseEventArgs)

public:
    DragEventArgs() noexcept : MouseEventArgs(StaticTypeId()) {}

    using MouseEventArgs::GetPosition;

    const Value& GetData() const noexcept { return data_; }
    void SetData(const Value& value) noexcept { data_ = value; }
    Input::DragDropEffects GetAllowedEffects() const noexcept { return allowedEffects_; }
    void SetAllowedEffects(Input::DragDropEffects value) noexcept { allowedEffects_ = value; }
    Input::DragDropEffects GetEffects() const noexcept { return effects_; }
    void SetEffects(Input::DragDropEffects value) noexcept { effects_ = value; }
    Base::Point GetPosition(const UIElement& relativeTo) const noexcept;
    void SetRootPosition(UIElement* root, Base::Point position) noexcept {
        root_ = root;
        rootPosition_ = position;
    }

private:
    Value data_;
    Input::DragDropEffects allowedEffects_ = static_cast<Input::DragDropEffects>(0U);
    Input::DragDropEffects effects_ = static_cast<Input::DragDropEffects>(0U);
    UIElement* root_ = nullptr;
    Base::Point rootPosition_;
};

struct GiveFeedbackEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(GiveFeedbackEventArgs, RoutedEventArgs)

public:
    GiveFeedbackEventArgs() noexcept : RoutedEventArgs(StaticTypeId()) {}
    Input::DragDropEffects GetEffects() const noexcept { return effects_; }
    void SetEffects(Input::DragDropEffects value) noexcept { effects_ = value; }
    bool GetUseDefaultCursors() const noexcept { return useDefaultCursors_; }
    void SetUseDefaultCursors(bool value) noexcept { useDefaultCursors_ = value; }

private:
    Input::DragDropEffects effects_ = static_cast<Input::DragDropEffects>(0U);
    bool useDefaultCursors_ = true;
};

struct DragCompletedEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(DragCompletedEventArgs, RoutedEventArgs)

public:
    DragCompletedEventArgs() noexcept : RoutedEventArgs(StaticTypeId()) {}
    const Value& GetData() const noexcept { return data_; }
    void SetData(const Value& value) noexcept { data_ = value; }
    Input::DragDropEffects GetEffects() const noexcept { return effects_; }
    void SetEffects(Input::DragDropEffects value) noexcept { effects_ = value; }
    bool GetCanceled() const noexcept { return canceled_; }
    void SetCanceled(bool value) noexcept { canceled_ = value; }

private:
    Value data_;
    Input::DragDropEffects effects_ = static_cast<Input::DragDropEffects>(0U);
    bool canceled_ = false;
};

struct KeyEventArgs : InputEventArgs {
    AERO_DECLARE_TYPE(KeyEventArgs, InputEventArgs)

public:
    constexpr KeyEventArgs() noexcept : InputEventArgs(StaticTypeId()) {}

    constexpr Input::KeyboardAction GetAction() const noexcept { return action_; }
    constexpr void SetAction(Input::KeyboardAction value) noexcept { action_ = value; }
    constexpr std::uint32_t GetKey() const noexcept { return key_; }
    constexpr void SetKey(std::uint32_t value) noexcept { key_ = value; }
    constexpr bool GetIsRepeat() const noexcept { return isRepeat_; }
    constexpr void SetIsRepeat(bool value) noexcept { isRepeat_ = value; }

private:
    Input::KeyboardAction action_{};
    std::uint32_t key_ = 0U;
    bool isRepeat_ = false;
};

struct TextCompositionEventArgs : InputEventArgs {
    AERO_DECLARE_TYPE(TextCompositionEventArgs, InputEventArgs)

public:
    constexpr TextCompositionEventArgs() noexcept : InputEventArgs(StaticTypeId()) {}

    constexpr StringView GetText() const noexcept { return text_; }
    constexpr void SetText(StringView value) noexcept { text_ = value; }

private:
    StringView text_;
};

struct KeyboardFocusChangedEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(KeyboardFocusChangedEventArgs, RoutedEventArgs)

public:
    constexpr KeyboardFocusChangedEventArgs() noexcept : RoutedEventArgs(StaticTypeId()) {}

    UIElement* GetOldFocus() const noexcept { return oldFocus_; }
    void SetOldFocus(UIElement* value) noexcept { oldFocus_ = value; }
    UIElement* GetNewFocus() const noexcept { return newFocus_; }
    void SetNewFocus(UIElement* value) noexcept { newFocus_ = value; }

private:
    UIElement* oldFocus_ = nullptr;
    UIElement* newFocus_ = nullptr;
};

using EventHandler = Base::Delegate<void(Base::Object*, EventArgs&)>;
using RoutedEventHandler = Base::Delegate<void(Base::Object*, RoutedEventArgs&)>;
using MouseEventHandler = Base::Delegate<void(Base::Object*, MouseEventArgs&)>;
using MouseButtonEventHandler = Base::Delegate<void(Base::Object*, MouseButtonEventArgs&)>;
using MouseWheelEventHandler = Base::Delegate<void(Base::Object*, MouseWheelEventArgs&)>;
using DragEventHandler = Base::Delegate<void(Base::Object*, DragEventArgs&)>;
using GiveFeedbackEventHandler = Base::Delegate<void(Base::Object*, GiveFeedbackEventArgs&)>;
using DragCompletedEventHandler = Base::Delegate<void(Base::Object*, DragCompletedEventArgs&)>;
using KeyEventHandler = Base::Delegate<void(Base::Object*, KeyEventArgs&)>;
using TextCompositionEventHandler = Base::Delegate<void(Base::Object*, TextCompositionEventArgs&)>;
using KeyboardFocusChangedEventHandler = Base::Delegate<void(Base::Object*, KeyboardFocusChangedEventArgs&)>;

// -----------------------------------------------------------------------------
// Application Lifetime Events
// -----------------------------------------------------------------------------

struct StartupEventArgs : EventArgs {
    AERO_DECLARE_TYPE(StartupEventArgs, EventArgs)

public:
    StartupEventArgs() noexcept : EventArgs(StaticTypeId()) {}
    explicit StartupEventArgs(StringView startupUri) noexcept : EventArgs(StaticTypeId()), startupUri_(startupUri) {}

    StringView GetStartupUri() const noexcept { return startupUri_; }

private:
    StringView startupUri_;
};

struct ExitEventArgs : EventArgs {
    AERO_DECLARE_TYPE(ExitEventArgs, EventArgs)

public:
    ExitEventArgs() noexcept : EventArgs(StaticTypeId()) {}
    explicit ExitEventArgs(int applicationExitCode) noexcept : EventArgs(StaticTypeId()),
          applicationExitCode_(applicationExitCode) {}

    int GetApplicationExitCode() const noexcept { return applicationExitCode_; }

private:
    int applicationExitCode_ = 0;
};

// -----------------------------------------------------------------------------
// Window Lifecycle Events
// -----------------------------------------------------------------------------

struct CancelEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(CancelEventArgs, RoutedEventArgs)

public:
    CancelEventArgs() noexcept : RoutedEventArgs(StaticTypeId()) {}

    bool GetCancel() const noexcept { return cancel_; }
    void SetCancel(bool value) noexcept { cancel_ = value; }

private:
    bool cancel_ = false;
};

using CancelEventHandler = Base::Delegate<void(Base::Object*, CancelEventArgs&)>;

// -----------------------------------------------------------------------------
// Command Routing Events
// -----------------------------------------------------------------------------

struct CanExecuteRoutedEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(CanExecuteRoutedEventArgs, RoutedEventArgs)

public:
    CanExecuteRoutedEventArgs() noexcept : RoutedEventArgs(StaticTypeId()) {}

    Input::RoutedCommand* GetCommand() const noexcept { return command_; }
    void SetCommand(Input::RoutedCommand* value) noexcept { command_ = value; }
    const Value& GetParameter() const noexcept { return parameter_; }
    void SetParameter(Value value) noexcept { parameter_ = std::move(value); }
    UIElement* GetTarget() const noexcept { return target_; }
    void SetTarget(UIElement* value) noexcept { target_ = value; }
    bool GetCanExecute() const noexcept { return canExecute_; }
    void SetCanExecute(bool value) noexcept { canExecute_ = value; }
    bool GetContinueRouting() const noexcept { return continueRouting_; }
    void SetContinueRouting(bool value) noexcept { continueRouting_ = value; }

private:
    Input::RoutedCommand* command_ = nullptr;
    Value parameter_;
    UIElement* target_ = nullptr;
    bool canExecute_ = false;
    bool continueRouting_ = true;
};

struct ExecutedRoutedEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(ExecutedRoutedEventArgs, RoutedEventArgs)

public:
    ExecutedRoutedEventArgs() noexcept : RoutedEventArgs(StaticTypeId()) {}

    Input::RoutedCommand* GetCommand() const noexcept { return command_; }
    void SetCommand(Input::RoutedCommand* value) noexcept { command_ = value; }
    const Value& GetParameter() const noexcept { return parameter_; }
    void SetParameter(Value value) noexcept { parameter_ = std::move(value); }
    UIElement* GetTarget() const noexcept { return target_; }
    void SetTarget(UIElement* value) noexcept { target_ = value; }
    bool GetContinueRouting() const noexcept { return continueRouting_; }
    void SetContinueRouting(bool value) noexcept { continueRouting_ = value; }

private:
    Input::RoutedCommand* command_ = nullptr;
    Value parameter_;
    UIElement* target_ = nullptr;
    bool continueRouting_ = true;
};

using CanExecuteRoutedEventHandler = Base::Delegate<void(Base::Object*, CanExecuteRoutedEventArgs&)>;
using ExecutedRoutedEventHandler = Base::Delegate<void(Base::Object*, ExecutedRoutedEventArgs&)>;

} // namespace Aero

// -----------------------------------------------------------------------------
// Subsystem Events: Collections
// -----------------------------------------------------------------------------

namespace Aero::Collections {

enum class ItemsChangeAction : std::uint8_t {
    Add = 0U,
    Remove,
    Replace,
    Move,
    Reset,
};

struct ItemsChangedEvent {
    ItemsChangeAction action{};
    std::uint32_t oldIndex = UINT32_MAX;
    std::uint32_t newIndex = UINT32_MAX;
    std::uint32_t oldCount = 0U;
    std::uint32_t newCount = 0U;
};

using ItemsChangedHandler = Base::Delegate<void(const ItemsChangedEvent&)>;

} // namespace Aero::Collections

// -----------------------------------------------------------------------------
// Subsystem Events: Controls
// -----------------------------------------------------------------------------

namespace Aero::Controls {

enum class ScrollInputKind : std::uint8_t {
    Line = 0U,
    Page,
    Wheel,
    Thumb,
    Touch,
};

struct ScrollData {
    double horizontalOffset = 0.0;
    double verticalOffset = 0.0;
    double extentWidth = 0.0;
    double extentHeight = 0.0;
    double viewportWidth = 0.0;
    double viewportHeight = 0.0;
};

struct ScrollChangedEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(ScrollChangedEventArgs, RoutedEventArgs)

public:
    ScrollChangedEventArgs() noexcept : RoutedEventArgs(StaticTypeId()) {}
    ScrollChangedEventArgs(ScrollData oldData, ScrollData newData, ScrollInputKind inputKind) noexcept
        : RoutedEventArgs(StaticTypeId()),
          oldData_(oldData), newData_(newData), inputKind_(inputKind) {}

    ScrollData GetOldData() const noexcept { return oldData_; }
    ScrollData GetNewData() const noexcept { return newData_; }
    ScrollInputKind GetInputKind() const noexcept { return inputKind_; }

private:
    ScrollData oldData_;
    ScrollData newData_;
    ScrollInputKind inputKind_ = ScrollInputKind::Line;
};

using ScrollChangedEventHandler = Base::Delegate<void(Base::Object*, ScrollChangedEventArgs&)>;

struct RangeValueChangedEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(RangeValueChangedEventArgs, RoutedEventArgs)

public:
    RangeValueChangedEventArgs() noexcept : RoutedEventArgs(StaticTypeId()) {}
    RangeValueChangedEventArgs(double oldValue, double newValue) noexcept : RoutedEventArgs(StaticTypeId()),
          oldValue_(oldValue), newValue_(newValue) {}

    double GetOldValue() const noexcept { return oldValue_; }
    double GetNewValue() const noexcept { return newValue_; }

private:
    double oldValue_ = 0.0;
    double newValue_ = 0.0;
};

using RangeValueChangedEventHandler = Base::Delegate<void(Base::Object*, RangeValueChangedEventArgs&)>;

namespace Primitives { class Selector; }

struct SelectionChangedEvent {
    Span<const std::uint32_t> removedIndices;
    Span<const std::uint32_t> addedIndices;
    std::uint32_t oldPrimaryIndex = UINT32_MAX;
    std::uint32_t newPrimaryIndex = UINT32_MAX;
    Ref<Base::Object> oldPrimaryItem;
    Ref<Base::Object> newPrimaryItem;
};

using SelectionChangedHandler = Base::Delegate<void(Primitives::Selector&, const SelectionChangedEvent&)>;

} // namespace Aero::Controls

// -----------------------------------------------------------------------------
// Subsystem Events: Documents
// -----------------------------------------------------------------------------

namespace Aero::Documents {

class Hyperlink;

struct RequestNavigateEventArgs : Aero::RoutedEventArgs {
    AERO_DECLARE_TYPE(RequestNavigateEventArgs, Aero::RoutedEventArgs)

public:
    RequestNavigateEventArgs() noexcept : Aero::RoutedEventArgs(StaticTypeId()) {}
    RequestNavigateEventArgs(StringView uri, Hyperlink* hyperlink) noexcept : Aero::RoutedEventArgs(StaticTypeId()),
          uri_(uri), hyperlink_(hyperlink) {}

    StringView GetUri() const noexcept { return uri_; }
    Hyperlink* GetHyperlink() const noexcept { return hyperlink_; }

private:
    StringView uri_;
    Hyperlink* hyperlink_ = nullptr;
};

using RequestNavigateEventHandler = Base::Delegate<void(Base::Object*, RequestNavigateEventArgs&)>;

} // namespace Aero::Documents
