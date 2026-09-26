#include <Aero/Animatable.hpp>

#include "gui/internal/AeroGuiInternal.hpp"
#include "gui/internal/PropertyStore.hpp"

namespace Aero {

Animatable::Animatable(Meta::TypeId runtimeType) noexcept
    : Freezable(runtimeType) {}

Animatable::~Animatable() = default;

bool Animatable::HasAnimatedProperties() const noexcept {
    const PropertyStore* store = AeroGuiInternal::Store(*this);
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

} // namespace Aero
