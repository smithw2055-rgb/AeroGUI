#pragma once

#include <Aero/Gui/DependencyProperty.hpp>

namespace Aero {

class Freezable;

using FreezableChangedHandler = Base::Delegate<void(Freezable&)>;

// Instance-level shareable dependency object. A frozen object rejects every
// dependency-property mutation and no longer participates in consumer
// invalidation. Freezing does not remove the object's dispatcher affinity.
class AERO_API Freezable : public DependencyObject {
    AERO_DECLARE_TYPE(Freezable, DependencyObject)
public:
    struct Impl;

    bool IsFrozen() const noexcept;
    bool CanFreeze() const noexcept;
    Base::Result<void> Freeze() noexcept;

    Base::Result<void> AddChangedHandlerChecked(
        const FreezableChangedHandler& handler) noexcept;
    void AddChangedHandler(
        const FreezableChangedHandler& handler) noexcept;
    bool RemoveChangedHandler(
        const FreezableChangedHandler& handler) noexcept;

protected:
    explicit Freezable(Meta::TypeId runtimeType) noexcept;
    ~Freezable() override;

    Base::Result<void> WritePreamble() const noexcept;
    void WritePostscript() noexcept;
    virtual bool FreezeCore(bool isChecking) noexcept;
    virtual void OnChanged() noexcept;

    void OnPropertyInvalidated(
        Meta::PropertyInvalidationFlags flags) noexcept override;
    Base::Result<void> VerifyMutationAllowed() const noexcept override;

private:
    Base::IAllocator* implAllocator_ = nullptr;
    Impl* impl_ = nullptr;
};

} // namespace Aero
