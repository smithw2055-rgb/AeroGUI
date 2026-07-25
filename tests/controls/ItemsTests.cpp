#include <Aero/Controls/Items.hpp>
#include <Aero/Controls/Metadata.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Presentation/Metadata.hpp>

#include <cstdio>
#include <memory>
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

class DataItem final : public Object {
    AERO_TYPED_META_NAMED(
        DataItem,
        Object,
        "urn:aero-items-tests",
        "DataItem")
public:
    explicit DataItem(std::uint32_t value) noexcept
        : value_(value) {}
    std::uint32_t Value() const noexcept {
        return value_;
    }
private:
    std::uint32_t value_ = 0U;
};

struct ChangeLog final {
    std::uint32_t count = 0U;
    ItemsChangedEvent last;
    void OnChanged(
        const ItemsChangedEvent& event) noexcept {
        ++count;
        last = event;
    }
};

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
    EffectiveValueEngine values{
        dispatcher, properties};
    ObjectTree tree{dispatcher, values};
    LayoutManager layout{dispatcher};

    bool Build() {
        MetaRegistrationContext registration(
            types,
            typeBehaviors,
            valueRegistrations,
            properties);
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
        CHECK(values.Initialize());
        CHECK(tree.Initialize());
        CHECK(layout.Initialize());
        return true;
    }
};

Result<Ref<Object>> MakeItemVisual(
    const Ref<Object>& item,
    void*) noexcept {
    auto& data = static_cast<DataItem&>(*item);
    Result<Ref<TextBlock>> made =
        MakeRef<TextBlock>();
    if (!made) return made.GetStatus();
    Ref<TextBlock> text =
        std::move(made).Value();
    char buffer[16]{};
    const int count = std::snprintf(
        buffer, sizeof(buffer), "%u",
        data.Value());
    if (count <= 0 ||
        !text->SetText(StringView(
            buffer,
            static_cast<std::uint32_t>(count)))) {
        return Status::Failure(
            ErrorCode::InternalError,
            "Failed to prepare item visual");
    }
    return Ref<Object>(std::move(text));
}

bool AttachRootChildren(
    Fixture& fixture,
    Grid& root,
    ItemsControl& owner,
    StackPanel& host) {
    CHECK(fixture.tree.SetRoot(&root));
    CHECK(fixture.tree.AttachLogical(root, owner));
    CHECK(fixture.tree.AttachVisual(root, owner));
    CHECK(fixture.layout.Attach(root, owner));
    CHECK(fixture.tree.AttachLogical(root, host));
    CHECK(fixture.tree.AttachVisual(root, host));
    CHECK(fixture.layout.Attach(root, host));
    CHECK(fixture.layout.SetRoot(
        &root, {200.0, 200.0}));
    return true;
}

bool DetachRootChildren(
    Fixture& fixture,
    Grid& root,
    ItemsControl& owner,
    StackPanel& host) {
    CHECK(fixture.layout.SetRoot(nullptr, {}));
    CHECK(fixture.layout.Detach(root, host));
    CHECK(fixture.tree.DetachVisual(root, host));
    CHECK(fixture.tree.DetachLogical(root, host));
    CHECK(fixture.layout.Detach(root, owner));
    CHECK(fixture.tree.DetachVisual(root, owner));
    CHECK(fixture.tree.DetachLogical(root, owner));
    CHECK(fixture.tree.SetRoot(nullptr));
    CHECK(fixture.values.DetachObject(host));
    CHECK(fixture.values.DetachObject(owner));
    CHECK(fixture.values.DetachObject(root));
    return true;
}

bool TestCollectionDeltas() {
    ItemsCollection items;
    ChangeLog log;
    ItemsChangedHandler handler(
        &log, &ChangeLog::OnChanged);
    CHECK(items.TryAddItemsChanged(handler));
    Result<Ref<DataItem>> first =
        MakeRef<DataItem>(1U);
    Result<Ref<DataItem>> second =
        MakeRef<DataItem>(2U);
    Result<Ref<DataItem>> third =
        MakeRef<DataItem>(3U);
    CHECK(first && second && third);
    Ref<Object> firstObject(
        std::move(first).Value());
    Ref<Object> secondObject(
        std::move(second).Value());
    Ref<Object> thirdObject(
        std::move(third).Value());
    CHECK(items.Add(firstObject));
    CHECK(items.Add(thirdObject));
    CHECK(items.Insert(1U, secondObject));
    CHECK(items.Count() == 3U);
    CHECK(items.ItemAt(0U).Get() ==
        firstObject.Get());
    CHECK(items.ItemAt(1U).Get() ==
        secondObject.Get());
    CHECK(items.ItemAt(2U).Get() ==
        thirdObject.Get());
    CHECK(items.Move(2U, 0U));
    CHECK(items.ItemAt(0U).Get() ==
        thirdObject.Get());
    Result<Ref<Object>> removed =
        items.RemoveAt(1U);
    CHECK(removed &&
        removed.Value().Get() ==
            firstObject.Get());
    CHECK(items.Count() == 2U);
    CHECK(log.count == 5U);
    CHECK(log.last.action ==
        ItemsChangeAction::Remove);
    CHECK(items.RemoveItemsChanged(handler));
    return true;
}

bool TestGeneratorIncrementalAndSourceSwitch() {
    Fixture fixture;
    CHECK(fixture.Build());
    Grid root;
    ItemsControl owner;
    StackPanel host;
    CHECK(AttachRootChildren(
        fixture, root, owner, host));

    DataTemplate itemTemplate(
        &MakeItemVisual);
    owner.SetItemTemplate(&itemTemplate);
    Style style(ItemContainer::StaticTypeId());
    CHECK(style.TryAddSetter(
        UIElement::IsEnabledProperty,
        Value::FromBoolean(
            BuiltinTypes::Boolean, false)));
    CHECK(style.Seal(fixture.properties));
    owner.SetItemContainerStyle(&style);

    Result<Ref<DataItem>> one =
        MakeRef<DataItem>(1U);
    Result<Ref<DataItem>> two =
        MakeRef<DataItem>(2U);
    Result<Ref<DataItem>> three =
        MakeRef<DataItem>(3U);
    CHECK(one && two && three);
    Ref<Object> oneObject(
        std::move(one).Value());
    Ref<Object> twoObject(
        std::move(two).Value());
    Ref<Object> threeObject(
        std::move(three).Value());
    CHECK(owner.Items().Add(oneObject));
    CHECK(owner.Items().Add(threeObject));

    StyleManager styles(
        fixture.values, fixture.properties);
    ItemContainerGenerator generator(
        fixture.tree,
        fixture.layout,
        fixture.values,
        &styles);
    CHECK(generator.Attach(owner, host));
    CHECK(generator.GeneratedCount() == 2U);
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::PropertyChanges));
    ItemContainer* first =
        generator.ContainerFromIndex(0U);
    ItemContainer* oldSecond =
        generator.ContainerFromIndex(1U);
    CHECK(first != nullptr && oldSecond != nullptr);
    CHECK(!first->IsEnabled());
    CHECK(first->Content() != nullptr);
    CHECK(first->LogicalParent() == &owner);
    CHECK(first->VisualParent() == &host);

    CHECK(owner.Items().Insert(1U, twoObject));
    CHECK(generator.LastError().IsOk());
    CHECK(generator.GeneratedCount() == 3U);
    CHECK(generator.ContainerFromIndex(0U) == first);
    CHECK(generator.ContainerFromIndex(2U) ==
        oldSecond);
    ItemContainer* inserted =
        generator.ContainerFromIndex(1U);
    CHECK(inserted != nullptr);
    CHECK(generator.ItemFromContainer(*inserted)
        .Get() == twoObject.Get());
    CHECK(generator.IndexFromContainer(*oldSecond) ==
        2U);

    CHECK(owner.Items().Replace(1U, oneObject));
    CHECK(generator.LastError().IsOk());
    CHECK(generator.ContainerFromIndex(0U) == first);
    CHECK(generator.ContainerFromIndex(1U) != nullptr);
    CHECK(generator.ContainerFromIndex(2U) ==
        oldSecond);

    CHECK(owner.Items().Move(2U, 0U));
    CHECK(generator.LastError().IsOk());
    CHECK(generator.ContainerFromIndex(0U) ==
        oldSecond);
    CHECK(owner.Items().RemoveAt(1U));
    CHECK(generator.LastError().IsOk());
    CHECK(generator.GeneratedCount() == 2U);

    ItemsCollection external;
    CHECK(external.Add(twoObject));
    CHECK(owner.SetItemsSource(&external));
    CHECK(generator.LastError().IsOk());
    CHECK(owner.ItemCount() == 1U);
    CHECK(generator.GeneratedCount() == 1U);
    CHECK(owner.Items().Add(oneObject));
    CHECK(generator.GeneratedCount() == 1U);
    CHECK(external.Add(threeObject));
    CHECK(generator.GeneratedCount() == 2U);
    CHECK(owner.SetItemsSource(nullptr));
    CHECK(generator.GeneratedCount() ==
        owner.Items().Count());

    CHECK(generator.Detach().Value());
    CHECK(host.VisualChildren().Empty());
    CHECK(DetachRootChildren(
        fixture, root, owner, host));
    return true;
}

} // namespace

int main() {
    if (!TestCollectionDeltas()) return 1;
    if (!TestGeneratorIncrementalAndSourceSwitch()) return 1;
    std::puts("Aero items tests passed");
    return 0;
}
