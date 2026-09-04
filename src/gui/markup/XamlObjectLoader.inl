// ===== LoaderResult =====



namespace Aero::Markup {

Base::Result<void> VisualContentPlan::Reserve(
    std::uint32_t contentEdgeCount,
    std::uint32_t mountEdgeCount,
    std::uint32_t nodeCount) noexcept {
    Base::Result<void> reserved = contentEdges.Reserve(contentEdgeCount);
    if (!reserved) return reserved.GetStatus();
    reserved = mountEdges.Reserve(mountEdgeCount);
    if (!reserved) return reserved.GetStatus();
    return nodes.Reserve(nodeCount);
}

Base::Result<void> VisualContentPlan::AddNode(
    Aero::Media::Visual& node) noexcept {
    for (Aero::Media::Visual* existing : nodes) {
        if (existing == &node) return {};
    }
    return nodes.PushBack(&node);
}

void VisualContentPlan::ReleaseContent() noexcept {
    for (std::uint32_t index = 0U; index < contentEdges.Size(); ++index) {
        VisualContentEdge& edge = contentEdges[index];
        bool firstForParent = true;
        for (std::uint32_t prior = 0U; prior < index; ++prior) {
            if (contentEdges[prior].parentOwner.Get() ==
                    edge.parentOwner.Get() &&
                (!edge.property ||
                 contentEdges[prior].member ==
                     edge.member)) {
                firstForParent = false;
                break;
            }
        }
        if (firstForParent && edge.metadata != nullptr && edge.parentOwner) {
            if (edge.property) {
                const Meta::PropertyInfo* property =
                    edge.metadata->Types().
                        FindProperty(edge.member);
                if (property != nullptr) {
                    (void)edge.metadata->SetProperty(
                        *edge.parentOwner.Get(),
                        edge.member,
                        Meta::Value::NullObject(
                            property->ValueType()));
                }
            } else {
                (void)edge.metadata->ClearContent(
                    *edge.parentOwner.Get(),
                    edge.member);
            }
        }
    }
}

void VisualContentPlan::Clear() noexcept {
    contentEdges.Clear();
    mountEdges.Clear();
    nodes.Clear();
}

} // namespace Aero::Markup


// ===== Loader =====








#include <Aero/Base/Hash.hpp>

#include <Aero/FrameworkElement.hpp>
#include <Aero/Resources.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <limits>


namespace Aero::Markup {
namespace LoaderDiagnosticCodes {
inline constexpr ::Aero::Diagnostics::DiagnosticCode InvalidUri =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 301U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode XamlProviderNotFound =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 302U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode SourceLoadFailed =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 303U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode SourceRejected =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 304U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode RecursiveLoad =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 305U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode LoadComponentTypeMismatch =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 306U);
inline constexpr ::Aero::Diagnostics::DiagnosticCode ResourceDependencyFailed =
    ::Aero::Diagnostics::MakeDiagnosticCode(::Aero::Diagnostics::DiagnosticDomain::Xaml, 307U);
} // namespace LoaderDiagnosticCodes

struct LoaderState {
    LoaderState(
        Schema& schema,
        XamlProviderRegistry& providers,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr,
        const LoadState* runtime = nullptr) noexcept;

    Base::Result<LoaderResult> Load(
        Base::StringView uri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<LoaderResult> Load(
        const Base::ResourceUri& uri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<LoaderResult> Parse(
        Base::StringView text,
        const Base::ResourceUri& baseUri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<LoaderResult> Parse(
        Base::Stream& stream,
        const Base::ResourceUri& baseUri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<LoaderResult> LoadComponent(
        Base::Object& existingRoot,
        Base::StringView uri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<LoaderResult> LoadComponent(
        Base::Object& existingRoot,
        const Base::ResourceUri& uri,
        const XamlReaderSettings& options = {}) noexcept;
    Base::Result<LoaderResult> LoadCompiled(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        const XamlReaderSettings& options = {}) noexcept;

private:
    struct Operation;

    Schema* schema_ = nullptr;
    XamlProviderRegistry* providers_ = nullptr;
    Diagnostics::IDiagnosticSink* diagnostics_ = nullptr;
    const LoadState* runtime_ = nullptr;
};

static_assert(
    sizeof(LoaderState) <= 512,
    "Loader inline state storage is too small");
static_assert(
    alignof(LoaderState) <= alignof(std::max_align_t),
    "Loader inline state alignment is insufficient");

using Aero::ResourceDictionary;

namespace {

class MemoryStream : public Base::Stream {
public:
    explicit MemoryStream(
        Base::Span<const std::uint8_t> bytes) noexcept
        : bytes_(bytes) {}

    bool CanRead() const noexcept override { return true; }

    Base::Result<std::uint32_t> Read(
        Base::Span<std::uint8_t> destination) noexcept override {
        const std::uint32_t available = bytes_.Size() - position_;
        const std::uint32_t count =
            std::min(available, destination.Size());
        if (count != 0U) {
            std::memcpy(
                destination.Data(),
                bytes_.Data() + position_,
                count);
            position_ += count;
        }
        return count;
    }

    bool CanSeek() const noexcept override { return true; }
    Base::Result<std::uint64_t> Position() const noexcept override {
        return static_cast<std::uint64_t>(position_);
    }
    Base::Result<std::uint64_t> Length() const noexcept override {
        return static_cast<std::uint64_t>(bytes_.Size());
    }
    Base::Result<std::uint64_t> Seek(
        std::int64_t offset,
        Base::SeekOrigin origin) noexcept override {
        const std::int64_t base = origin == Base::SeekOrigin::Begin
            ? 0
            : origin == Base::SeekOrigin::Current
                ? static_cast<std::int64_t>(position_)
                : static_cast<std::int64_t>(bytes_.Size());
        const std::int64_t next = base + offset;
        if (next < 0 || static_cast<std::uint64_t>(next) > bytes_.Size()) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "Memory stream seek is outside its bounds");
        }
        position_ = static_cast<std::uint32_t>(next);
        return static_cast<std::uint64_t>(position_);
    }

private:
    Base::Span<const std::uint8_t> bytes_;
    std::uint32_t position_ = 0U;
};

class FileStream : public Base::Stream {
public:
    FileStream(std::FILE* file, std::uint64_t length) noexcept
        : file_(file), length_(length) {}
    ~FileStream() noexcept override {
        if (file_ != nullptr) {
            std::fclose(file_);
            file_ = nullptr;
        }
    }

    bool CanRead() const noexcept override { return file_ != nullptr; }
    Base::Result<std::uint32_t> Read(
        Base::Span<std::uint8_t> destination) noexcept override {
        if (file_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "File stream is closed");
        }
        if (destination.Empty()) return std::uint32_t{0U};
        const std::size_t count = std::fread(
            destination.Data(), 1U, destination.Size(), file_);
        if (count == 0U && std::ferror(file_) != 0) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "File stream read failed");
        }
        return static_cast<std::uint32_t>(count);
    }
    bool CanSeek() const noexcept override { return file_ != nullptr; }
    Base::Result<std::uint64_t> Position() const noexcept override {
        if (file_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "File stream is closed");
        }
        const long position = std::ftell(file_);
        if (position < 0L) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "File stream position could not be read");
        }
        return static_cast<std::uint64_t>(position);
    }
    Base::Result<std::uint64_t> Length() const noexcept override {
        return length_;
    }
    Base::Result<std::uint64_t> Seek(
        std::int64_t offset,
        Base::SeekOrigin origin) noexcept override {
        if (file_ == nullptr ||
            offset < static_cast<std::int64_t>(std::numeric_limits<long>::min()) ||
            offset > static_cast<std::int64_t>(std::numeric_limits<long>::max())) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "File stream seek is outside its bounds");
        }
        const int whence = origin == Base::SeekOrigin::Begin
            ? SEEK_SET
            : origin == Base::SeekOrigin::Current
                ? SEEK_CUR
                : SEEK_END;
        if (std::fseek(file_, static_cast<long>(offset), whence) != 0) {
            return Base::Status::Failure(
                Base::ErrorCode::InternalError,
                "File stream seek failed");
        }
        return Position();
    }

private:
    std::FILE* file_ = nullptr;
    std::uint64_t length_ = 0U;
};

class HashingStream : public Base::Stream {
public:
    explicit HashingStream(Base::Stream& source) noexcept
        : source_(&source) {}

    bool CanRead() const noexcept override {
        return source_ != nullptr && source_->CanRead();
    }
    Base::Result<std::uint32_t> Read(
        Base::Span<std::uint8_t> destination) noexcept override {
        if (source_ == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "Hashing stream is not initialized");
        }
        Base::Result<std::uint32_t> result =
            source_->Read(destination);
        if (!result || result.Value() == 0U) return result;
        constexpr Base::HashCode Prime = UINT64_C(1099511628211);
        for (std::uint32_t index = 0U;
             index < result.Value(); ++index) {
            hash_ ^= static_cast<Base::HashCode>(
                destination[index]);
            hash_ *= Prime;
        }
        size_ += result.Value();
        return result;
    }
    bool CanSeek() const noexcept override { return false; }
    Base::HashCode Hash() const noexcept {
        return Base::MixHash64(hash_ ^ size_);
    }

private:
    Base::Stream* source_ = nullptr;
    Base::HashCode hash_ =
        UINT64_C(14695981039346656037) ^
        Base::MixHash64(0U);
    std::uint64_t size_ = 0U;
};

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
        replacement.Reserve(value.SizeBytes());
    if (!reserve) {
        return reserve.GetStatus();
    }
    for (char character : value) {
        const char lower = ToLowerAscii(character);
        Base::Result<void> append =
            replacement.AppendUnchecked(
                Base::StringView(&lower, 1U));
        if (!append) {
            return append.GetStatus();
        }
    }
    output = std::move(replacement);
    return {};
}

bool RegistrationMatches(
    const XamlProviderRegistration& registration,
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

Base::Result<::Aero::Markup::StreamResourceInfo> CreateMemoryResource(
    const Base::ResourceUri& uri,
    Base::Span<const std::uint8_t> bytes,
    std::uint64_t revision) noexcept {
    Base::Result<Base::Ref<MemoryStream>> stream =
        Base::MakeRef<MemoryStream>(bytes);
    if (!stream) return stream.GetStatus();
    ::Aero::Markup::StreamResourceInfo result;
    result.uri = uri;
    result.stream = std::move(stream).Value();
    result.revision = revision;
    return result;
}

Base::Result<Base::ResourceUri> ResolveRequestedUri(
    Base::StringView uri,
    const Base::ResourceUri& baseUri) noexcept {
    // WPF component URIs beginning with '/' are assembly resources, not
    // filesystem-rooted paths. Resolving them against a file-backed App.xaml
    // would otherwise incorrectly manufacture a file: URI.
    if (uri.SizeBytes() > 0U && uri[0] == '/') {
        for (std::uint32_t index = 1U;
             index + 10U <= uri.SizeBytes(); ++index) {
            if (uri.Substr(index, 10U) ==
                Base::StringView(";component")) {
                return Base::ResourceUri::Parse(uri);
            }
        }
    }
    if (!baseUri.Empty()) {
        return Base::ResourceUri::Resolve(
            baseUri, uri);
    }
    return Base::ResourceUri::Parse(uri);
}

} // namespace

Base::Result<void> XamlProviderRegistry::Set(
    Base::Ref<XamlProvider> provider,
    Base::StringView scheme,
    Base::StringView assembly,
    Base::Ref<XamlProvider>* replaced) noexcept {
    if (!provider) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Markup XAML provider is required");
    }
    XamlProviderRegistration registration;
    Base::Result<void> schemeResult =
        AssignLowerAscii(registration.scheme, scheme);
    if (!schemeResult) {
        return schemeResult.GetStatus();
    }
    Base::Result<void> assemblyResult =
        registration.assembly.Assign(assembly);
    if (!assemblyResult) {
        return assemblyResult.GetStatus();
    }
    registration.provider = std::move(provider);
    for (const XamlProviderRegistration& existing : registrations_) {
        if (existing.provider.Get() == registration.provider.Get()) {
            registration.identity = existing.identity;
            break;
        }
    }
    static std::atomic<std::uint64_t> nextIdentity{1U};
    if (registration.identity == 0U) {
        registration.identity =
            nextIdentity.fetch_add(1U, std::memory_order_relaxed);
        if (registration.identity == 0U) {
            registration.identity =
                nextIdentity.fetch_add(1U, std::memory_order_relaxed);
        }
    }

    for (XamlProviderRegistration& existing : registrations_) {
        if (existing.scheme.View() == registration.scheme.View() &&
            existing.assembly.View() ==
                registration.assembly.View()) {
            if (replaced != nullptr) {
                *replaced = std::move(existing.provider);
            }
            existing = std::move(registration);
            return {};
        }
    }
    return registrations_.PushBack(
        std::move(registration));
}

Base::Result<XamlProviderResolution>
XamlProviderRegistry::ResolveRoute(
    const Base::ResourceUri& uri,
    bool requireScheme,
    bool requireAssembly) const noexcept {
    for (const XamlProviderRegistration& registration : registrations_) {
        if (!RegistrationMatches(
                registration, uri, requireScheme, requireAssembly)) {
            continue;
        }
        XamlProviderResolution result;
        result.provider = registration.provider.Get();
        result.cacheIdentity = Base::MixHash64(
            registration.identity ^
            Base::DefaultHash<Base::StringView>{}(
                registration.scheme.View()) ^
            Base::DefaultHash<Base::StringView>{}(
                registration.assembly.View(), UINT64_C(0xA3E0)));
        return result;
    }
    if (parent_ != nullptr) {
        Base::Result<XamlProviderResolution> inherited =
            parent_->ResolveRoute(uri, requireScheme, requireAssembly);
        if (inherited) return inherited;
        if (inherited.GetStatus().code != Base::ErrorCode::NotFound) {
            return inherited.GetStatus();
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "No XAML source provider matches this route shape");
}

Base::Result<XamlProviderResolution>
XamlProviderRegistry::ResolveDetailed(
    const Base::ResourceUri& uri) const noexcept {
    const struct Route {
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
        Base::Result<XamlProviderResolution> resolved =
            ResolveRoute(uri, route.scheme, route.assembly);
        if (resolved) return resolved;
        if (resolved.GetStatus().code != Base::ErrorCode::NotFound) {
            return resolved.GetStatus();
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "No XAML source provider matches the resource URI");
}

Base::Result<XamlProvider*>
XamlProviderRegistry::Resolve(
    const Base::ResourceUri& uri) const noexcept {
    Base::Result<XamlProviderResolution> resolved =
        ResolveDetailed(uri);
    return resolved
        ? Base::Result<XamlProvider*>(resolved.Value().provider)
        : Base::Result<XamlProvider*>(resolved.GetStatus());
}

bool XamlProviderRegistry::Contains(
    const XamlProvider& provider) const noexcept {
    for (const XamlProviderRegistration& registration : registrations_) {
        if (registration.provider.Get() == &provider) return true;
    }
    return false;
}

Base::Result<void> EmbeddedXamlProvider::Add(
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
        entry.bytes.Append(bytes);
    if (!copied) {
        return copied.GetStatus();
    }
    entry.revision = revision;
    Base::Result<void> stored = entries_.PushBack(std::move(entry));
    if (!stored) return stored.GetStatus();
    return {};
}

Base::Result<void> EmbeddedXamlProvider::AddText(
    const Base::ResourceUri& uri,
    Base::StringView text,
    std::uint64_t revision) noexcept {
    return Add(
        uri,
        Base::Span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(text.Data()),
            text.SizeBytes()),
        revision);
}

Base::Result<void> EmbeddedXamlProvider::Freeze() noexcept {
    frozen_ = true;
    return {};
}

Base::Result<::Aero::Markup::StreamResourceInfo>
EmbeddedXamlProvider::Open(
    const Base::ResourceUri& uri) const noexcept {
    for (const Entry& entry : entries_) {
        if (entry.uri == uri) {
            return CreateMemoryResource(
                entry.uri,
                entry.bytes.AsSpan(),
                entry.revision);
        }
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "Embedded XAML source was not found");
}

Base::Result<std::uint64_t> EmbeddedXamlProvider::Revision(
    const Base::ResourceUri& uri) const noexcept {
    for (const Entry& entry : entries_) {
        if (entry.uri == uri) return entry.revision;
    }
    return Base::Status::Failure(
        Base::ErrorCode::NotFound,
        "Embedded XAML source was not found");
}

Base::Result<std::uint64_t> FileXamlProvider::Revision(
    const Base::ResourceUri& uri) const noexcept {
    if ((!uri.Scheme().Empty() &&
         uri.Scheme() != Base::StringView("file")) ||
        uri.Path().Empty() || maxFileBytes_ == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "File XAML source URI is invalid");
    }
    Base::String path;
    Base::Result<void> assigned = path.Assign(uri.Path());
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

Base::Result<::Aero::Markup::StreamResourceInfo>
FileXamlProvider::Open(
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
    Base::Result<void> assigned = path.Assign(uri.Path());
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

    Base::Result<Base::Ref<FileStream>> stream =
        Base::MakeRef<FileStream>(
            file, static_cast<std::uint64_t>(length));
    if (!stream) {
        std::fclose(file);
        return stream.GetStatus();
    }
    ::Aero::Markup::StreamResourceInfo source;
    source.uri = uri;
    source.stream = std::move(stream).Value();
    Base::Result<std::uint64_t> revision = Revision(uri);
    source.revision = revision
        ? revision.Value()
        : 0U;
    return source;
}

struct LoaderState::Operation {
    struct FinalizeState {
        Operation* operation = nullptr;
        const XamlReaderSettings* options = nullptr;
        const Base::ResourceUri* origin = nullptr;
        const CompiledDocument* compiled = nullptr;
    };

    struct PendingResourceMerge {
        ResourceDictionary target;
        ResourceDictionary source;
    };

    Operation(
        Schema& schema,
        XamlProviderRegistry& providers,
        Diagnostics::IDiagnosticSink* diagnostics,
        const LoadState* runtime) noexcept
        : schema_(&schema),
          providers_(&providers),
          diagnostics_(diagnostics),
          runtime_(runtime) {}

    const LoadState& Runtime() const noexcept {
        static const LoadState empty;
        return runtime_ != nullptr ? *runtime_ : empty;
    }

    Base::Result<LoaderResult> LoadCore(
        const Base::ResourceUri& uri,
        const XamlReaderSettings& options,
        const Base::Ref<Base::Object>& existingRoot) noexcept;
    Base::Result<LoaderResult> ParseCore(
        Base::StringView text,
        const Base::ResourceUri& baseUri,
        const XamlReaderSettings& options,
        const Base::Ref<Base::Object>& existingRoot,
        bool deferUnresolvedStaticResources = false) noexcept;
    Base::Result<LoaderResult> ParseStreamCore(
        Base::Stream& stream,
        const Base::ResourceUri& baseUri,
        const XamlReaderSettings& options,
        const Base::Ref<Base::Object>& existingRoot,
        bool deferUnresolvedStaticResources = false,
        Base::Vector<Node>* recordingNodes = nullptr) noexcept;
    Base::Result<LoaderResult> LoadCompiled(
        Base::Span<const std::uint8_t> bytes,
        const Base::ResourceUri& originUri,
        const XamlReaderSettings& options) noexcept;
    Base::Result<LoaderResult> LoadCompiledDocument(
        CompiledDocument& document,
        const Base::ResourceUri& originUri,
        const XamlReaderSettings& options,
        const Base::Ref<Base::Object>& existingRoot) noexcept;
    Base::Result<void> ResolveResourceDependencies(
        LoaderResult& result,
        const XamlReaderSettings& options) noexcept;
    Base::Result<void> ResolveDictionaryDependencies(
        ResourceDictionary& dictionary,
        LoaderResult& owner,
        const XamlReaderSettings& options,
        std::uint32_t& resourceCount,
        Base::Vector<PendingResourceMerge>& pending) noexcept;
    Base::Result<void> CommitResourceDependencies(
        Base::Vector<PendingResourceMerge>& pending) noexcept;
    Base::Result<void> AppendDependencies(
        LoaderResult& destination,
        const LoaderResult& source,
        const XamlReaderSettings& options) noexcept;
    Base::Result<void> AppendDependency(
        LoaderResult& destination,
        const Base::ResourceUri& dependency,
        const XamlReaderSettings& options) noexcept;
    Base::Result<void> FinalizeResult(
        LoaderResult& result,
        const XamlReaderSettings& options,
        const Base::ResourceUri& origin,
        const CompiledDocument* compiled) noexcept;
    static Base::Result<void> FinalizeLoad(
        LoaderResult& result,
        void* context) noexcept;
    Base::Result<void> ValidateOptions(
        const XamlReaderSettings& options) const noexcept;
    Base::Result<void> CheckPolicy(
        const Base::ResourceUri& uri,
        const XamlReaderSettings& options) noexcept;
    bool IsLoading(const Base::ResourceUri& uri) const noexcept;
    Base::Status Failure(
        Base::Status status,
        ::Aero::Diagnostics::DiagnosticCode code,
        Base::StringView message) noexcept;

    Schema* schema_ = nullptr;
    XamlProviderRegistry* providers_ = nullptr;
    Diagnostics::IDiagnosticSink* diagnostics_ = nullptr;
    const LoadState* runtime_ = nullptr;
    Base::Vector<Base::ResourceUri> loadStack_;
};

LoaderState::LoaderState(
    Schema& schema,
    XamlProviderRegistry& providers,
    Diagnostics::IDiagnosticSink* diagnostics,
    const LoadState* runtime) noexcept
    : schema_(&schema),
      providers_(&providers),
      diagnostics_(diagnostics),
      runtime_(runtime) {}

Base::Result<LoaderResult> LoaderState::Load(
    Base::StringView uri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    Base::Result<Base::ResourceUri> resolved =
        ResolveRequestedUri(uri, {});
    if (!resolved) {
        return operation.Failure(
            resolved.GetStatus(),
            LoaderDiagnosticCodes::InvalidUri,
            Base::StringView("XAML resource URI is invalid"));
    }
    return operation.LoadCore(
        resolved.Value(), options, {});
}

Base::Result<LoaderResult> LoaderState::Load(
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    return operation.LoadCore(uri, options, {});
}

Base::Result<LoaderResult> LoaderState::Parse(
    Base::StringView text,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    return operation.ParseCore(text, baseUri, options, {}, true);
}

Base::Result<LoaderResult> LoaderState::Parse(
    Base::Stream& stream,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    return operation.ParseStreamCore(stream, baseUri, options, {}, true);
}

Base::Result<LoaderResult> LoaderState::LoadComponent(
    Base::Object& existingRoot,
    Base::StringView uri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    Base::Result<Base::ResourceUri> resolved =
        ResolveRequestedUri(uri, {});
    if (!resolved) {
        return operation.Failure(
            resolved.GetStatus(),
            LoaderDiagnosticCodes::InvalidUri,
            Base::StringView("XAML component URI is invalid"));
    }
    Base::Ref<Base::Object> retained =
        Base::Ref<Base::Object>::TryFromBorrowed(existingRoot);
    if (!retained) {
        return operation.Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "LoadComponent requires a managed root object"),
            LoaderDiagnosticCodes::LoadComponentTypeMismatch,
            Base::StringView(
                "XAML component root cannot be retained"));
    }
    return operation.LoadCore(
        resolved.Value(), options, retained);
}

Base::Result<LoaderResult> LoaderState::LoadComponent(
    Base::Object& existingRoot,
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    Base::Ref<Base::Object> retained =
        Base::Ref<Base::Object>::TryFromBorrowed(existingRoot);
    if (!retained) {
        return operation.Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "LoadComponent requires a managed root object"),
            LoaderDiagnosticCodes::LoadComponentTypeMismatch,
            Base::StringView(
                "XAML component root cannot be retained"));
    }
    return operation.LoadCore(uri, options, retained);
}

Base::Result<LoaderResult> LoaderState::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    const XamlReaderSettings& options) noexcept {
    Operation operation(*schema_, *providers_, diagnostics_, runtime_);
    return operation.LoadCompiled(bytes, originUri, options);
}

Base::Result<LoaderResult>
LoaderState::Operation::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    const XamlReaderSettings& options) noexcept {
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
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "Compiled XAML source exceeds configured limits"));
    }
    Base::Result<CompiledDocument> document =
        CompiledDocument::Deserialize(
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

Base::Result<LoaderResult>
LoaderState::Operation::LoadCompiledDocument(
    CompiledDocument& document,
    const Base::ResourceUri& originUri,
    const XamlReaderSettings& options,
    const Base::Ref<Base::Object>& existingRoot) noexcept {
    const LoadState& runtime = Runtime();
    LoadState context;
    context.resources = runtime.resources;
    context.effectiveValues = runtime.effectiveValues;
    context.bindings = runtime.bindings;
    context.fallbackResources = runtime.fallbackResources;
    context.baseUri = &originUri;
    context.templatedParent = runtime.templatedParent;
    context.existingRoot = existingRoot;
    context.effectLifetime = runtime.effectLifetime;
    context.effectCommitMode = runtime.effectCommitMode;
    context.maxObjects = options.limits.maxObjects;
    // Source-backed compiled documents need the same two-phase resource
    // resolution as streamed documents. Merged ResourceDictionary sources
    // are committed by the load finalizer before queued StaticResource
    // references are written.
    context.deferUnresolvedStaticResources = true;
    FinalizeState finalize{
        this, &options, &originUri, &document};
    context.finalize = &FinalizeLoad;
    context.finalizeContext = &finalize;
    ObjectWriter writer(*schema_, diagnostics_);
    Base::Result<LoaderResult> loaded =
        runtime.dispatcher != nullptr &&
        runtime.dependencyProperties != nullptr
        ? [&]() noexcept -> Base::Result<LoaderResult> {
              Meta::ObjectFactoryScope services(
                  *runtime.dispatcher,
                  *runtime.dependencyProperties,
                  schema_->Metadata());
              ObjectBuilder state(writer);
              return state.Load(document, context);
          }()
        : [&]() noexcept {
              ObjectBuilder state(writer);
              return state.Load(document, context);
          }();
    if (!loaded) return loaded.GetStatus();
    return std::move(loaded).Value();
}

Base::Result<LoaderResult> LoaderState::Operation::LoadCore(
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options,
    const Base::Ref<Base::Object>& existingRoot) noexcept {
    const LoadState& runtime = Runtime();
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
            LoaderDiagnosticCodes::RecursiveLoad,
            Base::StringView(
                "Recursive XAML source load was detected"));
    }
    if (loadStack_.Size() >=
        options.limits.maxDependencyDepth) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                "XAML dependency depth exceeds configured limits"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML dependency depth exceeds configured limits"));
    }

    Base::Result<XamlProviderResolution> provider =
        providers_->ResolveDetailed(uri);
    if (!provider) {
        return Failure(
            provider.GetStatus(),
            LoaderDiagnosticCodes::XamlProviderNotFound,
            Base::StringView(
                "No XAML source provider matches the resource URI"));
    }
    Base::Result<void> pushed =
        loadStack_.PushBack(uri);
    if (!pushed) {
        return pushed.GetStatus();
    }

    if (runtime.documentCache != nullptr) {
        Base::Result<std::uint64_t> probedRevision =
            provider.Value().provider->Revision(uri);
        if (probedRevision && probedRevision.Value() != 0U) {
            Base::Result<DocumentCacheLookup> cached =
                runtime.documentCache->Lookup(
                    uri,
                    probedRevision.Value(),
                    provider.Value().cacheIdentity,
                    schema_->Domain(),
                    options.limits.compiled);
            if (cached && cached.Value().hit) {
                Base::Result<LoaderResult> loaded =
                    LoadCompiledDocument(
                        cached.Value().document,
                        uri,
                        options,
                        existingRoot);
                if (loaded) {
                    static_cast<void>(runtime.documentCache->Store(
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

    Base::Result<::Aero::Markup::StreamResourceInfo> source =
        provider.Value().provider->Open(uri);
    if (!source) {
        loadStack_.PopBack();
        return Failure(
            source.GetStatus(),
            LoaderDiagnosticCodes::SourceLoadFailed,
            Base::StringView("XAML source could not be loaded"));
    }
    ::Aero::Markup::StreamResourceInfo sourceInfo =
        std::move(source).Value();
    if (!sourceInfo.stream ||
        !sourceInfo.stream->CanRead()) {
        loadStack_.PopBack();
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML source stream is invalid"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML source stream could not be read"));
    }

    const Base::ResourceUri& origin =
        sourceInfo.uri.Empty()
        ? uri
        : sourceInfo.uri;
    HashingStream hashing(*sourceInfo.stream);
    Base::Vector<Node> recordedNodes;
    Base::Result<LoaderResult> loaded = ParseStreamCore(
        hashing,
        origin,
        options,
        existingRoot,
        true,
        runtime.documentCache != nullptr
            ? &recordedNodes
            : nullptr);
    if (sourceInfo.revision == 0U) {
        sourceInfo.revision = hashing.Hash();
    }
    if (loaded && runtime.documentCache != nullptr &&
        !recordedNodes.Empty()) {
        Base::Result<CompiledDocument> compiled =
            CompiledDocument::Compile(
                {recordedNodes.Data(), recordedNodes.Size()},
                *schema_,
                origin);
        if (compiled) {
            for (const Base::ResourceUri& dependency :
                 loaded.Value().dependencies) {
                Base::Result<void> added =
                    compiled.Value().AddDependency(dependency);
                if (!added) {
                    compiled = added.GetStatus();
                    break;
                }
            }
        }
        if (compiled) {
            static_cast<void>(runtime.documentCache->Store(
                uri,
                sourceInfo.revision,
                provider.Value().cacheIdentity,
                compiled.Value(),
                {loaded.Value().dependencies.Data(),
                 loaded.Value().dependencies.Size()}));
        }
    }
    loadStack_.PopBack();
    return loaded;
}

Base::Result<LoaderResult> LoaderState::Operation::ParseCore(
    Base::StringView text,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options,
    const Base::Ref<Base::Object>& existingRoot,
    bool deferUnresolvedStaticResources) noexcept {
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
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML source exceeds configured limits"));
    }

    const Base::Span<const std::uint8_t> bytes{
        reinterpret_cast<const std::uint8_t*>(text.Data()),
        text.SizeBytes()};
    Base::Result<Base::Ref<MemoryStream>> stream =
        Base::MakeRef<MemoryStream>(bytes);
    if (!stream) return stream.GetStatus();
    return ParseStreamCore(
        *stream.Value(),
        baseUri,
        options,
        existingRoot,
        deferUnresolvedStaticResources);
}

Base::Result<LoaderResult> LoaderState::Operation::ParseStreamCore(
    Base::Stream& stream,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options,
    const Base::Ref<Base::Object>& existingRoot,
    bool deferUnresolvedStaticResources,
    Base::Vector<Node>* recordingNodes) noexcept {
    const LoadState& runtime = Runtime();
    Base::Result<void> validOptions =
        ValidateOptions(options);
    if (!validOptions) {
        return validOptions.GetStatus();
    }

#if AERO_WITH_EXPAT
    ExpatXmlTokenizer tokenizer(options.limits.xml);
#else
    Utf8XmlTokenizer tokenizer(options.limits.xml);
#endif
    Base::Result<void> reset =
        tokenizer.Reset(stream, diagnostics_);
    if (!reset) return reset.GetStatus();

    NodeReader reader(tokenizer, diagnostics_);
    LoadState context;
    context.resources = runtime.resources;
    context.effectiveValues = runtime.effectiveValues;
    context.bindings = runtime.bindings;
    context.fallbackResources = runtime.fallbackResources;
    context.baseUri = &baseUri;
    context.templatedParent = runtime.templatedParent;
    context.existingRoot = existingRoot;
    context.effectLifetime = runtime.effectLifetime;
    context.effectCommitMode = runtime.effectCommitMode;
    context.maxObjects = options.limits.maxObjects;
    context.deferUnresolvedStaticResources =
        deferUnresolvedStaticResources;
    context.recordingNodes = recordingNodes;
    FinalizeState finalize{
        this, &options, &baseUri, nullptr};
    context.finalize = &FinalizeLoad;
    context.finalizeContext = &finalize;
    ObjectWriter writer(*schema_, diagnostics_);
    Base::Result<LoaderResult> loaded =
        runtime.dispatcher != nullptr &&
        runtime.dependencyProperties != nullptr
        ? [&]() noexcept -> Base::Result<LoaderResult> {
              Meta::ObjectFactoryScope services(
                  *runtime.dispatcher,
                  *runtime.dependencyProperties,
                  schema_->Metadata());
              ObjectBuilder state(writer);
              return state.Load(reader, context);
          }()
        : [&]() noexcept -> Base::Result<LoaderResult> {
              ObjectBuilder state(writer);
              return state.Load(reader, context);
          }();
    if (!loaded) return loaded.GetStatus();
    return std::move(loaded).Value();
}

Base::Result<void> LoaderState::Operation::FinalizeLoad(
    LoaderResult& result,
    void* context) noexcept {
    auto* finalize =
        static_cast<FinalizeState*>(context);
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

Base::Result<void> LoaderState::Operation::FinalizeResult(
    LoaderResult& result,
    const XamlReaderSettings& options,
    const Base::ResourceUri& origin,
    const CompiledDocument* compiled) noexcept {
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
LoaderState::Operation::ResolveResourceDependencies(
    LoaderResult& result,
    const XamlReaderSettings& options) noexcept {
    std::uint32_t resourceCount = 0U;
    Base::Vector<PendingResourceMerge> pending;

    auto resolveDictionary = [&](ResourceDictionary& dictionary)
        noexcept -> Base::Result<void> {
        if (dictionary.Size() == 0U &&
            dictionary.MergedDictionaryCount() == 0U &&
            dictionary.GetSource().Empty()) {
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

    for (Aero::Media::Visual* visual : result.visualContent.nodes) {
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
LoaderState::Operation::ResolveDictionaryDependencies(
    ResourceDictionary& dictionary,
    LoaderResult& owner,
    const XamlReaderSettings& options,
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
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML resource count exceeds configured limits"));
    }
    resourceCount += dictionary.Size();

    for (std::uint32_t index = 0U;
         index < dictionary.Size();
         ++index) {
        Base::Result<Aero::ResourceEntrySnapshot>
            entry = dictionary.EntryAt(index);
        if (!entry) return entry.GetStatus();
        const Meta::Value& value = entry.Value().value;
        if (value.Kind() != Meta::ValueKind::Object ||
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
             nested->GetSource().Empty())) {
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
        dictionary.GetSource();
    if (source.Empty()) return {};
    if ((!owner.canonicalUri.Empty() &&
         source == owner.canonicalUri) ||
        IsLoading(source)) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::CycleDetected,
                "Recursive ResourceDictionary Source was detected"),
            LoaderDiagnosticCodes::RecursiveLoad,
            Base::StringView(
                "Recursive ResourceDictionary Source was detected"));
    }

    // Merged dictionaries are evaluated in declaration order. Supply already
    // discovered siblings as ambient resources while loading the next source,
    // so WPF-style DynamicResource values in styles can resolve against an
    // earlier palette or brush dictionary.
    ResourceDictionary ambientResources;
    Base::Result<void> ambientMerged{};
    // Already-loaded sibling Source dictionaries first so a later merge
    // (Resources.xaml) can StaticResource keys defined in nested merges of
    // an earlier sibling (Colors.Dark inside Brushes.DarkBlue).
    for (PendingResourceMerge& discovered : pending) {
        if (ambientMerged) {
            ambientMerged = ambientResources.AddMerged(
                discovered.source);
        }
    }
    if (ambientMerged &&
        (dictionary.Size() > 0U ||
         dictionary.MergedDictionaryCount() > 0U)) {
        ambientMerged = ambientResources.AddMerged(dictionary);
    }
    if (!ambientMerged) return ambientMerged.GetStatus();
    const LoadState& runtime = Runtime();
    LoadState resourceContext = runtime;
    resourceContext.resources = &ambientResources;
    resourceContext.fallbackResources = &ambientResources;
    const LoadState* previousRuntime = runtime_;
    runtime_ = &resourceContext;
    Base::Result<LoaderResult> loaded =
        LoadCore(source, options, {});
    runtime_ = previousRuntime;
    if (!loaded) {
        return Failure(
            loaded.GetStatus(),
            LoaderDiagnosticCodes::ResourceDependencyFailed,
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
            LoaderDiagnosticCodes::ResourceDependencyFailed,
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
        pending.PushBack(std::move(merge));
    loaded.Value().Clear();
    return staged;
}

Base::Result<void>
LoaderState::Operation::CommitResourceDependencies(
    Base::Vector<PendingResourceMerge>& pending) noexcept {
    std::uint32_t committed = 0U;
    Base::Status failure = Base::Status::Failure(
        Base::ErrorCode::InvalidState,
        "XAML resource merge target is invalid");
    for (; committed < pending.Size(); ++committed) {
        PendingResourceMerge& merge = pending[committed];
        Base::Result<void> added =
        merge.target.AddMerged(merge.source);
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

Base::Result<void> LoaderState::Operation::AppendDependencies(
    LoaderResult& destination,
    const LoaderResult& source,
    const XamlReaderSettings& options) noexcept {
    for (const Base::ResourceUri& dependency :
         source.dependencies) {
        Base::Result<void> appended =
            AppendDependency(
                destination, dependency, options);
        if (!appended) return appended.GetStatus();
    }
    return {};
}

Base::Result<void> LoaderState::Operation::AppendDependency(
    LoaderResult& destination,
    const Base::ResourceUri& dependency,
    const XamlReaderSettings& options) noexcept {
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
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "XAML dependency count exceeds configured limits"));
    }
    return destination.dependencies.PushBack(
        dependency);
}

Base::Result<void> LoaderState::Operation::ValidateOptions(
    const XamlReaderSettings& options) const noexcept {
    const LoadState& runtime = Runtime();
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
    if ((runtime.dispatcher == nullptr) !=
        (runtime.dependencyProperties == nullptr)) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML object factory require dispatcher and property metadata");
    }
    return {};
}

Base::Result<void> LoaderState::Operation::CheckPolicy(
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    if (uri.Empty()) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML resource URI cannot be empty"),
            LoaderDiagnosticCodes::InvalidUri,
            Base::StringView("XAML resource URI cannot be empty"));
    }
    if (uri.IsNetwork() &&
        !options.policy.allowNetwork) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Network XAML sources are disabled by policy"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "Network XAML sources are disabled by policy"));
    }
    if (uri.Scheme() == Base::StringView("file") &&
        !options.policy.allowFile) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "File XAML sources are disabled by policy"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "File XAML sources are disabled by policy"));
    }
    if (uri.Scheme() == Base::StringView("pack") &&
        !options.policy.allowPackApplication) {
        return Failure(
            Base::Status::Failure(
                Base::ErrorCode::Unsupported,
                "Pack application XAML sources are disabled by policy"),
            LoaderDiagnosticCodes::SourceRejected,
            Base::StringView(
                "Pack application XAML sources are disabled by policy"));
    }
    return {};
}

bool LoaderState::Operation::IsLoading(
    const Base::ResourceUri& uri) const noexcept {
    for (const Base::ResourceUri& active : loadStack_) {
        if (active == uri) {
            return true;
        }
    }
    return false;
}

Base::Status LoaderState::Operation::Failure(
    Base::Status status,
    ::Aero::Diagnostics::DiagnosticCode code,
    Base::StringView message) noexcept {
    if (diagnostics_ != nullptr) {
        Base::Result<::Aero::Diagnostics::Diagnostic> diagnostic =
            ::Aero::Diagnostics::Diagnostic::Create(
                code,
                ::Aero::Diagnostics::DiagnosticSeverity::Error,
                message);
        if (diagnostic) {
            if (status.message != nullptr &&
                status.message[0] != '\0') {
                const Base::StringView detail(
                    status.message,
                    static_cast<std::uint32_t>(
                        std::strlen(status.message)));
                if (detail != message) {
                    static_cast<void>(
                        diagnostic.Value().AddNote(detail));
                }
            }
            diagnostics_->Report(
                std::move(diagnostic).Value());
        }
    }
    return status;
}

} // namespace Aero::Markup

namespace Aero::Markup {

namespace {

Base::Result<XamlDocument> AdoptResult(
    Base::Result<LoaderResult>&& loaded,
    Base::IAllocator& allocator) noexcept {
    if (!loaded) return loaded.GetStatus();
    Base::Result<XamlDocument> document =
        ::Aero::Markup::AdoptXamlDocument(
            std::move(loaded).Value(), allocator);
    return document;
}

} // namespace

Loader::Loader(
    Schema& schema,
    XamlProviderRegistry& providers,
    Diagnostics::IDiagnosticSink* diagnostics,
    Base::IAllocator* allocator,
    const LoadState* runtime) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    state_ = new (stateStorage_) LoaderState(
        schema, providers, diagnostics, runtime);
}

Loader::~Loader() noexcept {
    if (state_ == nullptr) return;
    state_->~LoaderState();
    state_ = nullptr;
}

Base::Result<XamlDocument> Loader::Load(
    Base::StringView uri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        state_->Load(uri, options), *allocator_);
}

Base::Result<XamlDocument> Loader::Load(
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        state_->Load(uri, options), *allocator_);
}

Base::Result<XamlDocument> Loader::Parse(
    Base::StringView text,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        state_->Parse(text, baseUri, options),
        *allocator_);
}

Base::Result<XamlDocument> Loader::Parse(
    Base::Stream& stream,
    const Base::ResourceUri& baseUri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        state_->Parse(stream, baseUri, options),
        *allocator_);
}

Base::Result<XamlDocument> Loader::LoadComponent(
    Base::Object& existingRoot,
    Base::StringView uri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        state_->LoadComponent(existingRoot, uri, options),
        *allocator_);
}

Base::Result<XamlDocument> Loader::LoadComponent(
    Base::Object& existingRoot,
    const Base::ResourceUri& uri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        state_->LoadComponent(existingRoot, uri, options),
        *allocator_);
}

Base::Result<XamlDocument> Loader::LoadCompiled(
    Base::Span<const std::uint8_t> bytes,
    const Base::ResourceUri& originUri,
    const XamlReaderSettings& options) noexcept {
    if (state_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfMemory,
            "Markup loader allocation failed");
    }
    return AdoptResult(
        state_->LoadCompiled(bytes, originUri, options),
        *allocator_);
}

} // namespace Aero::Markup


