#include <Aero/Controls/Metadata.hpp>
#include <Aero/Controls/Virtualization.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Presentation/Metadata.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

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

bool Near(double left, double right) noexcept {
    return std::fabs(left - right) < 0.000001;
}

class DataItem final : public Object {
    AERO_TYPED_META_NAMED(
        DataItem,
        Object,
        "urn:aero-virtualization-tests",
        "DataItem")
};

class FixedElement final : public FrameworkElement {
public:
    explicit FixedElement(Size desired) noexcept
        : FrameworkElement(
            BuiltinTypes::FrameworkElement),
          desired_(desired) {}

protected:
    Result<Size> MeasureOverride(
        Size available) noexcept override {
        return Size{
            std::min(desired_.width, available.width),
            std::min(desired_.height, available.height)};
    }

private:
    Size desired_;
};

class MutableSource final : public IItemsSource {
public:
    explicit MutableSource(
        std::uint32_t count) noexcept
        : count_(count) {}

    Result<void> Initialize() noexcept {
        Result<Ref<DataItem>> item =
            MakeRef<DataItem>();
        if (!item) return item.GetStatus();
        item_ = Ref<Object>(
            std::move(item).Value());
        return {};
    }

    std::uint32_t Count() const noexcept override {
        return count_;
    }

    Ref<Object> ItemAt(
        std::uint32_t index) const noexcept override {
        return index < count_
            ? item_
            : Ref<Object>();
    }

    Result<void> TryAddItemsChanged(
        const ItemsChangedHandler& handler) noexcept override {
        return changed_.TryAdd(handler);
    }

    bool RemoveItemsChanged(
        const ItemsChangedHandler& handler) noexcept override {
        return changed_.Remove(handler);
    }

    void Insert(
        std::uint32_t index,
        std::uint32_t count = 1U) noexcept {
        if (index > count_ || count == 0U) return;
        count_ += count;
        changed_.Invoke({
            ItemsChangeAction::Add,
            UINT32_MAX,
            index,
            0U,
            count});
    }

    void Remove(
        std::uint32_t index,
        std::uint32_t count = 1U) noexcept {
        if (index >= count_ ||
            count == 0U ||
            count > count_ - index) {
            return;
        }
        count_ -= count;
        changed_.Invoke({
            ItemsChangeAction::Remove,
            index,
            UINT32_MAX,
            count,
            0U});
    }

private:
    std::uint32_t count_ = 0U;
    Ref<Object> item_;
    ItemsChangedHandler changed_;
};

struct FactoryContext final {
    std::uint32_t creations = 0U;
    double itemExtent = 20.0;
};

Result<Ref<Object>> MakeItemVisual(
    const Ref<Object>&,
    void* rawContext) noexcept {
    auto* context =
        static_cast<FactoryContext*>(rawContext);
    ++context->creations;
    Result<Ref<FixedElement>> made =
        MakeRef<FixedElement>(
            Size{80.0, context->itemExtent});
    if (!made) return made.GetStatus();
    return Ref<Object>(
        std::move(made).Value());
}

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

bool AttachRootChildren(
    Fixture& fixture,
    Grid& root,
    ItemsControl& owner,
    VirtualizingStackPanel& host) {
    CHECK(fixture.tree.SetRoot(&root));
    CHECK(fixture.tree.AttachLogical(root, owner));
    CHECK(fixture.tree.AttachVisual(root, owner));
    CHECK(fixture.layout.Attach(root, owner));
    CHECK(fixture.tree.AttachLogical(root, host));
    CHECK(fixture.tree.AttachVisual(root, host));
    CHECK(fixture.layout.Attach(root, host));
    CHECK(fixture.layout.SetRoot(
        &root, {100.0, 100.0}));
    return true;
}

bool DetachRootChildren(
    Fixture& fixture,
    Grid& root,
    ItemsControl& owner,
    VirtualizingStackPanel& host) {
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

bool RunLayout(Fixture& fixture) {
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::PropertyChanges));
    CHECK(fixture.dispatcher.RunFramePhase(
        DispatcherFramePhase::Layout));
    return true;
}

bool TestVisibleRangeOverscanAndRecycling() {
    Fixture fixture;
    CHECK(fixture.Build());
    Grid root;
    ItemsControl owner;
    VirtualizingStackPanel host;
    CHECK(AttachRootChildren(
        fixture, root, owner, host));

    MutableSource source(10000U);
    CHECK(source.Initialize());
    FactoryContext factory;
    DataTemplate itemTemplate(
        &MakeItemVisual, &factory);
    owner.SetItemTemplate(&itemTemplate);
    CHECK(owner.SetItemsSource(&source));
    CHECK(host.SetEstimatedItemExtent(20.0));
    CHECK(host.SetOverscanCount(2U));
    CHECK(host.SetViewport({100.0, 100.0}));

    ItemContainerGenerator generator(
        fixture.tree,
        fixture.layout,
        fixture.values);
    CHECK(generator.AttachVirtualized(owner, host));
    CHECK(generator.GeneratedCount() == 7U);
    CHECK(generator.FirstGeneratedIndex() == 0U);
    CHECK(generator.CreatedContainerCount() == 7U);
    CHECK(generator.GeneratedCount() <
        source.Count());
    CHECK(host.VisualChildren().Size() == 7U);
    CHECK(RunLayout(fixture));
    CHECK(host.VisibleFirstIndex() == 0U);
    CHECK(host.VisibleCount() == 5U);
    CHECK(Near(host.Data().extentHeight, 200000.0));

    CHECK(host.SetVerticalOffset(4000.0));
    CHECK(RunLayout(fixture));
    CHECK(host.VisibleFirstIndex() == 200U);
    CHECK(host.VisibleCount() == 5U);
    CHECK(generator.FirstGeneratedIndex() == 198U);
    CHECK(generator.GeneratedCount() == 9U);
    CHECK(generator.CreatedContainerCount() == 9U);
    CHECK(generator.RecycledContainerUseCount() >= 7U);
    CHECK(generator.ContainerFromIndex(197U) == nullptr);
    CHECK(generator.ContainerFromIndex(198U) != nullptr);
    CHECK(generator.ContainerFromIndex(206U) != nullptr);
    CHECK(generator.ContainerFromIndex(207U) == nullptr);

    CHECK(host.SetVerticalOffset(12000.0));
    CHECK(RunLayout(fixture));
    CHECK(generator.CreatedContainerCount() == 9U);
    CHECK(generator.GeneratedCount() == 9U);
    CHECK(host.SetViewport({100.0, 60.0}));
    CHECK(host.VisibleCount() == 3U);
    CHECK(generator.GeneratedCount() == 7U);
    CHECK(generator.CreatedContainerCount() == 9U);

    CHECK(generator.Detach().Value());
    CHECK(host.VisualChildren().Empty());
    CHECK(owner.SetItemsSource(nullptr));
    CHECK(DetachRootChildren(
        fixture, root, owner, host));
    return true;
}

bool TestCollectionDeltaPreservesAnchor() {
    Fixture fixture;
    CHECK(fixture.Build());
    Grid root;
    ItemsControl owner;
    VirtualizingStackPanel host;
    CHECK(AttachRootChildren(
        fixture, root, owner, host));

    MutableSource source(10000U);
    CHECK(source.Initialize());
    FactoryContext factory;
    DataTemplate itemTemplate(
        &MakeItemVisual, &factory);
    owner.SetItemTemplate(&itemTemplate);
    CHECK(owner.SetItemsSource(&source));
    CHECK(host.SetEstimatedItemExtent(20.0));
    CHECK(host.SetOverscanCount(2U));
    CHECK(host.SetViewport({100.0, 100.0}));

    ItemContainerGenerator generator(
        fixture.tree,
        fixture.layout,
        fixture.values);
    CHECK(generator.AttachVirtualized(owner, host));
    CHECK(host.SetVerticalOffset(4000.0));
    CHECK(RunLayout(fixture));
    CHECK(host.VisibleFirstIndex() == 200U);

    source.Insert(0U);
    CHECK(generator.LastError().IsOk());
    CHECK(Near(host.Data().verticalOffset, 4020.0));
    CHECK(host.VisibleFirstIndex() == 201U);
    CHECK(generator.FirstGeneratedIndex() == 199U);
    CHECK(generator.GeneratedCount() == 9U);

    source.Remove(0U);
    CHECK(generator.LastError().IsOk());
    CHECK(Near(host.Data().verticalOffset, 4000.0));
    CHECK(host.VisibleFirstIndex() == 200U);
    CHECK(generator.FirstGeneratedIndex() == 198U);
    CHECK(generator.GeneratedCount() == 9U);
    CHECK(generator.CreatedContainerCount() == 9U);

    CHECK(generator.Detach().Value());
    CHECK(owner.SetItemsSource(nullptr));
    CHECK(DetachRootChildren(
        fixture, root, owner, host));
    return true;
}

#if defined(AERO_RUN_VIRTUALIZATION_BENCHMARK)
bool RunFormalBenchmark() {
    Fixture fixture;
    CHECK(fixture.Build());
    Grid root;
    ItemsControl owner;
    VirtualizingStackPanel host;
    CHECK(AttachRootChildren(
        fixture, root, owner, host));

    MutableSource source(10000U);
    CHECK(source.Initialize());
    FactoryContext factory;
    DataTemplate itemTemplate(
        &MakeItemVisual, &factory);
    owner.SetItemTemplate(&itemTemplate);
    CHECK(owner.SetItemsSource(&source));
    CHECK(host.SetEstimatedItemExtent(20.0));
    CHECK(host.SetOverscanCount(2U));
    CHECK(host.SetViewport({100.0, 100.0}));
    ItemContainerGenerator generator(
        fixture.tree,
        fixture.layout,
        fixture.values);
    CHECK(generator.AttachVirtualized(owner, host));
    CHECK(RunLayout(fixture));

    for (std::uint32_t frame = 0U;
        frame < 32U; ++frame) {
        CHECK(host.SetVerticalOffset(
            static_cast<double>(
                (frame * 173U) % 9900U) * 20.0));
        CHECK(RunLayout(fixture));
    }

    std::vector<double> frameMicroseconds;
    frameMicroseconds.reserve(500U);
    for (std::uint32_t frame = 0U;
        frame < 500U; ++frame) {
        const double offset =
            static_cast<double>(
                (frame * 197U) % 9900U) * 20.0;
        const auto begin =
            std::chrono::steady_clock::now();
        CHECK(host.SetVerticalOffset(offset));
        CHECK(RunLayout(fixture));
        const auto end =
            std::chrono::steady_clock::now();
        frameMicroseconds.push_back(
            std::chrono::duration<double, std::micro>(
                end - begin).count());
    }
    std::sort(
        frameMicroseconds.begin(),
        frameMicroseconds.end());
    const double median =
        frameMicroseconds[
            frameMicroseconds.size() / 2U];
    const double p95 =
        frameMicroseconds[
            frameMicroseconds.size() * 95U /
                100U];
    const double maximum =
        frameMicroseconds.back();
    std::printf(
        "VirtualizingStackPanel 10k benchmark: "
        "median=%.3fus p95=%.3fus max=%.3fus "
        "realized=%u created-containers=%u\n",
        median,
        p95,
        maximum,
        generator.GeneratedCount(),
        generator.CreatedContainerCount());
    CHECK(generator.GeneratedCount() <= 9U);
    CHECK(generator.CreatedContainerCount() <= 9U);
#if defined(NDEBUG)
    CHECK(p95 < 2000.0);
#endif

    CHECK(generator.Detach().Value());
    CHECK(owner.SetItemsSource(nullptr));
    CHECK(DetachRootChildren(
        fixture, root, owner, host));
    return true;
}
#endif

} // namespace

int main() {
    if (!TestVisibleRangeOverscanAndRecycling()) {
        return 1;
    }
    if (!TestCollectionDeltaPreservesAnchor()) {
        return 1;
    }
#if defined(AERO_RUN_VIRTUALIZATION_BENCHMARK)
    if (!RunFormalBenchmark()) return 1;
    std::puts(
        "Aero virtualization benchmark passed");
#else
    std::puts("Aero virtualization tests passed");
#endif
    return 0;
}
