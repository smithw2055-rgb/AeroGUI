#pragma once

#include <Aero/Controls/Templates.hpp>
#include <Aero/Type.hpp>
#include <Aero/Presentation/Commands.hpp>
#include <Aero/Presentation/Input.hpp>

namespace Aero::Controls {

enum class ClickMode : std::uint8_t {
    Release = 0U,
    Press,
    Hover,
};

enum class ToggleState : std::uint8_t {
    Unchecked = 0U,
    Checked,
    Indeterminate,
};

class ControlInteractionManager;

class AERO_API ButtonBase : public ContentControl {
    AERO_DECLARE_TYPE(ButtonBase, ContentControl)
public:
    inline static constexpr auto ClickEvent =
        Members::RoutedEvent<RoutedEventArgs>{"Click"};
    UIElement::RoutedEvent_<RoutedEventHandler> Click() noexcept {
        return {*this, ClickEvent};
    }

    ClickMode GetClickMode() const noexcept;
    ICommand* Command() const noexcept;
    Base::Ref<Base::Object> CommandParameter() const noexcept;
    UIElement* CommandTarget() const noexcept;
    bool IsCommandEnabled() const noexcept {
        return commandEnabled_;
    }

    Base::Result<void> SetClickMode(ClickMode value) noexcept;
    Base::Result<void> SetCommand(
        Base::Ref<ICommand> command) noexcept;
    Base::Result<void> SetCommandParameter(
        Base::Ref<Base::Object> parameter) noexcept;
    Base::Result<void> SetCommandTarget(
        Base::Ref<UIElement> target) noexcept;

    inline static constexpr auto ClickModeProperty =
        Members::Property<ClickMode>{"ClickMode"};
    inline static constexpr auto CommandProperty =
        Members::Property<ICommand>{"Command"};
    inline static constexpr auto CommandParameterProperty =
        Members::Property<Base::Object>{"CommandParameter"};
    inline static constexpr auto CommandTargetProperty =
        Members::Property<UIElement>{"CommandTarget"};

protected:
    explicit ButtonBase(TypeId runtimeType) noexcept
        : ContentControl(runtimeType) {}
    ~ButtonBase() override = default;

private:
    friend class ControlInteractionManager;
    bool commandEnabled_ = true;
};

class AERO_API Button final : public ButtonBase {
    AERO_DECLARE_TYPE(Button, ButtonBase)
public:
    Button() noexcept : ButtonBase(StaticTypeId()) {}
    ~Button() override = default;
};

class AERO_API RepeatButton final : public ButtonBase {
    AERO_DECLARE_TYPE(RepeatButton, ButtonBase)
public:
    RepeatButton() noexcept : ButtonBase(StaticTypeId()) {}
    ~RepeatButton() override = default;

    std::uint32_t Delay() const noexcept;
    std::uint32_t Interval() const noexcept;
    Base::Result<void> SetDelay(std::uint32_t value) noexcept;
    Base::Result<void> SetInterval(std::uint32_t value) noexcept;

    inline static constexpr auto DelayProperty =
        Members::Property<std::uint32_t>{"Delay"};
    inline static constexpr auto IntervalProperty =
        Members::Property<std::uint32_t>{"Interval"};
};

class AERO_API ToggleButton : public ButtonBase {
    AERO_DECLARE_TYPE(ToggleButton, ButtonBase)
public:
    ToggleButton() noexcept : ButtonBase(StaticTypeId()) {}
    ~ToggleButton() override = default;

    bool IsChecked() const noexcept;
    bool IsThreeState() const noexcept;
    bool IsIndeterminate() const noexcept;
    ToggleState GetToggleState() const noexcept;
    Base::Result<void> SetIsChecked(bool value) noexcept;
    Base::Result<void> SetIsThreeState(bool value) noexcept;

    inline static constexpr auto CheckedEvent =
        Members::RoutedEvent<RoutedEventArgs>{"Checked"};
    inline static constexpr auto UncheckedEvent =
        Members::RoutedEvent<RoutedEventArgs>{"Unchecked"};
    inline static constexpr auto IndeterminateEvent =
        Members::RoutedEvent<RoutedEventArgs>{"Indeterminate"};
    UIElement::RoutedEvent_<RoutedEventHandler> Checked() noexcept {
        return {*this, CheckedEvent};
    }
    UIElement::RoutedEvent_<RoutedEventHandler> Unchecked() noexcept {
        return {*this, UncheckedEvent};
    }
    UIElement::RoutedEvent_<RoutedEventHandler> Indeterminate() noexcept {
        return {*this, IndeterminateEvent};
    }

    inline static constexpr auto IsCheckedProperty =
        Members::Property<bool>{"IsChecked"};
    inline static constexpr auto IsThreeStateProperty =
        Members::Property<bool>{"IsThreeState"};
    inline static constexpr auto IsIndeterminateProperty =
        Members::Property<bool>{"IsIndeterminate"};

protected:
    explicit ToggleButton(TypeId runtimeType) noexcept
        : ButtonBase(runtimeType) {}

private:
    friend class ControlInteractionManager;
    Base::Result<void> SetToggleState(
        ToggleState value) noexcept;
};

class AERO_API CheckBox final : public ToggleButton {
    AERO_DECLARE_TYPE(CheckBox, ToggleButton)
public:
    CheckBox() noexcept : ToggleButton(StaticTypeId()) {}
    ~CheckBox() override = default;
};

class AERO_API RadioButton final : public ToggleButton {
    AERO_DECLARE_TYPE(RadioButton, ToggleButton)
public:
    RadioButton() noexcept : ToggleButton(StaticTypeId()) {}
    ~RadioButton() override = default;

    Base::StringView GroupName() const noexcept;
    Base::Result<void> SetGroupName(
        Base::StringView value) noexcept;

    inline static constexpr auto GroupNameProperty =
        Members::Property<Base::String>{"GroupName"};
};

class AERO_API ControlInteractionManager final {
public:
    ControlInteractionManager(
        ObjectTree& tree,
        RoutedEventManager& events,
        PointerInputManager& pointer,
        FocusManager& focus,
        CommandManager& commands,
        VisualStateManager* states = nullptr) noexcept;
    ~ControlInteractionManager() noexcept;

    Base::Result<void> Initialize() noexcept;
    Base::Result<void> Attach(ButtonBase& button) noexcept;
    Base::Result<bool> Detach(ButtonBase& button) noexcept;
    Base::Result<void> RefreshCanExecute(
        ButtonBase& button) noexcept;
    // Host-driven deterministic clock for RepeatButton. A single call emits
    // at most 1024 repeats and skips excess backlog.
    Base::Result<std::uint32_t> AdvanceTime(
        std::uint32_t elapsedMilliseconds) noexcept;

private:
    struct ButtonRecord final {
        VisualHandle handle;
        Base::Ref<ICommand> command;
        std::uint32_t pointerId = 0U;
        bool pointerDown = false;
        bool keyboardDown = false;
        bool wasMouseOver = false;
        std::uint64_t repeatElapsed = 0U;
        std::uint64_t nextRepeat = 0U;
        ToggleState toggleState = ToggleState::Unchecked;
        bool updatingToggle = false;
    };

    ObjectTree* tree_ = nullptr;
    RoutedEventManager* events_ = nullptr;
    PointerInputManager* pointer_ = nullptr;
    FocusManager* focus_ = nullptr;
    CommandManager* commands_ = nullptr;
    VisualStateManager* states_ = nullptr;
    Base::Vector<ButtonRecord> buttons_;
    MouseButtonEventHandler mouseDownHandler_;
    MouseButtonEventHandler mouseUpHandler_;
    KeyEventHandler keyDownHandler_;
    KeyEventHandler keyUpHandler_;
    KeyboardFocusChangedEventHandler focusChangedHandler_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    PointerStateChangedHandler pointerStateChangedHandler_;
    PointerCaptureChangedHandler captureChangedHandler_;
    RequerySuggestedHandler requeryHandler_;
    bool initialized_ = false;

    std::uint32_t FindButton(const ButtonBase& button) const noexcept;
    ButtonBase* ResolveButton(std::uint32_t index) noexcept;
    Base::Result<void> SubscribeCommand(
        ButtonBase& button,
        ButtonRecord& record) noexcept;
    void UnsubscribeCommand(ButtonRecord& record) noexcept;
    void RemoveAt(std::uint32_t index) noexcept;
    Base::Result<void> InvokeClick(ButtonBase& button) noexcept;
    Base::Result<void> ApplyToggleState(
        ToggleButton& button,
        ToggleState state) noexcept;
    void PublishToggleState(
        ToggleButton& button,
        ButtonRecord& record) noexcept;
    void UncheckRadioPeers(RadioButton& button) noexcept;
    void SyncVisualState(ButtonBase& button) noexcept;
    void OnMouseDown(
        Base::Object* sender,
        const MouseButtonEventArgs& args) noexcept;
    void OnMouseUp(
        Base::Object* sender,
        const MouseButtonEventArgs& args) noexcept;
    void OnKeyDown(
        Base::Object* sender,
        const KeyEventArgs& args) noexcept;
    void OnKeyUp(
        Base::Object* sender,
        const KeyEventArgs& args) noexcept;
    void OnFocusChanged(
        Base::Object* sender,
        const KeyboardFocusChangedEventArgs& args) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
    void OnPointerStateChanged(UIElement& element) noexcept;
    void OnCaptureChanged(
        std::uint32_t pointerId,
        UIElement* target,
        bool captured) noexcept;
    void OnRequerySuggested() noexcept;
};

} // namespace Aero::Controls

namespace Aero::Core {

template<>
struct MetaTypeTraits<Controls::ClickMode> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId("ClickMode");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return AeroNamespaceUri();
    }
    static constexpr Base::StringView Name() noexcept {
        return "ClickMode";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core
