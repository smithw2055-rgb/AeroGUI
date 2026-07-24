#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Metadata.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Core/ObjectServices.hpp>
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
    LayoutManager layout{dispatcher};

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
        CHECK(layout.Initialize());
        return true;
    }

    bool Load(StackPanel& root, Border& child) {
        CHECK(child.SetWidth(100.0));
        CHECK(child.SetHeight(30.0));
        CHECK(tree.SetRoot(&root));
        CHECK(tree.AttachLogical(root, child));
        CHECK(tree.AttachVisual(root, child));
        CHECK(layout.Attach(root, child));
        CHECK(layout.SetRoot(&root, {100.0, 80.0}));
        CHECK(dispatcher.RunFramePhase(
            DispatcherFramePhase::Lifecycle));
        CHECK(dispatcher.RunFramePhase(
            DispatcherFramePhase::Layout));
        CHECK(root.IsLoaded() && child.IsLoaded());
        CHECK(child.IsArrangeValid());
        return true;
    }

    bool Unload(StackPanel& root, Border& child) {
        CHECK(layout.SetRoot(nullptr, {}));
        CHECK(layout.Detach(root, child));
        CHECK(tree.DetachVisual(root, child));
        CHECK(tree.DetachLogical(root, child));
        CHECK(values.DetachObject(child));
        CHECK(tree.SetRoot(nullptr));
        CHECK(values.DetachObject(root));
        return true;
    }
};

bool TestUnifiedInteractionState() {
    Fixture fixture;
    CHECK(fixture.Build());
    StackPanel root;
    Border child;
    CHECK(fixture.Load(root, child));

    CHECK(root.IsEnabled());
    CHECK(child.IsEnabled());
    CHECK(!child.IsMouseOver());
    CHECK(!child.IsPressed());
    CHECK(!child.IsKeyboardFocused());

    Result<void> denied = child.SetValue(
        UIElement::IsMouseOverProperty,
        Value::FromBoolean(BuiltinTypes::Boolean, true));
    CHECK(!denied);
    CHECK(denied.GetStatus().code == ErrorCode::ReadOnly);

    HitTestManager hitTests;
    PointerInputManager pointer(hitTests, fixture.events, root);
    CHECK(pointer.Dispatch(
        {1U, PointerAction::Move, {10.0, 10.0}}));
    CHECK(child.IsMouseOver());
    CHECK(pointer.Dispatch(
        {2U, PointerAction::Move, {10.0, 10.0}}));
    CHECK(child.IsMouseOver());
    CHECK(pointer.Dispatch(
        {1U, PointerAction::Move, {200.0, 200.0}}));
    CHECK(child.IsMouseOver());
    CHECK(pointer.Dispatch(
        {2U, PointerAction::Move, {200.0, 200.0}}));
    CHECK(!child.IsMouseOver());

    CHECK(pointer.Dispatch(
        {3U, PointerAction::Down, {10.0, 10.0}}));
    CHECK(child.IsPressed());
    CHECK(pointer.CapturePointer(3U, child));
    Result<bool> released = pointer.ReleasePointer(3U);
    CHECK(released && released.Value());
    CHECK(!child.IsPressed());

    CHECK(pointer.Dispatch(
        {4U, PointerAction::Down, {10.0, 10.0}}));
    CHECK(child.IsPressed());
    CHECK(pointer.Dispatch(
        {4U, PointerAction::Up, {200.0, 200.0}}));
    CHECK(!child.IsPressed());

    FocusManager focus(fixture.tree, fixture.events);
    Result<bool> focused = focus.SetFocus(&child);
    CHECK(focused && focused.Value());
    CHECK(child.IsKeyboardFocused());
    CHECK(focus.FocusedNode() == &child);

    CHECK(child.SetEnabled(false));
    CHECK(!child.IsEnabled());
    Result<bool> rejected = focus.SetFocus(&child);
    CHECK(!rejected);
    CHECK(rejected.GetStatus().code == ErrorCode::InvalidState);
    KeyboardInputManager keyboard(
        focus, fixture.events, fixture.tree);
    Result<KeyboardDispatchResult> disabledInput = keyboard.Dispatch(
        {KeyboardAction::Down, 65U, 0U, false});
    CHECK(disabledInput);
    CHECK(!disabledInput.Value().routed);
    CHECK(disabledInput.Value().target == nullptr);
    CHECK(focus.FocusedNode() == nullptr);
    CHECK(!child.IsKeyboardFocused());

    CHECK(child.SetEnabled(true));
    CHECK(root.SetEnabled(false));
    CHECK(!child.IsEnabled());
    CHECK(root.SetEnabled(true));
    CHECK(child.IsEnabled());

    CHECK(fixture.Unload(root, child));
    return true;
}

} // namespace

int main() {
    if (!TestUnifiedInteractionState()) return 1;
    std::puts("Aero interaction state tests passed");
    return 0;
}
