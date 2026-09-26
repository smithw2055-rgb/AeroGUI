#pragma once

#include <Aero/Module.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>

namespace Aero::Meta { class Registry; class Registration; }
namespace Aero::Markup { class Schema; }


namespace Aero {

class ModuleSet  {
public:
    ModuleSet() noexcept;
    ~ModuleSet() noexcept;

    ModuleSet(const ModuleSet&) = delete;
    ModuleSet& operator=(const ModuleSet&) = delete;

    Base::Result<void> Add(
        const ModuleRegistration& registration) noexcept;
    Base::Result<void> RegisterMetadata(
        ::Aero::Meta::Registry& domain) const noexcept;
    Base::Result<void> RegisterResourceScopes(
        ::Aero::Markup::Schema& schema) const noexcept;
    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept;
    std::uint32_t ModuleCount() const noexcept;

private:
    struct Module {
        struct Dependency {
            Base::String name;
            std::uint32_t minimumSchemaVersion = 1U;
        };

        Base::String name;
        std::uint32_t schemaVersion = 1U;
        ModuleRegisterCallback registerModule = nullptr;
        ModuleRegisterContextCallback registerModuleWithContext = nullptr;
        void* context = nullptr;
        std::uint32_t abiVersion = ModuleAbiVersion;
        Base::Vector<Dependency> dependencies;
        Base::Vector<Markup::ResourceScopeRegistration> resourceScopes;
    };

    Base::Result<void> ResolveOrder(
        Base::Vector<std::uint32_t>& order) const noexcept;

    Base::Vector<Module> modules_;
    bool frozen_ = false;
};

} // namespace Aero
