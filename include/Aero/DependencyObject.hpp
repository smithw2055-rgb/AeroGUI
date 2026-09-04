#pragma once

#include <Aero/DispatcherObject.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/DispatcherReentrancyGuard.hpp>
#include <Aero/TryCast.hpp>

namespace Aero {

using ::Aero::Threading::DispatcherReentrancyGuard;

class AeroGuiInternal;
#if defined(AERO_GUI_IMPLEMENTATION)
struct StoredValueEntry;
#endif

class AERO_GUI_API DependencyObject : public DispatcherObject {
    AERO_DECLARE_TYPE(DependencyObject, DispatcherObject)
#if defined(AERO_GUI_IMPLEMENTATION)
    friend class ::Aero::AeroGuiInternal;
#endif
public:

    TypeId RuntimeType() const noexcept override {
        return runtimeType_;
    }
#if defined(AERO_GUI_IMPLEMENTATION)
    DependencyPropertyRegistry& PropertyRegistry() const noexcept {
        return *registry_;
    }
#endif

    PropertyValue GetValue(
        DependencyPropertyHandle property) const noexcept;
    template<class TOwner, class TValue>
    PropertyAccess<TValue> GetValue(
        const DependencyPropertyRef<TOwner, TValue>& property) const noexcept;
    template<class TOwner>
    StringView GetValue(
        const DependencyPropertyRef<TOwner, String>& property) const noexcept;
    template<class TOwner, class TValue>
    PropertyAccess<TValue> GetValue(
        const ReadOnlyPropertyRef<TOwner, TValue>& property) const noexcept;
    template<class TOwner>
    StringView GetValue(
        const ReadOnlyPropertyRef<TOwner, String>& property) const noexcept;
    PropertyValue ReadLocalValue(
        DependencyPropertyHandle property) const noexcept;
    EffectiveValueSource GetValueSource(
        DependencyPropertyHandle property) const noexcept;
    PropertyValueSourceInfo GetValueSourceInfo(
        DependencyPropertyHandle property) const noexcept;

    void SetValue(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    template<class TOwner, class TValue>
    void SetValue(
        const DependencyPropertyRef<TOwner, TValue>& property,
        PropertyAccess<TValue> value) noexcept;
    template<class TOwner>
    void SetValue(
        const DependencyPropertyRef<TOwner, String>& property,
        StringView value) noexcept;
    void SetValue(
        const DependencyPropertyKey& key,
        const PropertyValue& value) noexcept;

    void SetCurrentValue(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    template<class TOwner, class TValue>
    void SetCurrentValue(
        const DependencyPropertyRef<TOwner, TValue>& property,
        PropertyAccess<TValue> value) noexcept;
    void SetCurrentValue(
        const DependencyPropertyKey& key,
        const PropertyValue& value) noexcept;

    void SetTemplateValue(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;

    void ClearValue(
        DependencyPropertyHandle property) noexcept;
    template<class TOwner, class TValue>
    void ClearValue(
        const DependencyPropertyRef<TOwner, TValue>& property) noexcept {
        ClearValue(property.Handle());
    }
    void ClearValue(
        const DependencyPropertyKey& key) noexcept;

    void CoerceValue(
        DependencyPropertyHandle property) noexcept;
    template<class TOwner, class TValue>
    void CoerceValue(
        const DependencyPropertyRef<TOwner, TValue>& property) noexcept {
        CoerceValue(property.Handle());
    }

    // Listeners execute after the effective value has committed and after the
    // property's metadata callback. They are intended to queue later work,
    // not to synchronously mutate the same property.
    void AddValueChangedHandler(
        DependencyPropertyHandle property,
        const DependencyPropertyChangedEventHandler& handler) noexcept;
    template<class TOwner, class TValue>
    void AddValueChangedHandler(
        const DependencyPropertyRef<TOwner, TValue>& property,
        const DependencyPropertyChangedEventHandler& handler) noexcept {
        AddValueChangedHandler(property.Handle(), handler);
    }
    template<class TOwner, class TValue>
    void AddValueChangedHandler(
        const ReadOnlyPropertyRef<TOwner, TValue>& property,
        const DependencyPropertyChangedEventHandler& handler) noexcept {
        AddValueChangedHandler(property.Handle(), handler);
    }
    bool RemoveValueChangedHandler(
        DependencyPropertyHandle property,
        const DependencyPropertyChangedEventHandler& handler) noexcept;
    template<class TOwner, class TValue>
    bool RemoveValueChangedHandler(
        const ReadOnlyPropertyRef<TOwner, TValue>& property,
        const DependencyPropertyChangedEventHandler& handler) noexcept {
        return RemoveValueChangedHandler(
            property.Handle(), handler);
    }

    PropertyInvalidationFlags PendingInvalidations() const noexcept {
        return invalidations_;
    }
    PropertyInvalidationFlags TakeInvalidations() noexcept;
    std::uint32_t StoredValueCount() const noexcept;

protected:
    explicit DependencyObject(TypeId runtimeType) noexcept;
    ~DependencyObject() override;
    // Framework-owned state properties use this path so public SetValue calls
    // remain read-only while derived runtime types can publish state changes.
    void SetReadOnlyCurrentValue(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    template<class TOwner, class TValue>
    void SetReadOnlyCurrentValue(
        const ReadOnlyPropertyRef<TOwner, TValue>& property,
        PropertyAccess<TValue> value) noexcept;
    virtual void OnPropertyInvalidated(
        PropertyInvalidationFlags flags) noexcept;
    virtual void OnPropertyChanged(
        const DependencyPropertyChangedEventArgs& args) noexcept;
    virtual Result<void> VerifyMutationAllowed() const noexcept;

private:
    enum class ChangeKind : std::uint8_t {
        SetLocal,
        SetCurrent,
        Clear,
        ReCoerce
    };

    struct ChangeHandlerRecord {
        DependencyPropertyHandle property;
        DependencyPropertyChangedEventHandler handler;
        bool active = false;
    };

    class MutationScope {
    public:
        MutationScope() noexcept = default;
        MutationScope(MutationScope&& other) noexcept;
        MutationScope& operator=(MutationScope&& other) noexcept;
        ~MutationScope();

        MutationScope(const MutationScope&) = delete;
        MutationScope& operator=(const MutationScope&) = delete;

        void Release() noexcept;

    private:
        friend class DependencyObject;

        MutationScope(
            DependencyObject* owner,
            DispatcherReentrancyGuard&& guard) noexcept;

        DependencyObject* owner_ = nullptr;
        DispatcherReentrancyGuard dispatcherGuard_;
    };

    Meta::DependencyPropertyRegistry* registry_ = nullptr;
    TypeId runtimeType_ = InvalidTypeId;
    bool objectServicesAvailable_ = false;
    mutable bool typeVerified_ = false;
    void* valueStore_ = nullptr;
    Base::Vector<MemberId> updateStack_;
    Base::Vector<ChangeHandlerRecord> changeHandlers_;
    PropertyInvalidationFlags invalidations_ = PropertyInvalidationFlags::None;
    std::uint32_t changeHandlerNotificationDepth_ = 0U;
    std::uint64_t nextValueRevision_ = 1U;

    Result<void> VerifyReady() const noexcept;
    Result<MutationScope> BeginMutation(
        DependencyPropertyHandle property) noexcept;
    void LeaveMutation() noexcept;

#if defined(AERO_GUI_IMPLEMENTATION)
    StoredValueEntry* FindStoredEntry(
        DependencyPropertyHandle property) noexcept;
    const StoredValueEntry* FindStoredEntry(
        DependencyPropertyHandle property) const noexcept;
    Result<StoredValueEntry*> EnsureStoredEntry(
        DependencyPropertyHandle property) noexcept;
    Result<StoredValueEntry*> EnsureStoredEntryDirect(
        DependencyPropertyHandle canonicalHandle,
        const PropertyMetadata& metadata) noexcept;
    MemberId CanonicalPropertyKey(
        DependencyPropertyHandle property) const noexcept;
#endif
    Result<void> ApplyProviderContributionInternal(
        DependencyPropertyHandle property,
        PropertyProviderToken token,
        const PropertyValue& value) noexcept;
    Result<bool> ClearProviderContributionInternal(
        DependencyPropertyHandle property,
        PropertyProviderToken token) noexcept;
    Result<bool> ClearProviderOriginInternal(
        DependencyPropertyHandle property,
        std::uint32_t origin) noexcept;
    Result<void> ApplyLocalExpressionInternal(
        DependencyPropertyHandle property,
        const PropertyExpression& expression) noexcept;
    Result<bool> ClearLocalExpressionInternal(
        DependencyPropertyHandle property) noexcept;
    Result<bool> InvalidateBaseValueInternal(
        DependencyPropertyHandle property) noexcept;
    Result<void> ApplyAnimationValueInternal(
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept;
    Result<bool> ClearAnimationValueInternal(
        DependencyPropertyHandle property) noexcept;
    Result<PropertyValue> GetAnimationBaseValueInternal(
        DependencyPropertyHandle property) noexcept;
    Result<void> ApplyInheritedValueInternal(
        DependencyPropertyHandle property,
        const PropertyValue* value) noexcept;
    Result<void> RecomputeEffectiveValueInternal(
        DependencyPropertyHandle property) noexcept;
    Result<void> DropEngineValueStateInternal(
        DependencyPropertyHandle property) noexcept;
    Result<void> RecomputeEffectiveValueCore(
        DependencyPropertyHandle property,
        const Meta::DependencyProperty& registered,
        const PropertyMetadata& metadata,
        const PropertyValue& oldEffective,
        const PropertyValueSourceInfo& oldSourceInfo) noexcept;
#if defined(AERO_GUI_IMPLEMENTATION)
    void ReleaseExpression(StoredValueEntry& entry) noexcept;
    void RemoveStoredEntry(MemberId key) noexcept;
#endif
    static EffectiveValueSource ToLegacySource(
        const PropertyValueSourceInfo& source) noexcept;

    Result<void> ApplyChange(
        DependencyPropertyHandle property,
        const DependencyPropertyKey* key,
        ChangeKind kind,
        const PropertyValue* value) noexcept;
    void RemoveChangeHandler(std::uint32_t index) noexcept;
    void NotifyValueChanged(
        const DependencyPropertyChangedEventArgs& args) noexcept;
    PropertyInvalidationFlags AccumulateInvalidations(
        PropertyMetadataFlags metadataFlags) noexcept;
};

template<class TOwner, class TValue>
PropertyAccess<TValue> DependencyObject::GetValue(
    const DependencyPropertyRef<TOwner, TValue>& property) const noexcept {
    const PropertyValue stored = GetValue(property.Handle());
    Result<PropertyAccess<TValue>> decoded =
        Meta::ValueCodec<TValue>::Decode(stored);
    return decoded ? std::move(decoded).Value() : PropertyAccess<TValue>{};
}

template<class TOwner>
StringView DependencyObject::GetValue(
    const DependencyPropertyRef<TOwner, String>&
        property) const noexcept {
    const PropertyValue stored = GetValue(property.Handle());
    return stored.Kind() == Base::ValueKind::String
        ? stored.AsString()
        : StringView{};
}

template<class TOwner, class TValue>
PropertyAccess<TValue> DependencyObject::GetValue(
    const ReadOnlyPropertyRef<TOwner, TValue>& property) const noexcept {
    const PropertyValue stored = GetValue(property.Handle());
    Result<PropertyAccess<TValue>> decoded =
        Meta::ValueCodec<TValue>::Decode(stored);
    return decoded ? std::move(decoded).Value() : PropertyAccess<TValue>{};
}


template<class TOwner>
StringView DependencyObject::GetValue(
    const ReadOnlyPropertyRef<TOwner, String>& property) const noexcept {
    const PropertyValue stored = GetValue(property.Handle());
    return stored.Kind() == Base::ValueKind::String
        ? stored.AsString()
        : StringView{};
}

template<class TOwner, class TValue>
void DependencyObject::SetValue(
    const DependencyPropertyRef<TOwner, TValue>& property,
    PropertyAccess<TValue> value) noexcept {
    Result<PropertyValue> stored =
        Meta::ValueCodec<TValue>::Encode(value);
    if (!stored) return;
    SetValue(property.Handle(), stored.Value());
}

template<class TOwner, class TValue>
void DependencyObject::SetCurrentValue(
    const DependencyPropertyRef<TOwner, TValue>& property,
    PropertyAccess<TValue> value) noexcept {
    Result<PropertyValue> stored =
        Meta::ValueCodec<TValue>::Encode(value);
    if (!stored) return;
    SetCurrentValue(property.Handle(), stored.Value());
}

template<class TOwner>
void DependencyObject::SetValue(
    const DependencyPropertyRef<TOwner, String>& property,
    StringView value) noexcept {
    Result<PropertyValue> stored =
        Base::Value::TryFromString(Meta::TypeOf<String>(), value);
    if (!stored) return;
    SetValue(property.Handle(), stored.Value());
}

template<class TOwner, class TValue>
void DependencyObject::SetReadOnlyCurrentValue(
    const ReadOnlyPropertyRef<TOwner, TValue>& property,
    PropertyAccess<TValue> value) noexcept {
    Result<PropertyValue> stored =
        Meta::ValueCodec<TValue>::Encode(value);
    if (!stored) return;
    SetReadOnlyCurrentValue(
        property.Handle(), stored.Value());
}

} // namespace Aero
