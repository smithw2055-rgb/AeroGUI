#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Metadata.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Presentation/Commands.hpp>
#include <Aero/Presentation/Input.hpp>
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

struct Fixture final {
    Dispatcher dispatcher;
    TypeRegistry types;
    MetadataBehaviorRegistrationStore typeBehaviors{types};
    MetadataRegistrationTypes typeRegistration{types, typeBehaviors};
    MetadataValueRegistrationStore valueRegistrations{types};
    DependencyPropertyRegistry properties{types, typeBehaviors};
    ObjectServicesScope presentation{
        dispatcher, properties, valueRegistrations};
    RoutedEventCatalog eventCatalog{types, typeBehaviors};
    RoutedEventManager events{eventCatalog};
    EffectiveValueEngine values{dispatcher, properties};
    ObjectTree tree{dispatcher, values};

    bool Build() {
        MetaRegistrationContext registration(
            types, typeBehaviors, valueRegistrations, properties,
            &eventCatalog);
        CHECK(Aero::Core::Detail::PopulateCoreMetadata(registration));
        CHECK(Aero::Presentation::Detail::PopulatePresentationMetadata(
            registration));
        CHECK(Aero::Controls::Detail::PopulateControlsMetadata(
            registration));
        CHECK(types.Freeze());
        CHECK(typeBehaviors.Freeze());
        CHECK(valueRegistrations.Freeze());
        CHECK(properties.Freeze());
        CHECK(eventCatalog.Freeze());
        CHECK(values.Initialize());
        CHECK(tree.Initialize());
        return true;
    }

    bool Load(StackPanel& root, Border& child) {
        CHECK(tree.SetRoot(&root));
        CHECK(tree.AttachLogical(root, child));
        CHECK(tree.AttachVisual(root, child));
        CHECK(dispatcher.RunFramePhase(
            DispatcherFramePhase::Lifecycle));
        CHECK(root.IsLoaded());
        CHECK(child.IsLoaded());
        return true;
    }

    bool Unload(StackPanel& root, Border& child) {
        CHECK(tree.DetachVisual(root, child));
        CHECK(tree.DetachLogical(root, child));
        CHECK(values.DetachObject(child));
        CHECK(tree.SetRoot(nullptr));
        CHECK(values.DetachObject(root));
        return true;
    }
};

struct CommandLog final {
    bool enabled = false;
    std::uint32_t canExecuteCount = 0U;
    std::uint32_t executedCount = 0U;
    Aero::Base::Object* sender = nullptr;
    UIElement* target = nullptr;
    bool parameter = false;
};

struct CanExecuteRecorder final {
    CommandLog* log = nullptr;
    void operator()(Aero::Base::Object* sender,
        const CanExecuteRoutedEventArgs& args) const noexcept {
        ++log->canExecuteCount;
        log->sender = sender;
        log->target = args.target;
        log->parameter = args.parameter.Kind() == ValueKind::Boolean &&
            args.parameter.AsBoolean();
        args.canExecute = log->enabled;
        args.handled = true;
    }
};

struct ExecutedRecorder final {
    CommandLog* log = nullptr;
    void operator()(Aero::Base::Object* sender,
        const ExecutedRoutedEventArgs& args) const noexcept {
        ++log->executedCount;
        log->sender = sender;
        log->target = args.target;
        log->parameter = args.parameter.Kind() == ValueKind::Boolean &&
            args.parameter.AsBoolean();
        args.handled = true;
    }
};

struct Counter final {
    std::uint32_t* value = nullptr;
    void operator()() const noexcept { ++*value; }
};

struct KeyBlocker final {
    bool* block = nullptr;
    void operator()(Aero::Base::Object*,
        const KeyEventArgs& args) const noexcept {
        if (*block) args.handled = true;
    }
};

struct SelfRemovingRecorder final {
    CommandManager* manager = nullptr;
    CommandBindingHandle handle;
    mutable std::uint32_t count = 0U;
    mutable bool removed = false;

    void operator()(Aero::Base::Object*,
        const ExecutedRoutedEventArgs& args) const noexcept {
        ++count;
        Result<bool> result = manager->RemoveBinding(handle);
        removed = result && result.Value();
        args.handled = true;
    }
};

bool TestRoutedCommandsAndInputGestures() {
    Fixture fixture;
    CHECK(fixture.Build());
    CHECK(fixture.types.FindType(ICommand::StaticTypeId()) != nullptr);
    CHECK(fixture.types.FindType(InputGesture::StaticTypeId()) != nullptr);
    CHECK(fixture.types.FindType(KeyGesture::StaticTypeId()) != nullptr);
    CHECK(fixture.types.FindType(RoutedCommand::StaticTypeId()) != nullptr);

    StackPanel root;
    Border child;
    CHECK(fixture.Load(root, child));

    Result<Ref<RoutedCommand>> madeCommand =
        MakeRef<RoutedCommand>(StringView("Save"));
    CHECK(madeCommand);
    Ref<RoutedCommand> command = std::move(madeCommand).Value();
    Result<Ref<KeyGesture>> madeGesture =
        MakeRef<KeyGesture>(83U, 2U);
    CHECK(madeGesture);
    Ref<KeyGesture> gesture = std::move(madeGesture).Value();
    CHECK(command->TryAddInputGesture(Ref<InputGesture>(gesture)));
    CHECK(command->Name() == StringView("Save"));
    CHECK(command->MatchesInput(
        {KeyboardAction::Down, 83U, 2U, false}));
    CHECK(!command->MatchesInput(
        {KeyboardAction::Up, 83U, 2U, false}));

    CommandLog log;
    CanExecuteRecorder canExecute{&log};
    ExecutedRecorder executed{&log};
    CommandManager commands(fixture.tree);
    Result<CommandBindingHandle> binding = commands.TryAddBinding(
        root, CommandBinding(
            command,
            ExecutedRoutedEventHandler(&executed),
            CanExecuteRoutedEventHandler(&canExecute)));
    CHECK(binding);

    const Value parameter =
        Value::FromBoolean(BuiltinTypes::Boolean, true);
    Result<bool> disabled =
        command->CanExecute(commands, parameter, child);
    CHECK(disabled && !disabled.Value());
    CHECK(log.canExecuteCount == 1U);
    CHECK(log.sender == &root);
    CHECK(log.target == &child);
    CHECK(log.parameter);

    log.enabled = true;
    Result<bool> allowed =
        command->CanExecute(commands, parameter, child);
    CHECK(allowed && allowed.Value());
    CHECK(command->Execute(commands, parameter, child));
    CHECK(log.executedCount == 1U);
    CHECK(log.sender == &root);
    CHECK(log.target == &child);
    CHECK(log.parameter);

    FocusManager focus(fixture.tree, fixture.events);
    CHECK(focus.SetFocus(&child));
    bool blockKey = false;
    KeyBlocker blocker{&blockKey};
    CHECK(child.KeyDown().TryAdd(KeyEventHandler(&blocker)));
    KeyboardInputManager keyboard(
        focus, fixture.events, fixture.tree, &commands);
    Result<KeyboardDispatchResult> key = keyboard.Dispatch(
        {KeyboardAction::Down, 83U, 2U, false});
    CHECK(key && key.Value().routed);
    CHECK(key.Value().commandExecuted);
    CHECK(log.executedCount == 2U);

    blockKey = true;
    Result<KeyboardDispatchResult> handled = keyboard.Dispatch(
        {KeyboardAction::Down, 83U, 2U, false});
    CHECK(handled && handled.Value().routed);
    CHECK(!handled.Value().commandExecuted);
    CHECK(log.executedCount == 2U);

    std::uint32_t requeryCount = 0U;
    std::uint32_t changedCount = 0U;
    Counter requery{&requeryCount};
    Counter changed{&changedCount};
    RequerySuggestedHandler requeryHandler(&requery);
    CanExecuteChangedHandler changedHandler(&changed);
    CHECK(commands.TryAddRequerySuggested(requeryHandler));
    CHECK(command->TryAddCanExecuteChanged(changedHandler));
    commands.InvalidateRequerySuggested();
    command->InvalidateCanExecute();
    CHECK(requeryCount == 1U);
    CHECK(changedCount == 1U);
    CHECK(commands.RemoveRequerySuggested(requeryHandler));
    CHECK(command->RemoveCanExecuteChanged(changedHandler));

    Result<bool> removed = commands.RemoveBinding(binding.Value());
    CHECK(removed && removed.Value());
    Result<bool> missing = commands.RemoveBinding(binding.Value());
    CHECK(missing && !missing.Value());
    Result<bool> noRoute =
        command->CanExecute(commands, parameter, child);
    CHECK(noRoute && !noRoute.Value());

    Result<Ref<RoutedCommand>> madeTransient =
        MakeRef<RoutedCommand>(StringView("Transient"));
    CHECK(madeTransient);
    Ref<RoutedCommand> transient =
        std::move(madeTransient).Value();
    SelfRemovingRecorder selfRemoving;
    selfRemoving.manager = &commands;
    ExecutedRoutedEventHandler selfRemovingHandler(&selfRemoving);
    Result<CommandBindingHandle> transientBinding =
        commands.TryAddBinding(child, CommandBinding(
            transient, selfRemovingHandler));
    CHECK(transientBinding);
    selfRemoving.handle = transientBinding.Value();
    Result<bool> transientExecuted =
        commands.Execute(*transient, Value::Unset(), child);
    CHECK(transientExecuted && transientExecuted.Value());
    CHECK(selfRemoving.count == 1U);
    CHECK(selfRemoving.removed);
    Result<bool> transientMissing =
        commands.CanExecute(*transient, Value::Unset(), child);
    CHECK(transientMissing && !transientMissing.Value());

    CHECK(focus.ClearFocus());
    CHECK(fixture.Unload(root, child));
    return true;
}

} // namespace

int main() {
    if (!TestRoutedCommandsAndInputGestures()) return 1;
    std::puts("Aero command tests passed");
    return 0;
}
