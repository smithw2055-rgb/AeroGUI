#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Metadata.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Presentation/Metadata.hpp>

#include <cstdio>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Presentation;
using namespace Aero::Controls;

#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed %d: %s\n", \
            __LINE__, #expression); \
        return false; \
    } \
} while (false)

struct CommandLog final {
    bool canExecute = true;
    std::uint32_t executed = 0U;
};

struct ClickLog final {
    std::uint32_t count = 0U;
    void OnClick(Aero::Base::Object*, const RoutedEventArgs&) noexcept {
        ++count;
    }
};

struct ToggleEventLog final {
    std::uint32_t checked = 0U;
    std::uint32_t unchecked = 0U;
    std::uint32_t indeterminate = 0U;
    void OnChecked(
        Aero::Base::Object*,
        const RoutedEventArgs&) noexcept {
        ++checked;
    }
    void OnUnchecked(
        Aero::Base::Object*,
        const RoutedEventArgs&) noexcept {
        ++unchecked;
    }
    void OnIndeterminate(
        Aero::Base::Object*,
        const RoutedEventArgs&) noexcept {
        ++indeterminate;
    }
};

struct CommandHandlers final {
    CommandLog* log = nullptr;
    void CanExecute(
        Aero::Base::Object*,
        const CanExecuteRoutedEventArgs& args) noexcept {
        args.canExecute = log->canExecute;
        args.handled = true;
    }
    void Executed(
        Aero::Base::Object*,
        const ExecutedRoutedEventArgs& args) noexcept {
        ++log->executed;
        args.handled = true;
    }
};

bool TestButtonInputCommandAndCapture() {
    Dispatcher dispatcher;
    TypeRegistry types;
    MetadataBehaviorRegistrationStore typeBehaviors(types);
    MetadataRegistrationTypes typeRegistration(
        types, typeBehaviors);
    MetadataValueRegistrationStore valueRegistrations(types);
    DependencyPropertyRegistry properties(types, typeBehaviors);
    ObjectServicesScope services(
        dispatcher, properties, valueRegistrations);
    RoutedEventCatalog eventCatalog(types, typeBehaviors);
    RoutedEventManager events(eventCatalog);
    EffectiveValueEngine values(dispatcher, properties);
    ObjectTree tree(dispatcher, values);
    LayoutManager layout(dispatcher);
    MetaRegistrationContext registration(
        types, typeBehaviors, valueRegistrations,
        properties, &eventCatalog);
    CHECK(Aero::Core::Detail::PopulateCoreMetadata(registration));
    CHECK(Aero::Presentation::Detail::
        PopulatePresentationMetadata(registration));
    CHECK(Aero::Controls::Detail::
        PopulateControlsMetadata(registration));
    CHECK(types.Freeze());
    CHECK(typeBehaviors.Freeze());
    CHECK(valueRegistrations.Freeze());
    CHECK(properties.Freeze());
    CHECK(eventCatalog.Freeze());
    CHECK(values.Initialize());
    CHECK(tree.Initialize());
    CHECK(layout.Initialize());

    Button button;
    CHECK(button.SetWidth(100.0));
    CHECK(button.SetHeight(30.0));
    CHECK(tree.SetRoot(&button));
    CHECK(layout.SetRoot(&button, {100.0, 30.0}));
    CHECK(dispatcher.RunFramePhase(
        DispatcherFramePhase::Lifecycle));
    CHECK(dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));
    CHECK(button.IsLoaded());
    CHECK(button.IsTabStop());

    HitTestManager hitTests;
    PointerInputManager pointer(hitTests, events, button);
    FocusManager focus(tree, events);
    CommandManager commands(tree);
    ControlInteractionManager interactions(
        tree, events, pointer, focus, commands);

    Result<Ref<RoutedCommand>> commandResult =
        MakeRef<RoutedCommand>("Run");
    CHECK(commandResult);
    Ref<RoutedCommand> command =
        std::move(commandResult).Value();
    CommandLog commandLog;
    CommandHandlers commandHandlers{&commandLog};
    CommandBinding binding(
        command,
        ExecutedRoutedEventHandler(
            &commandHandlers, &CommandHandlers::Executed),
        CanExecuteRoutedEventHandler(
            &commandHandlers, &CommandHandlers::CanExecute));
    Result<CommandBindingHandle> bindingHandle =
        commands.TryAddBinding(button, binding);
    CHECK(bindingHandle);
    Ref<ICommand> commandInterface(command);
    CHECK(button.SetCommand(std::move(commandInterface)));

    ClickLog clickLog;
    RoutedEventHandler clickHandler(
        &clickLog, &ClickLog::OnClick);
    CHECK(button.Click().TryAdd(clickHandler));
    CHECK(interactions.Attach(button));

    Result<PointerDispatchResult> down = pointer.Dispatch(
        {1U, PointerAction::Down, {10.0, 10.0}});
    CHECK(down && down.Value().routed);
    CHECK(button.IsPressed());
    CHECK(button.IsKeyboardFocused());
    CHECK(pointer.CapturedNode(1U) == &button);
    Result<PointerDispatchResult> up = pointer.Dispatch(
        {1U, PointerAction::Up, {10.0, 10.0}});
    CHECK(up && up.Value().routed);
    CHECK(!button.IsPressed());
    CHECK(pointer.CapturedNode(1U) == nullptr);
    CHECK(clickLog.count == 1U);
    CHECK(commandLog.executed == 1U);

    CHECK(pointer.Dispatch(
        {1U, PointerAction::Down, {10.0, 10.0}}));
    CHECK(pointer.Dispatch(
        {1U, PointerAction::Move, {150.0, 10.0}}));
    CHECK(!button.IsMouseOver());
    CHECK(pointer.Dispatch(
        {1U, PointerAction::Up, {150.0, 10.0}}));
    CHECK(clickLog.count == 1U);

    CHECK(pointer.Dispatch(
        {1U, PointerAction::Down, {10.0, 10.0}}));
    Result<bool> released = pointer.ReleasePointer(1U);
    CHECK(released && released.Value());
    CHECK(!button.IsPressed());
    CHECK(pointer.Dispatch(
        {1U, PointerAction::Up, {10.0, 10.0}}));
    CHECK(clickLog.count == 1U);

    KeyboardInputManager keyboard(focus, events, tree);
    CHECK(keyboard.Dispatch(
        {KeyboardAction::Down, KeyboardKeySpace, 0U, false}));
    CHECK(button.IsPressed());
    CHECK(keyboard.Dispatch(
        {KeyboardAction::Up, KeyboardKeySpace, 0U, false}));
    CHECK(!button.IsPressed());
    CHECK(clickLog.count == 2U);
    CHECK(commandLog.executed == 2U);

    CHECK(button.SetClickMode(ClickMode::Press));
    CHECK(pointer.Dispatch(
        {1U, PointerAction::Down, {10.0, 10.0}}));
    CHECK(clickLog.count == 3U);
    CHECK(pointer.Dispatch(
        {1U, PointerAction::Up, {10.0, 10.0}}));
    CHECK(clickLog.count == 3U);

    commandLog.canExecute = false;
    command->InvalidateCanExecute();
    CHECK(!button.IsEnabled());
    CHECK(focus.FocusedNode() == nullptr);
    CHECK(pointer.Dispatch(
        {1U, PointerAction::Down, {10.0, 10.0}}));
    CHECK(clickLog.count == 3U);

    commandLog.canExecute = true;
    command->InvalidateCanExecute();
    CHECK(button.IsEnabled());
    CHECK(button.SetEnabled(false));
    CHECK(!button.IsEnabled());
    command->InvalidateCanExecute();
    CHECK(!button.IsEnabled());
    CHECK(button.SetEnabled(true));
    CHECK(button.IsEnabled());

    CHECK(interactions.Detach(button).Value());
    CHECK(button.Click().Remove(clickHandler));
    CHECK(commands.RemoveBinding(bindingHandle.Value()).Value());
    CHECK(layout.SetRoot(nullptr, {}));
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(button));
    return true;
}

bool TestRepeatButtonClock() {
    Dispatcher dispatcher;
    TypeRegistry types;
    MetadataBehaviorRegistrationStore typeBehaviors(types);
    MetadataRegistrationTypes typeRegistration(
        types, typeBehaviors);
    MetadataValueRegistrationStore valueRegistrations(types);
    DependencyPropertyRegistry properties(types, typeBehaviors);
    ObjectServicesScope services(
        dispatcher, properties, valueRegistrations);
    RoutedEventCatalog eventCatalog(types, typeBehaviors);
    RoutedEventManager events(eventCatalog);
    EffectiveValueEngine values(dispatcher, properties);
    ObjectTree tree(dispatcher, values);
    LayoutManager layout(dispatcher);
    MetaRegistrationContext registration(
        types, typeBehaviors, valueRegistrations,
        properties, &eventCatalog);
    CHECK(Aero::Core::Detail::PopulateCoreMetadata(registration));
    CHECK(Aero::Presentation::Detail::
        PopulatePresentationMetadata(registration));
    CHECK(Aero::Controls::Detail::
        PopulateControlsMetadata(registration));
    CHECK(types.Freeze());
    CHECK(typeBehaviors.Freeze());
    CHECK(valueRegistrations.Freeze());
    CHECK(properties.Freeze());
    CHECK(eventCatalog.Freeze());
    CHECK(values.Initialize());
    CHECK(tree.Initialize());
    CHECK(layout.Initialize());

    RepeatButton button;
    CHECK(button.GetClickMode() == ClickMode::Press);
    CHECK(button.Delay() == 400U);
    CHECK(button.Interval() == 100U);
    CHECK(button.SetWidth(100.0));
    CHECK(button.SetHeight(30.0));
    CHECK(tree.SetRoot(&button));
    CHECK(layout.SetRoot(&button, {100.0, 30.0}));
    CHECK(dispatcher.RunFramePhase(
        DispatcherFramePhase::Lifecycle));
    CHECK(dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));

    HitTestManager hitTests;
    PointerInputManager pointer(hitTests, events, button);
    FocusManager focus(tree, events);
    CommandManager commands(tree);
    ControlInteractionManager interactions(
        tree, events, pointer, focus, commands);
    ClickLog clickLog;
    RoutedEventHandler clickHandler(
        &clickLog, &ClickLog::OnClick);
    CHECK(button.Click().TryAdd(clickHandler));
    CHECK(interactions.Attach(button));

    CHECK(pointer.Dispatch(
        {1U, PointerAction::Down, {10.0, 10.0}}));
    CHECK(clickLog.count == 1U);
    Result<std::uint32_t> repeated =
        interactions.AdvanceTime(399U);
    CHECK(repeated && repeated.Value() == 0U);
    repeated = interactions.AdvanceTime(1U);
    CHECK(repeated && repeated.Value() == 1U);
    CHECK(clickLog.count == 2U);
    repeated = interactions.AdvanceTime(200U);
    CHECK(repeated && repeated.Value() == 2U);
    CHECK(clickLog.count == 4U);

    CHECK(pointer.Dispatch(
        {1U, PointerAction::Move, {150.0, 10.0}}));
    repeated = interactions.AdvanceTime(500U);
    CHECK(repeated && repeated.Value() == 0U);
    CHECK(pointer.Dispatch(
        {1U, PointerAction::Move, {10.0, 10.0}}));
    repeated = interactions.AdvanceTime(100U);
    CHECK(repeated && repeated.Value() == 1U);
    CHECK(clickLog.count == 5U);
    CHECK(pointer.Dispatch(
        {1U, PointerAction::Up, {10.0, 10.0}}));
    CHECK(clickLog.count == 5U);

    KeyboardInputManager keyboard(focus, events, tree);
    CHECK(keyboard.Dispatch(
        {KeyboardAction::Down, KeyboardKeyEnter, 0U, false}));
    CHECK(clickLog.count == 6U);
    repeated = interactions.AdvanceTime(400U);
    CHECK(repeated && repeated.Value() == 1U);
    CHECK(clickLog.count == 7U);
    CHECK(keyboard.Dispatch(
        {KeyboardAction::Up, KeyboardKeyEnter, 0U, false}));
    CHECK(clickLog.count == 7U);

    CHECK(interactions.Detach(button).Value());
    CHECK(button.Click().Remove(clickHandler));
    CHECK(layout.SetRoot(nullptr, {}));
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(button));
    return true;
}

bool TestToggleButtonsAndRadioGroups() {
    Dispatcher dispatcher;
    TypeRegistry types;
    MetadataBehaviorRegistrationStore typeBehaviors(types);
    MetadataRegistrationTypes typeRegistration(
        types, typeBehaviors);
    MetadataValueRegistrationStore valueRegistrations(types);
    DependencyPropertyRegistry properties(types, typeBehaviors);
    ObjectServicesScope services(
        dispatcher, properties, valueRegistrations);
    RoutedEventCatalog eventCatalog(types, typeBehaviors);
    RoutedEventManager events(eventCatalog);
    EffectiveValueEngine values(dispatcher, properties);
    ObjectTree tree(dispatcher, values);
    MetaRegistrationContext registration(
        types, typeBehaviors, valueRegistrations,
        properties, &eventCatalog);
    CHECK(Aero::Core::Detail::PopulateCoreMetadata(registration));
    CHECK(Aero::Presentation::Detail::
        PopulatePresentationMetadata(registration));
    CHECK(Aero::Controls::Detail::
        PopulateControlsMetadata(registration));
    CHECK(types.Freeze());
    CHECK(typeBehaviors.Freeze());
    CHECK(valueRegistrations.Freeze());
    CHECK(properties.Freeze());
    CHECK(eventCatalog.Freeze());
    CHECK(values.Initialize());
    CHECK(tree.Initialize());

    StackPanel root;
    CheckBox checkBox;
    RadioButton first;
    RadioButton second;
    RadioButton otherGroup;
    CHECK(tree.SetRoot(&root));
    CHECK(tree.AttachLogical(root, checkBox));
    CHECK(tree.AttachLogical(root, first));
    CHECK(tree.AttachLogical(root, second));
    CHECK(tree.AttachLogical(root, otherGroup));
    CHECK(dispatcher.RunFramePhase(
        DispatcherFramePhase::Lifecycle));
    CHECK(checkBox.IsLoaded());
    CHECK(!checkBox.IsChecked());
    CHECK(checkBox.GetToggleState() ==
        ToggleState::Unchecked);
    CHECK(first.SetGroupName("primary"));
    CHECK(second.SetGroupName("primary"));
    CHECK(otherGroup.SetGroupName("secondary"));
    CHECK(first.SetIsChecked(true));
    CHECK(second.SetIsChecked(true));

    HitTestManager hitTests;
    PointerInputManager pointer(hitTests, events, root);
    FocusManager focus(tree, events);
    CommandManager commands(tree);
    ControlInteractionManager interactions(
        tree, events, pointer, focus, commands);
    CHECK(interactions.Attach(checkBox));
    CHECK(interactions.Attach(first));
    CHECK(interactions.Attach(second));
    CHECK(interactions.Attach(otherGroup));
    CHECK(!first.IsChecked());
    CHECK(second.IsChecked());

    ToggleEventLog toggleLog;
    RoutedEventHandler checkedHandler(
        &toggleLog, &ToggleEventLog::OnChecked);
    RoutedEventHandler uncheckedHandler(
        &toggleLog, &ToggleEventLog::OnUnchecked);
    RoutedEventHandler indeterminateHandler(
        &toggleLog, &ToggleEventLog::OnIndeterminate);
    CHECK(checkBox.Checked().TryAdd(checkedHandler));
    CHECK(checkBox.Unchecked().TryAdd(uncheckedHandler));
    CHECK(checkBox.Indeterminate().TryAdd(
        indeterminateHandler));

    KeyboardInputManager keyboard(focus, events, tree);
    CHECK(focus.SetFocus(&checkBox).Value());
    CHECK(keyboard.Dispatch(
        {KeyboardAction::Down, KeyboardKeySpace, 0U, false}));
    CHECK(keyboard.Dispatch(
        {KeyboardAction::Up, KeyboardKeySpace, 0U, false}));
    CHECK(checkBox.GetToggleState() ==
        ToggleState::Checked);
    CHECK(toggleLog.checked == 1U);

    CHECK(checkBox.SetIsThreeState(true));
    CHECK(keyboard.Dispatch(
        {KeyboardAction::Down, KeyboardKeySpace, 0U, false}));
    CHECK(keyboard.Dispatch(
        {KeyboardAction::Up, KeyboardKeySpace, 0U, false}));
    CHECK(checkBox.GetToggleState() ==
        ToggleState::Indeterminate);
    CHECK(toggleLog.indeterminate == 1U);
    CHECK(keyboard.Dispatch(
        {KeyboardAction::Down, KeyboardKeySpace, 0U, false}));
    CHECK(keyboard.Dispatch(
        {KeyboardAction::Up, KeyboardKeySpace, 0U, false}));
    CHECK(checkBox.GetToggleState() ==
        ToggleState::Unchecked);
    CHECK(toggleLog.unchecked == 1U);

    CHECK(!checkBox.SetValue(
        ToggleButton::IsIndeterminateProperty,
        Value::FromBoolean(BuiltinTypes::Boolean, true)));
    CHECK(checkBox.SetIsChecked(true));
    CHECK(toggleLog.checked == 2U);
    CHECK(checkBox.SetIsChecked(false));
    CHECK(toggleLog.unchecked == 2U);

    CHECK(first.SetIsChecked(true));
    CHECK(first.IsChecked());
    CHECK(!second.IsChecked());
    CHECK(second.SetIsChecked(true));
    CHECK(!first.IsChecked());
    CHECK(second.IsChecked());
    CHECK(otherGroup.SetIsChecked(true));
    CHECK(second.IsChecked());
    CHECK(otherGroup.IsChecked());

    CHECK(otherGroup.SetGroupName("primary"));
    CHECK(!second.IsChecked());
    CHECK(otherGroup.IsChecked());
    CHECK(interactions.Detach(otherGroup).Value());
    CHECK(tree.DetachLogical(root, otherGroup));
    CHECK(dispatcher.RunFramePhase(
        DispatcherFramePhase::Lifecycle));
    CHECK(second.SetIsChecked(true));
    CHECK(second.IsChecked());

    CHECK(checkBox.Checked().Remove(checkedHandler));
    CHECK(checkBox.Unchecked().Remove(uncheckedHandler));
    CHECK(checkBox.Indeterminate().Remove(
        indeterminateHandler));
    CHECK(interactions.Detach(second).Value());
    CHECK(interactions.Detach(first).Value());
    CHECK(interactions.Detach(checkBox).Value());
    CHECK(tree.DetachLogical(root, second));
    CHECK(tree.DetachLogical(root, first));
    CHECK(tree.DetachLogical(root, checkBox));
    CHECK(tree.SetRoot(nullptr));
    CHECK(values.DetachObject(otherGroup));
    CHECK(values.DetachObject(second));
    CHECK(values.DetachObject(first));
    CHECK(values.DetachObject(checkBox));
    CHECK(values.DetachObject(root));
    return true;
}

} // namespace

int main() {
    if (!TestButtonInputCommandAndCapture()) return 1;
    if (!TestRepeatButtonClock()) return 1;
    if (!TestToggleButtonsAndRadioGroups()) return 1;
    std::puts("Aero button tests passed");
    return 0;
}
