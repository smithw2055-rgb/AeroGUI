#include <Aero/Base/Ref.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/RuntimeHost.hpp>
#include <Aero/Presentation/QueuedRenderBackend.hpp>
#include <Aero/RuntimeSafety.hpp>
#include <Aero/Markup/XamlCompiledCache.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>
#include <Aero/Module.hpp>

#include <cstdio>
#include <utility>

#define main AeroPhase1EmbeddedMain
#include "../presentation/Phase1RuntimeSafetyTests.cpp"
#undef main
#ifdef CHECK
#undef CHECK
#endif

namespace {

using namespace Aero;
using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Controls;
using namespace Aero::Markup;
using namespace Aero::Presentation;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

struct ModuleProbe final {
    std::uint32_t metadataCalls = 0U;
};

Result<void> RegisterModule(
    MetaRegistrationContext& context,
    void* userContext) noexcept {
    auto* probe = static_cast<ModuleProbe*>(userContext);
    ++probe->metadataCalls;
    Result<TypeId> registered =
        context.Types().TryRegisterType(
            TypeRegistration::Object(
                "urn:module-sdk-tests",
                "Widget",
                BuiltinTypes::FrameworkElement,
                TypeFlags::None,
                nullptr));
    return registered
        ? Result<void>()
        : Result<void>(registered.GetStatus());
}

bool TestRootModuleCatalogAndSchemaIdentity() {
    ModuleProbe probe;
    ModuleCatalog catalog;
    ModuleRegistration manifest;
    manifest.name = "Tests.ModuleSdk";
    manifest.schemaVersion = 3U;
    manifest.registerModule = &RegisterModule;
    manifest.context = &probe;
    CHECK(catalog.TryAdd(manifest));
    CHECK(!catalog.TryAdd(manifest));
    CHECK(catalog.ModuleCount() == 1U);
    CHECK(catalog.Freeze());
    CHECK(catalog.IsFrozen());
    CHECK(!catalog.TryAdd(manifest));

    MetadataDomain metadata;
    CHECK(catalog.RegisterMetadata(metadata));
    CHECK(probe.metadataCalls == 1U);
    CHECK(metadata.Seal());
    MetadataRuntime runtime(metadata);
    CHECK(runtime.Freeze());
    XamlSchemaContext schema(metadata, runtime);
    CHECK(schema.Freeze());

    Result<XamlCompiledCacheIdentity> matching =
        BuildXamlCompiledCacheIdentity(metadata);
    CHECK(matching);
    CHECK(ValidateXamlCompiledCacheIdentity(
        matching.Value(), metadata));
    XamlCompiledCacheIdentity changed = matching.Value();
    changed.metadataSchemaHash += 1U;
    Result<void> mismatch =
        ValidateXamlCompiledCacheIdentity(changed, metadata);
    CHECK(!mismatch);
    CHECK(mismatch.GetStatus().code ==
        ErrorCode::ValidationFailed);
    return true;
}

class ProbeRenderBackend final : public IRenderBackend {
public:
    Result<void> Submit(
        const RenderPlan& plan) noexcept override {
        ++submissions;
        lastHash = plan.StableHash();
        return {};
    }

    std::uint32_t submissions = 0U;
    std::uint64_t lastHash = 0U;
};

bool TestHostDrivenRenderQueue() {
    ProbeRenderBackend downstream;
    Presentation::QueuedRenderBackend queue;
    CHECK(queue.Initialize(
        downstream,
        2U,
        Presentation::FrameQueueFullPolicy::DropOldest));

    RenderPlan plan;
    CHECK(queue.Submit(plan));
    CHECK(queue.Submit(plan));
    CHECK(queue.Submit(plan));
    Presentation::FrameQueueStatistics before = queue.Statistics();
    CHECK(before.accepted == 3U);
    CHECK(before.dropped == 1U);
    CHECK(before.pending == 2U);
    CHECK(before.highWatermark == 2U);

    Result<std::uint32_t> drained = queue.Drain();
    CHECK(drained);
    CHECK(drained.Value() == 2U);
    CHECK(downstream.submissions == 2U);
    Presentation::FrameQueueStatistics after = queue.Statistics();
    CHECK(after.consumed == 2U);
    CHECK(after.pending == 0U);
    queue.Shutdown();
    return true;
}

bool TestRuntimeHostHighLevelMarkupApi() {
    RuntimeHost runtime;
    CHECK(runtime.Initialize());
    DiagnosticBag diagnostics;
    Result<Ref<Object>> loaded = runtime.LoadAndMountXaml(
        "<Border xmlns=\"urn:aero\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"RootBorder\" Width=\"240\" Height=\"120\"/>",
        {320.0, 200.0},
        &diagnostics);
    CHECK(loaded);
    CHECK(diagnostics.Size() == 0U);
    Border* border = runtime.FindNamed<Border>("RootBorder");
    CHECK(border != nullptr);
    CHECK(border->Width() == 240.0);
    CHECK(border->Height() == 120.0);
    CHECK(runtime.FindNamed<Border>("Missing") == nullptr);
    CHECK(runtime.RunFrame());
    CHECK(runtime.Unmount());
    runtime.Shutdown();
    return true;
}

bool TestRuntimeHostLifecycle() {
    RuntimeHost runtime;
    CHECK(runtime.Initialize());
    CHECK(runtime.IsInitialized());
    CHECK(runtime.Metadata() != nullptr);
    CHECK(runtime.Tree() != nullptr);
    CHECK(runtime.Layout() != nullptr);
    CHECK(runtime.Renderer() != nullptr);
    CHECK(runtime.Schema() != nullptr);

    Result<Ref<Border>> made = MakeRef<Border>();
    CHECK(made);
    Ref<Border> border = std::move(made).Value();
    CHECK(border->SetBackground(
        {0.25F, 0.5F, 0.75F, 1.0F}));
    Ref<Object> root(std::move(border));
    CHECK(runtime.Mount(root, {320.0, 200.0}));
    CHECK(runtime.IsMounted());
    CHECK(runtime.Root());

    Result<RuntimeFrameResult> frame = runtime.RunFrame();
    CHECK(frame);
    CHECK(frame.Value().frameNumber == 1U);
    CHECK(frame.Value().layout.arrangedCount >= 1U);
    CHECK(frame.Value().render.nodeCount >= 1U);
    CHECK(frame.Value().render.commandCount >= 1U);

    CHECK(runtime.Unmount());
    CHECK(!runtime.IsMounted());
    runtime.Shutdown();
    CHECK(!runtime.IsInitialized());
    return true;
}

struct RollbackOrder final {
    std::uint32_t entries[4]{};
    std::uint32_t count = 0U;
};

struct RollbackContext final {
    RollbackOrder* order = nullptr;
    std::uint32_t marker = 0U;
};

void RecordRollback(void* context) noexcept {
    auto* item = static_cast<RollbackContext*>(context);
    item->order->entries[item->order->count++] = item->marker;
}

class TrackedVisual final : public Visual {
public:
    explicit TrackedVisual(
        std::uint32_t* destroyed) noexcept
        : Visual(BuiltinTypes::Visual),
          destroyed_(destroyed) {}
    ~TrackedVisual() override {
        if (destroyed_ != nullptr) {
            ++*destroyed_;
        }
    }
private:
    std::uint32_t* destroyed_ = nullptr;
};

Result<void> CountDeferred(
    Object&,
    void* context) noexcept {
    ++*static_cast<std::uint32_t*>(context);
    return {};
}

bool TestMutationJournalRollbackOrder() {
    RollbackOrder order;
    RollbackContext first{&order, 1U};
    RollbackContext second{&order, 2U};
    {
        MutationJournal mutation;
        CHECK(mutation.TryAddRollback(
            &RecordRollback, &first));
        CHECK(mutation.TryAddRollback(
            &RecordRollback, &second));
        CHECK(mutation.ActionCount() == 2U);
    }
    CHECK(order.count == 2U);
    CHECK(order.entries[0] == 2U);
    CHECK(order.entries[1] == 1U);

    order.count = 0U;
    {
        MutationJournal mutation;
        CHECK(mutation.TryAddRollback(
            &RecordRollback, &first));
        mutation.Commit();
    }
    CHECK(order.count == 0U);
    return true;
}

bool TestSafeDeferredWorkSkipsDestroyedObjects() {
    SafeDeferredWorkQueue queue;
    std::uint32_t destroyed = 0U;
    std::uint32_t invoked = 0U;

    Result<Ref<TrackedVisual>> expiredMade =
        MakeRef<TrackedVisual>(&destroyed);
    CHECK(expiredMade);
    Ref<TrackedVisual> expired =
        std::move(expiredMade).Value();
    CHECK(queue.Enqueue(
        *expired, &CountDeferred, &invoked));
    expired.Reset();
    CHECK(destroyed == 1U);

    Result<Ref<TrackedVisual>> liveMade =
        MakeRef<TrackedVisual>(&destroyed);
    CHECK(liveMade);
    Ref<TrackedVisual> live =
        std::move(liveMade).Value();
    CHECK(queue.Enqueue(
        *live, &CountDeferred, &invoked));

    Result<std::uint32_t> flushed = queue.Flush();
    CHECK(flushed);
    CHECK(flushed.Value() == 1U);
    CHECK(invoked == 1U);
    DeferredWorkStatistics statistics = queue.Statistics();
    CHECK(statistics.queued == 2U);
    CHECK(statistics.executed == 1U);
    CHECK(statistics.expired == 1U);
    CHECK(statistics.pending == 0U);
    return true;
}

bool TestEventRouteLifetimeSnapshot() {
    std::uint32_t destroyed = 0U;
    Result<Ref<TrackedVisual>> made =
        MakeRef<TrackedVisual>(&destroyed);
    CHECK(made);
    Ref<TrackedVisual> visual =
        std::move(made).Value();

    EventRouteLifetimeSnapshot route;
    CHECK(route.TryAdd(*visual));
    CHECK(route.Size() == 1U);
    CHECK(visual->UseCount() == 2U);
    visual.Reset();
    CHECK(destroyed == 0U);
    CHECK(route[0] != nullptr);
    route.Clear();
    CHECK(destroyed == 1U);
    return true;
}

} // namespace

#include "RuntimeWindowTests.inc"
#include "M1M4ClosureTests.inc"

int main() {
    if (!TestRootModuleCatalogAndSchemaIdentity()) return 1;
    if (!TestHostDrivenRenderQueue()) return 1;
    if (!TestRuntimeHostLifecycle()) return 1;
    if (!TestRuntimeHostHighLevelMarkupApi()) return 1;
    if (!RunRuntimeWindowTests()) return 1;
    if (!TestMutationJournalRollbackOrder()) return 1;
    if (!TestSafeDeferredWorkSkipsDestroyedObjects()) return 1;
    if (!TestEventRouteLifetimeSnapshot()) return 1;
    if (AeroPhase1EmbeddedMain() != 0) return 1;
    if (!RunM1M4ClosureTests()) return 1;
    std::puts("Aero module/runtime tests passed");
    return 0;
}
