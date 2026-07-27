#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Module.hpp>
#include <Aero/Markup/Runtime/XamlDocumentCache.hpp>
#include <Aero/RuntimeHost.hpp>
#include <Aero/SchemaBundle.hpp>

namespace Aero {

class RuntimeView;

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
    Base::Result<Base::Ref<RuntimeView>> CreateView(
        const RuntimeHostOptions& options = {},
        Base::IAllocator* allocator = nullptr) noexcept;

    bool IsInitialized() const noexcept;
    SchemaBundle& Schema() noexcept;
    const SchemaBundle& Schema() const noexcept;
    Markup::XamlDocumentCache& Documents() noexcept;
    const Markup::XamlDocumentCache& Documents() const noexcept;

private:
    friend class RuntimeView;
    Base::IAllocator* allocator_ = nullptr;
    Base::Ref<Base::Object> state_;
};

class AERO_API RuntimeView final : public Base::Object {
public:
    explicit RuntimeView(
        RuntimeEnvironment& environment,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~RuntimeView() noexcept override { Shutdown(); }

    RuntimeView(const RuntimeView&) = delete;
    RuntimeView& operator=(const RuntimeView&) = delete;

    Base::Result<void> Initialize(
        const RuntimeHostOptions& options = {}) noexcept;
    void Shutdown() noexcept { host_.Shutdown(); }

    RuntimeHost& Host() noexcept { return host_; }
    const RuntimeHost& Host() const noexcept { return host_; }

private:
    Base::Ref<Base::Object> environmentState_;
    RuntimeHost host_;
};

} // namespace Aero
