#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Core/Property/PropertyValueSource.hpp>
#include <Aero/Core/Dispatcher.hpp>

#include <cstdint>

namespace Aero::Core {

using EffectiveValueDiagnostics = PropertyValueSourceInfo;

class AERO_API EffectiveValueEngine final {
public:
    EffectiveValueEngine(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& registry) noexcept;
    ~EffectiveValueEngine() noexcept;

    EffectiveValueEngine(const EffectiveValueEngine&) = delete;
    EffectiveValueEngine& operator=(const EffectiveValueEngine&) = delete;
    EffectiveValueEngine(EffectiveValueEngine&&) = delete;
    EffectiveValueEngine& operator=(EffectiveValueEngine&&) = delete;

    Base::Result<void> Initialize() noexcept;
    bool IsInitialized() const noexcept {
        return phaseHook_.IsValid();
    }

    Base::Result<void> SetInheritanceParent(
        DependencyObject& child,
        DependencyObject* parent) noexcept;
    DependencyObject* InheritanceParent(
        const DependencyObject& child) const noexcept;

    // Allocates a process-local provider origin unique to this value engine.
    // Provider sessions allocate lazily on the owning Dispatcher and retain the
    // origin for the lifetime of their Style, Theme or Template application.
    Base::Result<std::uint32_t> AllocateProviderOrigin() noexcept {
        Base::Result<void> access = dispatcher_->VerifyAccess();
        if (!access) return access.GetStatus();
        return providerOrigins_.Allocate();
    }

    // Canonical contribution API. Style, Template, Theme and Trigger runtimes
    // allocate a stable origin and use declaration ordinal within that origin.
    Base::Result<void> SetProviderContribution(
        DependencyObject& object,
        DependencyPropertyHandle property,
        PropertyProviderToken token,
        const PropertyValue& value) noexcept;
    Base::Result<bool> ClearProviderContribution(
        DependencyObject& object,
        DependencyPropertyHandle property,
        PropertyProviderToken token) noexcept;
    Base::Result<std::uint32_t> ClearProviderOrigin(
        DependencyObject& object,
        std::uint32_t origin) noexcept;

    // Ownership of expression.context transfers only after this call succeeds.
    Base::Result<void> SetLocalExpression(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyExpression& expression) noexcept;
    Base::Result<void> ClearLocalExpression(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept;

    Base::Result<void> SetAnimationValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    Base::Result<void> ClearAnimationValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept;

    Base::Result<void> Invalidate(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept;

    // Processes only the queue snapshot that existed at entry. Changes queued by
    // inheritance propagation are deferred to the next PropertyChanges phase.
    Base::Result<std::uint32_t> Flush() noexcept;

    Base::Result<EffectiveValueDiagnostics> Diagnostics(
        const DependencyObject& object,
        DependencyPropertyHandle property) const noexcept;

    // Tracked objects are non-owning. Hosts must detach an object before its
    // destruction unless the engine itself is destroyed first.
    Base::Result<void> DetachObject(
        DependencyObject& object) noexcept;

    std::uint32_t TrackedPropertyCount() const noexcept {
        return entries_.Size();
    }
    std::uint32_t PendingPropertyCount() const noexcept;
    bool IsFlushing() const noexcept {
        return flushing_;
    }

private:
    struct Entry final {
        DependencyObject* object = nullptr;
        DependencyPropertyHandle property;
        std::uint64_t queueSequence = 0U;
        bool queued = false;
    };

    struct ParentLink final {
        DependencyObject* child = nullptr;
        DependencyObject* parent = nullptr;
    };

    Dispatcher* dispatcher_ = nullptr;
    DependencyPropertyRegistry* registry_ = nullptr;
    Base::Vector<Entry> entries_;
    Base::Vector<ParentLink> parents_;
    Base::Vector<DependencyObject*> inheritanceSubscriptions_;
    DependencyPropertyChangedEventHandler
        inheritanceChangedHandler_;
    DispatcherFrameHookHandle phaseHook_;
    PropertyProviderOriginAllocator providerOrigins_;
    std::uint64_t nextQueueSequence_ = 1U;
    bool flushing_ = false;

    Base::Result<void> VerifyMutable() const noexcept;
    std::uint32_t FindEntryIndex(
        const DependencyObject& object,
        DependencyPropertyHandle property) const noexcept;
    Base::Result<std::uint32_t> EnsureEntry(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept;
    std::uint32_t FindParentIndex(
        const DependencyObject& child) const noexcept;

    Base::Result<void> QueueEntry(
        std::uint32_t index) noexcept;
    Base::Result<void> QueueDescendants(
        DependencyObject& parent,
        DependencyPropertyHandle property) noexcept;
    Base::Result<void> EnsureInheritanceSubscription(
        DependencyObject& object) noexcept;
    void RemoveInheritanceSubscription(
        DependencyObject& object) noexcept;
    void OnInheritancePropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs&
            args) noexcept;
    Base::Result<void> Apply(
        Entry& entry) noexcept;

    void RemoveEntry(std::uint32_t index) noexcept;
    void RemoveParent(std::uint32_t index) noexcept;

    static bool IsMutableBaseRank(PropertyValueRank rank) noexcept;
    static void PropertyChangesHook(void* context) noexcept;
};

} // namespace Aero::Core
