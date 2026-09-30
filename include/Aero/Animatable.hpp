#pragma once

#include <Aero/Freezable.hpp>

namespace Aero {

// WPF Animatable: the single base for brushes, geometries, transforms,
// effects and timelines. Clocks and sub-property invalidation attach here.
// Transform3D stays on Freezable; it does not join this layer.
class AERO_GUI_API Animatable : public Freezable {
    AERO_DECLARE_TYPE(Animatable, Freezable)

public:
    bool HasAnimatedProperties() const noexcept;

protected:
    explicit Animatable(Meta::TypeId runtimeType) noexcept;
    ~Animatable() override;

    void OnChanged() noexcept override;
    virtual void OnSubPropertyChanged() noexcept;

private:
};

} // namespace Aero
