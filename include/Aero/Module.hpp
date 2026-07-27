#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/MetadataDomain.hpp>
#include <Aero/Metadata.hpp>
#include <Aero/Version.hpp>

#include <cstdint>

namespace Aero::Markup {
class XamlRegistrationContext;
}

namespace Aero::Core {

// Product-facing names keep implementation-specific "metadata" terminology
// out of ordinary control declarations while preserving the underlying ABI.
using PropertyOptions = PropertyMetadataFlags;
using TypeOptions = TypeFlags;

} // namespace Aero::Core

namespace Aero {

using ModuleRegisterCallback = Core::MetadataModuleRegisterCallback;
using ModuleRegisterXamlCallback = Base::Result<void> (*)(
    Markup::XamlRegistrationContext& context,
    void* userContext) noexcept;

struct ModuleDependency final {
    Base::StringView name;
    std::uint32_t minimumSchemaVersion = 1U;
};

struct ModuleRegistration final {
    Base::StringView name;
    std::uint32_t schemaVersion = 1U;
    ModuleRegisterCallback registerModule = nullptr;
    void* context = nullptr;
    ModuleRegisterXamlCallback registerXaml = nullptr;
    std::uint32_t abiVersion = ModuleAbiVersion;
    Base::Span<const ModuleDependency> dependencies;
};

constexpr ModuleRegistration DefineModule(
    Base::StringView name,
    ModuleRegisterCallback registerModule,
    std::uint32_t schemaVersion = 1U,
    void* context = nullptr) noexcept {
    ModuleRegistration module;
    module.name = name;
    module.schemaVersion = schemaVersion;
    module.registerModule = registerModule;
    module.context = context;
    return module;
}

// Root-level module catalog for AeroGUI composition. Modules register metadata
// descriptors and facets once; Markup, Presentation, Controls, tools, and the
// runtime all consume the sealed MetadataDomain instead of maintaining a second
// XAML-specific registration path.
class AERO_API ModuleCatalog final {
public:
    Base::Result<void> TryAdd(
        const ModuleRegistration& registration) noexcept;
    Base::Result<void> RegisterMetadata(
        Core::MetadataDomain& domain) const noexcept;
    Base::Result<void> RegisterXaml(
        Markup::XamlRegistrationContext& context) const noexcept;
    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    std::uint32_t ModuleCount() const noexcept {
        return modules_.Size();
    }

private:
    struct Module final {
        Base::String name;
        std::uint32_t schemaVersion = 1U;
        ModuleRegisterCallback registerModule = nullptr;
        void* context = nullptr;
        ModuleRegisterXamlCallback registerXaml = nullptr;
        std::uint32_t abiVersion = ModuleAbiVersion;
        struct Dependency final {
            Base::String name;
            std::uint32_t minimumSchemaVersion = 1U;
        };
        Base::Vector<Dependency> dependencies;
    };

    Base::Result<void> ResolveOrder(
        Base::Vector<std::uint32_t>& order) const noexcept;
    Base::Vector<Module> modules_;
    bool frozen_ = false;
};

} // namespace Aero
