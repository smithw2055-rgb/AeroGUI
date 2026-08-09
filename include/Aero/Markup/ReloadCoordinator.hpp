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
class AERO_GUI_API ReloadCoordinator  {
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

    Result<void> Start(
        StringView rootUri,
        Aero::Base::Size availableSize,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    void Stop() noexcept;

    Result<ReloadResult> Poll(
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Result<ReloadResult> Reload(
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Result<ReloadResult> NotifySourceChanged(
        const Base::ResourceUri& changedUri,
        Diagnostics::IDiagnosticSink* diagnostics = nullptr) noexcept;

    bool IsActive() const noexcept;
    const Base::ResourceUri& RootUri() const noexcept;
    std::uint64_t Generation() const noexcept;
    std::uint32_t TrackedSourceCount() const noexcept;

private:
    Base::IAllocator* allocator_ = nullptr;
    void* state_ = nullptr;
};

} // namespace Aero::Markup
