#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/View.hpp>
#include <Aero/Media/Geometry.hpp>

#include <cstdint>

namespace Aero::Diagnostics {
class IDiagnosticSink;
}

namespace Aero::Markup {

struct ReloadResult  {
    bool changed = false;
    bool reloaded = false;
    std::uint32_t invalidatedDocuments = 0U;
    std::uint64_t generation = 0U;
    Base::ResourceUri changedUri;
};

// Development-time full-document reload coordinator. It polls source-provider
// revisions, invalidates the shared document cache through the reverse
// dependency graph, loads a replacement XamlDocument off to the side, and swaps
// it into a mounted View only after the replacement is valid. All calls must
// occur on the View owner thread.
class AERO_API ReloadCoordinator  {
public:
    explicit ReloadCoordinator(
        View& view,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~ReloadCoordinator() noexcept;

    ReloadCoordinator(ReloadCoordinator&& other) noexcept;
    ReloadCoordinator& operator=(
        ReloadCoordinator&& other) noexcept;

    ReloadCoordinator(const ReloadCoordinator&) = delete;
    ReloadCoordinator& operator=(
        const ReloadCoordinator&) = delete;

    Base::Result<void> Start(
        Base::StringView rootUri,
        Aero::Base::Size availableSize,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    void Stop() noexcept;

    Base::Result<ReloadResult> Poll(
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<ReloadResult> Reload(
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<ReloadResult> NotifySourceChanged(
        const Base::ResourceUri& changedUri,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;

    bool IsActive() const noexcept;
    const Base::ResourceUri& RootUri() const noexcept;
    std::uint64_t Generation() const noexcept;
    std::uint32_t TrackedSourceCount() const noexcept;

private:
    static bool ViewIsInitialized(const View& view) noexcept;
    static bool ViewIsMounted(const View& view) noexcept;
    static Base::Result<void> QueryReloadSource(
        View& view, const Base::ResourceUri& uri,
        std::uint64_t& sourceIdentity, std::uint64_t& revision) noexcept;
    static bool TryGetCachedReloadRevision(
        View& view, const Base::ResourceUri& uri,
        std::uint64_t sourceIdentity, std::uint64_t& revision) noexcept;
    static Base::Result<std::uint32_t> InvalidateReloadDocuments(
        View& view, const Base::ResourceUri& uri,
        bool includeDependents) noexcept;
    static Base::Result<void> ReplaceMountedDocument(
        View& view, XamlDocument&& document, Aero::Size availableSize) noexcept;
    static Base::Result<void> MountDocument(
        View& view, XamlDocument&& document, Aero::Size availableSize) noexcept;

    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Markup
