#include <Aero/Animatable.hpp>

#include "gui/core/PropertyStore.hpp"
#include "gui/core/Describe.hpp"

namespace Aero {

Animatable::Animatable(Meta::TypeId runtimeType) noexcept
    : Freezable(runtimeType) {}

Animatable::~Animatable() = default;

bool Animatable::HasAnimatedProperties() const noexcept {
    const PropertyStore* store = (*this).Store();
    if (store == nullptr) {
        return false;
    }
    for (const auto& record : store->entries) {
        if (record.Value().HasAnimation()) {
            return true;
        }
    }
    return false;
}

void Animatable::OnChanged() noexcept {
    OnSubPropertyChanged();
    Freezable::OnChanged();
}

void Animatable::OnSubPropertyChanged() noexcept {}

// Metadata registration for the types implemented in this file.
AERO_DESCRIBE(Animatable) {
    using namespace Aero::Meta;
    Register<Animatable>(context, TypeFlags::Abstract);
}

} // namespace Aero

