#pragma once

#include <Aero/Base/HashSet.hpp>
#include <Aero/DispatcherObject.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/TryCast.hpp>

namespace Aero {

namespace Meta {
class DependencyPropertyRegistry;
}
struct StoredValueEntry;
// Engine-owned state (definitions in src/gui/core/PropertyStore.hpp).
// Kept out of the public header for Noesis-parity slimness; ChangeKind stays
// here because include/Aero/Resources.hpp references it.
struct PropertyStore;
struct DependencyObjectRare;
class DependencyMutationScope;
class DependencyObjectAccess;
class Freezable;
class Animatable;
class BindingEngine;
class AnimationEngine;
class ElementTree;
class StyleEngine;
class TriggerEngine;
namespace Controls { class VisualStateManagerState; }
namespace Meta { class EffectiveValueEngine; }

class AERO_GUI_API DependencyObject : public DispatcherObject {
    AERO_DECLARE_TYPE(DependencyObject, DispatcherObject)

public:
    TypeId RuntimeType() const noexcept override { return runtimeType_; }
    PropertyValue GetValue(DependencyPropertyHandle property) const noexcept;
    StringView GetStringView(DependencyPropertyHandle property) const noexcept;
    template<class TOwner, class TValue> PropertyAccess<TValue> GetValue(
        const DependencyPropertyRef<TOwner, TValue>& property) const noexcept;
    template<class TOwner> StringView GetValue(const DependencyPropertyRef<TOwner, String>& property) const noexcept;
    template<class TOwner, class TValue> PropertyAccess<TValue> GetValue(
        const ReadOnlyPropertyRef<TOwner, TValue>& property) const noexcept;
    template<class TOwner> StringView GetValue(const ReadOnlyPropertyRef<TOwner, String>& property) const noexcept;
    PropertyValue ReadLocalValue(DependencyPropertyHandle property) const noexcept;
    EffectiveValueSource GetValueSource(DependencyPropertyHandle property) const noexcept;
    PropertyValueSourceInfo GetValueSourceInfo(DependencyPropertyHandle property) const noexcept;
    void SetValue(DependencyPropertyHandle property, const PropertyValue& value) noexcept;
    template<class TOwner, class TValue>
    void SetValue(const DependencyPropertyRef<TOwner, TValue>& property, PropertyAccess<TValue> value) noexcept;
    template<class TOwner>
    void SetValue(const DependencyPropertyRef<TOwner, String>& property, StringView value) noexcept;
    void SetValue(const DependencyPropertyKey& key, const PropertyValue& value) noexcept;
    void SetCurrentValue(DependencyPropertyHandle property, const PropertyValue& value) noexcept;
    template<class TOwner, class TValue>
    void SetCurrentValue(const DependencyPropertyRef<TOwner, TValue>& property, PropertyAccess<TValue> value) noexcept;
    void SetCurrentValue(const DependencyPropertyKey& key, const PropertyValue& value) noexcept;
    void SetTemplateValue(DependencyPropertyHandle property, const PropertyValue& value) noexcept;
    void ClearValue(DependencyPropertyHandle property) noexcept;
    template<class TOwner, class TValue> void ClearValue(
        const DependencyPropertyRef<TOwner, TValue>& property) noexcept { ClearValue(property.Handle()); }
    void ClearValue(const DependencyPropertyKey& key) noexcept;
    void CoerceValue(DependencyPropertyHandle property) noexcept;
    template<class TOwner, class TValue> void CoerceValue(
        const DependencyPropertyRef<TOwner, TValue>& property) noexcept { CoerceValue(property.Handle()); }
    PropertyInvalidationFlags PendingInvalidations() const noexcept { return invalidations_; }
    PropertyInvalidationFlags TakeInvalidations() noexcept;
    std::uint32_t StoredValueCount() const noexcept;

    // Listeners execute after the effective value has committed and after the
    // property's metadata callback. They are intended to queue later work,
    // not to synchronously mutate the same property.
    void AddValueChangedHandler(DependencyPropertyHandle property,
        const DependencyPropertyChangedEventHandler& handler) noexcept;
    template<class TOwner, class TValue> void AddValueChangedHandler(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const DependencyPropertyChangedEventHandler& handler) noexcept {
        AddValueChangedHandler(property.Handle(), handler);
    }
    template<class TOwner, class TValue> void AddValueChangedHandler(
        const ReadOnlyPropertyRef<TOwner, TValue>& property,
        const DependencyPropertyChangedEventHandler& handler) noexcept {
        AddValueChangedHandler(property.Handle(), handler);
    }
    bool RemoveValueChangedHandler(DependencyPropertyHandle property,
        const DependencyPropertyChangedEventHandler& handler) noexcept;
    template<class TOwner, class TValue> bool RemoveValueChangedHandler(
        const ReadOnlyPropertyRef<TOwner, TValue>& property,
        const DependencyPropertyChangedEventHandler& handler) noexcept {
        return RemoveValueChangedHandler(property.Handle(), handler);
    }

protected:
    explicit DependencyObject(TypeId runtimeType) noexcept;
    ~DependencyObject() override;

    // Framework-owned state properties use this path so public SetValue calls
    // remain read-only while derived runtime types can publish state changes.
    void SetReadOnlyCurrentValue(DependencyPropertyHandle property, const PropertyValue& value) noexcept;
    template<class TOwner, class TValue>
    void SetReadOnlyCurrentValue(const ReadOnlyPropertyRef<TOwner, TValue>& property, PropertyAccess<TValue> value) noexcept;
    virtual void OnPropertyInvalidated(PropertyInvalidationFlags flags) noexcept;
    virtual void OnPropertyChanged(const DependencyPropertyChangedEventArgs& args) noexcept;
    // WPF-parity coercion/validation extension points. Default preserves
    // current metadata-delegate behavior; overrides avoid per-DP delegates.
    virtual PropertyValue CoerceValueCore(DependencyPropertyHandle property, const PropertyValue& baseValue) noexcept {
        static_cast<void>(property);
        return baseValue;
    }
    virtual bool ValidateValueCore(DependencyPropertyHandle property, const PropertyValue& value) const noexcept {
        static_cast<void>(property);
        static_cast<void>(value);
        return true;
    }
    virtual bool VerifyMutationAllowed() const noexcept;

private:
    friend class DependencyMutationScope;
    // Property engine invokes the protected Coerce/Validate virtuals.
    friend class Meta::DependencyPropertyRegistry;
    friend class DependencyObjectAccess;
    friend class Freezable;
    friend class Animatable;
    friend class Meta::EffectiveValueEngine;
    friend class AnimationEngine;
    friend class ElementTree;
    friend class FrameworkElement;
    friend class StyleEngine;
    friend class TriggerEngine;
    friend class Controls::VisualStateManagerState;

    enum class ChangeKind : std::uint8_t {
        SetLocal,
        SetCurrent,
        Clear,
        ReCoerce
    };

    PropertyStore* Store() noexcept { return static_cast<PropertyStore*>(valueStore_); }
    const PropertyStore* Store() const noexcept { return static_cast<const PropertyStore*>(valueStore_); }
    void ForEachStoredKey(
        void (*visitor)(void*, MemberId) noexcept,
        void* context) noexcept;
    void CopyStoredKeys(Base::Vector<MemberId>& outKeys) const noexcept;
    const PropertyValue* FindEffectiveValuePtr(DependencyPropertyHandle property) const noexcept;
    std::uint32_t ClearAllProviderOrigin(std::uint32_t origin) noexcept;
    Result<void> DropAllEngineValueState() noexcept;
    bool HasUnfreezableValueState() const noexcept;
    Base::Result<void> VisitFreezableChildren(
        void* context,
        Base::Result<void> (*visitor)(void* context, Freezable& child) noexcept) noexcept;
    Base::Result<void> PrepareConsumerChange(
        Meta::DependencyPropertyHandle property,
        const Meta::PropertyValue& oldValue,
        const Meta::PropertyValue& newValue) noexcept;
    void CommitConsumerChange(
        Meta::DependencyPropertyHandle property,
        const Meta::PropertyValue& oldValue,
        const Meta::PropertyValue& newValue) noexcept;
    void InvalidateSubProperty(Meta::DependencyPropertyHandle property) noexcept;
    void DetachPropertyDependencyObjects(
        BindingEngine* bindings,
        Meta::EffectiveValueEngine* values,
        AnimationEngine* animations,
        Base::HashSet<DependencyObject*>& visited) noexcept;
    StoredValueEntry* FindStoredEntry(DependencyPropertyHandle property) noexcept;
    const StoredValueEntry* FindStoredEntry(DependencyPropertyHandle property) const noexcept;
    Result<StoredValueEntry*> EnsureStoredEntry(DependencyPropertyHandle property) noexcept;
    MemberId CanonicalPropertyKey(DependencyPropertyHandle property) const noexcept;
    Result<void> ApplyProviderContributionInternal(DependencyPropertyHandle property,
        PropertyProviderToken token, const PropertyValue& value) noexcept;
    Result<void> ApplyLocalExpressionInternal(DependencyPropertyHandle property,
        const PropertyExpression& expression) noexcept;
    bool InvalidateBaseValueInternal(DependencyPropertyHandle property) noexcept;
    Result<void> ApplyAnimationValueInternal(DependencyPropertyHandle property, const PropertyValue& value) noexcept;
    Result<void> ApplyInheritedValueInternal(DependencyPropertyHandle property, const PropertyValue* value) noexcept;
    Result<void> RecomputeEffectiveValueInternal(DependencyPropertyHandle property) noexcept;
    Result<void> DropEngineValueStateInternal(DependencyPropertyHandle property) noexcept;
    void ReleaseExpression(StoredValueEntry& entry) noexcept;
    void RemoveStoredEntry(MemberId key) noexcept;
    Meta::DependencyPropertyRegistry& PropertyRegistry() const noexcept { return *registry_; }
    Result<void> VerifyReady() const noexcept;
    Result<DependencyMutationScope> BeginMutation(DependencyPropertyHandle property) noexcept;
    void LeaveMutation() noexcept;
    Result<StoredValueEntry*> EnsureStoredEntryDirect(DependencyPropertyHandle canonicalHandle,
        const PropertyMetadata& metadata) noexcept;
    Result<void> RecomputeEffectiveValueCore(DependencyPropertyHandle property,
        const Meta::DependencyProperty& registered, const PropertyMetadata& metadata,
        const PropertyValue& oldEffective, const PropertyValueSourceInfo& oldSourceInfo,
        StoredValueEntry* storedEntry = nullptr) noexcept;
    static EffectiveValueSource ToLegacySource(const PropertyValueSourceInfo& source) noexcept;
    Result<void> ApplyChange(DependencyPropertyHandle property, const DependencyPropertyKey* key,
        ChangeKind kind, const PropertyValue* value) noexcept;
    void NotifyValueChanged(const DependencyPropertyChangedEventArgs& args) noexcept;
    PropertyInvalidationFlags AccumulateInvalidations(FrameworkPropertyMetadataOptions metadataFlags) noexcept;

    bool ClearProviderContributionInternal(DependencyPropertyHandle property,
        PropertyProviderToken token) noexcept;
    bool ClearProviderOriginInternal(DependencyPropertyHandle property, std::uint32_t origin) noexcept;
    bool ClearLocalExpressionInternal(DependencyPropertyHandle property) noexcept;
    bool ClearAnimationValueInternal(DependencyPropertyHandle property) noexcept;
    Result<PropertyValue> GetAnimationBaseValueInternal(DependencyPropertyHandle property) noexcept;
    bool HasPropertyRegistry() const noexcept { return registry_ != nullptr; }
    bool HasChangeHandlers() const noexcept;
    void NotifyReadOnlyRedirectedChange(
        DependencyPropertyHandle property,
        const PropertyValue& oldValue,
        const PropertyValue& newValue) noexcept;

    void RemoveChangeHandler(std::uint32_t index) noexcept;
    std::uint32_t StyleApplicationIndex() const noexcept {
        return styleAppIndex_ == UINT16_MAX ? UINT32_MAX : static_cast<std::uint32_t>(styleAppIndex_);
    }
    void SetStyleApplicationIndex(std::uint32_t index) noexcept {
        styleAppIndex_ = index >= UINT16_MAX ? UINT16_MAX : static_cast<std::uint16_t>(index);
    }

    DependencyObject* ExplicitInheritanceParent() const noexcept;
    void SetExplicitInheritanceParent(DependencyObject* parent) noexcept;

    Meta::DependencyPropertyRegistry* registry_ = nullptr;
    TypeId runtimeType_ = InvalidTypeId;
    void* valueStore_ = nullptr;
    DependencyObjectRare* rare_ = nullptr;
    std::uint16_t unmaterializedQueueMask_ = 0U;
    std::uint16_t registryEpoch_ = 0U;
    std::uint16_t styleAppIndex_ = UINT16_MAX;
    PropertyInvalidationFlags invalidations_ = PropertyInvalidationFlags::None;
    bool objectServicesAvailable_ : 1;
    mutable bool typeVerified_ : 1;
    bool hasExplicitInheritanceChildren_ : 1;
    bool hasExplicitInheritanceParent_ : 1;
};

template<class TOwner, class TValue> PropertyAccess<TValue> DependencyObject::GetValue(
    const DependencyPropertyRef<TOwner, TValue>& property) const noexcept {
    const PropertyValue* stored = FindEffectiveValuePtr(property.Handle());
    if (stored == nullptr) return PropertyAccess<TValue>{};
    Result<PropertyAccess<TValue>> decoded = Meta::ValueCodec<TValue>::Decode(*stored);
    return decoded ? std::move(decoded).Value() : PropertyAccess<TValue>{};
}

template<class TOwner> StringView DependencyObject::GetValue(
    const DependencyPropertyRef<TOwner, String>& property) const noexcept {
    return GetStringView(property.Handle());
}

template<class TOwner, class TValue> PropertyAccess<TValue> DependencyObject::GetValue(
    const ReadOnlyPropertyRef<TOwner, TValue>& property) const noexcept {
    if constexpr (std::is_same_v<TValue, double>) {
        constexpr MemberId kActualWidthId =
            Meta::MakeMemberId(Base::MakeMetaTypeId("FrameworkElement"), Meta::MemberKind::Property, "ActualWidth");
        constexpr MemberId kActualHeightId =
            Meta::MakeMemberId(Base::MakeMetaTypeId("FrameworkElement"), Meta::MemberKind::Property, "ActualHeight");
        if (property.Id() == kActualWidthId || property.Id() == kActualHeightId) {
            const PropertyValue redirected = GetValue(property.Handle());
            return redirected.Kind() == Base::ValueKind::Double ? redirected.AsDouble() : 0.0;
        }
    }
    const PropertyValue* stored = FindEffectiveValuePtr(property.Handle());
    if (stored == nullptr) return PropertyAccess<TValue>{};
    Result<PropertyAccess<TValue>> decoded = Meta::ValueCodec<TValue>::Decode(*stored);
    return decoded ? std::move(decoded).Value() : PropertyAccess<TValue>{};
}


template<class TOwner> StringView DependencyObject::GetValue(
    const ReadOnlyPropertyRef<TOwner, String>& property) const noexcept {
    return GetStringView(property.Handle());
}

template<class TOwner, class TValue> void DependencyObject::SetValue(
    const DependencyPropertyRef<TOwner, TValue>& property,
    PropertyAccess<TValue> value) noexcept {
    Result<PropertyValue> stored = Meta::ValueCodec<TValue>::Encode(value);
    if (!stored) return;
    SetValue(property.Handle(), stored.Value());
}

template<class TOwner, class TValue> void DependencyObject::SetCurrentValue(
    const DependencyPropertyRef<TOwner, TValue>& property,
    PropertyAccess<TValue> value) noexcept {
    Result<PropertyValue> stored = Meta::ValueCodec<TValue>::Encode(value);
    if (!stored) return;
    SetCurrentValue(property.Handle(), stored.Value());
}

template<class TOwner> void DependencyObject::SetValue(const DependencyPropertyRef<TOwner, String>& property,
    StringView value) noexcept {
    Result<PropertyValue> stored = Base::Value::TryFromString(Meta::TypeOf<String>(), value);
    if (!stored) return;
    SetValue(property.Handle(), stored.Value());
}

template<class TOwner, class TValue> void DependencyObject::SetReadOnlyCurrentValue(
    const ReadOnlyPropertyRef<TOwner, TValue>& property,
    PropertyAccess<TValue> value) noexcept {
    Result<PropertyValue> stored = Meta::ValueCodec<TValue>::Encode(value);
    if (!stored) return;
    SetReadOnlyCurrentValue(property.Handle(), stored.Value());
}

} // namespace Aero
