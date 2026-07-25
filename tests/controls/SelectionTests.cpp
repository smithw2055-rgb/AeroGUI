#include <Aero/Controls/Metadata.hpp>
#include <Aero/Controls/Selection.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Presentation/Metadata.hpp>

#include <cstdio>
#include <utility>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Controls;
using namespace Aero::Presentation;

#define CHECK(expression) do { \
    if (!(expression)) { \
        std::fprintf(stderr, "CHECK failed %d: %s\n", \
            __LINE__, #expression); \
        return false; \
    } \
} while (false)

struct Fixture final {
    TypeRegistry types;
    MetadataBehaviorRegistrationStore
        typeBehaviors{types};
    MetadataRegistrationTypes
        typeRegistration{types, typeBehaviors};
    MetadataValueRegistrationStore
        valueRegistrations{types};
    DependencyPropertyRegistry
        properties{types, typeBehaviors};
    Dispatcher dispatcher;
    ObjectServicesScope services{
        dispatcher, properties, valueRegistrations};
    RoutedEventCatalog eventCatalog{
        types, typeBehaviors};
    RoutedEventManager events{eventCatalog};
    EffectiveValueEngine values{
        dispatcher, properties};
    ObjectTree tree{dispatcher, values};
    LayoutManager layout{dispatcher};

    bool Build() {
        MetaRegistrationContext registration(
            types,
            typeBehaviors,
            valueRegistrations,
            properties,
            &eventCatalog);
        CHECK(Aero::Core::Detail::
            PopulateCoreMetadata(registration));
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
        return true;
    }
};

struct SelectionLog final {
    std::uint32_t count = 0U;
    std::uint32_t oldPrimary = UINT32_MAX;
    std::uint32_t newPrimary = UINT32_MAX;
    std::uint32_t added = 0U;
    std::uint32_t removed = 0U;

    void OnChanged(
        Selector&,
        const SelectionChangedEvent& event) noexcept {
        ++count;
        oldPrimary = event.oldPrimaryIndex;
        newPrimary = event.newPrimaryIndex;
        added = event.addedIndices.Size();
        removed = event.removedIndices.Size();
    }
};

Result<Ref<Object>> MakeText(
    StringView value) {
    Result<Ref<TextBlock>> made =
        MakeRef<TextBlock>();
    if (!made) return made.GetStatus();
    Ref<TextBlock> text =
        std::move(made).Value();
    Result<void> stored =
        text->SetText(value);
    if (!stored) return stored.GetStatus();
    stored = text->SetWidth(100.0);
    if (!stored) return stored.GetStatus();
    stored = text->SetHeight(20.0);
    if (!stored) return stored.GetStatus();
    return Ref<Object>(std::move(text));
}

bool Populate(ListBox& listBox) {
    Result<Ref<Object>> zero = MakeText("zero");
    Result<Ref<Object>> one = MakeText("one");
    Result<Ref<Object>> two = MakeText("two");
    Result<Ref<Object>> three = MakeText("three");
    CHECK(zero && one && two && three);
    CHECK(listBox.Items().Add(
        std::move(zero).Value()));
    CHECK(listBox.Items().Add(
        std::move(one).Value()));
    CHECK(listBox.Items().Add(
        std::move(two).Value()));
    CHECK(listBox.Items().Add(
        std::move(three).Value()));
    return true;
}

bool TestSelectionModelAndCollectionDeltas() {
    Fixture fixture;
    CHECK(fixture.Build());
    const DependencyProperty* selectedIndexProperty =
        fixture.properties.Find(
            Selector::SelectedIndexProperty);
    const DependencyProperty* selectedItemProperty =
        fixture.properties.Find(
            Selector::SelectedItemProperty);
    const DependencyProperty* selectedValueProperty =
        fixture.properties.Find(
            Selector::SelectedValueProperty);
    CHECK(selectedIndexProperty != nullptr);
    CHECK(selectedItemProperty != nullptr);
    CHECK(selectedValueProperty != nullptr);
    const PropertyMetadata* selectedIndexMetadata =
        selectedIndexProperty->MetadataFor(
            ListBox::StaticTypeId());
    const PropertyMetadata* selectedItemMetadata =
        selectedItemProperty->MetadataFor(
            ListBox::StaticTypeId());
    const PropertyMetadata* selectedValueMetadata =
        selectedValueProperty->MetadataFor(
            ListBox::StaticTypeId());
    CHECK(selectedIndexMetadata != nullptr);
    CHECK(selectedItemMetadata != nullptr);
    CHECK(selectedValueMetadata != nullptr);
    CHECK(HasFlag(
        selectedIndexMetadata->flags,
        PropertyMetadataFlags::BindsTwoWayByDefault));
    CHECK(HasFlag(
        selectedItemMetadata->flags,
        PropertyMetadataFlags::BindsTwoWayByDefault));
    CHECK(HasFlag(
        selectedValueMetadata->flags,
        PropertyMetadataFlags::BindsTwoWayByDefault));
    ListBox listBox;
    CHECK(Populate(listBox));
    SelectionLog log;
    SelectionChangedHandler handler(
        &log, &SelectionLog::OnChanged);
    CHECK(listBox.TryAddSelectionChanged(handler));
    CHECK(listBox.SetSelectionMode(
        SelectionMode::Extended));
    CHECK(listBox.Select(1U).Value());
    CHECK(listBox.Select(2U).Value());
    CHECK(listBox.SelectedCount() == 2U);
    CHECK(listBox.SelectedIndex() == 2U);
    CHECK(listBox.SelectedItem().Get() ==
        listBox.ItemAt(2U).Get());
    CHECK(listBox.SelectedValue().Get() ==
        listBox.SelectedItem().Get());

    Ref<Object> selectedItem =
        listBox.SelectedItem();
    Result<Ref<Object>> inserted =
        MakeText("inserted");
    CHECK(inserted);
    CHECK(listBox.Items().Insert(
        0U, std::move(inserted).Value()));
    CHECK(listBox.SelectedCount() == 2U);
    CHECK(listBox.IsSelected(2U));
    CHECK(listBox.IsSelected(3U));
    CHECK(listBox.SelectedIndex() == 3U);
    CHECK(listBox.SelectedItem().Get() ==
        selectedItem.Get());

    CHECK(listBox.Items().RemoveAt(3U));
    CHECK(listBox.SelectedCount() == 1U);
    CHECK(listBox.SelectedIndex() == 2U);
    CHECK(listBox.IsSelected(2U));

    CHECK(listBox.SetCurrentValue(
        Selector::SelectedIndexProperty,
        Value::FromUnsignedInteger(
            BuiltinTypes::UnsignedInteger, 0U)));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::PropertyChanges));
    CHECK(listBox.SelectedCount() == 1U);
    CHECK(listBox.IsSelected(0U));
    if (listBox.SelectedItem().Get() !=
        listBox.ItemAt(0U).Get()) {
        std::fprintf(stderr,
            "selected item=%p expected=%p index=%u error=%s\n",
            static_cast<void*>(
                listBox.SelectedItem().Get()),
            static_cast<void*>(
                listBox.ItemAt(0U).Get()),
            listBox.SelectedIndex(),
            listBox.LastSelectionError().message);
    }
    CHECK(listBox.SelectedItem().Get() ==
        listBox.ItemAt(0U).Get());

    CHECK(listBox.SetSelectionMode(
        SelectionMode::Multiple));
    CHECK(listBox.Toggle(2U).Value());
    CHECK(listBox.SelectedCount() == 2U);
    CHECK(listBox.SetSelectionMode(
        SelectionMode::Single));
    CHECK(listBox.SelectedCount() == 1U);
    CHECK(listBox.IsSelected(2U));
    CHECK(log.count >= 6U);
    CHECK(listBox.RemoveSelectionChanged(handler));
    return true;
}

bool TestContainersAndListBoxInteraction() {
    Fixture fixture;
    CHECK(fixture.Build());
    ListBox listBox;
    StackPanel host;
    CHECK(Populate(listBox));
    CHECK(listBox.SetSelectionMode(
        SelectionMode::Extended));
    CHECK(listBox.SetSelectedIndex(0U).Value());
    CHECK(fixture.tree.SetRoot(&listBox));
    CHECK(fixture.tree.AttachLogical(
        listBox, host));
    CHECK(fixture.tree.AttachVisual(
        listBox, host));
    CHECK(fixture.layout.Attach(
        listBox, host));

    ItemContainerGenerator generator(
        fixture.tree,
        fixture.layout,
        fixture.values);
    CHECK(generator.Attach(listBox, host));
    CHECK(generator.GeneratedCount() == 4U);
    for (std::uint32_t index = 0U;
        index < generator.GeneratedCount(); ++index) {
        ItemContainer* container =
            generator.ContainerFromIndex(index);
        CHECK(container != nullptr);
        CHECK(fixture.types.IsDerivedFrom(
            container->RuntimeType(),
            ListBoxItem::StaticTypeId()));
        CHECK(container->LogicalParent() ==
            &listBox);
        CHECK(container->VisualParent() ==
            &host);
    }
    CHECK(static_cast<ListBoxItem*>(
        generator.ContainerFromIndex(0U))
        ->IsSelected());

    CHECK(fixture.layout.SetRoot(
        &listBox, {120.0, 120.0}));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Lifecycle));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));

    FocusManager focus(
        fixture.tree, fixture.events);
    ListBoxInteractionManager interactions(
        fixture.tree, fixture.events, focus);
    CHECK(interactions.Attach(listBox));

    auto* first =
        static_cast<ListBoxItem*>(
            generator.ContainerFromIndex(1U));
    MouseButtonEventArgs click;
    click.changedButton = MouseButton::Left;
    click.buttonState =
        MouseButtonState::Pressed;
    CHECK(fixture.events.RaiseEvent(
        *first, UIElement::MouseDownEvent,
        &click));
    CHECK(click.handled);
    CHECK(listBox.SelectedIndex() == 1U);
    CHECK(first->IsSelected());
    CHECK(focus.FocusedNode() == first);

    KeyboardInputManager keyboard(
        focus, fixture.events, fixture.tree);
    Result<KeyboardDispatchResult> down =
        keyboard.Dispatch({
            KeyboardAction::Down,
            KeyboardKeyDown,
            static_cast<std::uint32_t>(
                KeyboardModifiers::Shift),
            false});
    CHECK(down && down.Value().routed);
    CHECK(listBox.SelectedCount() == 2U);
    CHECK(listBox.IsSelected(1U));
    CHECK(listBox.IsSelected(2U));
    CHECK(focus.FocusedNode() ==
        generator.ContainerFromIndex(2U));

    down = keyboard.Dispatch({
        KeyboardAction::Down,
        KeyboardKeyDown,
        static_cast<std::uint32_t>(
            KeyboardModifiers::Control),
        false});
    CHECK(down && down.Value().routed);
    CHECK(listBox.SelectedCount() == 2U);
    CHECK(focus.FocusedNode() ==
        generator.ContainerFromIndex(3U));

    down = keyboard.Dispatch({
        KeyboardAction::Down,
        KeyboardKeyHome,
        static_cast<std::uint32_t>(
            KeyboardModifiers::Control) |
            static_cast<std::uint32_t>(
                KeyboardModifiers::Shift),
        false});
    CHECK(down && down.Value().routed);
    CHECK(listBox.SelectedCount() == 3U);
    CHECK(listBox.IsSelected(0U));
    CHECK(listBox.IsSelected(1U));
    CHECK(listBox.IsSelected(2U));
    CHECK(focus.FocusedNode() ==
        generator.ContainerFromIndex(0U));

    down = keyboard.Dispatch({
        KeyboardAction::Down,
        KeyboardKeyEnd,
        0U,
        false});
    CHECK(down && down.Value().routed);
    CHECK(listBox.SelectedCount() == 1U);
    CHECK(listBox.SelectedIndex() == 3U);
    CHECK(focus.FocusedNode() ==
        generator.ContainerFromIndex(3U));

    CHECK(interactions.Detach(listBox).Value());
    CHECK(generator.Detach().Value());
    CHECK(fixture.layout.SetRoot(nullptr, {}));
    CHECK(fixture.layout.Detach(
        listBox, host));
    CHECK(fixture.tree.DetachVisual(
        listBox, host));
    CHECK(fixture.tree.DetachLogical(
        listBox, host));
    CHECK(fixture.tree.SetRoot(nullptr));
    CHECK(fixture.values.DetachObject(host));
    CHECK(fixture.values.DetachObject(listBox));
    return true;
}

bool TestBringIntoView() {
    Fixture fixture;
    CHECK(fixture.Build());
    Result<Ref<ScrollViewer>> viewerResult =
        MakeRef<ScrollViewer>();
    Result<Ref<ListBox>> listBoxResult =
        MakeRef<ListBox>();
    Result<Ref<StackPanel>> hostResult =
        MakeRef<StackPanel>();
    CHECK(viewerResult && listBoxResult && hostResult);
    Ref<ScrollViewer> viewer =
        std::move(viewerResult).Value();
    Ref<ListBox> listBox =
        std::move(listBoxResult).Value();
    Ref<StackPanel> host =
        std::move(hostResult).Value();
    CHECK(Populate(*listBox));
    CHECK(fixture.tree.SetRoot(viewer.Get()));
    CHECK(fixture.tree.AttachLogical(
        *viewer, *listBox));
    CHECK(fixture.tree.AttachLogical(
        *viewer, *host));
    CHECK(fixture.tree.AttachVisual(
        *viewer, *host));
    CHECK(fixture.layout.Attach(
        *viewer, *host));
    Ref<Object> hostOwner(host);
    CHECK(viewer->SetOwnedChild(
        hostOwner, *host));

    ItemContainerGenerator generator(
        fixture.tree,
        fixture.layout,
        fixture.values);
    CHECK(generator.Attach(*listBox, *host));
    CHECK(fixture.layout.SetRoot(
        viewer.Get(), {100.0, 30.0}));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Lifecycle));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));
    CHECK(viewer->ExtentHeight() >= 80.0);
    CHECK(viewer->VerticalOffset() == 0.0);
    Result<bool> brought =
        listBox->BringIntoView(3U);
    CHECK(brought && brought.Value());
    CHECK(viewer->VerticalOffset() > 0.0);
    CHECK(viewer->VerticalOffset() <= 50.0);

    CHECK(generator.Detach().Value());
    CHECK(fixture.layout.SetRoot(nullptr, {}));
    CHECK(fixture.layout.Detach(
        *viewer, *host));
    CHECK(fixture.tree.DetachVisual(
        *viewer, *host));
    CHECK(fixture.tree.DetachLogical(
        *viewer, *host));
    CHECK(viewer->SetChild(nullptr));
    CHECK(fixture.tree.DetachLogical(
        *viewer, *listBox));
    CHECK(fixture.tree.SetRoot(nullptr));
    CHECK(fixture.values.DetachObject(*host));
    CHECK(fixture.values.DetachObject(*listBox));
    CHECK(fixture.values.DetachObject(*viewer));
    return true;
}

} // namespace

int main() {
    if (!TestSelectionModelAndCollectionDeltas()) return 1;
    if (!TestContainersAndListBoxInteraction()) return 1;
    if (!TestBringIntoView()) return 1;
    std::puts("Aero selection tests passed");
    return 0;
}
