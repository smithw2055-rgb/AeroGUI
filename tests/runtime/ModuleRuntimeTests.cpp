#include <Aero/Base/Ref.hpp>
#include <Aero/Controls/Controls.hpp>
#include <Aero/Controls/Templates.hpp>
#include <Aero/Core/Diagnostics.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/RuntimeEnvironment.hpp>
#include <Aero/RuntimeHost.hpp>
#include <Aero/Presentation/Binding.hpp>
#include <Aero/Presentation/QueuedRenderBackend.hpp>
#include <Aero/RuntimeSafety.hpp>
#include <Aero/Markup/Compiled/XamlCompiledCache.hpp>
#include <Aero/Markup/Runtime/XamlLoader.hpp>
#include <Aero/Markup/Schema/XamlSchemaContext.hpp>
#include <Aero/Presentation/Resources.hpp>
#include <Aero/Presentation/Style.hpp>
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

struct OrderedModuleProbe final {
    StringView typeName;
    std::uint32_t marker = 0U;
    Vector<std::uint32_t>* order = nullptr;
};

Result<void> RegisterOrderedModule(
    MetaRegistrationContext& context,
    void* userContext) noexcept {
    auto* probe = static_cast<OrderedModuleProbe*>(userContext);
    if (probe == nullptr || probe->order == nullptr) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Ordered module probe is invalid");
    }
    Result<void> appended = probe->order->TryPushBack(probe->marker);
    if (!appended) return appended.GetStatus();
    Result<TypeId> registered = context.Types().TryRegisterType(
        TypeRegistration::Object(
            "urn:module-order-tests",
            probe->typeName,
            BuiltinTypes::FrameworkElement,
            TypeFlags::None,
            nullptr));
    return registered
        ? Result<void>()
        : Result<void>(registered.GetStatus());
}

bool TestModuleDependencyOrderAndValidation() {
    Vector<std::uint32_t> order;
    OrderedModuleProbe first{"First", 1U, &order};
    OrderedModuleProbe second{"Second", 2U, &order};
    OrderedModuleProbe third{"Third", 3U, &order};
    const ModuleDependency secondDependencies[] = {
        {"Tests.First", 2U}};
    const ModuleDependency thirdDependencies[] = {
        {"Tests.Second", 1U}};

    ModuleRegistration thirdModule;
    thirdModule.name = "Tests.Third";
    thirdModule.registerModule = &RegisterOrderedModule;
    thirdModule.context = &third;
    thirdModule.dependencies = {thirdDependencies, 1U};
    ModuleRegistration secondModule;
    secondModule.name = "Tests.Second";
    secondModule.registerModule = &RegisterOrderedModule;
    secondModule.context = &second;
    secondModule.dependencies = {secondDependencies, 1U};
    ModuleRegistration firstModule;
    firstModule.name = "Tests.First";
    firstModule.schemaVersion = 2U;
    firstModule.registerModule = &RegisterOrderedModule;
    firstModule.context = &first;

    ModuleCatalog catalog;
    CHECK(catalog.TryAdd(thirdModule));
    CHECK(catalog.TryAdd(secondModule));
    CHECK(catalog.TryAdd(firstModule));
    CHECK(catalog.Freeze());
    MetadataDomain metadata;
    CHECK(catalog.RegisterMetadata(metadata));
    // MetadataDomain rebuilds its candidate storage for each registration, so
    // prior callbacks are replayed. The final replay must still follow the
    // resolved dependency order.
    CHECK(order.Size() == 6U);
    CHECK(order[3] == 1U && order[4] == 2U && order[5] == 3U);

    const ModuleDependency missingDependencies[] = {
        {"Tests.Missing", 1U}};
    ModuleRegistration missing = firstModule;
    missing.name = "Tests.NeedsMissing";
    missing.dependencies = {missingDependencies, 1U};
    ModuleCatalog missingCatalog;
    CHECK(missingCatalog.TryAdd(missing));
    Result<void> missingResult = missingCatalog.Freeze();
    CHECK(!missingResult);
    CHECK(missingResult.GetStatus().code == ErrorCode::NotFound);

    const ModuleDependency cycleADependencies[] = {
        {"Tests.CycleB", 1U}};
    const ModuleDependency cycleBDependencies[] = {
        {"Tests.CycleA", 1U}};
    ModuleRegistration cycleA = firstModule;
    cycleA.name = "Tests.CycleA";
    cycleA.dependencies = {cycleADependencies, 1U};
    ModuleRegistration cycleB = secondModule;
    cycleB.name = "Tests.CycleB";
    cycleB.dependencies = {cycleBDependencies, 1U};
    ModuleCatalog cycleCatalog;
    CHECK(cycleCatalog.TryAdd(cycleA));
    CHECK(cycleCatalog.TryAdd(cycleB));
    Result<void> cycleResult = cycleCatalog.Freeze();
    CHECK(!cycleResult);
    CHECK(cycleResult.GetStatus().code == ErrorCode::CycleDetected);
    return true;
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
    Result<Ref<Object>> loaded = runtime.ParseAndMountXaml(
        "<Border xmlns=\"urn:aero\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"RootBorder\" Width=\"240\" Height=\"120\"/>",
        {},
        {320.0, 200.0},
        &diagnostics);
    if (!loaded) {
        std::fprintf(
            stderr,
            "ParseAndMountXaml failed: %s\n",
            loaded.GetStatus().message);
    }
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

bool TestRuntimeHostResourceDictionaryDependencies() {
    RuntimeHost runtime;
    CHECK(runtime.Initialize());
    EmbeddedXamlSourceProvider* embedded =
        runtime.EmbeddedXamlSources();
    CHECK(embedded != nullptr);

    Result<ResourceUri> rootUri = ResourceUri::Parse(
        "pack://application:,,,/Aero.Tests;component/Themes/Root.xaml");
    Result<ResourceUri> firstUri = ResourceUri::Parse(
        "pack://application:,,,/Aero.Tests;component/Themes/First.xaml");
    Result<ResourceUri> secondUri = ResourceUri::Parse(
        "pack://application:,,,/Aero.Tests;component/Themes/Second.xaml");
    CHECK(rootUri && firstUri && secondUri);
    CHECK(embedded->TryAddText(
        firstUri.Value(),
        "<ResourceDictionary xmlns=\"urn:aero\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Color x:Key=\"FirstOnly\" Value=\"#FFFF0000\"/>"
        "<Color x:Key=\"Shared\" Value=\"#FFFF0000\"/>"
        "</ResourceDictionary>"));
    CHECK(embedded->TryAddText(
        secondUri.Value(),
        "<ResourceDictionary xmlns=\"urn:aero\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Color x:Key=\"Shared\" Value=\"#FF0000FF\"/>"
        "</ResourceDictionary>"));
    CHECK(embedded->TryAddText(
        rootUri.Value(),
        "<ResourceDictionary xmlns=\"urn:aero\">"
        "<ResourceDictionary.MergedDictionaries>"
        "<ResourceDictionary Source=\"First.xaml\"/>"
        "<ResourceDictionary Source=\"Second.xaml\"/>"
        "</ResourceDictionary.MergedDictionaries>"
        "</ResourceDictionary>"));

    DiagnosticBag diagnostics;
    Result<Ref<Object>> loaded = runtime.LoadXaml(
        rootUri.Value().Canonical(),
        &diagnostics);
    if (!loaded) {
        std::fprintf(
            stderr,
            "Resource dictionary load failed: %s\n",
            loaded.GetStatus().message);
    }
    CHECK(loaded);
    CHECK(diagnostics.Size() == 0U);
    CHECK(loaded.Value()->RuntimeType() ==
        ResourceDictionary::StaticTypeId());
    auto& dictionary =
        static_cast<ResourceDictionary&>(
            *loaded.Value());
    CHECK(dictionary.MergedDictionaryCount() == 2U);
    Result<ResourceValue> first =
        dictionary.Lookup("FirstOnly");
    Result<ResourceValue> shared =
        dictionary.Lookup("Shared");
    CHECK(first && shared);
    CHECK(first.Value().Type() == TypeOf<Color>());
    CHECK(shared.Value().Type() == TypeOf<Color>());
    const auto* firstColor =
        static_cast<const Color*>(
            first.Value().AsCustom());
    const auto* sharedColor =
        static_cast<const Color*>(
            shared.Value().AsCustom());
    CHECK(firstColor != nullptr && sharedColor != nullptr);
    CHECK(firstColor->red == 1.0F &&
        firstColor->blue == 0.0F);
    CHECK(sharedColor->red == 0.0F &&
        sharedColor->blue == 1.0F);
    runtime.Shutdown();
    return true;
}

bool TestRuntimeHostImplicitAndExplicitStyles() {
    RuntimeHost runtime;
    CHECK(runtime.Initialize());
    DiagnosticBag diagnostics;
    Result<Ref<Object>> loaded = runtime.ParseXaml(
        "<Grid xmlns=\"urn:aero\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid.Resources><ResourceDictionary>"
        "<Style TargetType=\"Border\">"
        "<Setter Property=\"Width\" Value=\"123\"/>"
        "</Style>"
        "<Style x:Key=\"ExplicitBorder\" TargetType=\"Border\">"
        "<Setter Property=\"Height\" Value=\"77\"/>"
        "</Style>"
        "</ResourceDictionary></Grid.Resources>"
        "<Border x:Name=\"ImplicitBorder\"/>"
        "<Border x:Name=\"ExplicitBorderTarget\" "
        "Style=\"{StaticResource ExplicitBorder}\"/>"
        "</Grid>",
        {},
        &diagnostics);
    if (!loaded) {
        std::fprintf(
            stderr,
            "Style integration load failed: %s\n",
            loaded.GetStatus().message);
    }
    CHECK(loaded);
    CHECK(runtime.Mount({400.0, 240.0}));
    CHECK(diagnostics.Size() == 0U);
    Border* implicit =
        runtime.FindNamed<Border>("ImplicitBorder");
    Border* explicitTarget =
        runtime.FindNamed<Border>(
            "ExplicitBorderTarget");
    CHECK(implicit != nullptr &&
        explicitTarget != nullptr);
    CHECK(implicit->Width() == 123.0);
    CHECK(!implicit->HasHeight());
    CHECK(explicitTarget->Height() == 77.0);
    CHECK(!explicitTarget->HasWidth());
    CHECK(runtime.Styles()->AppliedStyle(*implicit) != nullptr);
    CHECK(runtime.Styles()->AppliedStyle(*explicitTarget) != nullptr);
    CHECK(runtime.Unmount());

    diagnostics.Clear();
    loaded = runtime.ParseXaml(
        "<Border xmlns=\"urn:aero\" Width=\"32\"/>",
        {},
        &diagnostics);
    CHECK(loaded);
    CHECK(runtime.Mount({100.0, 100.0}));
    CHECK(diagnostics.Size() == 0U);
    CHECK(runtime.Unmount());
    runtime.Shutdown();
    return true;
}

bool TestRuntimeHostResourceLayers() {
    RuntimeHost runtime;
    CHECK(runtime.Initialize());
    EmbeddedXamlSourceProvider* embedded =
        runtime.EmbeddedXamlSources();
    CHECK(embedded != nullptr);

    Result<ResourceUri> systemUri = ResourceUri::Parse(
        "pack://application:,,,/Aero.Tests;component/Layers/System.xaml");
    Result<ResourceUri> themeUri = ResourceUri::Parse(
        "pack://application:,,,/Aero.Tests;component/Layers/Theme.xaml");
    Result<ResourceUri> applicationUri = ResourceUri::Parse(
        "pack://application:,,,/Aero.Tests;component/Layers/Application.xaml");
    CHECK(systemUri && themeUri && applicationUri);
    CHECK(embedded->TryAddText(
        systemUri.Value(),
        "<ResourceDictionary xmlns=\"urn:aero\">"
        "<Style TargetType=\"Border\">"
        "<Setter Property=\"Width\" Value=\"10\"/>"
        "</Style>"
        "</ResourceDictionary>"));
    CHECK(embedded->TryAddText(
        themeUri.Value(),
        "<ResourceDictionary xmlns=\"urn:aero\">"
        "<Style TargetType=\"Border\">"
        "<Setter Property=\"Width\" Value=\"20\"/>"
        "</Style>"
        "</ResourceDictionary>"));
    CHECK(embedded->TryAddText(
        applicationUri.Value(),
        "<ResourceDictionary xmlns=\"urn:aero\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Color x:Key=\"LayerColor\" Value=\"#FFFF0000\"/>"
        "<Style TargetType=\"Border\">"
        "<Setter Property=\"Width\" Value=\"30\"/>"
        "</Style>"
        "</ResourceDictionary>"));

    CHECK(runtime.LoadResources(
        RuntimeResourceLayer::System,
        systemUri.Value().Canonical()));
    CHECK(runtime.LoadResources(
        RuntimeResourceLayer::Theme,
        themeUri.Value().Canonical()));
    CHECK(runtime.LoadResources(
        RuntimeResourceLayer::Application,
        applicationUri.Value().Canonical()));
    Result<Ref<Object>> loaded = runtime.ParseXaml(
        "<Border xmlns=\"urn:aero\" "
        "Background=\"{StaticResource LayerColor}\"/>");
    CHECK(loaded);
    CHECK(runtime.Mount({100.0, 100.0}));
    auto* border =
        static_cast<Border*>(loaded.Value().Get());
    CHECK(border->Width() == 30.0);
    const Color background = border->Background();
    CHECK(background.red == 1.0F &&
        background.green == 0.0F &&
        background.blue == 0.0F);
    CHECK(runtime.Unmount());
    runtime.Shutdown();
    return true;
}

bool TestRuntimeHostDynamicResourceChain() {
    RuntimeHost runtime;
    CHECK(runtime.Initialize());
    EmbeddedXamlSourceProvider* embedded =
        runtime.EmbeddedXamlSources();
    CHECK(embedded != nullptr);
    Result<ResourceUri> applicationUri =
        ResourceUri::Parse(
            "pack://application:,,,/Aero.Tests;component/Dynamic/Application.xaml");
    CHECK(applicationUri);
    CHECK(embedded->TryAddText(
        applicationUri.Value(),
        "<ResourceDictionary xmlns=\"urn:aero\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Color x:Key=\"AppAccent\" Value=\"#FF0000FF\"/>"
        "</ResourceDictionary>"));
    CHECK(runtime.LoadResources(
        RuntimeResourceLayer::Application,
        applicationUri.Value().Canonical()));

    Result<Ref<Object>> loaded = runtime.ParseXaml(
        "<Grid xmlns=\"urn:aero\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"Root\">"
        "<Grid.Resources><ResourceDictionary>"
        "<Color x:Key=\"LocalAccent\" Value=\"#FFFF0000\"/>"
        "</ResourceDictionary></Grid.Resources>"
        "<Border x:Name=\"LocalTarget\" "
        "Background=\"{DynamicResource LocalAccent}\"/>"
        "<Border x:Name=\"ApplicationTarget\" "
        "Background=\"{DynamicResource AppAccent}\"/>"
        "</Grid>");
    CHECK(loaded);
    CHECK(runtime.Mount({200.0, 120.0}));
    Grid* root = runtime.FindNamed<Grid>("Root");
    Border* local =
        runtime.FindNamed<Border>("LocalTarget");
    Border* application =
        runtime.FindNamed<Border>(
            "ApplicationTarget");
    CHECK(root != nullptr && local != nullptr &&
        application != nullptr);
    CHECK(local->Background().red == 1.0F);
    CHECK(application->Background().blue == 1.0F);

    Result<Value> green =
        runtime.MetadataRuntime()->TryConvertText(
            TypeOf<Color>(),
            "#FF00FF00");
    CHECK(green);
    CHECK(root->Resources().TrySet(
        "LocalAccent", green.Value()));
    CHECK(runtime.RunFrame());
    CHECK(local->Background().red == 0.0F &&
        local->Background().green == 1.0F);

    Result<Value> white =
        runtime.MetadataRuntime()->TryConvertText(
            TypeOf<Color>(),
            "#FFFFFFFF");
    CHECK(white);
    CHECK(runtime.ApplicationResources()->TrySet(
        "AppAccent", white.Value()));
    CHECK(runtime.RunFrame());
    CHECK(application->Background().red == 1.0F &&
        application->Background().green == 1.0F &&
        application->Background().blue == 1.0F);

    CHECK(runtime.Unmount());
    runtime.Shutdown();
    return true;
}

bool TestRuntimeHostXamlTemplate() {
    RuntimeHost runtime;
    CHECK(runtime.Initialize());
    DiagnosticBag diagnostics;
    Result<Ref<Object>> loaded = runtime.ParseXaml(
        "<Grid xmlns=\"urn:aero\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Grid.Resources><ResourceDictionary>"
        "<Style TargetType=\"Button\">"
        "<Setter Property=\"Template\">"
        "<Setter.Value>"
        "<ControlTemplate TargetType=\"Button\">"
        "<ControlTemplate.VisualTree>"
        "<Border x:Name=\"Chrome\" Width=\"55\">"
        "<ContentPresenter x:Name=\"Presenter\"/>"
        "</Border>"
        "</ControlTemplate.VisualTree>"
        "</ControlTemplate>"
        "</Setter.Value>"
        "</Setter>"
        "</Style>"
        "</ResourceDictionary></Grid.Resources>"
        "<Button x:Name=\"TemplatedButton\">"
        "<TextBlock x:Name=\"ButtonContent\" Text=\"Run\"/>"
        "</Button>"
        "</Grid>",
        {},
        &diagnostics);
    if (!loaded) {
        std::fprintf(
            stderr,
            "Template integration load failed: %s\n",
            loaded.GetStatus().message);
    }
    CHECK(loaded);
    CHECK(diagnostics.Size() == 0U);
    CHECK(runtime.Mount({300.0, 180.0}));
    Button* button =
        runtime.FindNamed<Button>(
            "TemplatedButton");
    TextBlock* content =
        runtime.FindNamed<TextBlock>(
            "ButtonContent");
    CHECK(button != nullptr && content != nullptr);
    CHECK(button->TemplateChild() != nullptr);
    const TemplateHandle handle =
        runtime.Templates()->AppliedHandle(*button);
    CHECK(handle.IsValid());
    const ControlTemplate* appliedTemplate =
        runtime.Templates()->AppliedTemplate(handle);
    CHECK(appliedTemplate != nullptr);
    CHECK(appliedTemplate->IsSealed());
    auto* chrome = static_cast<Border*>(
        runtime.Templates()->FindName(
            handle,
            "Chrome"));
    auto* presenter =
        static_cast<ContentPresenter*>(
            runtime.Templates()->FindName(
                handle,
                "Presenter"));
    CHECK(chrome != nullptr &&
        presenter != nullptr);
    CHECK(chrome->Width() == 55.0);
    CHECK(presenter->Content() == content);
    CHECK(content->LogicalParent() == button);
    CHECK(content->VisualParent() == presenter);
    CHECK(runtime.Unmount());
    runtime.Shutdown();
    return true;
}

bool TestEnvironmentStateOutlivesFacade() {
    Ref<RuntimeView> view;
    {
        RuntimeEnvironment environment;
        CHECK(environment.Initialize());
        Result<Ref<RuntimeView>> created = environment.CreateView();
        CHECK(created);
        view = std::move(created).Value();
    }

    Result<Ref<Border>> made = MakeRef<Border>();
    CHECK(made);
    Ref<Object> root(std::move(made).Value());
    CHECK(view->Host().Mount(root, {160.0, 90.0}));
    CHECK(view->Host().RunFrame());
    CHECK(view->Host().Unmount());
    view.Reset();
    return true;
}

bool TestUiDocumentDefersEffectsUntilMount() {
    RuntimeEnvironment environment;
    CHECK(environment.Initialize());
    RuntimeView view(environment);
    CHECK(view.Initialize());
    EmbeddedXamlSourceProvider* embedded =
        view.Host().EmbeddedXamlSources();
    CHECK(embedded != nullptr);
    Result<ResourceUri> resourcesUri = ResourceUri::Parse(
        "pack://application:,,,/Aero.Tests;component/Deferred/Resources.xaml");
    CHECK(resourcesUri);
    CHECK(embedded->TryAddText(
        resourcesUri.Value(),
        "<ResourceDictionary xmlns=\"urn:aero\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Color x:Key=\"DeferredAccent\" Value=\"#FF0000FF\"/>"
        "</ResourceDictionary>"));
    CHECK(view.Host().LoadResources(
        RuntimeResourceLayer::Application,
        resourcesUri.Value().Canonical()));

    Result<UiDocument> loaded = view.Host().ParseUiDocument(
        "<Border xmlns=\"urn:aero\" "
        "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
        "x:Name=\"Target\" "
        "Background=\"{DynamicResource DeferredAccent}\"/>");
    CHECK(loaded);
    auto* pending = static_cast<Border*>(
        loaded.Value().FindNamedObject(
            "Target", Border::StaticTypeId()));
    CHECK(pending != nullptr);
    CHECK(pending->Background().blue == 0.0F);
    CHECK(view.Host().Mount(std::move(loaded).Value(), {320.0, 180.0}));
    CHECK(view.Host().RunFrame());
    Border* target = view.Host().FindNamed<Border>("Target");
    CHECK(target != nullptr);
    CHECK(target->Background().blue == 1.0F);
    CHECK(view.Host().Unmount());
    return true;
}

bool TestUiDocumentIsViewAffineAndSafeAfterShutdown() {
    RuntimeEnvironment environment;
    CHECK(environment.Initialize());
    RuntimeView first(environment);
    RuntimeView second(environment);
    CHECK(first.Initialize());
    CHECK(second.Initialize());

    Result<UiDocument> loaded = first.Host().ParseUiDocument(
        "<Border xmlns=\"urn:aero\" Width=\"80\"/>");
    CHECK(loaded);
    UiDocument document = std::move(loaded).Value();
    Result<void> wrongView = second.Host().Mount(
        std::move(document), {100.0, 100.0});
    CHECK(!wrongView);
    CHECK(wrongView.GetStatus().code == ErrorCode::InvalidArgument);
    CHECK(document.IsValid());
    CHECK(first.Host().Mount(std::move(document), {100.0, 100.0}));
    CHECK(first.Host().Unmount());

    UiDocument detached;
    {
        RuntimeView temporary(environment);
        CHECK(temporary.Initialize());
        EmbeddedXamlSourceProvider* embedded =
            temporary.Host().EmbeddedXamlSources();
        CHECK(embedded != nullptr);
        Result<ResourceUri> resourceUri = ResourceUri::Parse(
            "pack://application:,,,/Aero.Tests;component/Detached/Resources.xaml");
        CHECK(resourceUri);
        CHECK(embedded->TryAddText(
            resourceUri.Value(),
            "<ResourceDictionary xmlns=\"urn:aero\" "
            "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
            "<Color x:Key=\"SafeAccent\" Value=\"#FFFF0000\"/>"
            "</ResourceDictionary>"));
        CHECK(temporary.Host().LoadResources(
            RuntimeResourceLayer::Application,
            resourceUri.Value().Canonical()));
        Result<UiDocument> pending = temporary.Host().ParseUiDocument(
            "<Border xmlns=\"urn:aero\" "
            "Background=\"{DynamicResource SafeAccent}\"/>");
        CHECK(pending);
        detached = std::move(pending).Value();
    }
    CHECK(detached.IsValid());
    detached = UiDocument{};
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
    if (!TestModuleDependencyOrderAndValidation()) return 1;
    if (!TestHostDrivenRenderQueue()) return 1;
    if (!TestEnvironmentStateOutlivesFacade()) return 1;
    if (!TestUiDocumentDefersEffectsUntilMount()) return 1;
    if (!TestUiDocumentIsViewAffineAndSafeAfterShutdown()) return 1;
    if (!TestRuntimeHostLifecycle()) return 1;
    if (!TestRuntimeHostHighLevelMarkupApi()) return 1;
    if (!TestRuntimeHostResourceDictionaryDependencies()) return 1;
    if (!TestRuntimeHostImplicitAndExplicitStyles()) return 1;
    if (!TestRuntimeHostResourceLayers()) return 1;
    if (!TestRuntimeHostDynamicResourceChain()) return 1;
    if (!TestRuntimeHostXamlTemplate()) return 1;
    if (!RunRuntimeWindowTests()) return 1;
    if (!TestMutationJournalRollbackOrder()) return 1;
    if (!TestSafeDeferredWorkSkipsDestroyedObjects()) return 1;
    if (!TestEventRouteLifetimeSnapshot()) return 1;
    if (AeroPhase1EmbeddedMain() != 0) return 1;
    if (!RunM1M4ClosureTests()) return 1;
    std::puts("Aero module/runtime tests passed");
    return 0;
}
