#pragma once

#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include "EffectiveValueEngine.hpp"

#include <cstdint>
#include <utility>

namespace Aero::Core::Detail {

// Manager-owned provider state. One session belongs to one StyleManager,
// ThemeStyleManager or TemplateManager and allocates all origins through the
// shared EffectiveValueEngine, preventing cross-manager token collisions.
class PropertyProviderSession final {
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
        Base::Result<void> retained = setterRecords_.TryPushBack(
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
        Base::Result<void> retained = triggerRecords_.TryPushBack(
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
    struct ContributionRecord final {
        DependencyObject* object = nullptr;
        DependencyPropertyHandle property;
        PropertyProviderToken token;
    };

    struct ObjectState final {
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
        Base::Result<void> retained = states_.TryPushBack(
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

class StyleProviderSession final {
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

class ThemeStyleProviderSession final {
public:
    explicit ThemeStyleProviderSession(
        EffectiveValueEngine& engine) noexcept
        : session_(
              engine,
              PropertyValueRank::ThemeStyleSetter,
              PropertyValueRank::ThemeStyleTrigger) {}

    Base::Result<void> SetThemeStyleValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept {
        return session_.SetSetterValue(object, property, value);
    }
    Base::Result<void> ClearThemeStyleValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return session_.ClearSetterValue(object, property);
    }
    Base::Result<void> SetThemeTriggerValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept {
        return session_.SetTriggerValue(object, property, value);
    }
    Base::Result<void> ClearThemeTriggerValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return session_.ClearTriggerValue(object, property);
    }

private:
    PropertyProviderSession session_;
};

class TemplatedParentProviderSession final {
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

} // namespace Aero::Core::Detail
