#pragma once

#include <Aero/DependencyObject.hpp>

namespace Aero {

class Freezable;

using FreezableChangedHandler = Base::Delegate<void(Freezable&)>;

// Instance-level shareable dependency object. A frozen object rejects every
// dependency-property mutation and no longer participates in consumer
// invalidation. Freezing does not remove the object's dispatcher affinity.
class AERO_GUI_API Freezable : public DependencyObject {
    AERO_DECLARE_TYPE(Freezable, DependencyObject)
public:
    struct State;  // Freezable.cpp-local program data (nested; definition private to TU)

    bool IsFrozen() const noexcept;
    bool CanFreeze() const noexcept;
    Result<void> Freeze() noexcept;

    void AddChangedHandler(const FreezableChangedHandler& handler) noexcept;
    bool RemoveChangedHandler(const FreezableChangedHandler& handler) noexcept;

protected:
    explicit Freezable(Meta::TypeId runtimeType) noexcept;
    ~Freezable() override;

    Result<void> WritePreamble() const noexcept;
    void WritePostscript() noexcept;
    virtual bool FreezeCore(bool isChecking) noexcept;
    virtual void OnChanged() noexcept;

    void OnPropertyInvalidated(Meta::PropertyInvalidationFlags flags) noexcept override;
    Result<void> VerifyMutationAllowed() const noexcept override;

public:
    std::uint64_t Revision() const noexcept;
    bool CheckFreezeCore() noexcept;
    DependencyObject* Parent() const noexcept;
    Base::Result<void> AttachConsumer(DependencyObject& object, Meta::DependencyPropertyHandle property) noexcept;
    void DetachConsumer(DependencyObject& object, Meta::DependencyPropertyHandle property) noexcept;
private:
    bool EnsureState() noexcept;
    State* state_ = nullptr;
};

} // namespace Aero
