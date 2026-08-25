#pragma once

// Dependency-property evaluation, ambient services and provider sessions.

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Diagnostics/PropertyValueSource.hpp>
#include <Aero/Threading.hpp>

#include <cstdint>

namespace Aero::Meta { class Registry; class Registration; }

namespace Aero::Meta {

using ::Aero::Threading::Dispatcher;
using ::Aero::Threading::DispatcherFrameHookHandle;
using ::Aero::Threading::DispatcherThreadToken;
using ::Aero::Threading::CurrentDispatcherThreadToken;

using EffectiveValueDiagnostics = PropertyValueSourceInfo;

class EffectiveValueEngine {
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
        return pending_.Size();
    }
    std::uint32_t PendingPropertyCount() const noexcept;
    bool IsFlushing() const noexcept {
        return flushing_;
    }

private:
    struct Pending {
        DependencyObject* object = nullptr;
        DependencyPropertyHandle property;
        std::uint64_t queueSequence = 0U;
    };

    Dispatcher* dispatcher_ = nullptr;
    DependencyPropertyRegistry* registry_ = nullptr;
    Base::Vector<Pending> pending_;
    Base::HashMap<DependencyObject*, DependencyObject*> parents_;
    Base::Vector<DependencyObject*> inheritanceSubscriptions_;
    DependencyPropertyChangedEventHandler
        inheritanceChangedHandler_;
    DispatcherFrameHookHandle phaseHook_;
    PropertyProviderOriginAllocator providerOrigins_;
    std::uint64_t nextQueueSequence_ = 1U;
    bool flushing_ = false;

    Base::Result<void> VerifyMutable() const noexcept;
    Base::Result<void> QueueObjectProperty(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept;
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
        Pending& entry) noexcept;

    static bool IsMutableBaseRank(PropertyValueRank rank) noexcept;
    static void PropertyChangesHook(void* context) noexcept;
};

} // namespace Aero::Meta


namespace Aero::Meta {


struct ObjectFactoryState {
    Dispatcher* dispatcher = nullptr;
    DependencyPropertyRegistry* dependencyProperties = nullptr;
    Meta::Registry* metadata = nullptr;

    bool IsValid() const noexcept {
        return dispatcher != nullptr && dependencyProperties != nullptr;
    }
};

ObjectFactoryState CurrentObjectFactory() noexcept;
bool HasObjectFactory() noexcept;

Base::Result<Value> TryEncodeValue(
    TypeId type,
    const void* source) noexcept;

class ObjectFactoryScope {
public:
    ObjectFactoryScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties) noexcept;
    ObjectFactoryScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties,
        Meta::Registry& runtime) noexcept;
    ObjectFactoryScope(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& properties,
        Meta::Registry* runtime) noexcept;
    ~ObjectFactoryScope();

    ObjectFactoryScope(const ObjectFactoryScope&) = delete;
    ObjectFactoryScope& operator=(const ObjectFactoryScope&) = delete;
    ObjectFactoryScope(ObjectFactoryScope&&) = delete;
    ObjectFactoryScope& operator=(ObjectFactoryScope&&) = delete;

private:
    ObjectFactoryState state_;
    ObjectFactoryState* previous_ = nullptr;
    DispatcherThreadToken ownerThread_ = 0U;
};

} // namespace Aero::Meta


#include <utility>

namespace Aero {

using namespace ::Aero::Meta;

// Canonical expression-to-DP boundary. Binding and MultiBinding use this
// after their explicit converters; other expression runtimes can share it
// without depending on Data. Type-specific conversions belong to metadata
// codecs or an authored converter; this helper only normalizes text,
// null-object typing and object covariance.
Base::Result<PropertyValue> NormalizeValueForProperty(
    Meta::Registry* metadata,
    const DependencyProperty& property,
    PropertyValue value) noexcept;

// Manager-owned provider state. One session belongs to one StyleEngine,
// StyleEngine or TemplateEngine allocates all provider origins through the
// shared EffectiveValueEngine, preventing cross-manager token collisions.
class PropertyProviderSession {
public:
    PropertyProviderSession(
        EffectiveValueEngine& engine,
        PropertyValueRank setterRank,
        PropertyValueRank triggerRank) noexcept
        : engine_(&engine),
          setterRank_(setterRank),
          triggerRank_(triggerRank) {}

    PropertyProviderSession(const PropertyProviderSession&) = delete;
    PropertyProviderSession& operator=(const PropertyProviderSession&) = delete;

    Base::Result<void> SetSetterValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept {
        ContributionRecord* existing = FindRecord(
            setterRecords_, object, property);
        if (existing != nullptr) {
            return engine_->SetProviderContribution(
                object, property, existing->token, value);
        }

        Base::Result<ObjectState*> stateResult = EnsureState(object);
        if (!stateResult) return stateResult.GetStatus();
        ObjectState& state = *stateResult.Value();
        Base::Result<std::uint32_t> origin = EnsureOrigin(
            state.setterOrigin);
        if (!origin) return origin.GetStatus();
        Base::Result<std::uint32_t> ordinal = NextOrdinal(
            state.nextSetterOrdinal,
            "Property setter ordinal limit reached");
        if (!ordinal) return ordinal.GetStatus();

        const PropertyProviderToken token{
            setterRank_, origin.Value(), ordinal.Value()};
        Base::Result<void> applied = engine_->SetProviderContribution(
            object, property, token, value);
        if (!applied) return applied.GetStatus();

        ContributionRecord record;
        record.object = &object;
        record.property = property;
        record.token = token;
        Base::Result<void> retained = setterRecords_.PushBack(
            std::move(record));
        if (!retained) {
            static_cast<void>(engine_->ClearProviderContribution(
                object, property, token));
            return retained.GetStatus();
        }
        return {};
    }

    Base::Result<void> ClearSetterValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        Base::Result<std::uint32_t> cleared = ClearRecords(
            setterRecords_, object, property);
        if (!cleared) return cleared.GetStatus();
        PruneState(object);
        return {};
    }

    Base::Result<void> SetTriggerValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept {
        Base::Result<ObjectState*> stateResult = EnsureState(object);
        if (!stateResult) return stateResult.GetStatus();
        ObjectState& state = *stateResult.Value();
        Base::Result<std::uint32_t> origin = EnsureOrigin(
            state.triggerOrigin);
        if (!origin) return origin.GetStatus();
        Base::Result<std::uint32_t> ordinal = NextOrdinal(
            state.nextTriggerOrdinal,
            "Property trigger ordinal limit reached");
        if (!ordinal) return ordinal.GetStatus();

        const PropertyProviderToken token{
            triggerRank_, origin.Value(), ordinal.Value()};
        Base::Result<void> applied = engine_->SetProviderContribution(
            object, property, token, value);
        if (!applied) return applied.GetStatus();

        ContributionRecord record;
        record.object = &object;
        record.property = property;
        record.token = token;
        Base::Result<void> retained = triggerRecords_.PushBack(
            std::move(record));
        if (!retained) {
            static_cast<void>(engine_->ClearProviderContribution(
                object, property, token));
            return retained.GetStatus();
        }
        return {};
    }

    Base::Result<void> ClearTriggerValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        Base::Result<std::uint32_t> cleared = ClearRecords(
            triggerRecords_, object, property);
        if (!cleared) return cleared.GetStatus();
        PruneState(object);
        return {};
    }

    Base::Result<void> ClearObjectProviders(
        DependencyObject& object) noexcept {
        Base::Result<std::uint32_t> setters = ClearObjectRecords(
            setterRecords_, object);
        if (!setters) return setters.GetStatus();
        Base::Result<std::uint32_t> triggers = ClearObjectRecords(
            triggerRecords_, object);
        if (!triggers) return triggers.GetStatus();
        RemoveState(object);
        return {};
    }

    Base::Result<void> DetachObject(
        DependencyObject& object) noexcept {
        Base::Result<void> cleared = ClearObjectProviders(object);
        if (!cleared) return cleared.GetStatus();
        return engine_->DetachObject(object);
    }

    Base::Result<std::uint32_t> Flush() noexcept {
        return engine_->Flush();
    }

    bool IsFlushing() const noexcept {
        return engine_->IsFlushing();
    }

private:
    struct ContributionRecord {
        DependencyObject* object = nullptr;
        DependencyPropertyHandle property;
        PropertyProviderToken token;
    };

    struct ObjectState {
        DependencyObject* object = nullptr;
        std::uint32_t setterOrigin = 0U;
        std::uint32_t triggerOrigin = 0U;
        std::uint32_t nextSetterOrdinal = 0U;
        std::uint32_t nextTriggerOrdinal = 0U;
    };

    EffectiveValueEngine* engine_ = nullptr;
    PropertyValueRank setterRank_ = PropertyValueRank::Default;
    PropertyValueRank triggerRank_ = PropertyValueRank::Default;
    Base::Vector<ContributionRecord> setterRecords_;
    Base::Vector<ContributionRecord> triggerRecords_;
    Base::Vector<ObjectState> states_;

    Base::Result<ObjectState*> EnsureState(
        DependencyObject& object) noexcept {
        for (ObjectState& state : states_) {
            if (state.object == &object) return &state;
        }
        ObjectState state;
        state.object = &object;
        Base::Result<void> retained = states_.PushBack(
            std::move(state));
        if (!retained) return retained.GetStatus();
        return &states_[states_.Size() - 1U];
    }

    Base::Result<std::uint32_t> EnsureOrigin(
        std::uint32_t& origin) noexcept {
        if (origin != 0U) return origin;
        Base::Result<std::uint32_t> allocated =
            engine_->AllocateProviderOrigin();
        if (!allocated) return allocated.GetStatus();
        origin = allocated.Value();
        return origin;
    }

    static Base::Result<std::uint32_t> NextOrdinal(
        std::uint32_t& next,
        const char* message) noexcept {
        if (next == UINT32_MAX) {
            return Base::Status::Failure(
                Base::ErrorCode::OutOfRange,
                message);
        }
        return next++;
    }

    static ContributionRecord* FindRecord(
        Base::Vector<ContributionRecord>& records,
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        for (ContributionRecord& record : records) {
            if (record.object == &object && record.property == property) {
                return &record;
            }
        }
        return nullptr;
    }

    Base::Result<std::uint32_t> ClearRecords(
        Base::Vector<ContributionRecord>& records,
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        std::uint32_t removed = 0U;
        std::uint32_t index = 0U;
        while (index < records.Size()) {
            ContributionRecord& record = records[index];
            if (record.object != &object || record.property != property) {
                ++index;
                continue;
            }
            Base::Result<bool> cleared =
                engine_->ClearProviderContribution(
                    object, property, record.token);
            if (!cleared) return cleared.GetStatus();
            RemoveAt(records, index);
            ++removed;
        }
        return removed;
    }

    Base::Result<std::uint32_t> ClearObjectRecords(
        Base::Vector<ContributionRecord>& records,
        DependencyObject& object) noexcept {
        std::uint32_t removed = 0U;
        std::uint32_t index = 0U;
        while (index < records.Size()) {
            ContributionRecord& record = records[index];
            if (record.object != &object) {
                ++index;
                continue;
            }
            Base::Result<bool> cleared =
                engine_->ClearProviderContribution(
                    object, record.property, record.token);
            if (!cleared) return cleared.GetStatus();
            RemoveAt(records, index);
            ++removed;
        }
        return removed;
    }

    bool HasRecords(const DependencyObject& object) const noexcept {
        for (const ContributionRecord& record : setterRecords_) {
            if (record.object == &object) return true;
        }
        for (const ContributionRecord& record : triggerRecords_) {
            if (record.object == &object) return true;
        }
        return false;
    }

    void PruneState(DependencyObject& object) noexcept {
        if (!HasRecords(object)) RemoveState(object);
    }

    void RemoveState(DependencyObject& object) noexcept {
        for (std::uint32_t index = 0U; index < states_.Size(); ++index) {
            if (states_[index].object != &object) continue;
            RemoveAt(states_, index);
            return;
        }
    }

    template<class T>
    static void RemoveAt(
        Base::Vector<T>& values,
        std::uint32_t index) noexcept {
        for (std::uint32_t next = index + 1U;
             next < values.Size();
             ++next) {
            values[next - 1U] = std::move(values[next]);
        }
        values.PopBack();
    }
};

class StyleProviderSession {
public:
    explicit StyleProviderSession(
        EffectiveValueEngine& engine) noexcept
        : session_(
              engine,
              PropertyValueRank::StyleSetter,
              PropertyValueRank::StyleTrigger) {}

    Base::Result<void> SetStyleValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept {
        return session_.SetSetterValue(object, property, value);
    }
    Base::Result<void> ClearStyleValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return session_.ClearSetterValue(object, property);
    }
    Base::Result<void> SetTriggerValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept {
        return session_.SetTriggerValue(object, property, value);
    }
    Base::Result<void> ClearTriggerValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return session_.ClearTriggerValue(object, property);
    }
    Base::Result<std::uint32_t> Flush() noexcept {
        return session_.Flush();
    }
    bool IsFlushing() const noexcept {
        return session_.IsFlushing();
    }

private:
    PropertyProviderSession session_;
};

class TemplatedParentProviderSession {
public:
    explicit TemplatedParentProviderSession(
        EffectiveValueEngine& engine) noexcept
        : session_(
              engine,
              PropertyValueRank::TemplatedParentSetter,
              PropertyValueRank::TemplatedParentTrigger) {}

    Base::Result<void> SetTemplateValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept {
        return session_.SetSetterValue(object, property, value);
    }
    Base::Result<void> ClearTemplateValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return session_.ClearSetterValue(object, property);
    }
    Base::Result<void> SetTriggerValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept {
        return session_.SetTriggerValue(object, property, value);
    }
    Base::Result<void> ClearTriggerValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return session_.ClearTriggerValue(object, property);
    }
    Base::Result<void> DetachObject(
        DependencyObject& object) noexcept {
        return session_.DetachObject(object);
    }

private:
    PropertyProviderSession session_;
};

} // namespace Aero

