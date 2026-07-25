from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def replace_section(text: str, start: str, end: str, replacement: str, label: str) -> str:
    begin = text.find(start)
    if begin < 0:
        raise RuntimeError(f"{label}: start marker not found")
    finish = text.find(end, begin)
    if finish < 0:
        raise RuntimeError(f"{label}: end marker not found")
    return text[:begin] + replacement.rstrip() + "\n\n" + text[finish:]


path = "samples/ControlGallery/GalleryRuntime.cpp"
text = read(path)
text = replace_once(text,
    "#include <Aero/Markup/XamlCompiledDocument.hpp>\n",
    "#include <Aero/Markup/RuntimeHost.hpp>\n#include <Aero/Markup/XamlCompiledDocument.hpp>\n",
    "ControlGallery RuntimeHost include")
text = replace_section(text,
    "struct GalleryRuntime::Impl final {",
    "    Result<void> LoadDocument(",
    """struct GalleryRuntime::Impl final {\n    NullRenderBackend nullBackend;\n    RuntimeHost runtime;\n    Base::Ref<Base::Object> root;\n    Base::Vector<Control*> themedControls;\n    GalleryItemsSource items;\n    DataTemplate itemTemplate{&MakeVirtualizedItem, nullptr};\n    std::unique_ptr<ItemContainerGenerator> generator;\n    ListBox* listBox = nullptr;\n    VirtualizingStackPanel* virtualizingPanel = nullptr;\n    GallerySnapshot snapshot;\n\n    ~Impl() { Cleanup(); }\n\n    MetadataDomain& Metadata() noexcept { return *runtime.Metadata(); }\n    XamlObjectWriter& Writer() noexcept { return *runtime.Writer(); }\n\n""",
    "ControlGallery RuntimeHost fields")
text = replace_section(text,
    "    Result<void> LoadDocument(",
    "    Result<void> LoadTheme(",
    """    Result<void> LoadDocument(\n        const std::string& assetDirectory,\n        GalleryLoadMode mode) noexcept {\n        const std::string sourcePath = assetDirectory + \"/ControlGallery.xaml\";\n        std::vector<std::uint8_t> source;\n        if (mode == GalleryLoadMode::Runtime) {\n            if (!ReadFile(sourcePath, source)) {\n                return Failure(\"ControlGallery runtime XAML is unavailable\");\n            }\n            DiagnosticBag diagnostics;\n            Utf8XmlTokenizer tokenizer;\n            Result<void> reset = tokenizer.Reset({\n                reinterpret_cast<const char*>(source.data()),\n                static_cast<std::uint32_t>(source.size())}, &diagnostics);\n            if (!reset) return reset.GetStatus();\n            XamlNodeReader reader(tokenizer, &diagnostics);\n            Result<Ref<Object>> loaded = runtime.Load(reader);\n            if (!loaded) return loaded.GetStatus();\n            root = std::move(loaded).Value();\n            return {};\n        }\n\n        const std::string compiledPath = assetDirectory + \"/ControlGallery.axir\";\n        if (!ReadFile(compiledPath, source)) {\n            return Failure(\"ControlGallery compiled XAML is unavailable\");\n        }\n        Result<XamlCompiledDocument> document =\n            XamlCompiledDocument::Deserialize(\n                {source.data(), static_cast<std::uint32_t>(source.size())},\n                Metadata(), {}, runtime.Modules().ManifestHash());\n        if (!document) return document.GetStatus();\n        Result<Ref<Object>> loaded = runtime.Load(document.Value());\n        if (!loaded) return loaded.GetStatus();\n        root = std::move(loaded).Value();\n        return {};\n    }\n""",
    "ControlGallery RuntimeHost load")
text = replace_section(text,
    "    Result<void> LoadTheme(",
    "    Result<void> ApplyTheme()",
    """    Result<void> LoadTheme(\n        const std::string& assetDirectory,\n        GalleryTheme requested) noexcept {\n        std::vector<std::uint8_t> generic;\n        std::vector<std::uint8_t> palette;\n        if (!ReadFile(assetDirectory + \"/themes/Generic.xaml\", generic) ||\n            !ReadFile(assetDirectory +\n                (requested == GalleryTheme::Light\n                    ? \"/themes/Light.xaml\" : \"/themes/Dark.xaml\"),\n                palette)) {\n            return Failure(\"ControlGallery theme assets are unavailable\");\n        }\n        Result<std::unique_ptr<XamlTheme>> loaded = XamlTheme::Load(\n            {reinterpret_cast<const char*>(generic.data()),\n             static_cast<std::uint32_t>(generic.size())},\n            {reinterpret_cast<const char*>(palette.data()),\n             static_cast<std::uint32_t>(palette.size())},\n            Metadata().DependencyProperties());\n        if (!loaded) return loaded.GetStatus();\n        theme = std::move(loaded).Value();\n        return {};\n    }\n\n    std::unique_ptr<XamlTheme> theme;\n""",
    "ControlGallery RuntimeHost theme load")
text = replace_section(text,
    "    Result<void> ApplyTheme()",
    "    Result<void> ConfigureBinding()",
    """    Result<void> ApplyTheme() noexcept {\n        const StringView names[] = {\n            \"PrimaryButton\", \"FeatureCheck\", \"LightChoice\",\n            \"DarkChoice\", \"BigList\"};\n        for (StringView name : names) {\n            Object* object = Writer().DocumentNameScope().Find(name);\n            if (object == nullptr ||\n                !Metadata().Descriptors().IsDerivedFrom(\n                    object->RuntimeType(), Control::StaticTypeId())) {\n                return Failure(\"ControlGallery themed control is missing\");\n            }\n            auto* control = static_cast<Control*>(object);\n            if (theme->FindTemplate(control->RuntimeType()) == nullptr) continue;\n            Result<TemplateHandle> applied =\n                theme->Apply(*runtime.Templates(), *control);\n            if (!applied) return applied.GetStatus();\n            Result<void> tracked = themedControls.TryPushBack(control);\n            if (!tracked) return tracked.GetStatus();\n        }\n        return {};\n    }\n""",
    "ControlGallery RuntimeHost theme apply")
text = replace_section(text,
    "    Result<void> ConfigureBinding()",
    "    Result<void> ConfigureVirtualization()",
    """    Result<void> ConfigureBinding() noexcept {\n        TextBox* input = FindNamed<TextBox>(Writer(), \"Input\");\n        TextBlock* mirror = FindNamed<TextBlock>(Writer(), \"BindingMirror\");\n        if (input == nullptr || mirror == nullptr) {\n            return Failure(\"ControlGallery binding endpoints are missing\");\n        }\n        BindingDescriptor descriptor;\n        descriptor.source = input;\n        descriptor.sourceProperty = TextBox::TextProperty;\n        descriptor.target = mirror;\n        descriptor.targetProperty = TextBlock::TextProperty;\n        descriptor.mode = BindingMode::OneWay;\n        Result<BindingHandle> attached = runtime.Bindings()->Attach(descriptor);\n        if (!attached) return attached.GetStatus();\n        Result<void> changed = input->SetText(\"Binding validation passed\");\n        if (!changed) return changed.GetStatus();\n        Result<std::uint32_t> flushed = runtime.Bindings()->Flush();\n        if (!flushed) return flushed.GetStatus();\n        if (mirror->Text() != input->Text()) {\n            return Failure(\"ControlGallery binding validation failed\");\n        }\n        return {};\n    }\n""",
    "ControlGallery RuntimeHost binding")
text = replace_section(text,
    "    Result<void> ConfigureVirtualization()",
    "    Result<void> RunFrame()",
    """    Result<void> ConfigureVirtualization() noexcept {\n        listBox = FindNamed<ListBox>(Writer(), \"BigList\");\n        virtualizingPanel =\n            FindNamed<VirtualizingStackPanel>(Writer(), \"BigListPanel\");\n        if (listBox == nullptr || virtualizingPanel == nullptr) {\n            return Failure(\"ControlGallery virtualization endpoints are missing\");\n        }\n        Result<void> initialized = items.Initialize();\n        if (!initialized) return initialized.GetStatus();\n        listBox->SetItemTemplate(&itemTemplate);\n        Result<void> source = listBox->SetItemsSource(&items);\n        if (!source) return source.GetStatus();\n        Result<void> extent = virtualizingPanel->SetEstimatedItemExtent(24.0);\n        if (extent) extent = virtualizingPanel->SetOverscanCount(2U);\n        if (!extent) return extent.GetStatus();\n        Result<bool> viewport = virtualizingPanel->SetViewport({360.0, 120.0});\n        if (!viewport) return viewport.GetStatus();\n        generator = std::make_unique<ItemContainerGenerator>(\n            *runtime.Tree(), *runtime.Layout(), *runtime.EffectiveValues(),\n            nullptr, runtime.Renderer());\n        Result<void> attached =\n            generator->AttachVirtualized(*listBox, *virtualizingPanel);\n        if (!attached) return attached.GetStatus();\n        Result<bool> scrolled = virtualizingPanel->SetVerticalOffset(4800.0);\n        if (!scrolled) return scrolled.GetStatus();\n        return {};\n    }\n""",
    "ControlGallery RuntimeHost virtualization")
text = replace_section(text,
    "    Result<void> RunFrame()",
    "    void Cleanup()",
    """    Result<void> RunFrame() noexcept {\n        Result<RuntimeFrameResult> frame = runtime.RunFrame();\n        return frame ? Result<void>() : Result<void>(frame.GetStatus());\n    }\n""",
    "ControlGallery RuntimeHost frame")
text = replace_section(text,
    "    void Cleanup()",
    "};\n\nGalleryRuntime::GalleryRuntime",
    """    void Cleanup() noexcept {\n        if (generator) {\n            (void)generator->Detach();\n            generator.reset();\n        }\n        if (listBox != nullptr) {\n            (void)listBox->SetItemsSource(nullptr);\n            listBox->SetItemTemplate(nullptr);\n        }\n        listBox = nullptr;\n        virtualizingPanel = nullptr;\n        if (runtime.Templates() != nullptr) {\n            for (Control* control : themedControls) {\n                if (control != nullptr) (void)runtime.Templates()->Clear(*control);\n            }\n        }\n        themedControls.Clear();\n        if (runtime.IsMounted()) (void)runtime.Unmount();\n        root.Reset();\n        runtime.Shutdown();\n    }\n};\n\nGalleryRuntime::GalleryRuntime""",
    "ControlGallery RuntimeHost cleanup")
text = replace_section(text,
    "Result<void> GalleryRuntime::Initialize(",
    "void GalleryRuntime::Shutdown()",
    """Result<void> GalleryRuntime::Initialize(\n    StringView assetDirectory,\n    GalleryLoadMode loadMode,\n    GalleryTheme requestedTheme) noexcept {\n    if (impl_) {\n        return Failure(\"ControlGallery runtime is already initialized\");\n    }\n    if (assetDirectory.Empty()) {\n        return Status::Failure(\n            ErrorCode::InvalidArgument,\n            \"ControlGallery asset directory is empty\");\n    }\n\n    std::unique_ptr<Impl> state = std::make_unique<Impl>();\n    Result<void> status = state->runtime.Modules().TryAdd(\n        MakeStatusBadgeModuleManifest());\n    RuntimeHostOptions options;\n    options.renderBackend = &state->nullBackend;\n    if (status) status = state->runtime.Initialize(options);\n    if (!status) return status.GetStatus();\n\n    const std::string assetPath(\n        assetDirectory.Data(), assetDirectory.SizeBytes());\n    status = state->LoadTheme(assetPath, requestedTheme);\n    if (status) status = state->LoadDocument(assetPath, loadMode);\n    if (!status) return status.GetStatus();\n    if (!state->root || state->root->RuntimeType() != Border::StaticTypeId()) {\n        return Failure(\"ControlGallery root is not a Border\");\n    }\n    status = state->runtime.Mount({900.0, 640.0});\n    if (status) status = state->ApplyTheme();\n    if (status) status = state->ConfigureBinding();\n    if (status) status = state->ConfigureVirtualization();\n    if (status) status = state->RunFrame();\n    if (!status) return status.GetStatus();\n\n    const RenderPlan& plan = state->runtime.Renderer()->CurrentPlan();\n    state->snapshot.planHash = plan.StableHash();\n    state->snapshot.nodeCount = plan.Nodes().Size();\n    state->snapshot.commandCount = plan.Commands().Size();\n    state->snapshot.namedObjectCount =\n        state->Writer().DocumentNameScope().Size();\n    state->snapshot.itemCount = state->items.Count();\n    state->snapshot.realizedItemCount = state->generator->GeneratedCount();\n    state->snapshot.createdContainerCount =\n        state->generator->CreatedContainerCount();\n    state->snapshot.loadMode = loadMode;\n    state->snapshot.theme = requestedTheme;\n    if (state->snapshot.nodeCount == 0U ||\n        state->snapshot.commandCount == 0U ||\n        state->snapshot.namedObjectCount < 10U ||\n        state->snapshot.itemCount != 10000U ||\n        state->snapshot.realizedItemCount == 0U ||\n        state->snapshot.realizedItemCount >= state->snapshot.itemCount ||\n        state->snapshot.createdContainerCount > 16U) {\n        std::fprintf(stderr,\n            \"ControlGallery metrics: nodes=%u commands=%u names=%u \"\n            \"items=%u realized=%u created=%u\\n\",\n            state->snapshot.nodeCount, state->snapshot.commandCount,\n            state->snapshot.namedObjectCount, state->snapshot.itemCount,\n            state->snapshot.realizedItemCount,\n            state->snapshot.createdContainerCount);\n        return Failure(\"ControlGallery acceptance metrics are invalid\");\n    }\n    impl_ = std::move(state);\n    return {};\n}\n\nvoid GalleryRuntime::Shutdown()""",
    "ControlGallery RuntimeHost initialization")
text = replace_once(text,
    """    return impl_\n        ? impl_->renderer->CurrentPlan()\n        : empty;\n""",
    """    return impl_ && impl_->runtime.Renderer() != nullptr\n        ? impl_->runtime.Renderer()->CurrentPlan()\n        : empty;\n""",
    "ControlGallery RuntimeHost plan accessor")
write(path, text)
