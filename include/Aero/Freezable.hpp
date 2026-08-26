#pragma once

#include <Aero/DependencyObject.hpp>

namespace Aero {

class Freezable;
struct FreezableState;

using FreezableChangedHandler = Base::Delegate<void(Freezable&)>;

// Instance-level shareable dependency object. A frozen object rejects every
// dependency-property mutation and no longer participates in consumer
// invalidation. Freezing does not remove the object's dispatcher affinity.
class AERO_GUI_API Freezable : public DependencyObject {
    AERO_DECLARE_TYPE(Freezable, DependencyObject)
public:

    bool IsFrozen() const noexcept;
    bool CanFreeze() const noexcept;
    Result<void> Freeze() noexcept;

    Result<void> AddChangedHandlerChecked(
        const FreezableChangedHandler& handler) noexcept;
    void AddChangedHandler(
        const FreezableChangedHandler& handler) noexcept;
    bool RemoveChangedHandler(
        const FreezableChangedHandler& handler) noexcept;

protected:
    explicit Freezable(Meta::TypeId runtimeType) noexcept;
    ~Freezable() override;

    Result<void> WritePreamble() const noexcept;
    void WritePostscript() noexcept;
    virtual bool FreezeCore(bool isChecking) noexcept;
    virtual void OnChanged() noexcept;

    void OnPropertyInvalidated(
        Meta::PropertyInvalidationFlags flags) noexcept override;
    Result<void> VerifyMutationAllowed() const noexcept override;

private:
    friend struct FreezableState;
    #if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
    #endif

    bool EnsureState() noexcept;
    FreezableState* impl_ = nullptr;
};

} // namespace Aero
