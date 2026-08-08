#pragma once

#include <Aero/Base/Delegate.hpp>
#include <Aero/Base/Geometry.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Gui/DependencyProperty.hpp>
#include <Aero/Gui/RoutedEvent.hpp>
#include <Aero/Value.hpp>

#include <cstdint>

namespace Aero {

struct EventArgs {
    AERO_DECLARE_TYPE(EventArgs, Meta::NoMetadataBase)
public:
    explicit constexpr EventArgs(
        Meta::TypeId type = StaticTypeId()) noexcept
        : eventArgsType_(type) {}

    constexpr Meta::TypeId GetEventArgsType() const noexcept {
        return eventArgsType_;
    }

private:
    Meta::TypeId eventArgsType_ = StaticTypeId();
};

struct RoutedEventArgs : EventArgs {
    AERO_DECLARE_TYPE(RoutedEventArgs, EventArgs)
public:
    explicit constexpr RoutedEventArgs(
        Meta::TypeId type = StaticTypeId()) noexcept
        : EventArgs(type) {}

    constexpr ::Aero::RoutedEvent GetRoutedEvent() const noexcept {
        return routedEvent_;
    }
    constexpr void SetRoutedEvent(
        ::Aero::RoutedEvent value) noexcept {
        routedEvent_ = value;
    }
    Base::Object* GetSource() const noexcept { return source_; }
    void SetSource(Base::Object* value) noexcept { source_ = value; }
    Base::Object* GetOriginalSource() const noexcept {
        return originalSource_;
    }
    void SetOriginalSource(Base::Object* value) noexcept {
        originalSource_ = value;
    }
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
    explicit constexpr InputEventArgs(
        Meta::TypeId type = StaticTypeId()) noexcept
        : RoutedEventArgs(type) {}

    constexpr std::uint32_t GetModifiers() const noexcept {
        return modifiers_;
    }
    constexpr void SetModifiers(std::uint32_t value) noexcept {
        modifiers_ = value;
    }

private:
    std::uint32_t modifiers_ = 0U;
};

struct MouseEventArgs : InputEventArgs {
    AERO_DECLARE_TYPE(MouseEventArgs, InputEventArgs)
public:
    explicit constexpr MouseEventArgs(
        Meta::TypeId type = StaticTypeId()) noexcept
        : InputEventArgs(type) {}

    constexpr std::uint32_t GetPointerId() const noexcept {
        return pointerId_;
    }
    constexpr void SetPointerId(std::uint32_t value) noexcept {
        pointerId_ = value;
    }
    constexpr Base::Point GetPosition() const noexcept {
        return position_;
    }
    constexpr void SetPosition(Base::Point value) noexcept {
        position_ = value;
    }

private:
    std::uint32_t pointerId_ = 0U;
    Base::Point position_;
};

struct MouseButtonEventArgs : MouseEventArgs {
    AERO_DECLARE_TYPE(MouseButtonEventArgs, MouseEventArgs)
public:
    constexpr MouseButtonEventArgs() noexcept
        : MouseEventArgs(StaticTypeId()) {}

    constexpr Input::MouseButton GetChangedButton() const noexcept {
        return changedButton_;
    }
    constexpr void SetChangedButton(Input::MouseButton value) noexcept {
        changedButton_ = value;
    }
    constexpr Input::MouseButtonState GetButtonState() const noexcept {
        return buttonState_;
    }
    constexpr void SetButtonState(Input::MouseButtonState value) noexcept {
        buttonState_ = value;
    }

private:
    Input::MouseButton changedButton_{};
    Input::MouseButtonState buttonState_{};
};

struct MouseWheelEventArgs : MouseEventArgs {
    AERO_DECLARE_TYPE(MouseWheelEventArgs, MouseEventArgs)
public:
    constexpr MouseWheelEventArgs() noexcept
        : MouseEventArgs(StaticTypeId()) {}

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
    DragEventArgs() noexcept
        : MouseEventArgs(StaticTypeId()) {}

    using MouseEventArgs::GetPosition;

    const Value& GetData() const noexcept { return data_; }
    void SetData(const Value& value) noexcept { data_ = value; }
    Input::DragDropEffects GetAllowedEffects() const noexcept {
        return allowedEffects_;
    }
    void SetAllowedEffects(Input::DragDropEffects value) noexcept {
        allowedEffects_ = value;
    }
    Input::DragDropEffects GetEffects() const noexcept {
        return effects_;
    }
    void SetEffects(Input::DragDropEffects value) noexcept {
        effects_ = value;
    }
    Base::Point GetPosition(const UIElement& relativeTo) const noexcept;
    void SetRootPosition(
        UIElement* root, Base::Point position) noexcept {
        root_ = root;
        rootPosition_ = position;
    }

private:
    Value data_;
    Input::DragDropEffects allowedEffects_ =
        static_cast<Input::DragDropEffects>(0U);
    Input::DragDropEffects effects_ =
        static_cast<Input::DragDropEffects>(0U);
    UIElement* root_ = nullptr;
    Base::Point rootPosition_;
};

struct GiveFeedbackEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(GiveFeedbackEventArgs, RoutedEventArgs)
public:
    GiveFeedbackEventArgs() noexcept
        : RoutedEventArgs(StaticTypeId()) {}
    Input::DragDropEffects GetEffects() const noexcept {
        return effects_;
    }
    void SetEffects(Input::DragDropEffects value) noexcept {
        effects_ = value;
    }
    bool GetUseDefaultCursors() const noexcept {
        return useDefaultCursors_;
    }
    void SetUseDefaultCursors(bool value) noexcept {
        useDefaultCursors_ = value;
    }
private:
    Input::DragDropEffects effects_ = static_cast<Input::DragDropEffects>(0U);
    bool useDefaultCursors_ = true;
};

struct DragCompletedEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(DragCompletedEventArgs, RoutedEventArgs)
public:
    DragCompletedEventArgs() noexcept
        : RoutedEventArgs(StaticTypeId()) {}
    const Value& GetData() const noexcept { return data_; }
    void SetData(const Value& value) noexcept { data_ = value; }
    Input::DragDropEffects GetEffects() const noexcept {
        return effects_;
    }
    void SetEffects(Input::DragDropEffects value) noexcept {
        effects_ = value;
    }
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
    constexpr KeyEventArgs() noexcept
        : InputEventArgs(StaticTypeId()) {}

    constexpr Input::KeyboardAction GetAction() const noexcept {
        return action_;
    }
    constexpr void SetAction(Input::KeyboardAction value) noexcept {
        action_ = value;
    }
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
    constexpr TextCompositionEventArgs() noexcept
        : InputEventArgs(StaticTypeId()) {}

    constexpr Base::StringView GetText() const noexcept { return text_; }
    constexpr void SetText(Base::StringView value) noexcept { text_ = value; }

private:
    Base::StringView text_;
};

struct KeyboardFocusChangedEventArgs : RoutedEventArgs {
    AERO_DECLARE_TYPE(KeyboardFocusChangedEventArgs, RoutedEventArgs)
public:
    constexpr KeyboardFocusChangedEventArgs() noexcept
        : RoutedEventArgs(StaticTypeId()) {}

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

} // namespace Aero
