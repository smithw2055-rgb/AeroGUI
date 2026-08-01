#pragma once

#include <Aero/Module.hpp>

namespace Aero::Core {
class MetadataDomain;
}

namespace Aero {

class ModuleCatalog final {
public:
    ModuleCatalog() noexcept;
    ~ModuleCatalog() noexcept;

    ModuleCatalog(const ModuleCatalog&) = delete;
    ModuleCatalog& operator=(const ModuleCatalog&) = delete;

    Base::Result<void> Add(
        const ModuleRegistration& registration) noexcept;
    Base::Result<void> RegisterMetadata(
        Core::MetadataDomain& domain) const noexcept;
    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept;
    std::uint32_t ModuleCount() const noexcept;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace Aero
