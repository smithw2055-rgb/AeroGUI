#include <Aero/Base/Ref.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Markup/RuntimeHost.hpp>
#include <Aero/Markup/XamlCompiledCache.hpp>
#include <Aero/Markup/XamlModuleSdk.hpp>

#include <cstdio>
#include <utility>

namespace {

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
    std::uint32_t xamlCalls = 0U;
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

Result<void> ConfigureModule(
    XamlSchemaContext&,
    XamlActivationProviderRegistry&,
    void* userContext) noexcept {
    ++static_cast<ModuleProbe*>(userContext)->xamlCalls;
    return {};
}

bool TestSharedModuleCatalogAndManifestIdentity() {
    ModuleProbe probe;
    XamlModuleCatalog catalog;
    XamlModuleManifest manifest;
    manifest.name = "Tests.ModuleSdk";
    manifest.metadataSchemaVersion = 3U;
    manifest.xamlSchemaVersion = 7U;
    manifest.registerMetadata = &RegisterModule;
    manifest.configureXaml = &ConfigureModule;
    manifest.context = &probe;
    CHECK(catalog.TryAdd(manifest));
    CHECK(!catalog.TryAdd(manifest));
    CHECK(catalog.ModuleCount() == 1U);
    CHECK(catalog.ManifestHash() != 0U);

    MetadataDomain metadata;
    CHECK(catalog.RegisterMetadata(metadata));
    CHECK(probe.metadataCalls == 1U);
    CHECK(metadata.Seal());
    MetadataRuntime runtime(metadata);
    CHECK(runtime.Freeze());
    XamlSchemaContext schema(metadata, runtime);
    XamlActivationProviderRegistry activation(schema);
    CHECK(catalog.ConfigureXaml(schema, activation));
    CHECK(probe.xamlCalls == 1U);
    CHECK(schema.ModuleManifestHash() ==
        catalog.ManifestHash());
    CHECK(schema.Freeze());

    Result<XamlCompiledCacheIdentity> matching =
        BuildXamlCompiledCacheIdentity(
            metadata, catalog.ManifestHash());
    CHECK(matching);
    CHECK(ValidateXamlCompiledCacheIdentity(
        matching.Value(),
        metadata,
        catalog.ManifestHash()));
    Result<void> mismatch =
        ValidateXamlCompiledCacheIdentity(
            matching.Value(),
            metadata,
            catalog.ManifestHash() + 1U);
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
    QueuedRenderBackend queue;
    CHECK(queue.Initialize(
        downstream,
        2U,
        FrameQueueFullPolicy::DropOldest));

    RenderPlan plan;
    CHECK(queue.Submit(plan));
    CHECK(queue.Submit(plan));
    CHECK(queue.Submit(plan));
    FrameQueueStatistics before = queue.Statistics();
    CHECK(before.accepted == 3U);
    CHECK(before.dropped == 1U);
    CHECK(before.pending == 2U);
    CHECK(before.highWatermark == 2U);

    Result<std::uint32_t> drained = queue.Drain();
    CHECK(drained);
    CHECK(drained.Value() == 2U);
    CHECK(downstream.submissions == 2U);
    FrameQueueStatistics after = queue.Statistics();
    CHECK(after.consumed == 2U);
    CHECK(after.pending == 0U);
    queue.Shutdown();
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

} // namespace

int main() {
    if (!TestSharedModuleCatalogAndManifestIdentity()) return 1;
    if (!TestHostDrivenRenderQueue()) return 1;
    if (!TestRuntimeHostLifecycle()) return 1;
    std::puts("Aero XAML module SDK tests passed");
    return 0;
}
