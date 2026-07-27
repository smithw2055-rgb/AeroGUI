#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Module.hpp>
#include <Aero/RuntimeHost.hpp>
#include <Aero/SchemaBundle.hpp>

namespace Aero::Markup {
class DocumentCache;
}

namespace Aero {

class View;

// Process/application-level immutable composition. Its internal state is
// reference counted so views remain valid when the lightweight environment
// facade is released. Modules and schemas are still frozen exactly once.
class AERO_API RuntimeEnvironment final {
public:
    explicit RuntimeEnvironment(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~RuntimeEnvironment() noexcept = default;

    RuntimeEnvironment(const RuntimeEnvironment&) = delete;
    RuntimeEnvironment& operator=(const RuntimeEnvironment&) = delete;

    Base::Result<void> AddModule(
        const ModuleRegistration& registration) noexcept;
    Base::Result<void> Initialize() noexcept;
    Base::Result<Base::Ref<View>> CreateView(
        const RuntimeHostOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept;

    bool IsInitialized() const noexcept;
    SchemaBundle& Schema() noexcept;
    const SchemaBundle& Schema() const noexcept;
    Markup::DocumentCache& Documents() noexcept;
    const Markup::DocumentCache& Documents() const noexcept;

private:
    friend class View;
    Base::IAllocator* allocator_ = nullptr;
    Base::Ref<Base::Object> state_;
};

class AERO_API View final : public Base::Object {
public:
    explicit View(
        RuntimeEnvironment& environment,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~View() noexcept override { Shutdown(); }

    View(const View&) = delete;
    View& operator=(const View&) = delete;

    Base::Result<void> Initialize(
        const RuntimeHostOptions& options = {}) noexcept;
    void Shutdown() noexcept { host_.Shutdown(); }

    Base::Result<UiDocument> Load(
        Base::StringView uri,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<UiDocument> Parse(
        Base::StringView source,
        const Base::ResourceUri& baseUri = {},
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;
    Base::Result<void> SetContent(
        UiDocument&& document,
        Presentation::Size availableSize) noexcept;
    Base::Result<void> LoadContent(
        Base::StringView uri,
        Presentation::Size availableSize,
        Core::IDiagnosticSink* diagnostics = nullptr) noexcept;

    RuntimeHost& Host() noexcept { return host_; }
    const RuntimeHost& Host() const noexcept { return host_; }

private:
    Base::Ref<Base::Object> environmentState_;
    RuntimeHost host_;
};

} // namespace Aero
