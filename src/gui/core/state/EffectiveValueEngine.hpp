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
        return initialized_;
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

    void Shutdown() noexcept;

    std::uint32_t TrackedPropertyCount() const noexcept {
        return queueCount_;
    }
    std::uint32_t PendingPropertyCount() const noexcept;
    bool IsFlushing() const noexcept {
        return flushing_;
    }

private:
    // P2.2: recycled intrusive queue link. The old pending_ vector is gone;
    // links are popped from / returned to linkFree_, so steady-state
    // enqueue/dequeue performs zero allocations and can never reallocate.
    // NOTE: links are engine-owned rather than embedded in StoredValueEntry
    // because a queued entry may be erased synchronously (e.g. ClearValue
    // back to default recomputes inline and drops the entry via
    // RemoveStoredEntry); an entry-owned link would die with it and sever
    // the chain. Dedup still rides on the per-entry Queued() flag.
    struct QueueLink {
        DependencyObject* object = nullptr;
        DependencyPropertyHandle property;
        QueueLink* next = nullptr;
    };

    Dispatcher* dispatcher_ = nullptr;
    DependencyPropertyRegistry* registry_ = nullptr;
    QueueLink* queueHead_ = nullptr;
    QueueLink* queueTail_ = nullptr;
    QueueLink* linkFree_ = nullptr;
    std::uint32_t queueCount_ = 0U;
    Base::HashMap<DependencyObject*, DependencyObject*> parents_;
    Base::Vector<DependencyObject*> inheritanceSubscriptions_;
    DependencyPropertyChangedEventHandler
        inheritanceChangedHandler_;
    PropertyProviderOriginAllocator providerOrigins_;
    bool initialized_ = false;
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
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept;
    // Pops a link from the free list or allocates a fresh one. Only fails on
    // OOM when the free list is empty (steady state never allocates).
    Base::Result<QueueLink*> AcquireLink() noexcept;
    void RecycleLink(QueueLink* link) noexcept;
    void ClearQueueLinks() noexcept;

    static bool IsMutableBaseRank(PropertyValueRank rank) noexcept;
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
        // C1: O(1) indexed update. Setter semantics keep a single live
        // contribution per (object, property); repeat sets reuse the token.
        const ContributionKey key{&object, property};
        if (PropertyProviderToken* existing = setterRecords_.Find(key)) {
            return engine_->SetProviderContribution(
                object, property, *existing, value);
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

        Base::Result<Base::HashMap<ContributionKey, PropertyProviderToken,
            ContributionKeyHash>::InsertResult> retained =
            setterRecords_.Insert(key, token);
        if (!retained) {
            static_cast<void>(engine_->ClearProviderContribution(
                object, property, token));
            return retained.GetStatus();
        }
        ++state.liveContributions;
        return {};
    }

    Base::Result<void> ClearSetterValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        const ContributionKey key{&object, property};
        PropertyProviderToken* found = setterRecords_.Find(key);
        if (found == nullptr) {
            PruneState(object);
            return {};
        }
        const PropertyProviderToken token = *found;
        Base::Result<bool> cleared =
            engine_->ClearProviderContribution(object, property, token);
        if (!cleared) return cleared.GetStatus();
        setterRecords_.Erase(key);
        if (ObjectState* state = states_.Find(&object)) {
            if (state->liveContributions != 0U) --state->liveContributions;
        }
        PruneState(object);
        return {};
    }

    Base::Result<void> SetTriggerValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept {
        // C1: trigger semantics stack: several simultaneously active triggers
        // may contribute to the same (object, property) with distinct
        // ordinals (later declaration wins via IsStronger). The per-pair
        // token vector is usually length 1.
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

        const ContributionKey key{&object, property};
        Base::Vector<PropertyProviderToken>* stored =
            triggerRecords_.Find(key);
        bool fresh = false;
        if (stored == nullptr) {
            Base::Result<Base::HashMap<ContributionKey,
                Base::Vector<PropertyProviderToken>,
                ContributionKeyHash>::InsertResult> inserted =
                triggerRecords_.Insert(
                    key, Base::Vector<PropertyProviderToken>{});
            if (!inserted) {
                static_cast<void>(engine_->ClearProviderContribution(
                    object, property, token));
                return inserted.GetStatus();
            }
            stored = &inserted.Value().entry->Value();
            fresh = true;
        }
        Base::Result<void> retained = stored->PushBack(token);
        if (!retained) {
            static_cast<void>(engine_->ClearProviderContribution(
                object, property, token));
            if (fresh) triggerRecords_.Erase(key);
            return retained.GetStatus();
        }
        ++state.liveContributions;
        return {};
    }

    Base::Result<void> ClearTriggerValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        const ContributionKey key{&object, property};
        Base::Vector<PropertyProviderToken>* stored =
            triggerRecords_.Find(key);
        if (stored == nullptr) {
            PruneState(object);
            return {};
        }
        for (const PropertyProviderToken& token : *stored) {
            Base::Result<bool> cleared =
                engine_->ClearProviderContribution(object, property, token);
            if (!cleared) return cleared.GetStatus();
        }
        if (ObjectState* state = states_.Find(&object)) {
            const std::uint32_t count = stored->Size();
            state->liveContributions = (state->liveContributions >= count)
                ? state->liveContributions - count
                : 0U;
        }
        triggerRecords_.Erase(key);
        PruneState(object);
        return {};
    }

    Base::Result<void> ClearObjectProviders(
        DependencyObject& object) noexcept {
        // C1: single indexed sweep per table. Erase-during-iteration is safe:
        // HashMap::Erase marks a tombstone without rehashing, and the
        // index-based iterator skips non-occupied buckets.
        ObjectState* state = states_.Find(&object);
        for (auto it = setterRecords_.begin();
             it != setterRecords_.end();) {
            if (it->Key().object != &object) {
                ++it;
                continue;
            }
            const ContributionKey key = it->Key();
            const PropertyProviderToken token = it->Value();
            ++it;
            Base::Result<bool> cleared =
                engine_->ClearProviderContribution(
                    object, key.property, token);
            if (!cleared) return cleared.GetStatus();
            setterRecords_.Erase(key);
            if (state != nullptr && state->liveContributions != 0U) {
                --state->liveContributions;
            }
        }
        for (auto it = triggerRecords_.begin();
             it != triggerRecords_.end();) {
            if (it->Key().object != &object) {
                ++it;
                continue;
            }
            const ContributionKey key = it->Key();
            ++it;
            Base::Vector<PropertyProviderToken>* stored =
                triggerRecords_.Find(key);
            if (stored != nullptr) {
                for (const PropertyProviderToken& token : *stored) {
                    Base::Result<bool> cleared =
                        engine_->ClearProviderContribution(
                            object, key.property, token);
                    if (!cleared) return cleared.GetStatus();
                }
                if (state != nullptr) {
                    const std::uint32_t count = stored->Size();
                    state->liveContributions =
                        (state->liveContributions >= count)
                            ? state->liveContributions - count
                            : 0U;
                }
            }
            triggerRecords_.Erase(key);
        }
        states_.Erase(&object);
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
    // C1: O(1) contribution index. Setter side keeps one live token per
    // (object, property); trigger side stacks one token vector per pair.
    struct ContributionKey {
        DependencyObject* object = nullptr;
        DependencyPropertyHandle property;

        bool operator==(const ContributionKey& other) const noexcept {
            return object == other.object && property == other.property;
        }
    };

    struct ContributionKeyHash {
        Base::HashCode operator()(
            const ContributionKey& key,
            Base::HashCode seed = 0U) const noexcept {
            const Base::HashCode objectHash =
                Base::DefaultHash<DependencyObject*>{}(
                    key.object, seed);
            return Base::MixHash64(
                objectHash ^
                static_cast<Base::HashCode>(key.property.value));
        }
    };

    struct ObjectState {
        std::uint32_t setterOrigin = 0U;
        std::uint32_t triggerOrigin = 0U;
        std::uint32_t nextSetterOrdinal = 0U;
        std::uint32_t nextTriggerOrdinal = 0U;
        std::uint32_t liveContributions = 0U;
    };

    EffectiveValueEngine* engine_ = nullptr;
    PropertyValueRank setterRank_ = PropertyValueRank::Default;
    PropertyValueRank triggerRank_ = PropertyValueRank::Default;
    Base::HashMap<ContributionKey, PropertyProviderToken,
        ContributionKeyHash> setterRecords_;
    Base::HashMap<ContributionKey, Base::Vector<PropertyProviderToken>,
        ContributionKeyHash> triggerRecords_;
    Base::HashMap<DependencyObject*, ObjectState> states_;

    Base::Result<ObjectState*> EnsureState(
        DependencyObject& object) noexcept {
        if (ObjectState* existing = states_.Find(&object)) {
            return existing;
        }
        Base::Result<Base::HashMap<DependencyObject*, ObjectState>::
            InsertResult> inserted = states_.Insert(
                &object, ObjectState{});
        if (!inserted) return inserted.GetStatus();
        return &inserted.Value().entry->Value();
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

    void PruneState(DependencyObject& object) noexcept {
        // C1: liveness is tracked inline; no record scan needed.
        ObjectState* state = states_.Find(&object);
        if (state != nullptr && state->liveContributions == 0U) {
            states_.Erase(&object);
        }
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
    Base::Result<std::uint32_t> Flush() noexcept {
        return session_.Flush();
    }

private:
    PropertyProviderSession session_;
};

} // namespace Aero

