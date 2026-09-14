#pragma once

#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/triggers/TriggerPlan.hpp"
#include "gui/triggers/TriggerTypes.hpp"
#include <Aero/Base/HashMap.hpp>
#include <Aero/Base/Allocator.hpp>
#include <Aero/Controls/ControlTemplate.hpp>
#include <Aero/Data/BindingExpression.hpp>
#include <Aero/Resources.hpp>
#include <Aero/Style.hpp>
#include <Aero/TextProperties.hpp>
#include <Aero/Value.hpp>

#include <cstdint>
#include <new>
#include <utility>

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;

struct StyleSetter {
    DependencyPropertyHandle property;
    PropertyValue value;
};

// Resource-assignment helpers used by style and markup application.
inline ResourceDictionary& EnsureOwnedResources(
    ResourceDictionary*& slot) noexcept {
    if (slot != nullptr) {
        return *slot;
    }
    slot = new (std::nothrow) ResourceDictionary();
    if (slot == nullptr) {
        Base::ReportOutOfMemory(
            sizeof(ResourceDictionary),
            alignof(ResourceDictionary),
            Base::MemoryTag::Object);
        static ResourceDictionary fallback;
        return fallback;
    }
    return *slot;
}

inline Base::Result<void> AssignResourceDictionary(
    ResourceDictionary& target,
    Base::Ref<ResourceDictionary> source,
    const char* alreadyAssignedMessage) noexcept {
    if (!source) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Resources expects a non-null ResourceDictionary");
    }
    if (target.Size() != 0U ||
        target.MergedDictionaryCount() != 0U ||
        !target.GetSource().Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::AlreadyExists,
            alreadyAssignedMessage);
    }
    target = std::move(*source);
    return {};
}

// Style program accessors (formerly StyleState statics).
Base::Result<void> SealStyle(
    Style& style,
    const Meta::DependencyPropertyRegistry& properties) noexcept;
Base::Span<const StyleSetter> StyleRuntimeSetters(
    const Style& style) noexcept;
Base::Span<const TriggerPlan> StyleRuntimeTriggers(
    const Style& style) noexcept;
Base::Result<void> ApplyStyleSetters(
    const Style& style,
    DependencyObject& object,
    StyleProviderSession& values) noexcept;
Base::Result<void> ClearStyleSetters(
    const Style& style,
    DependencyObject& object,
    StyleProviderSession& values) noexcept;


class TriggerEngine;

class StyleEngine {
public:
    using TriggerActionHandler = ::Aero::TriggerActionHandler;

    explicit StyleEngine(
        EffectiveValueEngine& values,
        DependencyPropertyRegistry& properties) noexcept;
    ~StyleEngine() noexcept;

    Base::Result<void> Apply(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> Clear(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> SetBindingTriggerState(
        DependencyObject& object,
        const Style& style,
        std::uint32_t triggerIndex,
        bool active) noexcept;
    // Tree/object ownership code calls this before destroying an object.
    Base::Result<bool> DetachObject(
        DependencyObject& object) noexcept;
    const Style* AppliedStyle(
        const DependencyObject& object)
        const noexcept;
    void SetTriggerActionHandler(
        TriggerActionHandler handler,
        void* context) noexcept;
    const Base::Status& LastActionStatus() const noexcept;
    // P3.2: ViewFrame drives the owned TriggerEngine's DataBind phase
    // directly. May be null when the engine failed to allocate it.
    TriggerEngine* Triggers() noexcept {
        return triggerEngine_;
    }
    // Thin DataBind pump: flushes deferred property-trigger re-evals queued
    // while EffectiveValueEngine was flushing.
    Base::Result<std::uint32_t> Flush() noexcept;

private:
    ::Aero::StyleProviderSession providerSession_;
    ::Aero::StyleProviderSession* values_ = nullptr;
    DependencyPropertyRegistry* properties_ = nullptr;
    Base::Vector<StyleApplication> applications_;
    Base::HashMap<const DependencyObject*, std::uint32_t> objectIndexMap_;
    Base::Result<void> VerifyTarget(
        const DependencyObject& object,
        const Style& style) const noexcept;
    std::uint32_t FindApplication(
        const DependencyObject& object) const noexcept;
    Base::Result<void> ClearSetters(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> AttachSetterBindings(
        DependencyObject& object,
        const Style& style) noexcept;
    void DetachSetterBindings(DependencyObject& object) noexcept;

    struct SetterBinding {
        DependencyObject* object = nullptr;
        Data::BindingHandle handle;
    };
    Base::Vector<SetterBinding> setterBindings_;

    // Owned trigger-evaluation engine. StyleEngine delegates all trigger
    // state, subscription and deferred-evaluation behavior to it.
    TriggerEngine* triggerEngine_ = nullptr;
};

} // namespace Aero
