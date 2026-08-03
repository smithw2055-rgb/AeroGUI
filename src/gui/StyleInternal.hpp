#pragma once

// Style authoring bridge, compiled state and theme-style lookup.

#include <Aero/Base/Object.hpp>
#include <Aero/Value.hpp>
#include <Aero/DependencyProperty.hpp>

#include <cstdint>

namespace Aero {

// Private compatibility owners used by the built-in theme schema. They are
// registered for XAML compatibility but are not C++ authoring APIs.
class Element : public Base::Object {
    AERO_DECLARE_TYPE(Element, Base::Object)
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    inline static constexpr Meta::AttachedPropertyRef<Element, double>
        PPAAOutProperty{"PPAAOut"};
    inline static constexpr Meta::AttachedPropertyRef<Element, bool>
        IsFocusEngagedProperty{"IsFocusEngaged"};
};

class TextProperties : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        TextProperties, Base::Object, "urn:aero", "Text")
public:
    Meta::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    inline static constexpr Meta::AttachedPropertyRef<
        TextProperties, std::uint32_t>
        PasswordLengthProperty{"PasswordLength"};
};

} // namespace Aero

#include <Aero/Styling.hpp>
#include <Aero/Style.hpp>

namespace Aero::Internal {
using StylePrivate = ::Aero::Style::Impl;
}

#include "gui/ElementInternal.hpp"
#include "gui/PropertyInternal.hpp"


namespace Aero::Internal {

using namespace Aero::Meta;
using namespace Aero::Threading;

class AERO_API StyleEngine {
public:
    using TriggerActionHandler = Base::Result<void>(*)(
        DependencyObject& owner,
        Base::Span<const Base::Ref<Base::Object>> actions,
        void* context) noexcept;

    explicit StyleEngine(
        EffectiveValueEngine& values,
        DependencyPropertyRegistry& properties) noexcept
        : providerSession_(values),
          values_(&providerSession_),
          properties_(&properties),
          applications_(),
          propertyChangedHandler_(
              this, &StyleEngine::OnPropertyChanged) {}
    ~StyleEngine() noexcept;

    Base::Result<void> Apply(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> Clear(
        DependencyObject& object,
        const Style& style) noexcept;
    // Tree/object ownership code calls this before destroying an object.
    Base::Result<bool> DetachObject(
        DependencyObject& object) noexcept;
    const Style* AppliedStyle(
        const DependencyObject& object)
        const noexcept;
    void SetTriggerActionHandler(
        TriggerActionHandler handler,
        void* context) noexcept {
        triggerActionHandler_ = handler;
        triggerActionContext_ = context;
    }
    const Base::Status& LastActionStatus() const noexcept {
        return lastActionStatus_;
    }

private:
    Internal::StyleProviderSession providerSession_;
    Internal::StyleProviderSession* values_ = nullptr;
    DependencyPropertyRegistry* properties_ = nullptr;
    struct Application {
        DependencyObject* object = nullptr;
        const Style* style = nullptr;
        Base::Vector<std::uint8_t> triggerStates;
    };
    Base::Vector<Application> applications_;
    DependencyPropertyChangedEventHandler propertyChangedHandler_;
    Dispatcher* dispatcher_ = nullptr;
    DispatcherFrameHookHandle triggerPhaseHook_;
    Base::Vector<DependencyObject*>
        pendingTriggerEvaluations_;
    TriggerActionHandler triggerActionHandler_ = nullptr;
    void* triggerActionContext_ = nullptr;
    Base::Status lastActionStatus_;

    Base::Result<void> VerifyTarget(
        const DependencyObject& object,
        const Style& style) const noexcept;
    std::uint32_t FindApplication(
        const DependencyObject& object) const noexcept;
    Base::Result<void> ClearSetters(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> SubscribeTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
    void UnsubscribeTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> EvaluateTriggers(
        DependencyObject& object,
        const Style& style) noexcept;
    Base::Result<void> ExecuteTriggerActions(
        DependencyObject& object,
        Base::Span<const Base::Ref<Base::Object>>
            actions) noexcept;
    Base::Result<void> EnsureTriggerPhaseHook(
        DependencyObject& object) noexcept;
    Base::Result<void> QueueTriggerEvaluation(
        DependencyObject& object) noexcept;
    void RemovePendingTriggerEvaluation(
        DependencyObject& object) noexcept;
    Base::Result<std::uint32_t>
        FlushPendingTriggerEvaluations() noexcept;
    static void TriggerPhaseHook(void* context) noexcept;
    Base::Result<void> ClearTriggerSetters(
        DependencyObject& object,
        const Style& style) noexcept;
    void OnPropertyChanged(
        DependencyObject& object,
        const DependencyPropertyChangedEventArgs& args) noexcept;
};


} // namespace Aero::Internal

// Resource-assignment helpers used by style and markup application.

#include <Aero/Resources.hpp>

#include <utility>

namespace Aero::Internal {

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

} // namespace Aero::Internal
