#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/MetadataDomain.hpp>
#include <Aero/Version.hpp>

#include <cstdint>

namespace Aero {

using ModuleRegisterCallback = Core::MetadataModuleRegisterCallback;
using ModuleRegisterContextCallback =
    Core::MetadataModuleRegisterContextCallback;
struct ModuleDependency final {
    Base::StringView name;
    std::uint32_t minimumSchemaVersion = 1U;
};

struct ModuleRegistration final {
    Base::StringView name;
    std::uint32_t schemaVersion = 1U;
    ModuleRegisterCallback registerModule = nullptr;
    ModuleRegisterContextCallback registerModuleWithContext = nullptr;
    void* context = nullptr;
    std::uint32_t abiVersion = ModuleAbiVersion;
    Base::Span<const ModuleDependency> dependencies;
};

constexpr ModuleRegistration DefineModule(
    Base::StringView name,
    ModuleRegisterCallback registerModule) noexcept {
    ModuleRegistration registration;
    registration.name = name;
    registration.registerModule = registerModule;
    return registration;
}

// Root-level module catalog for AeroGUI composition. Modules describe types
// once; markup, presentation, controls, tools, and runtime all consume the
// same sealed MetadataDomain.
class AERO_API ModuleCatalog final {
public:
    Base::Result<void> Add(
        const ModuleRegistration& registration) noexcept;
    Base::Result<void> RegisterMetadata(
        Core::MetadataDomain& domain) const noexcept;
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
        ModuleRegisterContextCallback registerModuleWithContext = nullptr;
        void* context = nullptr;
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
