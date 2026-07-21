#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/Dispatcher.hpp>

#include <cstdint>

namespace Aero::Core {

enum class EffectiveValueProvider : std::uint8_t {
    Default = 0U,
    Inherited,
    Style,
    Template,
    Local,
    LocalExpression,
    Animation
};

enum class PropertyExpressionKind : std::uint8_t {
    Custom = 0U,
    Binding,
    DynamicResource
};

using PropertyExpressionEvaluateCallback = Base::Result<PropertyValue> (*)(
    void* context,
    DependencyObject& object,
    DependencyPropertyHandle property) noexcept;
using PropertyExpressionCleanupCallback = void (*)(void* context) noexcept;

struct PropertyExpression final {
    void* context = nullptr;
    PropertyExpressionEvaluateCallback evaluate = nullptr;
    PropertyExpressionCleanupCallback cleanup = nullptr;
    PropertyExpressionKind kind = PropertyExpressionKind::Custom;

    AERO_NODISCARD bool IsValid() const noexcept {
        return evaluate != nullptr;
    }
};

struct EffectiveValueDiagnostics final {
    EffectiveValueProvider provider = EffectiveValueProvider::Default;
    PropertyExpressionKind expressionKind = PropertyExpressionKind::Custom;
    bool hasExpression = false;
    bool isInherited = false;
    bool isAnimated = false;
    std::uint64_t revision = 0U;
};

class AERO_API EffectiveValueEngine final {
public:
    EffectiveValueEngine(
        Dispatcher& dispatcher,
        DependencyPropertyRegistry& registry,
        Base::IAllocator* allocator = nullptr) noexcept;
    ~EffectiveValueEngine() noexcept;

    EffectiveValueEngine(const EffectiveValueEngine&) = delete;
    EffectiveValueEngine& operator=(const EffectiveValueEngine&) = delete;
    EffectiveValueEngine(EffectiveValueEngine&&) = delete;
    EffectiveValueEngine& operator=(EffectiveValueEngine&&) = delete;

    AERO_NODISCARD Base::Result<void> Initialize() noexcept;
    AERO_NODISCARD bool IsInitialized() const noexcept {
        return phaseHook_.IsValid();
    }

    AERO_NODISCARD Base::Result<void> SetInheritanceParent(
        DependencyObject& child,
        DependencyObject* parent) noexcept;
    AERO_NODISCARD DependencyObject* InheritanceParent(
        const DependencyObject& child) const noexcept;

    AERO_NODISCARD Base::Result<void> SetStyleValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    AERO_NODISCARD Base::Result<void> ClearStyleValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept;

    AERO_NODISCARD Base::Result<void> SetTemplateValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    AERO_NODISCARD Base::Result<void> ClearTemplateValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept;

    // Ownership of expression.context transfers only after this call succeeds.
    AERO_NODISCARD Base::Result<void> SetLocalExpression(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyExpression& expression) noexcept;
    AERO_NODISCARD Base::Result<void> ClearLocalExpression(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept;

    AERO_NODISCARD Base::Result<void> SetAnimationValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    AERO_NODISCARD Base::Result<void> ClearAnimationValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept;

    AERO_NODISCARD Base::Result<void> Invalidate(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept;

    // Processes only the queue snapshot that existed at entry. Changes queued by
    // inheritance propagation are deferred to the next PropertyChanges phase.
    AERO_NODISCARD Base::Result<std::uint32_t> Flush() noexcept;

    AERO_NODISCARD Base::Result<EffectiveValueDiagnostics> Diagnostics(
        const DependencyObject& object,
        DependencyPropertyHandle property) const noexcept;

    // Tracked objects are non-owning. Hosts must detach an object before its
    // destruction unless the engine itself is destroyed first.
    AERO_NODISCARD Base::Result<void> DetachObject(
        DependencyObject& object) noexcept;

    AERO_NODISCARD std::uint32_t TrackedPropertyCount() const noexcept {
        return entries_.Size();
    }
    AERO_NODISCARD std::uint32_t PendingPropertyCount() const noexcept;

private:
    struct ProviderSlot final {
        PropertyValue value;
        bool hasValue = false;
    };

    struct ExpressionSlot final {
        PropertyExpression expression;
        bool hasExpression = false;
    };

    struct Entry final {
        DependencyObject* object = nullptr;
        DependencyPropertyHandle property;
        ProviderSlot style;
        ProviderSlot templated;
        ProviderSlot animation;
        ExpressionSlot localExpression;
        EffectiveValueDiagnostics diagnostics;
        std::uint64_t queueSequence = 0U;
        bool queued = false;
    };

    struct ParentLink final {
        DependencyObject* child = nullptr;
        DependencyObject* parent = nullptr;
    };

    struct Resolution final {
        PropertyValue value;
        EffectiveValueProvider provider = EffectiveValueProvider::Default;
        PropertyExpressionKind expressionKind =
            PropertyExpressionKind::Custom;
        bool hasExpression = false;
    };

    Dispatcher* dispatcher_ = nullptr;
    DependencyPropertyRegistry* registry_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<Entry> entries_;
    Base::Vector<ParentLink> parents_;
    DispatcherFrameHookHandle phaseHook_;
    std::uint64_t nextQueueSequence_ = 1U;
    std::uint64_t nextRevision_ = 1U;
    bool flushing_ = false;

    AERO_NODISCARD Base::Result<void> VerifyMutable() const noexcept;
    AERO_NODISCARD std::uint32_t FindEntryIndex(
        const DependencyObject& object,
        DependencyPropertyHandle property) const noexcept;
    AERO_NODISCARD Base::Result<std::uint32_t> EnsureEntry(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept;
    AERO_NODISCARD std::uint32_t FindParentIndex(
        const DependencyObject& child) const noexcept;

    AERO_NODISCARD Base::Result<void> SetProviderValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        EffectiveValueProvider provider,
        const PropertyValue& value) noexcept;
    AERO_NODISCARD Base::Result<void> ClearProviderValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        EffectiveValueProvider provider) noexcept;

    AERO_NODISCARD Base::Result<void> QueueEntry(
        std::uint32_t index) noexcept;
    AERO_NODISCARD Base::Result<void> QueueDescendants(
        DependencyObject& parent,
        DependencyPropertyHandle property) noexcept;
    AERO_NODISCARD Base::Result<Resolution> Resolve(
        Entry& entry) noexcept;
    AERO_NODISCARD Base::Result<void> Apply(
        Entry& entry,
        const Resolution& resolution) noexcept;

    void ReleaseExpression(ExpressionSlot& slot) noexcept;
    void RemoveEntry(std::uint32_t index) noexcept;
    void RemoveParent(std::uint32_t index) noexcept;

    static void PropertyChangesHook(void* context) noexcept;
};

} // namespace Aero::Core
