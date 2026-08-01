#pragma once

#include <Aero/Styling.hpp>

namespace Aero::Detail {

// Runtime-only type-keyed default-style table. Public controls expose WPF
// Style semantics; lookup storage and base-type traversal belong to the view.
class ThemeStyleRegistry final {
public:
    explicit ThemeStyleRegistry(
        const Core::DependencyPropertyRegistry& properties) noexcept
        : properties_(&properties) {}

    Base::Result<void> TryRegister(
        Core::TypeId controlType,
        const Style& style) noexcept;
    const Style* Find(Core::TypeId controlType) const noexcept;

private:
    struct Entry final {
        Core::TypeId controlType = Core::InvalidTypeId;
        const Style* style = nullptr;
    };

    const Core::DependencyPropertyRegistry* properties_ = nullptr;
    Base::Vector<Entry> entries_;
};

} // namespace Aero::Detail
