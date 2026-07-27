#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Module.hpp>
#include <Aero/Markup/Runtime/XamlDocumentCache.hpp>
#include <Aero/RuntimeHost.hpp>
#include <Aero/SchemaBundle.hpp>

namespace Aero {

// Process/application-level immutable composition. Multiple RuntimeView objects
// can share its MetadataRuntime and XAML schema while retaining independent
// layout, input, resources, bindings, and rendering state.
class AERO_API RuntimeEnvironment final {
public:
    explicit RuntimeEnvironment(
        Base::IAllocator* allocator = nullptr) noexcept;

    RuntimeEnvironment(const RuntimeEnvironment&) = delete;
    RuntimeEnvironment& operator=(const RuntimeEnvironment&) = delete;

    Base::Result<void> AddModule(
        const ModuleRegistration& registration) noexcept;
    Base::Result<void> Initialize() noexcept;

    bool IsInitialized() const noexcept;
    SchemaBundle& Schema() noexcept { return schema_; }
    const SchemaBundle& Schema() const noexcept { return schema_; }
    Markup::XamlDocumentCache& Documents() noexcept { return documents_; }
    const Markup::XamlDocumentCache& Documents() const noexcept {
        return documents_;
    }

private:
    Base::IAllocator* allocator_ = nullptr;
    ModuleCatalog modules_;
    SchemaBundle schema_;
    Markup::XamlDocumentCache documents_;
    bool initialized_ = false;
};

class AERO_API RuntimeView final {
public:
    explicit RuntimeView(
        RuntimeEnvironment& environment,
        Base::IAllocator* allocator = nullptr) noexcept;

    RuntimeView(const RuntimeView&) = delete;
    RuntimeView& operator=(const RuntimeView&) = delete;

    Base::Result<void> Initialize(
        const RuntimeHostOptions& options = {}) noexcept;
    void Shutdown() noexcept { host_.Shutdown(); }

    RuntimeHost& Host() noexcept { return host_; }
    const RuntimeHost& Host() const noexcept { return host_; }

private:
    RuntimeEnvironment* environment_ = nullptr;
    RuntimeHost host_;
};

} // namespace Aero
