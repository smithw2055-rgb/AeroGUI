#include <Aero/Controls/Buttons.hpp>
#include <Aero/Controls/Metadata.hpp>
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

} // namespace

int main() {
    if (!TestButtonInputCommandAndCapture()) return 1;
    std::puts("Aero button tests passed");
    return 0;
}
