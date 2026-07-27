#include <Aero/Markup/Runtime/XamlLoader.hpp>

#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Markup/Runtime/XamlActivation.hpp>
#if AERO_WITH_EXPAT
#include <Aero/Markup/Parsing/ExpatXmlTokenizer.hpp>
#endif
#include <Aero/Markup/Runtime/XamlLoadSession.hpp>
#include <Aero/Markup/Runtime/XamlDocumentCache.hpp>
#include <Aero/Markup/Parsing/XamlNodeReader.hpp>
#include <Aero/Markup/Schema/XamlSchemaContext.hpp>
#include <Aero/Presentation/Rendering.hpp>
#include <Aero/Presentation/Resources.hpp>

#include <cstdio>
#include <filesystem>
#include <utility>

namespace Aero::Markup {
namespace {

char ToLowerAscii(char value) noexcept {
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value - 'A' + 'a')
        : value;
}

Base::Result<void> AssignLowerAscii(
    Base::String& output,
    Base::StringView value) noexcept {
    Base::String replacement(&output.Allocator());
    Base::Result<void> reserve =
        replacement.TryReserve(value.SizeBytes());
    if (!reserve) {
        return reserve.GetStatus();
    }
    for (char character : value) {
        const char lower = ToLowerAscii(character);
        Base::Result<void> append =
            replacement.TryAppendUnchecked(
                Base::StringView(&lower, 1U));
        if (!append) {
            return append.GetStatus();
        }
    }
    output = std::move(replacement);
    return {};
}

bool RegistrationMatches(
    const XamlSourceProviderRegistration& registration,
    const Base::ResourceUri& uri,
    bool requireScheme,
    bool requireAssembly) noexcept {
    const bool schemeMatches = requireScheme
        ? !registration.scheme.Empty() &&
            registration.scheme.View() == uri.Scheme()
        : registration.scheme.Empty();
    const bool assemblyMatches = requireAssembly
        ? !registration.assembly.Empty() &&
            registration.assembly.View() == uri.Assembly()
        : registration.assembly.Empty();
    return schemeMatches && assemblyMatches;
}

Base::Result<XamlSource> CloneSource(
    const Base::ResourceUri& uri,
    Base::Span<const std::uint8_t> bytes,
    std::uint64_t revision) noexcept {
    XamlSource source;
    source.uri = uri;
    Base::Result<void> copied = source.bytes.TryAppend(bytes);
    if (!copied) {
        return copied.GetStatus();
    }
    source.revision = revision;
    return source;
}

Base::Result<Base::ResourceUri> ResolveRequestedUri(
    Base::StringView uri,
    const XamlLoadOptions& options) noexcept {
    if (!options.baseUri.Empty()) {
        return Base::ResourceUri::Resolve(
            options.baseUri, uri);
    }
    return Base::ResourceUri::Parse(uri);
}

} // namespace

Base::Result<void> XamlSourceProviderRegistry::TryRegister(
    IXamlSourceProvider& provider,
    Base::StringView scheme,
    Base::StringView assembly) noexcept {
    XamlSourceProviderRegistration registration;
    Base::Result<void> schemeResult =
        AssignLowerAscii(registration.scheme, scheme);
    if (!schemeResult) {
        return schemeResult.GetStatus();
    }
    Base::Result<void> assemblyResult =
        registration.assembly.TryAssign(assembly);
    if (!assemblyResult) {
        return assemblyResult.GetStatus();
    }
    registration.provider = &provider;

    for (const XamlSourceProviderRegistration& existing :
         registrations_) {
        if (existing.scheme.View() == registration.scheme.View() &&
            existing.assembly.View() ==
                registration.assembly.View()) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "A XAML source provider is already registered for this route");
        }
    }
    return registrations_.TryPushBack(
        std::move(registration));
}

Base::Result<XamlSourceProviderResolution>
XamlSourceProviderRegistry::ResolveDetailed(
    const Base::ResourceUri& uri) const noexcept {
    const struct Route final {
        bool scheme;
        bool assembly;
    } routes[] = {
        {true, true},
        {true, false},
        {false, true},
        {false, false}};

    for (const Route route : routes) {
        if ((route.scheme && uri.Scheme().Empty()) ||
            (route.assembly && uri.Assembly().Empty())) {
            continue;
        }
        for (const XamlSourceProviderRegistration& registration :
             registrations_) {
            if (RegistrationMatches(
                    registration,
                    uri,
                    route.scheme,
                    route.assembly)) {
                XamlSourceProviderResolution result;
                result.provider = registration.provider;
                result.cacheIdentity = Base::MixHash64(
                    registration.provider->CacheIdentity() ^
                    Base::DefaultHash<Base::StringView>{}(
                        registration.scheme.View()) ^
                    Base::DefaultHash<Base::StringView>{}(
                        registration.assembly.View(), UINT64_C(0xA3E0)));
                return result;
            }
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "No XAML source provider matches the resource URI");
}

Base::Result<IXamlSourceProvider*>
XamlSourceProviderRegistry::Resolve(
    const Base::ResourceUri& uri) const noexcept {
    Base::Result<XamlSourceProviderResolution> resolved =
        ResolveDetailed(uri);
    return resolved
        ? Base::Result<IXamlSourceProvider*>(resolved.Value().provider)
        : Base::Result<IXamlSourceProvider*>(resolved.GetStatus());
}

Base::Result<void> EmbeddedXamlSourceProvider::TryAdd(
    const Base::ResourceUri& uri,
    Base::Span<const std::uint8_t> bytes,
    std::uint64_t revision) noexcept {
    if (frozen_) {
        return Base::Status::Failure(
            Base::ErrorCode::ReadOnly,
            "Embedded XAML source provider is frozen");
    }
    if (uri.Empty() || bytes.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Embedded XAML source registration is invalid");
    }
    for (const Entry& entry : entries_) {
        if (entry.uri == uri) {
            return Base::Status::Failure(
                Base::ErrorCode::AlreadyExists,
                "Embedded XAML source URI is already registered");
        }
    }

    Entry entry;
    entry.uri = uri;
    Base::Result<void> copied =
        entry.bytes.TryAppend(bytes);
    if (!copied) {
        return copied.GetStatus();
    }
    entry.revision = revision;
    Base::Result<void> stored = entries_.TryPushBack(std::move(entry));
    if (!stored) return stored.GetStatus();
    cacheIdentity_ = Base::HashBytes(
        uri.Canonical().Data(),
        uri.Canonical().SizeBytes(),
        cacheIdentity_ ^ revision);
    cacheIdentity_ = Base::HashBytes(
        bytes.Data(), bytes.Size(), cacheIdentity_);
    return {};
}

Base::Result<void> EmbeddedXamlSourceProvider::TryAddText(
    const Base::ResourceUri& uri,
    Base::StringView text,
    std::uint64_t revision) noexcept {
    return TryAdd(
        uri,
        Base::Span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(text.Data()),
            text.SizeBytes()),
        revision);
}

Base::Result<void> EmbeddedXamlSourceProvider::Freeze() noexcept {
    frozen_ = true;
    return {};
}

Base::Result<XamlSource> EmbeddedXamlSourceProvider::Load(
    const Base::ResourceUri& uri) const noexcept {
    for (const Entry& entry : entries_) {
        if (entry.uri == uri) {
            return CloneSource(
                entry.uri,
                entry.bytes.AsSpan(),
                entry.revision);
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "Embedded XAML source was not found");
}

Base::Result<std::uint64_t> EmbeddedXamlSourceProvider::Revision(
    const Base::ResourceUri& uri) const noexcept {
    for (const Entry& entry : entries_) {
        if (entry.uri == uri) return entry.revision;
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "Embedded XAML source was not found");
}

Base::Result<std::uint64_t> FileXamlSourceProvider::Revision(
    const Base::ResourceUri& uri) const noexcept {
    if ((!uri.Scheme().Empty() &&
         uri.Scheme() != Base::StringView("file")) ||
        uri.Path().Empty() || maxFileBytes_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "File XAML source URI is invalid");
    }
    Base::String path;
    Base::Result<void> assigned = path.TryAssign(uri.Path());
    if (!assigned) return assigned.GetStatus();
    std::error_code error;
    const std::filesystem::path filePath(path.CStr());
    const std::uintmax_t size =
        std::filesystem::file_size(filePath, error);
    if (error || size > maxFileBytes_ || size > UINT32_MAX) {
        return Base::Status::Failure(
            error ? Base::ErrorCode::NotFound : Base::ErrorCode::OutOfRange,
            "XAML source file revision could not be read");
    }
    const auto writeTime =
        std::filesystem::last_write_time(filePath, error);
    if (error) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML source file timestamp could not be read");
    }
    const std::uint64_t ticks = static_cast<std::uint64_t>(
        writeTime.time_since_epoch().count());
    return Base::MixHash64(
        static_cast<std::uint64_t>(size) ^ Base::MixHash64(ticks));
}

Base::Result<XamlSource> FileXamlSourceProvider::Load(
    const Base::ResourceUri& uri) const noexcept {
    if ((!uri.Scheme().Empty() &&
         uri.Scheme() != Base::StringView("file")) ||
        uri.Path().Empty() ||
        maxFileBytes_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "File XAML source URI is invalid");
    }

    Base::String path;
    Base::Result<void> assigned = path.TryAssign(uri.Path());
    if (!assigned) {
        return assigned.GetStatus();
    }
    std::FILE* file = nullptr;
#if defined(_MSC_VER)
    if (fopen_s(&file, path.CStr(), "rb") != 0) {
        file = nullptr;
    }
#else
    file = std::fopen(path.CStr(), "rb");
#endif
    if (file == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "XAML source file could not be opened");
    }
    if (std::fseek(file, 0L, SEEK_END) != 0) {
        std::fclose(file);
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "XAML source file size could not be read");
    }
    const long length = std::ftell(file);
    if (length < 0 ||
        static_cast<std::uint64_t>(length) > maxFileBytes_ ||
        static_cast<std::uint64_t>(length) > UINT32_MAX) {
        std::fclose(file);
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "XAML source file exceeds configured limits");
    }
    if (std::fseek(file, 0L, SEEK_SET) != 0) {
        std::fclose(file);
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "XAML source file could not be rewound");
    }

    XamlSource source;
    source.uri = uri;
    Base::Result<void> resized =
        source.bytes.TryResize(
            static_cast<std::uint32_t>(length));
    if (!resized) {
        std::fclose(file);
        return resized.GetStatus();
    }
    const std::size_t read = length == 0
        ? 0U
        : std::fread(
              source.bytes.Data(),
              1U,
              static_cast<std::size_t>(length),
              file);
    const bool closeOk = std::fclose(file) == 0;
    if (read != static_cast<std::size_t>(length) ||
        !closeOk) {
        return Base::Status::Failure(
            Base::ErrorCode::InternalError,
            "XAML source file could not be read completely");
    }
    Base::Result<std::uint64_t> revision = Revision(uri);
    source.revision = revision
        ? revision.Value()
        : Base::HashBytes(
              source.bytes.Data(), source.bytes.Size());
    return source;
}

struct XamlLoader::Operation final {
    struct FinalizeContext final {
        Operation* operation = nullptr;
        const XamlLoadOptions* options = nullptr;
        const Base::ResourceUri* origin = nullptr;
        const XamlCompiledDocument* compiled = nullptr;
    };

    struct PendingResourceMerge final {
        ResourceDictionary target;
        ResourceDictionary source;
    };

    Operation(
        XamlSchemaContext& schema,
        XamlSourceProviderRegistry& providers,
        Core::IDiagnosticSink* diagnostics) noexcept
        : schema_(&schema),
          providers_(&providers),
          diagnostics_(diagnostics) {}

    Base::Result<XamlLoadResult> LoadCore(
        const Base::ResourceUri& uri,
        const XamlLoadOptions& options,
        const Base::Ref<Base::Object>& existingRoot) noexcept;
    Base::Result<XamlLoadResult> ParseCore(
        Base::StringView text,
        const Base::ResourceUri& baseUri,
        const XamlLoadOptions& options,
        const Base::Ref<Base::Object>& existingRoot) noexcept;
    Base::Result<XamlLoadResult> LoadCompiled(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        const XamlLoadOptions& options) noexcept;
    Base::Result<XamlLoadResult> LoadCompiledDocument(
        XamlCompiledDocument& document,
        const Base::ResourceUri& originUri,
        const XamlLoadOptions& options,
        const Base::Ref<Base::Object>& existingRoot) noexcept;
    Base::Result<void> PopulateDocumentCache(
        const XamlSource& source,
        const Base::ResourceUri& origin,
        std::uint64_t sourceIdentity,
        const XamlLoadResult& loaded,
        const XamlLoadOptions& options) noexcept;
    Base::Result<void> ResolveResourceDependencies(
        XamlLoadResult& result,
        const XamlLoadOptions& options) noexcept;
    Base::Result<void> ResolveDictionaryDependencies(
        ResourceDictionary& dictionary,
        XamlLoadResult& owner,
        const XamlLoadOptions& options,
        std::uint32_t& resourceCount,
        Base::Vector<PendingResourceMerge>& pending) noexcept;
    Base::Result<void> CommitResourceDependencies(
        Base::Vector<PendingResourceMerge>& pending) noexcept;
    Base::Result<void> AppendDependencies(
        XamlLoadResult& destination,
        const XamlLoadResult& source,
        const XamlLoadOptions& options) noexcept;
    Base::Result<void> AppendDependency(
        XamlLoadResult& destination,
        const Base::ResourceUri& dependency,
        const XamlLoadOptions& options) noexcept;
    Base::Result<void> FinalizeResult(
        XamlLoadResult& result,
        const XamlLoadOptions& options,
        const Base::ResourceUri& origin,
        const XamlCompiledDocument* compiled) noexcept;
    static Base::Result<void> FinalizeLoad(
        XamlLoadResult& result,
        void* context) noexcept;
    Base::Result<void> ValidateOptions(
        const XamlLoadOptions& options) const noexcept;
    Base::Result<void> CheckPolicy(
        const Base::ResourceUri& uri,
        const XamlLoadOptions& options) noexcept;
    bool IsLoading(const Base::ResourceUri& uri) const noexcept;
    Base::Status Failure(
        Base::Status status,
        Core::DiagnosticCode code,
        Base::StringView message) noexcept;

    XamlSchemaContext* schema_ = nullptr;
    XamlSourceProviderRegistry* providers_ = nullptr;
    Core::IDiagnosticSink* diagnostics_ = nullptr;
    Base::Vector<Base::ResourceUri> loadStack_;
};

XamlLoader::XamlLoader(
    XamlSchemaContext& schema,
    XamlSourceProviderRegistry& providers,
    Core::IDiagnosticSink* diagnostics) noexcept
    : schema_(&schema),
      providers_(&providers),
      diagnostics_(diagnostics) {}

Base::Result<XamlLoadResult> XamlLoader::Load(
    Base::StringView uri,
    const XamlLoadOptions& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_);
    Base::Result<Base::ResourceUri> resolved =
        ResolveRequestedUri(uri, options);
    if (!resolved) {
        return operation.Failure(
            resolved.GetStatus(),
            XamlLoaderDiagnosticCodes::InvalidUri,
            Base::StringView("XAML resource URI is invalid"));
    }
    return operation.LoadCore(
        resolved.Value(), options, {});
}

Base::Result<XamlLoadResult> XamlLoader::Load(
    const Base::ResourceUri& uri,
    const XamlLoadOptions& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_);
    return operation.LoadCore(uri, options, {});
}

Base::Result<XamlLoadResult> XamlLoader::Parse(
    Base::StringView text,
    const Base::ResourceUri& baseUri,
    const XamlLoadOptions& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_);
    return operation.ParseCore(text, baseUri, options, {});
}

Base::Result<XamlLoadResult> XamlLoader::LoadComponent(
    Base::Object& existingRoot,
    Base::StringView uri,
    const XamlLoadOptions& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_);
    Base::Result<Base::ResourceUri> resolved =
        ResolveRequestedUri(uri, options);
    if (!resolved) {
        return operation.Failure(
            resolved.GetStatus(),
            XamlLoaderDiagnosticCodes::InvalidUri,
            Base::StringView("XAML component URI is invalid"));
    }
    Base::Ref<Base::Object> retained =
        Base::Ref<Base::Object>::TryFromBorrowed(existingRoot);
    if (!retained) {
        return operation.Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "LoadComponent requires a managed root object"),
            XamlLoaderDiagnosticCodes::LoadComponentTypeMismatch,
            Base::StringView(
                "XAML component root cannot be retained"));
    }
    return operation.LoadCore(
        resolved.Value(), options, retained);
}

Base::Result<XamlLoadResult> XamlLoader::LoadComponent(
    Base::Object& existingRoot,
    const Base::ResourceUri& uri,
    const XamlLoadOptions& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_);
    Base::Ref<Base::Object> retained =
        Base::Ref<Base::Object>::TryFromBorrowed(existingRoot);
    if (!retained) {
        return operation.Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "LoadComponent requires a managed root object"),
            XamlLoaderDiagnosticCodes::LoadComponentTypeMismatch,
            Base::StringView(
                "XAML component root cannot be retained"));
    }
    return operation.LoadCore(uri, options, retained);
}

Base::Result<XamlLoadResult> XamlLoader::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    const XamlLoadOptions& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_);
    return operation.LoadCompiled(bytes, originUri, options);
}

Base::Result<XamlLoadResult>
XamlLoader::Operation::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    const XamlLoadOptions& options) noexcept {
    Base::Result<void> validOptions =
        ValidateOptions(options);
    if (!validOptions) {
        return validOptions.GetStatus();
    }
    if (bytes.Size() > options.limits.maxSourceBytes) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Compiled XAML source exceeds configured limits"),
            XamlLoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "Compiled XAML source exceeds configured limits"));
    }
    Base::Result<XamlCompiledDocument> document =
        XamlCompiledDocument::Deserialize(
            bytes,
            schema_->Domain(),
            options.limits.compiled);
    if (!document) {
        const Base::Status status = document.GetStatus();
        if (!originUri.Empty() &&
            (status.code == Base::ErrorCode::Unsupported ||
             status.code == Base::ErrorCode::ValidationFailed)) {
            // A compatible source is authoritative when the cache identity no
            // longer matches this runtime. Hosts may persist a replacement
            // cache after this successful source load.
            return LoadCore(originUri, options, {});
        }
        return status;
    }

    return LoadCompiledDocument(
        document.Value(), originUri, options, {});
}

Base::Result<XamlLoadResult>
XamlLoader::Operation::LoadCompiledDocument(
    XamlCompiledDocument& document,
    const Base::ResourceUri& originUri,
    const XamlLoadOptions& options,
    const Base::Ref<Base::Object>& existingRoot) noexcept {
    XamlLoadContext context;
    context.activation = options.activation;
    context.activationFacets = options.activationFacets;
    context.resources = options.resources;
    context.effectiveValues = options.effectiveValues;
    context.bindings = options.bindings;
    context.fallbackResources = options.fallbackResources;
    context.baseUri = &originUri;
    context.templatedParent = options.templatedParent;
    context.existingRoot = existingRoot;
    context.effectLifetime = options.effectLifetime;
    context.effectCommitMode = options.effectCommitMode;
    context.maxObjects = options.limits.maxObjects;
    FinalizeContext finalize{
        this, &options, &originUri, &document};
    context.finalize = &FinalizeLoad;
    context.finalizeContext = &finalize;
    XamlLoadSession session(*schema_, diagnostics_);
    Base::Result<XamlLoadResult> loaded =
        options.activation != nullptr &&
        options.activation->dispatcher != nullptr &&
        options.activation->dependencyProperties != nullptr
        ? [&]() noexcept -> Base::Result<XamlLoadResult> {
              Core::ObjectServicesScope services(
                  *options.activation->dispatcher,
                  *options.activation->dependencyProperties,
                  schema_->Runtime());
              return session.Load(document, context);
          }()
        : session.Load(document, context);
    if (!loaded) return loaded.GetStatus();
    return std::move(loaded).Value();
}

Base::Result<XamlLoadResult> XamlLoader::Operation::LoadCore(
    const Base::ResourceUri& uri,
    const XamlLoadOptions& options,
    const Base::Ref<Base::Object>& existingRoot) noexcept {
    Base::Result<void> validOptions =
        ValidateOptions(options);
    if (!validOptions) {
        return validOptions.GetStatus();
    }
    Base::Result<void> policy = CheckPolicy(uri, options);
    if (!policy) {
        return policy.GetStatus();
    }
    if (IsLoading(uri)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "Recursive XAML source load was detected"),
            XamlLoaderDiagnosticCodes::RecursiveLoad,
            Base::StringView(
                "Recursive XAML source load was detected"));
    }
    if (loadStack_.Size() >=
        options.limits.maxDependencyDepth) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML dependency depth exceeds configured limits"),
            XamlLoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML dependency depth exceeds configured limits"));
    }

    Base::Result<XamlSourceProviderResolution> provider =
        providers_->ResolveDetailed(uri);
    if (!provider) {
        return Failure(
            provider.GetStatus(),
            XamlLoaderDiagnosticCodes::SourceProviderNotFound,
            Base::StringView(
                "No XAML source provider matches the resource URI"));
    }
    Base::Result<void> pushed =
        loadStack_.TryPushBack(uri);
    if (!pushed) {
        return pushed.GetStatus();
    }

    if (options.documentCache != nullptr) {
        Base::Result<std::uint64_t> probedRevision =
            provider.Value().provider->Revision(uri);
        if (probedRevision && probedRevision.Value() != 0U) {
            Base::Result<XamlDocumentCacheLookup> cached =
                options.documentCache->Lookup(
                    uri,
                    probedRevision.Value(),
                    provider.Value().cacheIdentity,
                    schema_->Domain(),
                    options.limits.compiled);
            if (cached && cached.Value().hit) {
                Base::Result<XamlLoadResult> loaded =
                    LoadCompiledDocument(
                        cached.Value().document,
                        uri,
                        options,
                        existingRoot);
                if (loaded) {
                    static_cast<void>(options.documentCache->Store(
                        uri,
                        probedRevision.Value(),
                        provider.Value().cacheIdentity,
                        cached.Value().document,
                        {loaded.Value().dependencies.Data(),
                         loaded.Value().dependencies.Size()}));
                }
                loadStack_.PopBack();
                return loaded;
            }
        }
    }

    Base::Result<XamlSource> source =
        provider.Value().provider->Load(uri);
    if (!source) {
        loadStack_.PopBack();
        return Failure(
            source.GetStatus(),
            XamlLoaderDiagnosticCodes::SourceLoadFailed,
            Base::StringView("XAML source could not be loaded"));
    }
    if (source.Value().bytes.Size() >
        options.limits.maxSourceBytes) {
        loadStack_.PopBack();
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML source exceeds configured limits"),
            XamlLoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML source exceeds configured limits"));
    }

    const Base::ResourceUri& origin =
        source.Value().uri.Empty()
        ? uri
        : source.Value().uri;
    const std::uint64_t sourceRevision =
        source.Value().revision != 0U
        ? source.Value().revision
        : Base::HashBytes(
              source.Value().bytes.Data(),
              source.Value().bytes.Size());

    source.Value().revision = sourceRevision;
    Base::Result<XamlLoadResult> loaded = ParseCore(
        source.Value().Text(),
        origin,
        options,
        existingRoot);
    if (loaded && options.documentCache != nullptr) {
        static_cast<void>(PopulateDocumentCache(
            source.Value(),
            origin,
            provider.Value().cacheIdentity,
            loaded.Value(),
            options));
    }
    loadStack_.PopBack();
    return loaded;
}

Base::Result<void> XamlLoader::Operation::PopulateDocumentCache(
    const XamlSource& source,
    const Base::ResourceUri& origin,
    std::uint64_t sourceIdentity,
    const XamlLoadResult& loaded,
    const XamlLoadOptions& options) noexcept {
    if (options.documentCache == nullptr || source.bytes.Empty()) return {};
#if AERO_WITH_EXPAT
    ExpatXmlTokenizer tokenizer(options.limits.xml);
#else
    Utf8XmlTokenizer tokenizer(options.limits.xml);
#endif
    Base::Result<void> reset = tokenizer.Reset(source.Text());
    if (!reset) return reset.GetStatus();
    XamlNodeReader reader(tokenizer);
    Base::Result<XamlCompiledDocument> compiled =
        XamlCompiledDocument::Compile(reader, *schema_, origin);
    if (!compiled) return compiled.GetStatus();
    for (const Base::ResourceUri& dependency : loaded.dependencies) {
        Base::Result<void> added =
            compiled.Value().TryAddDependency(dependency);
        if (!added) return added.GetStatus();
    }
    return options.documentCache->Store(
        origin,
        source.revision,
        sourceIdentity,
        compiled.Value(),
        {loaded.dependencies.Data(), loaded.dependencies.Size()});
}

Base::Result<XamlLoadResult> XamlLoader::Operation::ParseCore(
    Base::StringView text,
    const Base::ResourceUri& baseUri,
    const XamlLoadOptions& options,
    const Base::Ref<Base::Object>& existingRoot) noexcept {
    Base::Result<void> validOptions =
        ValidateOptions(options);
    if (!validOptions) {
        return validOptions.GetStatus();
    }
    if (text.SizeBytes() >
        options.limits.maxSourceBytes) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML source exceeds configured limits"),
            XamlLoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML source exceeds configured limits"));
    }

#if AERO_WITH_EXPAT
    ExpatXmlTokenizer tokenizer(options.limits.xml);
#else
    Utf8XmlTokenizer tokenizer(options.limits.xml);
#endif
    Base::Result<void> reset =
        tokenizer.Reset(text, diagnostics_);
    if (!reset) {
        return reset.GetStatus();
    }
    XamlNodeReader reader(tokenizer, diagnostics_);
    XamlLoadContext context;
    context.activation = options.activation;
    context.activationFacets = options.activationFacets;
    context.resources = options.resources;
    context.effectiveValues = options.effectiveValues;
    context.bindings = options.bindings;
    context.fallbackResources = options.fallbackResources;
    context.baseUri = &baseUri;
    context.templatedParent = options.templatedParent;
    context.existingRoot = existingRoot;
    context.effectLifetime = options.effectLifetime;
    context.effectCommitMode = options.effectCommitMode;
    context.maxObjects = options.limits.maxObjects;
    FinalizeContext finalize{
        this, &options, &baseUri, nullptr};
    context.finalize = &FinalizeLoad;
    context.finalizeContext = &finalize;
    XamlLoadSession session(*schema_, diagnostics_);
    Base::Result<XamlLoadResult> loaded =
        options.activation != nullptr &&
        options.activation->dispatcher != nullptr &&
        options.activation->dependencyProperties != nullptr
        ? [&]() noexcept -> Base::Result<XamlLoadResult> {
              Core::ObjectServicesScope services(
                  *options.activation->dispatcher,
                  *options.activation->dependencyProperties,
                  schema_->Runtime());
              return session.Load(reader, context);
          }()
        : session.Load(reader, context);
    if (!loaded) {
        return loaded.GetStatus();
    }
    return std::move(loaded).Value();
}

Base::Result<void> XamlLoader::Operation::FinalizeLoad(
    XamlLoadResult& result,
    void* context) noexcept {
    auto* finalize =
        static_cast<FinalizeContext*>(context);
    if (finalize == nullptr ||
        finalize->operation == nullptr ||
        finalize->options == nullptr ||
        finalize->origin == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML load finalization context is invalid");
    }
    return finalize->operation->FinalizeResult(
        result,
        *finalize->options,
        *finalize->origin,
        finalize->compiled);
}

Base::Result<void> XamlLoader::Operation::FinalizeResult(
    XamlLoadResult& result,
    const XamlLoadOptions& options,
    const Base::ResourceUri& origin,
    const XamlCompiledDocument* compiled) noexcept {
    const Base::ResourceUri& effectiveOrigin =
        compiled != nullptr && origin.Empty()
        ? compiled->OriginUri()
        : origin;
    result.canonicalUri = effectiveOrigin;
    if (compiled != nullptr) {
        for (const Base::ResourceUri& dependency :
             compiled->Dependencies()) {
            Base::Result<void> appended =
                AppendDependency(
                    result, dependency, options);
            if (!appended) return appended.GetStatus();
        }
    }
    Base::Result<void> originDependency =
        AppendDependency(
            result, effectiveOrigin, options);
    if (!originDependency) {
        return originDependency.GetStatus();
    }
    return ResolveResourceDependencies(
        result, options);
}

Base::Result<void>
XamlLoader::Operation::ResolveResourceDependencies(
    XamlLoadResult& result,
    const XamlLoadOptions& options) noexcept {
    std::uint32_t resourceCount = 0U;
    Base::Vector<PendingResourceMerge> pending;

    auto resolveDictionary = [&](ResourceDictionary& dictionary)
        noexcept -> Base::Result<void> {
        if (dictionary.Size() == 0U &&
            dictionary.MergedDictionaryCount() == 0U &&
            dictionary.Source().Empty()) {
            return {};
        }
        return ResolveDictionaryDependencies(
            dictionary,
            result,
            options,
            resourceCount,
            pending);
    };

    Base::Result<void> resolved = resolveDictionary(result.resources);
    if (!resolved) return resolved.GetStatus();

    ResourceDictionary* rootResources = nullptr;
    if (result.root) {
        rootResources = schema_->ResolveResourceScope(
            result.root->RuntimeType(), *result.root);
        if (rootResources != nullptr) {
            resolved = resolveDictionary(*rootResources);
            if (!resolved) return resolved.GetStatus();
        }
    }

    for (Presentation::Visual* visual : result.visualContent.nodes) {
        if (visual == nullptr ||
            (result.root && visual == result.root.Get())) {
            continue;
        }
        ResourceDictionary* resources = schema_->ResolveResourceScope(
            visual->RuntimeType(), *visual);
        if (resources == nullptr || resources == rootResources) continue;
        resolved = resolveDictionary(*resources);
        if (!resolved) return resolved.GetStatus();
    }
    return CommitResourceDependencies(pending);
}

Base::Result<void>
XamlLoader::Operation::ResolveDictionaryDependencies(
    ResourceDictionary& dictionary,
    XamlLoadResult& owner,
    const XamlLoadOptions& options,
    std::uint32_t& resourceCount,
    Base::Vector<PendingResourceMerge>& pending) noexcept {
    if (resourceCount >
            options.limits.maxResources ||
        dictionary.Size() >
            options.limits.maxResources - resourceCount) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML resource count exceeds configured limits"),
            XamlLoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML resource count exceeds configured limits"));
    }
    resourceCount += dictionary.Size();

    for (std::uint32_t index = 0U;
         index < dictionary.Size();
         ++index) {
        Base::Result<Presentation::ResourceEntrySnapshot>
            entry = dictionary.EntryAt(index);
        if (!entry) return entry.GetStatus();
        const Core::Value& value = entry.Value().value;
        if (value.Kind() != Core::ValueKind::Object ||
            value.IsNullObject() ||
            !value.AsObject()) {
            continue;
        }
        Base::Object& object = *value.AsObject();
        ResourceDictionary* nested = schema_->ResolveResourceScope(
            object.RuntimeType(), object);
        if (nested == nullptr ||
            (nested->Size() == 0U &&
             nested->MergedDictionaryCount() == 0U &&
             nested->Source().Empty())) {
            continue;
        }
        Base::Result<void> resolved =
            ResolveDictionaryDependencies(
                *nested,
                owner,
                options,
                resourceCount,
                pending);
        if (!resolved) return resolved.GetStatus();
    }

    const std::uint32_t mergedCount =
        dictionary.MergedDictionaryCount();
    for (std::uint32_t index = 0U;
         index < mergedCount;
         ++index) {
        Base::Result<ResourceDictionary> merged =
            dictionary.MergedDictionaryAt(index);
        if (!merged) return merged.GetStatus();
        Base::Result<void> resolved =
            ResolveDictionaryDependencies(
                merged.Value(),
                owner,
                options,
                resourceCount,
                pending);
        if (!resolved) return resolved.GetStatus();
    }

    const Base::ResourceUri source =
        dictionary.Source();
    if (source.Empty()) return {};
    if ((!owner.canonicalUri.Empty() &&
         source == owner.canonicalUri) ||
        IsLoading(source)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "Recursive ResourceDictionary Source was detected"),
            XamlLoaderDiagnosticCodes::RecursiveLoad,
            Base::StringView(
                "Recursive ResourceDictionary Source was detected"));
    }

    Base::Result<XamlLoadResult> loaded =
        LoadCore(source, options, {});
    if (!loaded) {
        return Failure(
            loaded.GetStatus(),
            XamlLoaderDiagnosticCodes::ResourceDependencyFailed,
            Base::StringView(
                "ResourceDictionary Source could not be loaded"));
    }
    if (!loaded.Value().root ||
        loaded.Value().root->RuntimeType() !=
            ResourceDictionary::StaticTypeId()) {
        loaded.Value().Clear();
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "ResourceDictionary Source root has an incompatible type"),
            XamlLoaderDiagnosticCodes::ResourceDependencyFailed,
            Base::StringView(
                "ResourceDictionary Source root must be ResourceDictionary"));
    }
    auto& sourceDictionary =
        static_cast<ResourceDictionary&>(
            *loaded.Value().root);
    Base::Result<void> dependencies =
        AppendDependencies(owner, loaded.Value(), options);
    if (!dependencies) {
        loaded.Value().Clear();
        return dependencies.GetStatus();
    }
    PendingResourceMerge merge;
    Base::Result<ResourceDictionary> target =
        dictionary.Share();
    if (!target) {
        loaded.Value().Clear();
        return target.GetStatus();
    }
    merge.target = std::move(target).Value();
    merge.source = std::move(sourceDictionary);
    Base::Result<void> staged =
        pending.TryPushBack(std::move(merge));
    loaded.Value().Clear();
    return staged;
}

Base::Result<void>
XamlLoader::Operation::CommitResourceDependencies(
    Base::Vector<PendingResourceMerge>& pending) noexcept {
    std::uint32_t committed = 0U;
    Base::Status failure = Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "XAML resource merge target is invalid");
    for (; committed < pending.Size(); ++committed) {
        PendingResourceMerge& merge = pending[committed];
        Base::Result<void> added =
            merge.target.TryAddMerged(merge.source);
        if (!added) {
            failure = added.GetStatus();
            break;
        }
    }
    if (committed == pending.Size()) return {};

    for (std::uint32_t index = committed;
         index > 0U;
         --index) {
        PendingResourceMerge& merge =
            pending[index - 1U];
        Base::Result<bool> removed =
            merge.target.RemoveMerged(
                merge.source);
        if (!removed || !removed.Value()) {
            return removed
                ? Base::Result<void>(
                      Base::Status::Failure(
                          Base::ErrorCode::InvalidState,
                          "XAML resource dependency rollback failed"))
                : Base::Result<void>(
                      removed.GetStatus());
        }
    }
    return failure;
}

Base::Result<void> XamlLoader::Operation::AppendDependencies(
    XamlLoadResult& destination,
    const XamlLoadResult& source,
    const XamlLoadOptions& options) noexcept {
    for (const Base::ResourceUri& dependency :
         source.dependencies) {
        Base::Result<void> appended =
            AppendDependency(
                destination, dependency, options);
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> XamlLoader::Operation::AppendDependency(
    XamlLoadResult& destination,
    const Base::ResourceUri& dependency,
    const XamlLoadOptions& options) noexcept {
    if (dependency.Empty()) return {};
    for (const Base::ResourceUri& existing :
         destination.dependencies) {
        if (existing == dependency) return {};
    }
    if (destination.dependencies.Size() >=
        options.limits.compiled.maxDependencies) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML dependency count exceeds configured limits"),
            XamlLoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML dependency count exceeds configured limits"));
    }
    return destination.dependencies.TryPushBack(
        dependency);
}

Base::Result<void> XamlLoader::Operation::ValidateOptions(
    const XamlLoadOptions& options) const noexcept {
    if (schema_ == nullptr || providers_ == nullptr ||
        !schema_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML loader requires a frozen schema and provider registry");
    }
    if (options.limits.maxSourceBytes == 0U ||
        options.limits.maxObjects == 0U ||
        options.limits.maxResources == 0U ||
        options.limits.maxDependencyDepth == 0U ||
        options.limits.xml.maxInputBytes == 0U ||
        options.limits.xml.maxDepth == 0U ||
        options.limits.compiled.maxNodes == 0U ||
        options.limits.compiled.maxStringBytes == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML load limits must be positive");
    }
    if (options.activationFacets != nullptr &&
        options.activation == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML activation providers require an activation context");
    }
    return {};
}

Base::Result<void> XamlLoader::Operation::CheckPolicy(
    const Base::ResourceUri& uri,
    const XamlLoadOptions& options) noexcept {
    if (uri.Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML resource URI cannot be empty"),
            XamlLoaderDiagnosticCodes::InvalidUri,
            Base::StringView("XAML resource URI cannot be empty"));
    }
    if (uri.IsNetwork() &&
        !options.policy.allowNetwork) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Network XAML sources are disabled by policy"),
            XamlLoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "Network XAML sources are disabled by policy"));
    }
    if (uri.Scheme() == Base::StringView("file") &&
        !options.policy.allowFile) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "File XAML sources are disabled by policy"),
            XamlLoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "File XAML sources are disabled by policy"));
    }
    if (uri.Scheme() == Base::StringView("pack") &&
        !options.policy.allowPackApplication) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Pack application XAML sources are disabled by policy"),
            XamlLoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "Pack application XAML sources are disabled by policy"));
    }
    return {};
}

bool XamlLoader::Operation::IsLoading(
    const Base::ResourceUri& uri) const noexcept {
    for (const Base::ResourceUri& active : loadStack_) {
        if (active == uri) {
            return true;
        }
    }
    return false;
}

Base::Status XamlLoader::Operation::Failure(
    Base::Status status,
    Core::DiagnosticCode code,
    Base::StringView message) noexcept {
    if (diagnostics_ != nullptr) {
        Base::Result<Core::Diagnostic> diagnostic =
            Core::Diagnostic::TryCreate(
                code,
                Core::DiagnosticSeverity::Error,
                message);
        if (diagnostic) {
            diagnostics_->Report(
                std::move(diagnostic).Value());
        }
    }
    return status;
}

} // namespace Aero::Markup
