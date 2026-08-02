#pragma once

#include <Aero/Module.hpp>

namespace Aero::Meta { class Registry; class Registration; }


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
    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept;
    std::uint32_t ModuleCount() const noexcept;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace Aero
