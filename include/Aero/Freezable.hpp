#pragma once

#include <Aero/DependencyObject.hpp>

namespace Aero {

class Freezable;
class BindingEngine;

using FreezableChangedHandler = Base::Delegate<void(Freezable&)>;

// Instance-level shareable dependency object. A frozen object rejects every
// dependency-property mutation and no longer participates in consumer
// invalidation. Freezing does not remove the object's dispatcher affinity.
class AERO_GUI_API Freezable : public DependencyObject {
    AERO_DECLARE_TYPE(Freezable, DependencyObject)

public:
    struct State;  // Freezable.cpp-local program data (nested; definition private to TU)
    struct FreezeCheckContext;  // freeze-walk context (defined in Freezable.cpp)

    bool CanFreeze() const noexcept;
    bool Freeze() noexcept;

    bool IsFrozen() const noexcept;

    Base::Ref<Freezable> Clone() const;
    Base::Ref<Freezable> CloneCurrentValue() const;

    void AddChangedHandler(const FreezableChangedHandler& handler) noexcept;
    bool RemoveChangedHandler(const FreezableChangedHandler& handler) noexcept;

protected:
    explicit Freezable(Meta::TypeId runtimeType) noexcept;
    ~Freezable() override;

    virtual Base::Ref<Freezable> CreateInstanceCore() const;
    virtual void CloneCore(const Freezable& source);
    virtual void CloneCurrentValueCore(const Freezable& source);

    bool WritePreamble() const noexcept;
    void WritePostscript() noexcept;
    virtual bool FreezeCore(bool isChecking) noexcept;
    virtual void OnChanged() noexcept;
    void OnPropertyInvalidated(Meta::PropertyInvalidationFlags flags) noexcept override;
    bool VerifyMutationAllowed() const noexcept override;
    std::uint64_t Revision() const noexcept;

private:
    friend class DependencyObject;
    friend class BindingEngine;

    bool CheckFreezeCore() noexcept;
    DependencyObject* Parent() const noexcept;
    Base::Result<void> AttachConsumer(DependencyObject& object, Meta::DependencyPropertyHandle property) noexcept;
    void DetachConsumer(DependencyObject& object, Meta::DependencyPropertyHandle property) noexcept;
    static Base::Result<void> CheckFreezeNode(FreezeCheckContext& context, Freezable& value) noexcept;
    static Base::Result<void> CheckFreezeChild(void* context, Freezable& child) noexcept;
    bool EnsureState() noexcept;

    State* state_ = nullptr;
};

} // namespace Aero
