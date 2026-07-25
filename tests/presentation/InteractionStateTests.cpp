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

bool TestKeyboardNavigationAndFocusScopes() {
    Fixture fixture;
    CHECK(fixture.Build());
    StackPanel root;
    Border first;
    Border second;
    Border disabled;
    StackPanel scope;
    Border scopeFirst;
    Border scopeSecond;

    CHECK(first.SetWidth(100.0));
    CHECK(first.SetHeight(10.0));
    CHECK(second.SetWidth(100.0));
    CHECK(second.SetHeight(10.0));
    CHECK(disabled.SetWidth(100.0));
    CHECK(disabled.SetHeight(10.0));
    CHECK(scopeFirst.SetWidth(100.0));
    CHECK(scopeFirst.SetHeight(10.0));
    CHECK(scopeSecond.SetWidth(100.0));
    CHECK(scopeSecond.SetHeight(10.0));
    CHECK(first.SetTabStop(true));
    CHECK(first.SetTabIndex(2U));
    CHECK(second.SetTabStop(true));
    CHECK(second.SetTabIndex(0U));
    CHECK(disabled.SetTabStop(true));
    CHECK(disabled.SetTabIndex(1U));
    CHECK(disabled.SetEnabled(false));
    CHECK(scope.SetFocusScope(true));
    CHECK(scopeFirst.SetTabStop(true));
    CHECK(scopeFirst.SetTabIndex(10U));
    CHECK(scopeSecond.SetTabStop(true));
    CHECK(scopeSecond.SetTabIndex(11U));

    CHECK(fixture.tree.SetRoot(&root));
    CHECK(fixture.tree.AttachLogical(root, first));
    CHECK(fixture.tree.AttachVisual(root, first));
    CHECK(fixture.layout.Attach(root, first));
    CHECK(fixture.tree.AttachLogical(root, second));
    CHECK(fixture.tree.AttachVisual(root, second));
    CHECK(fixture.layout.Attach(root, second));
    CHECK(fixture.tree.AttachLogical(root, disabled));
    CHECK(fixture.tree.AttachVisual(root, disabled));
    CHECK(fixture.layout.Attach(root, disabled));
    CHECK(fixture.tree.AttachLogical(root, scope));
    CHECK(fixture.tree.AttachVisual(root, scope));
    CHECK(fixture.layout.Attach(root, scope));
    CHECK(fixture.tree.AttachLogical(scope, scopeFirst));
    CHECK(fixture.tree.AttachVisual(scope, scopeFirst));
    CHECK(fixture.layout.Attach(scope, scopeFirst));
    CHECK(fixture.tree.AttachLogical(scope, scopeSecond));
    CHECK(fixture.tree.AttachVisual(scope, scopeSecond));
    CHECK(fixture.layout.Attach(scope, scopeSecond));
    CHECK(fixture.layout.SetRoot(&root, {100.0, 100.0}));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Lifecycle));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));

    FocusManager focus(fixture.tree, fixture.events);
    Result<bool> next =
        focus.MoveFocus(FocusNavigationDirection::Next);
    CHECK(next && next.Value());
    CHECK(focus.FocusedNode() == &second);
    CHECK(focus.FocusedElement(root) == &second);
    next = focus.MoveFocus(FocusNavigationDirection::Next);
    CHECK(next && next.Value());
    CHECK(focus.FocusedNode() == &first);
    next = focus.MoveFocus(FocusNavigationDirection::Next);
    CHECK(next && next.Value());
    CHECK(focus.FocusedNode() == &scopeFirst);

    CHECK(focus.SetFocus(&second));
    Result<bool> previous =
        focus.MoveFocus(FocusNavigationDirection::Previous);
    CHECK(previous && previous.Value());
    CHECK(focus.FocusedNode() == &scopeSecond);

    CHECK(focus.SetFocus(&scopeFirst));
    CHECK(focus.FocusedElement(scope) == &scopeFirst);
    next = focus.MoveFocus(FocusNavigationDirection::Next);
    CHECK(next && next.Value());
    CHECK(focus.FocusedNode() == &scopeSecond);
    CHECK(focus.FocusedElement(scope) == &scopeSecond);
    next = focus.MoveFocus(FocusNavigationDirection::Next);
    CHECK(next && next.Value());
    CHECK(focus.FocusedNode() == &scopeFirst);

    KeyboardInputManager keyboard(
        focus, fixture.events, fixture.tree);
    Result<KeyboardDispatchResult> tab = keyboard.Dispatch(
        {KeyboardAction::Down, KeyboardKeyTab, 0U, false});
    CHECK(tab && tab.Value().routed);
    CHECK(tab.Value().focusMoved);
    CHECK(focus.FocusedNode() == &scopeSecond);
    Result<KeyboardDispatchResult> shiftTab = keyboard.Dispatch(
        {KeyboardAction::Down, KeyboardKeyTab,
            static_cast<std::uint32_t>(KeyboardModifiers::Shift), false});
    CHECK(shiftTab && shiftTab.Value().focusMoved);
    CHECK(focus.FocusedNode() == &scopeFirst);

    CHECK(focus.ClearFocus());
    tab = keyboard.Dispatch(
        {KeyboardAction::Down, KeyboardKeyTab, 0U, false});
    CHECK(tab && !tab.Value().routed && tab.Value().focusMoved);
    CHECK(tab.Value().target == &second);
    CHECK(focus.FocusedNode() == &second);

    CHECK(focus.ClearFocus());
    CHECK(fixture.layout.SetRoot(nullptr, {}));
    CHECK(fixture.layout.Detach(scope, scopeSecond));
    CHECK(fixture.tree.DetachVisual(scope, scopeSecond));
    CHECK(fixture.tree.DetachLogical(scope, scopeSecond));
    CHECK(fixture.values.DetachObject(scopeSecond));
    CHECK(fixture.layout.Detach(scope, scopeFirst));
    CHECK(fixture.tree.DetachVisual(scope, scopeFirst));
    CHECK(fixture.tree.DetachLogical(scope, scopeFirst));
    CHECK(fixture.values.DetachObject(scopeFirst));
    CHECK(fixture.layout.Detach(root, scope));
    CHECK(fixture.tree.DetachVisual(root, scope));
    CHECK(fixture.tree.DetachLogical(root, scope));
    CHECK(fixture.values.DetachObject(scope));
    CHECK(fixture.layout.Detach(root, disabled));
    CHECK(fixture.tree.DetachVisual(root, disabled));
    CHECK(fixture.tree.DetachLogical(root, disabled));
    CHECK(fixture.values.DetachObject(disabled));
    CHECK(fixture.layout.Detach(root, second));
    CHECK(fixture.tree.DetachVisual(root, second));
    CHECK(fixture.tree.DetachLogical(root, second));
    CHECK(fixture.values.DetachObject(second));
    CHECK(fixture.layout.Detach(root, first));
    CHECK(fixture.tree.DetachVisual(root, first));
    CHECK(fixture.tree.DetachLogical(root, first));
    CHECK(fixture.values.DetachObject(first));
    CHECK(fixture.tree.SetRoot(nullptr));
    CHECK(fixture.values.DetachObject(root));
    return true;
}

} // namespace

int main() {
    if (!TestUnifiedInteractionState()) return 1;
    if (!TestKeyboardNavigationAndFocusScopes()) return 1;
    std::puts("Aero interaction state tests passed");
    return 0;
}
