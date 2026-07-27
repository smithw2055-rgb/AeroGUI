#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Presentation/Layout.hpp>

#include <cstdint>

namespace Aero::Core {
class IDiagnosticSink;
}

namespace Aero {

class RuntimeHost;

struct XamlReloadResult final {
    bool changed = false;
    bool reloaded = false;
    std::uint32_t invalidatedDocuments = 0U;
    std::uint64_t generation = 0U;
    Base::ResourceUri changedUri;
};

// Development-time full-document reload coordinator. It polls source-provider
// revisions, invalidates the shared document cache through the reverse
// dependency graph, loads a replacement UiDocument off to the side, and swaps
// it into a mounted RuntimeHost only after the replacement is valid. All calls
// must occur on the RuntimeHost owner thread.
class AERO_API XamlReloadCoordinator final {
public:
    explicit XamlReloadCoordinator(
        RuntimeHost& host,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~XamlReloadCoordinator() noexcept;

    XamlReloadCoordinator(XamlReloadCoordinator&& other) noexcept;
    XamlReloadCoordinator& operator=(
        XamlReloadCoordinator&& other) noexcept;

    XamlReloadCoordinator(const XamlReloadCoordinator&) = delete;
    XamlReloadCoordinator& operator=(
        const XamlReloadCoordinator&) = delete;

    Base::Result<void> Start(
        Base::StringView rootUri,
        Presentation::Size availableSize,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    void Stop() noexcept;

    Base::Result<XamlReloadResult> Poll(
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<XamlReloadResult> Reload(
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<XamlReloadResult> NotifySourceChanged(
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

} // namespace Aero
