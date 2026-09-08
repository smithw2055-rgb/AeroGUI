#include <Aero/Gui.hpp>
#include <Aero/View.hpp>
#include <Aero/TryCast.hpp>
#include <Aero/UIElement.hpp>

#include <Aero/Markup/XamlReader.hpp>
#include <Aero/Markup/XamlProvider.hpp>
#include <Aero/Media/TextureProvider.hpp>
#include <Aero/Media/FontProvider.hpp>
#include <Aero/ViewOptions.hpp>
#include "gui/GuiState.hpp"
#include "gui/ViewState.hpp"
#include <Aero/BuiltinThemes.generated.hpp>
#include <Aero/Base/String.hpp>

#include <new>
#include <utility>


namespace Aero {

Base::Result<void> RegisterDefaultXamlProviders(
    Markup::XamlProviderRegistry& providers,
    const Ref<Markup::EmbeddedXamlProvider>& embedded,
    const Ref<Markup::FileXamlProvider>& file) noexcept {
    if (!embedded || !file) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Built-in XAML providers are unavailable");
    }
    Base::Result<Base::ResourceUri> light = BuiltInThemeUri("Light.xaml");
    if (!light) return light.GetStatus();
    Base::Result<void> status = embedded->Add(
        light.Value(), {::Aero::AeroThemeLightSource,
            static_cast<std::uint32_t>(sizeof(::Aero::AeroThemeLightSource))});
    if (!status) return status.GetStatus();
    Base::Result<Base::ResourceUri> dark = BuiltInThemeUri("Dark.xaml");
    if (!dark) return dark.GetStatus();
    status = embedded->Add(
        dark.Value(), {::Aero::AeroThemeDarkSource,
            static_cast<std::uint32_t>(sizeof(::Aero::AeroThemeDarkSource))});
    if (!status) return status.GetStatus();
    Base::Result<Base::ResourceUri> generic = BuiltInThemeUri("Generic.xaml");
    if (!generic) return generic.GetStatus();
    status = embedded->Add(
        generic.Value(), {::Aero::AeroThemeGenericSource,
            static_cast<std::uint32_t>(sizeof(::Aero::AeroThemeGenericSource))});
    if (!status) return status.GetStatus();
    auto addEmbedded = [&](Base::StringView uri,
                           const std::uint8_t* bytes,
                           std::uint32_t size) noexcept -> Base::Result<void> {
        Base::Result<Base::ResourceUri> parsed =
            Base::ResourceUri::Parse(uri);
        if (!parsed) return parsed.GetStatus();
        return embedded->Add(parsed.Value(), {bytes, size});
    };
    struct ExtensionSource {
        Base::StringView name;
        const std::uint8_t* bytes;
        std::uint32_t size;
    };
    const ExtensionSource extensionSources[] = {
        {"AeroTheme.LightBlue.xaml", ::Aero::AeroExtensionsLightSource,
            ::Aero::AeroExtensionsLightSourceSize},
        {"AeroTheme.DarkBlue.xaml", ::Aero::AeroExtensionsDarkSource,
            ::Aero::AeroExtensionsDarkSourceSize},
        {"AeroTheme.Brushes.LightBlue.xaml",
            ::Aero::AeroExtensionsLightBrushesSource,
            ::Aero::AeroExtensionsLightBrushesSourceSize},
        {"AeroTheme.Brushes.DarkBlue.xaml",
            ::Aero::AeroExtensionsDarkBrushesSource,
            ::Aero::AeroExtensionsDarkBrushesSourceSize},
        {"AeroTheme.Colors.Light.xaml",
            ::Aero::AeroExtensionsLightColorsSource,
            ::Aero::AeroExtensionsLightColorsSourceSize},
        {"AeroTheme.Colors.Dark.xaml",
            ::Aero::AeroExtensionsDarkColorsSource,
            ::Aero::AeroExtensionsDarkColorsSourceSize},
        {"AeroTheme.Fonts.xaml", ::Aero::AeroExtensionsFontsSource,
            ::Aero::AeroExtensionsFontsSourceSize},
        {"AeroTheme.Styles.xaml", ::Aero::AeroExtensionsStylesSource,
            ::Aero::AeroExtensionsStylesSourceSize}};
    auto rewriteThemeFileName = [](Base::StringView aeroName,
                                   Base::StringView themePrefix,
                                   Base::String& out) noexcept -> Base::Result<void> {
        constexpr Base::StringView aeroPrefix("AeroTheme");
        Base::Result<void> assigned = out.Assign(themePrefix);
        if (!assigned) return assigned.GetStatus();
        if (aeroName.SizeBytes() >= aeroPrefix.SizeBytes() &&
            aeroName.Substr(0U, aeroPrefix.SizeBytes()) == aeroPrefix) {
            return out.Append(aeroName.Substr(
                aeroPrefix.SizeBytes(),
                aeroName.SizeBytes() - aeroPrefix.SizeBytes()));
        }
        return out.Append(aeroName);
    };
    const struct {
        Base::StringView assembly;
        Base::StringView themePrefix;
    } extensionAssemblies[] = {
        {Base::StringView("Aero.GUI.Extensions"),
         Base::StringView("AeroTheme")},
        // Tutorial/ControlGallery App.xaml still references the Noesis pack
        // assembly and NoesisTheme.* file names. Serve the same Aero theme
        // bytes under that spelling so Source= pack URIs load.
        {Base::StringView("Noesis.GUI.Extensions"),
         Base::StringView("NoesisTheme")}};
    for (const auto& assembly : extensionAssemblies) {
        for (const ExtensionSource& source : extensionSources) {
            Base::String fileName;
            status = rewriteThemeFileName(
                source.name, assembly.themePrefix, fileName);
            if (!status) return status.GetStatus();
            Base::String uri;
            status = uri.Assign("pack://application:,,,/");
            if (!status) return status.GetStatus();
            status = uri.Append(assembly.assembly);
            if (!status) return status.GetStatus();
            status = uri.Append(";component/Theme/");
            if (!status) return status.GetStatus();
            status = uri.Append(fileName.View());
            if (!status) return status.GetStatus();
            status = addEmbedded(
                uri.View(), source.bytes, source.size);
            if (!status) return status.GetStatus();
        }
    }
    // WPF also accepts a leading-slash component URI without an explicit
    // pack scheme. Keep both root dictionaries available under that spelling.
    status = addEmbedded(
        "/Aero.GUI.Extensions;component/Theme/AeroTheme.LightBlue.xaml",
        ::Aero::AeroExtensionsLightSource,
        ::Aero::AeroExtensionsLightSourceSize);
    if (!status) return status.GetStatus();
    status = addEmbedded(
        "/Aero.GUI.Extensions;component/Theme/AeroTheme.DarkBlue.xaml",
        ::Aero::AeroExtensionsDarkSource,
        ::Aero::AeroExtensionsDarkSourceSize);
    if (!status) return status.GetStatus();
    status = addEmbedded(
        "/Noesis.GUI.Extensions;component/Theme/NoesisTheme.LightBlue.xaml",
        ::Aero::AeroExtensionsLightSource,
        ::Aero::AeroExtensionsLightSourceSize);
    if (!status) return status.GetStatus();
    status = addEmbedded(
        "/Noesis.GUI.Extensions;component/Theme/NoesisTheme.DarkBlue.xaml",
        ::Aero::AeroExtensionsDarkSource,
        ::Aero::AeroExtensionsDarkSourceSize);
    if (!status) return status.GetStatus();

    auto registerProvider = [&](Ref<Markup::XamlProvider> provider,
                                Base::StringView scheme) noexcept -> Base::Result<void> {
        return providers.Set(std::move(provider), scheme);
    };
    status = registerProvider(embedded, "pack");
    if (!status) return status.GetStatus();
    status = providers.Set(
        embedded, {}, "Aero.GUI.Extensions");
    if (!status) return status.GetStatus();
    status = providers.Set(
        embedded, {}, "Noesis.GUI.Extensions");
    if (!status) return status.GetStatus();
    status = registerProvider(file, "file");
    if (!status) return status.GetStatus();
    return registerProvider(file, {});
}

} // namespace Aero


namespace Aero {

namespace {

void RemovePendingDocument(
    GuiState& state,
    std::uint32_t index) noexcept {
    if (index + 1U < state.pendingDocuments.Size()) {
        state.pendingDocuments[index] =
            std::move(state.pendingDocuments.Back());
    }
    state.pendingDocuments.PopBack();
}

void CollectUnclaimedDocuments(GuiState& state) noexcept {
    std::uint32_t index = 0U;
    while (index < state.pendingDocuments.Size()) {
        const PendingXamlDocument& pending =
            state.pendingDocuments[index];
        const Base::Ref<Base::Object>& root =
            pending.document.root;
        if (!root ||
            root->UseCount() <= pending.internalRootReferences) {
            RemovePendingDocument(state, index);
            continue;
        }
        ++index;
    }
}

Base::Result<Base::Ref<Base::Object>> RetainLoadedDocument(
    GuiState& state,
    Markup::XamlDocument&& document,
    std::uint32_t externalRootReferences = 0U) noexcept {
    Base::Ref<Base::Object> root = document.Root();
    if (!root) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Loaded XAML document has no root object");
    }
    Markup::LoaderResult pending =
        Markup::TakeXamlDocument(document);
    const std::uint32_t rootReferences = root->UseCount();
    if (rootReferences <= externalRootReferences ||
        rootReferences - externalRootReferences <= 1U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Loaded XAML document lost its internal root ownership");
    }
    PendingXamlDocument retainedDocument{
        std::move(pending),
        rootReferences - externalRootReferences - 1U};
    Base::Result<PendingXamlDocument*> retained =
        state.pendingDocuments.EmplaceBack(
            std::move(retainedDocument));
    if (!retained) return retained.GetStatus();
    return root;
}

} // namespace


void GuiState::OnXamlChanged(const Base::ResourceUri& uri) noexcept {
    if (!dispatcher.CheckAccess()) return;
    if (uri.Empty()) {
        documents.Clear();
    } else {
        static_cast<void>(documents.Invalidate(uri, true));
    }
    XamlProviderChangeRecord record;
    record.uri = uri;
    record.generation = ++xamlChangeGeneration;
    xamlChanges.PushBack(std::move(record));
}

void GuiState::OnTextureChanged(const Base::ResourceUri& uri) noexcept {
    if (!dispatcher.CheckAccess()) return;
    XamlProviderChangeRecord record;
    record.uri = uri;
    record.generation = ++textureChangeGeneration;
    textureChanges.PushBack(std::move(record));
}

void GuiState::OnFontChanged(const Media::FontProviderChange& change) noexcept {
    if (!dispatcher.CheckAccess()) return;
    fontChangedBaseUri = change.baseUri;
    static_cast<void>(fontChangedFamily.Assign(change.familyName));
    ++fontChangeGeneration;
}

Base::Result<void> GuiState::QuerySource(
    const Base::ResourceUri& uri,
    std::uint64_t& sourceIdentity,
    std::uint64_t& revision) noexcept {
    if (uri.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML source URI is empty");
    }
    Base::Result<Markup::XamlProviderResolution> resolved =
        xamlProviders.ResolveDetailed(uri);
    if (!resolved) return resolved.GetStatus();
    if (resolved.Value().provider == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML source provider is unavailable");
    }
    sourceIdentity = resolved.Value().cacheIdentity;

    Base::Result<std::uint64_t> probed =
        resolved.Value().provider->Revision(uri);
    if (probed && probed.Value() != 0U) {
        revision = probed.Value();
        return {};
    }
    Base::Result<::Aero::Markup::StreamResourceInfo> source =
        resolved.Value().provider->Open(uri);
    if (!source) return source.GetStatus();
    if (source.Value().revision != 0U) {
        revision = source.Value().revision;
        return {};
    }
    if (!source.Value().stream) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML source stream is invalid");
    }

    constexpr Base::HashCode OffsetBasis =
        UINT64_C(14695981039346656037);
    constexpr Base::HashCode Prime = UINT64_C(1099511628211);
    Base::HashCode hash = OffsetBasis ^ Base::MixHash64(0U);
    std::uint64_t size = 0U;
    std::uint8_t buffer[4096];
    for (;;) {
        Base::Result<std::uint32_t> read =
            source.Value().stream->Read({buffer, sizeof(buffer)});
        if (!read) return read.GetStatus();
        if (read.Value() == 0U) break;
        for (std::uint32_t index = 0U;
             index < read.Value(); ++index) {
            hash ^= static_cast<Base::HashCode>(buffer[index]);
            hash *= Prime;
        }
        size += read.Value();
    }
    revision = Base::MixHash64(hash ^ size);
    return {};
}

Gui::Gui(
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    Base::Result<Base::Ref<GuiState>> made =
        Base::MakeRefWithAllocator<GuiState>(
            *allocator_, *allocator_);
    if (!made) {
        Base::ReportOutOfMemory(
            sizeof(GuiState),
            alignof(GuiState),
            Base::MemoryTag::Object);
    }
    state_ = Base::Ref<Base::Object>(std::move(made).Value());
}

Gui::~Gui() noexcept {
    state_.Reset();
}

Base::Result<void> Gui::AddModule(
    const ModuleRegistration& registration) noexcept {
    GuiState& state = static_cast<GuiState&>(*state_);
    if (state.initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Gui modules are frozen");
    }
    return state.modules.Add(registration);
}

Base::Result<void> Gui::SetXamlProvider(
    Ref<Markup::XamlProvider> provider,
    Base::StringView scheme,
    Base::StringView assembly) noexcept {
    GuiState& state = static_cast<GuiState&>(*state_);
    if (state.initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Gui providers are frozen after Initialize");
    }
    if (!provider) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Gui XAML provider is required");
    }
    bool newlySubscribed = true;
    for (const Ref<Markup::XamlProvider>& subscribed :
         state.subscribedXamlProviders) {
        if (subscribed.Get() == provider.Get()) {
            newlySubscribed = false;
            break;
        }
    }
    if (newlySubscribed) {
        provider->AddChangedHandler(state.xamlChanged);
        state.subscribedXamlProviders.PushBack(provider);
    }
    Ref<Markup::XamlProvider> replaced;
    Base::Result<void> configured = state.xamlProviders.Set(
        std::move(provider), scheme, assembly, &replaced);
    if (!configured) {
        if (newlySubscribed) {
            Ref<Markup::XamlProvider> added =
                std::move(state.subscribedXamlProviders.Back());
            state.subscribedXamlProviders.PopBack();
            if (added) {
                static_cast<void>(added->RemoveChangedHandler(
                    state.xamlChanged));
            }
        }
        return configured.GetStatus();
    }
    if (replaced && !state.xamlProviders.Contains(*replaced)) {
        static_cast<void>(replaced->RemoveChangedHandler(state.xamlChanged));
        for (std::uint32_t index = 0U;
             index < state.subscribedXamlProviders.Size(); ++index) {
            if (state.subscribedXamlProviders[index].Get() != replaced.Get()) {
                continue;
            }
            if (index + 1U < state.subscribedXamlProviders.Size()) {
                state.subscribedXamlProviders[index] =
                    std::move(state.subscribedXamlProviders.Back());
            }
            state.subscribedXamlProviders.PopBack();
            break;
        }
    }
    return {};
}

Base::Result<void> Gui::SetTextureProvider(
    Ref<Media::TextureProvider> provider) noexcept {
    GuiState& state = static_cast<GuiState&>(*state_);
    if (state.initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Gui providers are frozen after Initialize");
    }
    if (!provider) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Gui texture provider is required");
    }
    if (state.textureProvider) {
        static_cast<void>(state.textureProvider->RemoveChangedHandler(
            state.textureChanged));
    }
    provider->AddChangedHandler(state.textureChanged);
    state.textureProvider = std::move(provider);
    return {};
}

Base::Result<void> Gui::SetFontProvider(
    Ref<Media::FontProvider> provider) noexcept {
    GuiState& state = static_cast<GuiState&>(*state_);
    if (state.initialized) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Gui providers are frozen after Initialize");
    }
    if (!provider) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Gui font provider is required");
    }
    if (state.fontProvider) {
        static_cast<void>(state.fontProvider->RemoveChangedHandler(
            state.fontChanged));
    }
    provider->AddChangedHandler(state.fontChanged);
    state.fontProvider = std::move(provider);
    return {};
}

Base::Result<void> Gui::Initialize() noexcept {
    GuiState& state = static_cast<GuiState&>(*state_);
    if (state.initialized) return {};
    Base::Result<void> prepared = state.schema.Prepare(state.modules);
    if (!prepared) return prepared.GetStatus();
    Base::Result<void> finalized = state.schema.Finalize(
        GuiSchemaOptions{state.allocator});
    if (!finalized) return finalized.GetStatus();
    Base::Result<Ref<Markup::EmbeddedXamlProvider>> embedded =
        Base::MakeRefWithAllocator<Markup::EmbeddedXamlProvider>(
            *state.allocator);
    if (!embedded) return embedded.GetStatus();
    Base::Result<Ref<Markup::FileXamlProvider>> file =
        Base::MakeRefWithAllocator<Markup::FileXamlProvider>(
            *state.allocator);
    if (!file) return file.GetStatus();
    state.embeddedXaml = std::move(embedded).Value();
    state.fileXaml = std::move(file).Value();
    Base::Result<void> providers =
        Aero::RegisterDefaultXamlProviders(
            state.builtinXamlProviders,
            state.embeddedXaml,
            state.fileXaml);
    if (!providers) return providers.GetStatus();
    Base::Result<void> frozen = state.modules.Freeze();
    if (!frozen) return frozen.GetStatus();
    state.initialized = true;
    return {};
}

Base::Result<Base::Ref<Base::Object>> Gui::LoadXamlRoot(
    Base::StringView uri,
    Base::MetaTypeId expectedRoot) noexcept {
    if (!IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Gui must be initialized before XAML loading");
    }
    GuiState& state = static_cast<GuiState&>(*state_);
    CollectUnclaimedDocuments(state);
    Markup::XamlReader reader(*this);
    Base::Result<Markup::XamlDocument> loaded = reader.Load(uri);
    if (!loaded) return loaded.GetStatus();
    const Base::Ref<Base::Object>& root = loaded.Value().Root();
    if (!root || expectedRoot == Base::InvalidMetaTypeId ||
        !state.schema.Metadata().Types().IsDerivedFrom(
            root->RuntimeType(), expectedRoot)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML root is incompatible with the requested type");
    }
    return RetainLoadedDocument(
        state, std::move(loaded).Value());
}

Base::Result<void> Gui::LoadComponent(
    Base::Object& component,
    Base::StringView uri,
    ResourceDictionary* resources) noexcept {
    if (!IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Gui must be initialized before XAML component loading");
    }
    const std::uint32_t externalRootReferences =
        component.UseCount();
    Base::Ref<Base::Object> root =
        Base::Ref<Base::Object>::TryFromBorrowed(component);
    if (!root) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML component requires a managed root object");
    }
    GuiState& state = static_cast<GuiState&>(*state_);
    CollectUnclaimedDocuments(state);
    Markup::XamlReader reader(*this);
    Base::Result<Markup::XamlDocument> loaded =
        reader.LoadComponentInto(
            std::move(root), uri, {}, nullptr, resources);
    if (!loaded) return loaded.GetStatus();
    Base::Result<Base::Ref<Base::Object>> retained =
        RetainLoadedDocument(
            state,
            std::move(loaded).Value(),
            externalRootReferences);
    if (!retained) return retained.GetStatus();
    if (UIElement* element = TryCast<UIElement>(&component)) {
        if (ElementTree* tree = VisualTree(element)) {
            if (ViewState* viewState = tree->GetViewState()) {
                for (std::uint32_t index = 0U;
                     index < state.pendingDocuments.Size(); ++index) {
                    Markup::LoaderResult& pending =
                        state.pendingDocuments[index].document;
                    if (pending.root.Get() != &component) continue;
                    Markup::LoaderResult taken = std::move(pending);
                    RemovePendingDocument(state, index);
                    Base::Result<void> adopted =
                        AdoptLoadedComponent(
                            *viewState, std::move(taken));
                    if (!adopted) return adopted.GetStatus();
                    element->InvalidateMeasure();
                    element->InvalidateArrange();
                    break;
                }
            }
        }
    }
    return {};
}

Base::Result<bool> Gui::TakeLoadedDocument(
    Base::Object& root,
    Markup::XamlDocument& document) noexcept {
    GuiState& state = static_cast<GuiState&>(*state_);
    for (std::uint32_t index = 0U;
         index < state.pendingDocuments.Size(); ++index) {
        Markup::LoaderResult& pending =
            state.pendingDocuments[index].document;
        if (pending.root.Get() != &root) continue;
        Base::Result<Markup::XamlDocument> adopted =
            Markup::AdoptXamlDocument(
                std::move(pending), *state.allocator);
        if (!adopted) return adopted.GetStatus();
        document = std::move(adopted).Value();
        RemovePendingDocument(state, index);
        return true;
    }
    return false;
}

Base::Result<Base::Ref<View>> Gui::CreateView(
    Base::IAllocator* allocator) noexcept {
    return CreateView(ViewOptions{}, allocator);
}

Base::Result<Base::Ref<View>> Gui::CreateView(
    const ViewOptions& options,
    Base::IAllocator* allocator) noexcept {
    if (!IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "Gui must be initialized before creating a view");
    }
    Base::IAllocator& selected = allocator != nullptr
        ? *allocator : *allocator_;
    Base::Result<Base::Ref<View>> made =
        Base::MakeRefWithAllocator<View>(
            selected,
            View::ConstructionToken{},
            *this,
            &selected);
    if (!made) return made.GetStatus();
    Base::Result<void> initialized =
        made.Value()->Initialize(options);
    if (!initialized) return initialized.GetStatus();
    return std::move(made).Value();
}

Base::Result<Base::Ref<View>> Gui::CreateView(
    Base::Ref<FrameworkElement> content,
    const ViewOptions& options,
    Base::IAllocator* allocator) noexcept {
    if (!content) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "View content is required");
    }
    Base::Result<Base::Ref<View>> made =
        CreateView(options, allocator);
    if (!made) return made.GetStatus();
    Base::Result<void> mounted =
        made.Value()->SetContent(std::move(content), Aero::Size{});
    if (!mounted) return mounted.GetStatus();
    return std::move(made).Value();
}

bool Gui::IsInitialized() const noexcept {
    const GuiState& state = static_cast<const GuiState&>(*state_);
    return state.initialized && state.schema.IsFrozen();
}

} // namespace Aero
