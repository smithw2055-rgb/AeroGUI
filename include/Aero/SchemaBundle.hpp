#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Module.hpp>

namespace Aero::Core {
class DependencyPropertyRegistry;
class EffectiveValueEngine;
class MetadataDomain;
class MetadataRuntime;
}

namespace Aero::Markup {
class Schema;
}

namespace Aero::Presentation {
class ResourceDictionary;
}

namespace Aero {

struct SchemaBundleServices final {
    Base::IAllocator* allocator = nullptr;
};

// Owns one immutable metadata + XAML schema composition. Construction is split
// because runtime property services require the sealed MetadataDomain, while
// XAML extensions are registered only after those services exist.
class AERO_API SchemaBundle final {
public:
    explicit SchemaBundle(
        Base::IAllocator* allocator = nullptr) noexcept;
    ~SchemaBundle() noexcept;

    SchemaBundle(const SchemaBundle&) = delete;
    SchemaBundle& operator=(const SchemaBundle&) = delete;

    Base::Result<void> Prepare(
        const ModuleCatalog& modules) noexcept;
    Base::Result<void> Finalize(
        const ModuleCatalog& modules,
        const SchemaBundleServices& services) noexcept;

    bool IsPrepared() const noexcept;
    bool IsFrozen() const noexcept;

    Core::MetadataDomain& Metadata() noexcept;
    const Core::MetadataDomain& Metadata() const noexcept;
    Core::MetadataRuntime& Runtime() noexcept;
    const Core::MetadataRuntime& Runtime() const noexcept;
    Markup::Schema& Schema() noexcept;
    const Markup::Schema& Schema() const noexcept;
private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero
