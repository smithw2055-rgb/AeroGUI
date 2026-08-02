#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Version.hpp>

#include <cstdint>

namespace Aero {

namespace Meta { class Registration; }


using ModuleRegisterCallback = Base::Result<void> (*)(
    Meta::Registration& registration) noexcept;
using ModuleRegisterContextCallback = Base::Result<void> (*)(
    Meta::Registration& registration,
    void* userContext) noexcept;
struct ModuleDependency  {
    Base::StringView name;
    std::uint32_t minimumSchemaVersion = 1U;
};

struct ModuleRegistration  {
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

} // namespace Aero
