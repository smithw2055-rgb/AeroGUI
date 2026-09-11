// Included from AeroGuiInternal.hpp inside class AeroGuiInternal.
// Path / freezable consumers and the property store.

    // --- Path / freezable / DP consumers ---
    static void PathInvalidateGeometry(Shapes::Path& path) noexcept;
    static void PathAttachMeshResources(
        Shapes::Path& path,
        void* services,
        bool invalidate) noexcept;
    using FreezableVisitor = Base::Result<void> (*)(
        void* context,
        Freezable& child) noexcept;
    static bool HasUnfreezableValueState(
        const DependencyObject& object) noexcept;
    static Base::Result<void> VisitFreezableChildren(
        DependencyObject& object,
        void* context,
        FreezableVisitor visitor) noexcept;
    static Base::Result<void> PrepareConsumerChange(
        DependencyObject& object,
        Meta::DependencyPropertyHandle property,
        const Meta::PropertyValue& oldValue,
        const Meta::PropertyValue& newValue) noexcept;
    static void CommitConsumerChange(
        DependencyObject& object,
        Meta::DependencyPropertyHandle property,
        const Meta::PropertyValue& oldValue,
        const Meta::PropertyValue& newValue) noexcept;
    static void InvalidateSubProperty(
        DependencyObject& object,
        Meta::DependencyPropertyHandle property) noexcept;
    static Base::Result<void> AttachFreezableConsumer(
        Freezable& value,
        DependencyObject& object,
        Meta::DependencyPropertyHandle property) noexcept;
    static void DetachFreezableConsumer(
        Freezable& value,
        DependencyObject& object,
        Meta::DependencyPropertyHandle property) noexcept;
    static std::uint64_t FreezableRevision(const Freezable& value) noexcept;
    static bool FreezableCheckCore(Freezable& value) noexcept;
    static DependencyObject* FreezableParent(const Freezable& value) noexcept;
    static void DetachPropertyDependencyObjects(
        DependencyObject& object,
        BindingEngine* bindings,
        Meta::EffectiveValueEngine* values,
        AnimationEngine* animations,
        Base::Vector<DependencyObject*>& visited) noexcept;

    // --- Property store ---
    static PropertyStore* Store(DependencyObject& object) noexcept {
        return static_cast<PropertyStore*>(
            AERO_GET_FIELD(object, DO_valueStore));
    }
    static const PropertyStore* Store(const DependencyObject& object) noexcept {
        return static_cast<const PropertyStore*>(
            AERO_GET_FIELD(object, DO_valueStore));
    }
    static Meta::DependencyPropertyRegistry& PropertyRegistry(
        const DependencyObject& object) noexcept {
        return *AERO_GET_FIELD(object, DO_registry);
    }
    static Meta::DependencyPropertyRegistry& PropertyRegistry(
        const DependencyObject* object) noexcept {
        return *AERO_GET_FIELD(*object, DO_registry);
    }
    static MemberId CanonicalKey(
        const DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return AERO_CALL_METHOD(object, DO_CanonicalPropertyKey, property);
    }
    static StoredValueEntry* FindEntry(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return AERO_CALL_METHOD(object, DO_FindStoredEntry, property);
    }
    static const StoredValueEntry* FindEntry(
        const DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return AERO_CALL_METHOD(object, DO_FindStoredEntryConst, property);
    }
    static Base::Result<StoredValueEntry*> EnsureEntry(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return AERO_CALL_METHOD(object, DO_EnsureStoredEntry, property);
    }
    static void RemoveEntry(
        DependencyObject& object,
        MemberId key) noexcept {
        AERO_CALL_METHOD(object, DO_RemoveStoredEntry, key);
    }
    static void ReleaseExpression(StoredValueEntry& entry) noexcept {
        // Implemented as DependencyObject::ReleaseExpression; callers pass
        // the owning object when they have one.
        static_cast<void>(entry);
    }
    static Base::Result<void> ApplyProviderContribution(
        DependencyObject& object,
        DependencyPropertyHandle property,
        PropertyProviderToken token,
        const PropertyValue& value) noexcept {
        return AERO_CALL_METHOD(object, DO_ApplyProviderContribution, property, token, value);
    }
    static Base::Result<bool> ClearProviderContribution(
        DependencyObject& object,
        DependencyPropertyHandle property,
        PropertyProviderToken token) noexcept {
        return AERO_CALL_METHOD(object, DO_ClearProviderContribution, property, token);
    }
    static Base::Result<bool> ClearProviderOrigin(
        DependencyObject& object,
        DependencyPropertyHandle property,
        std::uint32_t origin) noexcept {
        return AERO_CALL_METHOD(object, DO_ClearProviderOrigin, property, origin);
    }
    static Base::Result<std::uint32_t> ClearProviderOrigin(
        DependencyObject& object,
        std::uint32_t origin) noexcept {
        std::uint32_t removed = 0U;
        Base::Vector<MemberId> keys;
        ForEachStoredKey(
            object,
            [](void* context, MemberId key) noexcept {
                static_cast<Base::Vector<MemberId>*>(context)->PushBack(key);
            },
            &keys);
        for (MemberId key : keys) {
            Base::Result<bool> cleared =
                AERO_CALL_METHOD(object, DO_ClearProviderOrigin,
                    DependencyPropertyHandle{key}, origin);
            if (!cleared) return cleared.GetStatus();
            if (cleared.Value()) ++removed;
        }
        return removed;
    }
    static Base::Result<void> ApplyLocalExpression(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyExpression& expression) noexcept {
        return AERO_CALL_METHOD(object, DO_ApplyLocalExpression, property, expression);
    }
    static Base::Result<bool> ClearLocalExpression(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return AERO_CALL_METHOD(object, DO_ClearLocalExpression, property);
    }
    static Base::Result<bool> InvalidateBaseValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return AERO_CALL_METHOD(object, DO_InvalidateBaseValue, property);
    }
    static Base::Result<void> ApplyAnimationValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue& value) noexcept {
        return AERO_CALL_METHOD(object, DO_ApplyAnimationValue, property, value);
    }
    static Base::Result<bool> ClearAnimationValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return AERO_CALL_METHOD(object, DO_ClearAnimationValue, property);
    }
    static Base::Result<PropertyValue> GetAnimationBaseValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return AERO_CALL_METHOD(object, DO_GetAnimationBaseValue, property);
    }
    static Base::Result<void> ApplyInheritedValue(
        DependencyObject& object,
        DependencyPropertyHandle property,
        const PropertyValue* value) noexcept {
        return AERO_CALL_METHOD(object, DO_ApplyInheritedValue, property, value);
    }
    static Base::Result<void> RecomputeEffectiveValue(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return AERO_CALL_METHOD(object, DO_RecomputeEffectiveValue, property);
    }
    static Base::Result<void> DropEngineValueState(
        DependencyObject& object,
        DependencyPropertyHandle property) noexcept {
        return AERO_CALL_METHOD(object, DO_DropEngineValueState, property);
    }
    static Base::Result<void> DropAllEngineValueState(
        DependencyObject& object) noexcept {
        Base::Vector<MemberId> keys;
        ForEachStoredKey(
            object,
            [](void* context, MemberId key) noexcept {
                static_cast<Base::Vector<MemberId>*>(context)->PushBack(key);
            },
            &keys);
        for (MemberId key : keys) {
            Base::Result<void> dropped =
                AERO_CALL_METHOD(object, DO_DropEngineValueState,
                    DependencyPropertyHandle{key});
            if (!dropped) return dropped.GetStatus();
        }
        return {};
    }
    static void ForEachStoredKey(
        DependencyObject& object,
        void (*visitor)(void*, MemberId) noexcept,
        void* context) noexcept {
        PropertyStore* store = Store(object);
        if (store == nullptr || visitor == nullptr) return;
        for (auto& record : store->entries) {
            visitor(context, record.Key());
        }
    }
