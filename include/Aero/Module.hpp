#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Version.hpp>

#include <cstdint>

namespace Aero {

namespace Core {
class MetadataContext;
}

using ModuleRegisterCallback = Base::Result<void> (*)(
    Core::MetadataContext& context) noexcept;
using ModuleRegisterContextCallback = Base::Result<void> (*)(
    Core::MetadataContext& context,
    void* userContext) noexcept;
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

} // namespace Aero
