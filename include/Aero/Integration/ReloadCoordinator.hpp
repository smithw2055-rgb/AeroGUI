#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Integration/ViewHost.hpp>
#include <Aero/Media/Geometry.hpp>

#include <cstdint>

namespace Aero::Core {
class IDiagnosticSink;
}

namespace Aero::Integration {

struct ReloadResult final {
    bool changed = false;
    bool reloaded = false;
    std::uint32_t invalidatedDocuments = 0U;
    std::uint64_t generation = 0U;
    Base::ResourceUri changedUri;
};

// Development-time full-document reload coordinator. It polls source-provider
// revisions, invalidates the shared document cache through the reverse
// dependency graph, loads a replacement UiDocument off to the side, and swaps
// it into a mounted View only after the replacement is valid. All calls must
// occur on the View owner thread.
class AERO_API ReloadCoordinator final {
public:
    explicit ReloadCoordinator(
        ViewHost& host,
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
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    void Stop() noexcept;

    Base::Result<ReloadResult> Poll(
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<ReloadResult> Reload(
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<ReloadResult> NotifySourceChanged(
        const Base::ResourceUri& changedUri,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;

    bool IsActive() const noexcept;
    const Base::ResourceUri& RootUri() const noexcept;
    std::uint64_t Generation() const noexcept;
    std::uint32_t TrackedSourceCount() const noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero::Integration
