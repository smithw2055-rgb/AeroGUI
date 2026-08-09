#include <Aero/Markup/ReloadCoordinator.hpp>

#include <Aero/Base/Vector.hpp>
#include <Aero/Diagnostics.hpp>
#include <Aero/View.hpp>
#include <Aero/Markup/XamlReader.hpp>


#include <new>
#include <utility>

#include "gui/GuiData.hpp"

namespace Aero::Markup {

struct ReloadCoordinatorState final {
    struct RevisionRecord  {
        Base::ResourceUri uri;
        std::uint64_t revision = 0U;
    };

    ReloadCoordinatorState(
        View& valueView,
        Base::IAllocator& valueAllocator,
        GuiState* valueGui) noexcept
        : view(&valueView),
          allocator(&valueAllocator),
          gui(valueGui),
          revisions(&valueAllocator) {}

    Base::Result<std::uint64_t> ReadRevision(
        const Base::ResourceUri& uri) noexcept {
        if (view == nullptr || gui == nullptr || uri.Empty()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML reload source is unavailable");
        }
        std::uint64_t sourceIdentity = 0U;
        std::uint64_t revision = 0U;
        Base::Result<void> queried = gui->xaml.QuerySource(
            gui->xamlProviders, uri, sourceIdentity, revision);
        return queried
            ? Base::Result<std::uint64_t>(revision)
            : Base::Result<std::uint64_t>(queried.GetStatus());
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
        if (view == nullptr || gui == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML reload View is unavailable");
        }
        std::uint64_t sourceIdentity = 0U;
        std::uint64_t currentRevision = 0U;
        Base::Result<void> queried = gui->xaml.QuerySource(
            gui->xamlProviders, uri, sourceIdentity, currentRevision);
        if (!queried) return queried.GetStatus();
        std::uint64_t revision = currentRevision;
        static_cast<void>(gui->xaml.TryGetCachedRevision(
            uri, sourceIdentity, revision));
        RevisionRecord record;
        record.uri = uri;
        record.revision = revision;
        return records.PushBack(std::move(record));
    }

    Base::Result<void> BuildTrackedSources(
        const XamlDocument& document,
        Base::ResourceUri& resolvedRoot,
        Base::Vector<RevisionRecord>& output) noexcept {
        output.Clear();
        if (view == nullptr || !document.IsValid()) {
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
        Diagnostics::IDiagnosticSink* diagnostics) noexcept {
        if (!active || view == nullptr || view->GetContent() == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML reload coordinator is not active");
        }
        if (gui == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidState,
                "XAML reload Gui state is unavailable");
        }
        Base::Result<std::uint32_t> invalidated =
            gui->xaml.Invalidate(changed, true);
        if (!invalidated) return invalidated.GetStatus();
        const std::uint32_t invalidatedCount = invalidated.Value();

        XamlReader reader(view->GetGui());
        Base::Result<XamlDocument> replacement =
            reader.Load(rootUri.Canonical(), {}, diagnostics);
        if (!replacement) return replacement.GetStatus();
        Base::ResourceUri replacementRoot = rootUri;
        Base::Vector<RevisionRecord> replacementRevisions(allocator);
        Base::Result<void> tracked = BuildTrackedSources(
            replacement.Value(), replacementRoot, replacementRevisions);
        if (!tracked) return tracked.GetStatus();
        Base::Result<void> replaced = view->SetContent(
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

    View* view = nullptr;
    Base::IAllocator* allocator = nullptr;
    GuiState* gui = nullptr;
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
    Gui& gui = view.GetGui();
    GuiState* guiState = gui.state_
        ? &static_cast<GuiState&>(*gui.state_)
        : nullptr;
    void* memory = allocator_->Allocate({sizeof(ReloadCoordinatorState),
        alignof(ReloadCoordinatorState), Base::MemoryTag::Markup});
    if (memory == nullptr) {
        Base::ReportOutOfMemory(sizeof(ReloadCoordinatorState),
            alignof(ReloadCoordinatorState), Base::MemoryTag::Markup);
    }
    state_ = new (memory) ReloadCoordinatorState(
        view, *allocator_, guiState);
}

ReloadCoordinator::~ReloadCoordinator() noexcept {
    if (state_ == nullptr) return;
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    state->~ReloadCoordinatorState();
    allocator_->Deallocate(state, sizeof(ReloadCoordinatorState),
        alignof(ReloadCoordinatorState), Base::MemoryTag::Markup);
}

ReloadCoordinator::ReloadCoordinator(
    ReloadCoordinator&& other) noexcept
    : allocator_(other.allocator_), state_(other.state_) {
    other.allocator_ = nullptr;
    other.state_ = nullptr;
}

ReloadCoordinator& ReloadCoordinator::operator=(
    ReloadCoordinator&& other) noexcept {
    if (this == &other) return *this;
    if (state_ != nullptr) {
        auto* state = static_cast<ReloadCoordinatorState*>(state_);
        state->~ReloadCoordinatorState();
        allocator_->Deallocate(state, sizeof(ReloadCoordinatorState),
            alignof(ReloadCoordinatorState), Base::MemoryTag::Markup);
    }
    allocator_ = other.allocator_;
    state_ = other.state_;
    other.allocator_ = nullptr;
    other.state_ = nullptr;
    return *this;
}

Base::Result<void> ReloadCoordinator::Start(
    Base::StringView rootUri,
    Aero::Size availableSize,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    if (state == nullptr || state->view == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotInitialized,
            "XAML reload requires a View");
    }
    if (state->active || state->view->GetContent() != nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            "XAML reload requires an unmounted View");
    }
    Base::Result<Base::ResourceUri> parsed =
        Base::ResourceUri::Parse(rootUri);
    if (!parsed) return parsed.GetStatus();
    XamlReader reader(state->view->GetGui());
    Base::Result<XamlDocument> document =
        reader.Load(rootUri, {}, diagnostics);
    if (!document) return document.GetStatus();
    Base::ResourceUri resolvedRoot = parsed.Value();
    Base::Vector<ReloadCoordinatorState::RevisionRecord> revisions(
        state->allocator);
    Base::Result<void> tracked = state->BuildTrackedSources(
        document.Value(), resolvedRoot, revisions);
    if (!tracked) return tracked.GetStatus();
    Base::Result<void> mounted = state->view->SetContent(
        std::move(document).Value(), availableSize);
    if (!mounted) return mounted.GetStatus();

    state->rootUri = resolvedRoot;
    state->revisions = std::move(revisions);
    state->availableSize = availableSize;
    state->active = true;
    return {};
}

void ReloadCoordinator::Stop() noexcept {
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    if (state == nullptr) return;
    state->active = false;
    state->revisions.Clear();
    state->rootUri = {};
}

Base::Result<ReloadResult> ReloadCoordinator::Poll(
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    ReloadResult noChange;
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    if (state == nullptr || !state->active) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML reload coordinator is not active");
    }
    noChange.generation = state->generation;
    for (const ReloadCoordinatorState::RevisionRecord& record : state->revisions) {
        Base::Result<std::uint64_t> current =
            state->ReadRevision(record.uri);
        if (!current) return current.GetStatus();
        if (current.Value() == record.revision) continue;
        return state->ReloadFor(record.uri, diagnostics);
    }
    return noChange;
}

Base::Result<ReloadResult> ReloadCoordinator::Reload(
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    if (state == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML reload coordinator is unavailable");
    }
    return state->ReloadFor(state->rootUri, diagnostics);
}

Base::Result<ReloadResult>
ReloadCoordinator::NotifySourceChanged(
    const Base::ResourceUri& changedUri,
    Diagnostics::IDiagnosticSink* diagnostics) noexcept {
    auto* state = static_cast<ReloadCoordinatorState*>(state_);
    if (state == nullptr || changedUri.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML reload changed URI is invalid");
    }
    return state->ReloadFor(changedUri, diagnostics);
}

bool ReloadCoordinator::IsActive() const noexcept {
    const auto* state = static_cast<const ReloadCoordinatorState*>(state_);
    return state != nullptr && state->active;
}

const Base::ResourceUri& ReloadCoordinator::RootUri() const noexcept {
    static const Base::ResourceUri empty;
    const auto* state = static_cast<const ReloadCoordinatorState*>(state_);
    return state != nullptr ? state->rootUri : empty;
}

std::uint64_t ReloadCoordinator::Generation() const noexcept {
    const auto* state = static_cast<const ReloadCoordinatorState*>(state_);
    return state != nullptr ? state->generation : 0U;
}

std::uint32_t ReloadCoordinator::TrackedSourceCount() const noexcept {
    const auto* state = static_cast<const ReloadCoordinatorState*>(state_);
    return state != nullptr ? state->revisions.Size() : 0U;
}

} // namespace Aero::Markup
