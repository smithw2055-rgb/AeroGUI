#include <Aero/Integration/ReloadCoordinator.hpp>

#include <Aero/Base/Hash.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Diagnostics.hpp>
#include "markup/Loader.hpp"
#include <Aero/View.hpp>
#include <Aero/Markup/XamlDocument.hpp>

#include "runtime/ViewState.hpp"
#include "runtime/ViewAccess.hpp"

#include <new>
#include <utility>

#include "controls/RuntimeManagers.hpp"

namespace Aero::Integration {

struct ReloadCoordinator::Impl final {
    struct RevisionRecord final {
        Base::ResourceUri uri;
        std::uint64_t revision = 0U;
    };

    Impl(Aero::Detail::ViewState& valueHost, View& valueView, Base::IAllocator& valueAllocator) noexcept
        : host(&valueHost),
          view(&valueView),
          allocator(&valueAllocator),
          revisions(&valueAllocator) {}

    Base::Result<std::uint64_t> ReadRevision(
        const Base::ResourceUri& uri) noexcept {
        if (host == nullptr || uri.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML reload source is unavailable");
        }
        Markup::SourceProviderRegistry* providers =
            Aero::Detail::ViewAccess::Sources(*view);
        if (providers == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotInitialized,
                "XAML reload requires source providers");
        }
        Base::Result<Markup::SourceProviderResolution> provider =
            providers->ResolveDetailed(uri);
        if (!provider) return provider.GetStatus();
        Base::Result<std::uint64_t> revision =
            provider.Value().provider->Revision(uri);
        if (revision && revision.Value() != 0U) {
            return revision.Value();
        }
        Base::Result<Markup::Source> source =
            provider.Value().provider->Load(uri);
        if (!source) return source.GetStatus();
        return source.Value().revision != 0U
            ? source.Value().revision
            : Base::HashBytes(
                  source.Value().bytes.Data(),
                  source.Value().bytes.Size());
    }

    static bool HasTracked(
        const Base::Vector<RevisionRecord>& records,
        const Base::ResourceUri& uri) noexcept {
        for (const RevisionRecord& record : records) {
            if (record.uri == uri) return true;
        }
        return false;
    }

    Base::Result<void> AddTracked(
        Base::Vector<RevisionRecord>& records,
        const Base::ResourceUri& uri) noexcept {
        if (uri.Empty() || HasTracked(records, uri)) return {};
        std::uint64_t revision = 0U;
        std::uint64_t sourceIdentity = 0U;
        Markup::SourceProviderRegistry* providers =
            host != nullptr && view != nullptr
            ? Aero::Detail::ViewAccess::Sources(*view)
            : nullptr;
        if (providers != nullptr) {
            Base::Result<Markup::SourceProviderResolution> resolved =
                providers->ResolveDetailed(uri);
            if (resolved) sourceIdentity = resolved.Value().cacheIdentity;
        }
        Markup::DocumentCache* cache =
            host != nullptr && view != nullptr
            ? Aero::Detail::ViewAccess::DocumentCache(*view)
            : nullptr;
        if (cache == nullptr ||
            !cache->TryGetSourceRevision(
                uri, sourceIdentity, revision)) {
            Base::Result<std::uint64_t> current = ReadRevision(uri);
            if (!current) return current.GetStatus();
            revision = current.Value();
        }
        RevisionRecord record;
        record.uri = uri;
        record.revision = revision;
        return records.TryPushBack(std::move(record));
    }

    Base::Result<void> BuildTrackedSources(
        const UiDocument& document,
        Base::ResourceUri& resolvedRoot,
        Base::Vector<RevisionRecord>& output) noexcept {
        output.Clear();
        if (host == nullptr || !document.IsValid()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML reload document is unavailable");
        }
        if (!document.CanonicalUri().Empty()) {
            resolvedRoot = document.CanonicalUri();
        }
        if (resolvedRoot.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "XAML reload root URI is empty");
        }
        Base::Result<void> root = AddTracked(output, resolvedRoot);
        if (!root) return root.GetStatus();
        for (const Base::ResourceUri& dependency : document.Dependencies()) {
            Base::Result<void> tracked = AddTracked(output, dependency);
            if (!tracked) return tracked.GetStatus();
        }
        return {};
    }

    Base::Result<ReloadResult> ReloadFor(
        const Base::ResourceUri& changed,
        Core::IDiagnosticSink* diagnostics) noexcept {
        if (!active || host == nullptr || !host->IsMounted()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML reload coordinator is not active");
        }
        Markup::DocumentCache* cache =
            Aero::Detail::ViewAccess::DocumentCache(*view);
        std::uint32_t invalidatedCount = 0U;
        if (cache != nullptr) {
            Base::Result<std::uint32_t> invalidated =
                cache->Invalidate(changed, true);
            if (!invalidated) return invalidated.GetStatus();
            invalidatedCount = invalidated.Value();
        }

        Base::Result<UiDocument> replacement =
            host->Load(rootUri.Canonical(), diagnostics);
        if (!replacement) return replacement.GetStatus();
        Base::ResourceUri replacementRoot = rootUri;
        Base::Vector<RevisionRecord> replacementRevisions(allocator);
        Base::Result<void> tracked = BuildTrackedSources(
            replacement.Value(), replacementRoot, replacementRevisions);
        if (!tracked) return tracked.GetStatus();
        Base::Result<void> replaced =
            host->ReplaceMountedDocument(
                std::move(replacement).Value(), availableSize);
        if (!replaced) return replaced.GetStatus();
        rootUri = replacementRoot;
        revisions = std::move(replacementRevisions);

        ReloadResult result;
        result.changed = true;
        result.reloaded = true;
        result.invalidatedDocuments = invalidatedCount;
        result.generation = ++generation;
        result.changedUri = changed;
        return result;
    }

    Aero::Detail::ViewState* host = nullptr;
    View* view = nullptr;
    Base::IAllocator* allocator = nullptr;
    Base::ResourceUri rootUri;
    Aero::Size availableSize;
    Base::Vector<RevisionRecord> revisions;
    std::uint64_t generation = 0U;
    bool active = false;
};

ReloadCoordinator::ReloadCoordinator(
    View& view,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr
          ? allocator
          : &Base::GetDefaultAllocator()) {
    void* memory = allocator_->Allocate({
        sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
    }
    Aero::Detail::ViewState* state = static_cast<Aero::Detail::ViewState*>(
        view.InternalState());
    if (state == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
    }
    impl_ = new (memory) Impl(*state, view, *allocator_);
}

ReloadCoordinator::~ReloadCoordinator() noexcept {
    if (impl_ == nullptr) return;
    impl_->~Impl();
    allocator_->Deallocate(
        impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
}

ReloadCoordinator::ReloadCoordinator(
    ReloadCoordinator&& other) noexcept
    : allocator_(other.allocator_), impl_(other.impl_) {
    other.allocator_ = nullptr;
    other.impl_ = nullptr;
}

ReloadCoordinator& ReloadCoordinator::operator=(
    ReloadCoordinator&& other) noexcept {
    if (this == &other) return *this;
    if (impl_ != nullptr) {
        impl_->~Impl();
        allocator_->Deallocate(
            impl_, sizeof(Impl), alignof(Impl), Base::MemoryTag::Markup);
    }
    allocator_ = other.allocator_;
    impl_ = other.impl_;
    other.allocator_ = nullptr;
    other.impl_ = nullptr;
    return *this;
}

Base::Result<void> ReloadCoordinator::Start(
    Base::StringView rootUri,
    Aero::Size availableSize,
    Core::IDiagnosticSink* diagnostics) noexcept {
    if (impl_ == nullptr || impl_->host == nullptr ||
        !impl_->host->IsInitialized()) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "XAML reload requires an initialized View");
    }
    if (impl_->active || impl_->host->IsMounted()) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML reload requires an unmounted View");
    }
    Base::Result<Base::ResourceUri> parsed =
        Base::ResourceUri::Parse(rootUri);
    if (!parsed) return parsed.GetStatus();
    Base::Result<UiDocument> document =
        impl_->host->Load(rootUri, diagnostics);
    if (!document) return document.GetStatus();
    Base::ResourceUri resolvedRoot = parsed.Value();
    Base::Vector<Impl::RevisionRecord> revisions(impl_->allocator);
    Base::Result<void> tracked = impl_->BuildTrackedSources(
        document.Value(), resolvedRoot, revisions);
    if (!tracked) return tracked.GetStatus();
    Base::Result<void> mounted = impl_->host->Mount(
        std::move(document).Value(), availableSize);
    if (!mounted) return mounted.GetStatus();

    impl_->rootUri = resolvedRoot;
    impl_->revisions = std::move(revisions);
    impl_->availableSize = availableSize;
    impl_->active = true;
    return {};
}

void ReloadCoordinator::Stop() noexcept {
    if (impl_ == nullptr) return;
    impl_->active = false;
    impl_->revisions.Clear();
    impl_->rootUri = {};
}

Base::Result<ReloadResult> ReloadCoordinator::Poll(
    Core::IDiagnosticSink* diagnostics) noexcept {
    ReloadResult noChange;
    if (impl_ == nullptr || !impl_->active) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML reload coordinator is not active");
    }
    noChange.generation = impl_->generation;
    for (const Impl::RevisionRecord& record : impl_->revisions) {
        Base::Result<std::uint64_t> current =
            impl_->ReadRevision(record.uri);
        if (!current) return current.GetStatus();
        if (current.Value() == record.revision) continue;
        return impl_->ReloadFor(record.uri, diagnostics);
    }
    return noChange;
}

Base::Result<ReloadResult> ReloadCoordinator::Reload(
    Core::IDiagnosticSink* diagnostics) noexcept {
    if (impl_ == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML reload coordinator is unavailable");
    }
    return impl_->ReloadFor(impl_->rootUri, diagnostics);
}

Base::Result<ReloadResult>
ReloadCoordinator::NotifySourceChanged(
    const Base::ResourceUri& changedUri,
    Core::IDiagnosticSink* diagnostics) noexcept {
    if (impl_ == nullptr || changedUri.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML reload changed URI is invalid");
    }
    return impl_->ReloadFor(changedUri, diagnostics);
}

bool ReloadCoordinator::IsActive() const noexcept {
    return impl_ != nullptr && impl_->active;
}

const Base::ResourceUri& ReloadCoordinator::RootUri() const noexcept {
    static const Base::ResourceUri empty;
    return impl_ != nullptr ? impl_->rootUri : empty;
}

std::uint64_t ReloadCoordinator::Generation() const noexcept {
    return impl_ != nullptr ? impl_->generation : 0U;
}

std::uint32_t ReloadCoordinator::TrackedSourceCount() const noexcept {
    return impl_ != nullptr ? impl_->revisions.Size() : 0U;
}

} // namespace Aero::Integration
