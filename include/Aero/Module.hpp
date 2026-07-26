#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Metadata/MetadataDomain.hpp>

#include <cstdint>

namespace Aero {

using ModuleRegisterCallback = Core::MetadataModuleRegisterCallback;

struct ModuleRegistration final {
    Base::StringView name;
    std::uint32_t schemaVersion = 1U;
    ModuleRegisterCallback registerModule = nullptr;
    void* context = nullptr;
};

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
    };

    Base::Vector<Module> modules_;
    bool frozen_ = false;
};

} // namespace Aero
