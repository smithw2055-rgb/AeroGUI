#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Core/Presentation.hpp>
#include "TestAllocatorScope.hpp"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <utility>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

class TrackingAllocator final : public IAllocator {
public:
    void* Allocate(const AllocationRequest& request) noexcept override {
        if (failAfter_ == 0U) {
            return nullptr;
        }
        if (failAfter_ != UINT32_MAX) {
            --failAfter_;
        }
        void* memory = upstream_.Allocate(request);
        if (memory != nullptr) {
            ++active_;
            ++total_;
        }
        return memory;
    }

    void Deallocate(
        void* memory,
        std::size_t size,
        std::size_t alignment,
        MemoryTag tag) noexcept override {
        if (memory != nullptr) {
            if (active_ == 0U) {
                std::abort();
            }
            --active_;
        }
        upstream_.Deallocate(memory, size, alignment, tag);
    }

    void FailAfter(std::uint32_t successfulAllocations) noexcept {
        failAfter_ = successfulAllocations;
    }

    void DisableFailures() noexcept {
        failAfter_ = UINT32_MAX;
    }

    std::uint32_t Active() const noexcept { return active_; }
    std::uint32_t Total() const noexcept { return total_; }

private:
    MallocAllocator upstream_;
    std::uint32_t failAfter_ = UINT32_MAX;
    std::uint32_t active_ = 0U;
    std::uint32_t total_ = 0U;
};

struct CallbackState final {
    DependencyPropertyHandle width;
    TypeId doubleType = InvalidTypeId;
    std::uint32_t changedCount = 0U;
    double oldValue = 0.0;
    double newValue = 0.0;
    bool reenter = false;
    ErrorCode reentrantStatus = ErrorCode::Ok;
};

CallbackState* gCallbacks = nullptr;

bool ValidateWidth(const PropertyValue& value) noexcept {
    return value.Kind() == PropertyValueKind::Double &&
        value.AsDouble() >= 0.0;
}

Result<PropertyValue> CoerceWidth(
    DependencyObject&,
    const DependencyProperty&,
    const PropertyValue& baseValue) noexcept {
    const double value = baseValue.AsDouble();
    return PropertyValue::FromDouble(
        baseValue.Type(), value > 100.0 ? 100.0 : value);
}

void WidthChanged(
    DependencyObject& object,
    const DependencyPropertyChangedEventArgs& args) noexcept {
    if (gCallbacks == nullptr) {
        return;
    }

    ++gCallbacks->changedCount;
    gCallbacks->oldValue = args.oldValue.AsDouble();
    gCallbacks->newValue = args.newValue.AsDouble();

    if (gCallbacks->reenter) {
        gCallbacks->reenter = false;
        Result<void> nested = object.SetValue(
            gCallbacks->width,
            PropertyValue::FromDouble(gCallbacks->doubleType, 25.0));
        gCallbacks->reentrantStatus = nested.GetStatus().code;
    }
}

PropertyMetadata WidthMetadata(
    TypeId doubleType,
    double defaultValue,
    PropertyMetadataFlags flags,
    UpdateSourceTrigger trigger = UpdateSourceTrigger::Default) noexcept {
    PropertyMetadata metadata;
    metadata.defaultValue = PropertyValue::FromDouble(doubleType, defaultValue);
    metadata.flags = flags;
    metadata.defaultUpdateSourceTrigger = trigger;
    metadata.validate = &ValidateWidth;
    metadata.coerce = &CoerceWidth;
    metadata.changed = &WidthChanged;
    return metadata;
}

class TestElement : public DependencyObject {
public:
    explicit TestElement(TypeId runtimeType) noexcept
        : DependencyObject(runtimeType) {}

    ~TestElement() override = default;

    void RejectInvalidation(bool value) noexcept { rejectInvalidation_ = value; }

protected:
    Result<void> OnPropertyInvalidated(PropertyInvalidationFlags flags) noexcept override {
        if (rejectInvalidation_) {
            return Status::Failure(ErrorCode::InvalidState,
                "Synthetic invalidation routing failure");
        }
        return DependencyObject::OnPropertyInvalidated(flags);
    }

private:
    bool rejectInvalidation_ = false;
};

struct Fixture final {
    TrackingAllocator allocator;
    Aero::Tests::ScopedDefaultAllocator allocatorScope{allocator};
    TypeRegistry types;
    DependencyPropertyRegistry properties{types};

    TypeId object = InvalidTypeId;
    TypeId doubleType = InvalidTypeId;
    TypeId boolType = InvalidTypeId;
    TypeId uiElement = InvalidTypeId;
    TypeId button = InvalidTypeId;
    TypeId other = InvalidTypeId;

    DependencyPropertyHandle width;
    DependencyPropertyHandle height;
    DependencyPropertyHandle isLocked;
    DependencyPropertyHandle dataContext;
    DependencyPropertyKey isLockedKey;

    bool Build() {
        const StringView ns("urn:aero");
        object = MakeTypeId(ns, StringView("Object"));
        doubleType = MakeTypeId(ns, StringView("Double"));
        boolType = MakeTypeId(ns, StringView("Boolean"));
        uiElement = MakeTypeId(ns, StringView("UIElement"));
        button = MakeTypeId(ns, StringView("Button"));
        other = MakeTypeId(ns, StringView("OtherElement"));

        CHECK(types.TryRegisterType({
            ns, StringView("Object"), InvalidTypeId,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({
            ns, StringView("Double"), InvalidTypeId,
            TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
        CHECK(types.TryRegisterType({
            ns, StringView("Boolean"), InvalidTypeId,
            TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
        CHECK(types.TryRegisterType({
            ns, StringView("UIElement"), object,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({
            ns, StringView("Button"), uiElement,
            TypeFlags::Sealed, nullptr}));
        CHECK(types.TryRegisterType({
            ns, StringView("OtherElement"), object,
            TypeFlags::None, nullptr}));

        DependencyPropertyRegistration widthRegistration;
        widthRegistration.name = StringView("Width");
        widthRegistration.ownerType = uiElement;
        widthRegistration.valueType = doubleType;
        widthRegistration.metadata = WidthMetadata(
            doubleType,
            0.0,
            PropertyMetadataFlags::AffectsMeasure |
                PropertyMetadataFlags::AffectsArrange);

        Result<DependencyPropertyRegistrationResult> widthResult =
            properties.TryRegister(widthRegistration);
        CHECK(widthResult);
        width = widthResult.Value().property;
        CHECK(!widthResult.Value().readOnlyKey.IsValid());

        DependencyPropertyRegistration heightRegistration;
        heightRegistration.name = StringView("Height");
        heightRegistration.ownerType = uiElement;
        heightRegistration.valueType = doubleType;
        heightRegistration.metadata.defaultValue =
            PropertyValue::FromDouble(doubleType, 0.0);
        Result<DependencyPropertyRegistrationResult> heightResult =
            properties.TryRegister(heightRegistration);
        CHECK(heightResult);
        height = heightResult.Value().property;

        DependencyPropertyRegistration lockedRegistration;
        lockedRegistration.name = StringView("IsLocked");
        lockedRegistration.ownerType = uiElement;
        lockedRegistration.valueType = boolType;
        lockedRegistration.flags = DependencyPropertyFlags::ReadOnly;
        lockedRegistration.metadata.defaultValue =
            PropertyValue::FromBoolean(boolType, false);

        Result<DependencyPropertyRegistrationResult> lockedResult =
            properties.TryRegister(lockedRegistration);
        CHECK(lockedResult);
        isLocked = lockedResult.Value().property;
        isLockedKey = lockedResult.Value().readOnlyKey;
        CHECK(isLockedKey.IsValid());

        DependencyPropertyRegistration contextRegistration;
        contextRegistration.name = StringView("DataContext");
        contextRegistration.ownerType = uiElement;
        contextRegistration.valueType = object;
        contextRegistration.metadata.defaultValue =
            PropertyValue::NullObject(object);
        contextRegistration.metadata.flags = PropertyMetadataFlags::Inherits;

        Result<DependencyPropertyRegistrationResult> contextResult =
            properties.TryRegister(contextRegistration);
        CHECK(contextResult);
        dataContext = contextResult.Value().property;

        CHECK(properties.TryAddOwner(
            width,
            other,
            WidthMetadata(
                doubleType,
                5.0,
                PropertyMetadataFlags::AffectsMeasure)));

        CHECK(properties.TryOverrideMetadata(
            width,
            button,
            WidthMetadata(
                doubleType,
                10.0,
                PropertyMetadataFlags::AffectsRender |
                    PropertyMetadataFlags::BindsTwoWayByDefault,
                UpdateSourceTrigger::PropertyChanged)));

        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        return true;
    }
};

struct NestedNotificationState final {
    DependencyPropertyHandle height;
    DependencyPropertyHandle width;
    DependencyPropertyChangedEventHandler victim;
    TypeId doubleType = InvalidTypeId;
    ErrorCode removeStatus = ErrorCode::InternalError;
    ErrorCode nestedSetStatus = ErrorCode::InternalError;
    std::uint32_t sequence[4]{};
    std::uint32_t count = 0U;
};

struct NestedHeightHandler final {
    NestedNotificationState* state = nullptr;
    void operator()(DependencyObject&,
        const DependencyPropertyChangedEventArgs&) const noexcept {
        state->sequence[state->count++] = 2U;
    }
};

struct RemovedWidthHandler final {
    NestedNotificationState* state = nullptr;
    void operator()(DependencyObject&,
        const DependencyPropertyChangedEventArgs&) const noexcept {
        state->sequence[state->count++] = 3U;
    }
};

struct NestedWidthHandler final {
    NestedNotificationState* state = nullptr;
    void operator()(DependencyObject& object,
        const DependencyPropertyChangedEventArgs&) const noexcept {
        state->sequence[state->count++] = 1U;
        Result<bool> removed = object.RemoveValueChangedHandler(
            state->width, state->victim);
        state->removeStatus = removed
            ? ErrorCode::Ok
            : removed.GetStatus().code;
        Result<void> nested = object.SetValue(
            state->height,
            PropertyValue::FromDouble(state->doubleType, 42.0));
        state->nestedSetStatus = nested
            ? ErrorCode::Ok
            : nested.GetStatus().code;
    }
};

bool TestRegistrationAndMetadata() {
    Fixture fixture;
    CHECK(fixture.Build());

    CHECK(fixture.properties.PropertyCount() == 4U);
    CHECK(fixture.properties.IsFrozen());
    CHECK(fixture.properties.Freeze());

    const DependencyProperty* width = fixture.properties.Find(fixture.width);
    CHECK(width != nullptr);
    CHECK(width->Name() == StringView("Width"));
    CHECK(width->ValueType() == fixture.doubleType);
    CHECK(width->RegisteredOwnerType() == fixture.uiElement);
    CHECK(width->MetadataCount() == 3U);

    const DependencyProperty* inherited = fixture.properties.Find(
        fixture.button, StringView("Width"));
    const DependencyProperty* addedOwner = fixture.properties.Find(
        fixture.other, StringView("Width"));
    CHECK(inherited == width);
    CHECK(addedOwner == width);

    const PropertyInfo* originalMember = fixture.types.FindProperty(
        fixture.uiElement, StringView("Width"), false);
    const PropertyInfo* aliasMember = fixture.types.FindProperty(
        fixture.other, StringView("Width"), false);
    CHECK(originalMember != nullptr);
    CHECK(aliasMember != nullptr);
    CHECK(originalMember->Id() == fixture.width.value);
    CHECK(aliasMember->Id() != fixture.width.value);

    const PropertyMetadata* baseMetadata = width->MetadataFor(fixture.uiElement);
    const PropertyMetadata* buttonMetadata = width->MetadataFor(fixture.button);
    const PropertyMetadata* otherMetadata = width->MetadataFor(fixture.other);
    CHECK(baseMetadata != nullptr);
    CHECK(buttonMetadata != nullptr);
    CHECK(otherMetadata != nullptr);
    CHECK(baseMetadata->defaultValue.AsDouble() == 0.0);
    CHECK(buttonMetadata->defaultValue.AsDouble() == 10.0);
    CHECK(otherMetadata->defaultValue.AsDouble() == 5.0);
    CHECK(HasFlag(
        buttonMetadata->flags,
        PropertyMetadataFlags::BindsTwoWayByDefault));
    CHECK(buttonMetadata->defaultUpdateSourceTrigger ==
        UpdateSourceTrigger::PropertyChanged);

    DependencyPropertyRegistration late;
    late.name = StringView("Late");
    late.ownerType = fixture.uiElement;
    late.valueType = fixture.boolType;
    late.metadata.defaultValue = PropertyValue::FromBoolean(
        fixture.boolType, false);
    Result<DependencyPropertyRegistrationResult> lateResult =
        fixture.properties.TryRegister(late);
    CHECK(!lateResult);
    CHECK(lateResult.GetStatus().code == ErrorCode::InvalidState);
    return true;
}

bool TestEffectiveValuesAndInvalidation() {
    Fixture fixture;
    CHECK(fixture.Build());
    Dispatcher dispatcher;
    PresentationContextScope presentation(dispatcher, fixture.properties);

    Result<Ref<TestElement>> made = MakeRef<TestElement>(fixture.button);
    CHECK(made);
    Ref<TestElement> element = std::move(made).Value();

    CallbackState callbacks;
    callbacks.width = fixture.width;
    callbacks.doubleType = fixture.doubleType;
    gCallbacks = &callbacks;

    Result<PropertyValue> value = element->GetValue(fixture.width);
    CHECK(value && value.Value().AsDouble() == 10.0);
    Result<EffectiveValueSource> source = element->GetValueSource(fixture.width);
    CHECK(source && source.Value() == EffectiveValueSource::Default);
    CHECK(element->StoredValueCount() == 0U);

    CHECK(element->SetValue(
        fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, 200.0)));
    value = element->GetValue(fixture.width);
    CHECK(value && value.Value().AsDouble() == 100.0);
    source = element->GetValueSource(fixture.width);
    CHECK(source && source.Value() == EffectiveValueSource::Local);
    Result<PropertyValue> local = element->ReadLocalValue(fixture.width);
    CHECK(local && local.Value().AsDouble() == 200.0);
    CHECK(callbacks.changedCount == 1U);
    CHECK(callbacks.oldValue == 10.0);
    CHECK(callbacks.newValue == 100.0);
    CHECK(element->StoredValueCount() == 1U);

    Result<PropertyInvalidationFlags> invalidations =
        element->TakeInvalidations();
    CHECK(invalidations);
    CHECK(HasFlag(invalidations.Value(), PropertyInvalidationFlags::Render));
    CHECK(!HasFlag(invalidations.Value(), PropertyInvalidationFlags::Measure));
    CHECK(element->PendingInvalidations() == PropertyInvalidationFlags::None);

    CHECK(element->SetCurrentValue(
        fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, 80.0)));
    value = element->GetValue(fixture.width);
    CHECK(value && value.Value().AsDouble() == 80.0);
    source = element->GetValueSource(fixture.width);
    CHECK(source && source.Value() == EffectiveValueSource::Current);
    local = element->ReadLocalValue(fixture.width);
    CHECK(local && local.Value().AsDouble() == 200.0);

    CHECK(element->ClearValue(fixture.width));
    value = element->GetValue(fixture.width);
    CHECK(value && value.Value().AsDouble() == 10.0);
    source = element->GetValueSource(fixture.width);
    CHECK(source && source.Value() == EffectiveValueSource::Default);
    CHECK(element->StoredValueCount() == 0U);

    CHECK(element->SetValue(
        fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, 50.0)));
    CHECK(element->SetCurrentValue(
        fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, 40.0)));
    CHECK(element->SetValue(
        fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, 60.0)));
    value = element->GetValue(fixture.width);
    source = element->GetValueSource(fixture.width);
    CHECK(value && value.Value().AsDouble() == 60.0);
    CHECK(source && source.Value() == EffectiveValueSource::Local);

    gCallbacks = nullptr;
    return true;
}

bool TestReadOnlyAndInheritance() {
    Fixture fixture;
    CHECK(fixture.Build());
    Dispatcher dispatcher;
    PresentationContextScope presentation(dispatcher, fixture.properties);

    Result<Ref<TestElement>> made = MakeRef<TestElement>(fixture.button);
    CHECK(made);
    Ref<TestElement> element = std::move(made).Value();

    Result<void> denied = element->SetValue(
        fixture.isLocked,
        PropertyValue::FromBoolean(fixture.boolType, true));
    CHECK(!denied);
    CHECK(denied.GetStatus().code == ErrorCode::ReadOnly);

    CHECK(element->SetValue(
        fixture.isLockedKey,
        PropertyValue::FromBoolean(fixture.boolType, true)));
    Result<PropertyValue> locked = element->GetValue(fixture.isLocked);
    CHECK(locked && locked.Value().AsBoolean());

    denied = element->ClearValue(fixture.isLocked);
    CHECK(!denied);
    CHECK(denied.GetStatus().code == ErrorCode::ReadOnly);
    CHECK(element->ClearValue(fixture.isLockedKey));
    locked = element->GetValue(fixture.isLocked);
    CHECK(locked && !locked.Value().AsBoolean());

    Ref<Object> asObject(element);
    CHECK(element->SetValue(
        fixture.dataContext,
        PropertyValue::FromObject(fixture.button, std::move(asObject))));
    Result<PropertyValue> context = element->GetValue(fixture.dataContext);
    CHECK(context);
    CHECK(context.Value().AsObject().Get() == element.Get());

    Result<PropertyInvalidationFlags> invalidations =
        element->TakeInvalidations();
    CHECK(invalidations);
    CHECK(HasFlag(
        invalidations.Value(),
        PropertyInvalidationFlags::Inheritance));

    Result<void> wrongType = element->SetValue(
        fixture.dataContext,
        PropertyValue::FromDouble(fixture.doubleType, 1.0));
    CHECK(!wrongType);
    CHECK(wrongType.GetStatus().code == ErrorCode::InvalidArgument);

    CHECK(element->SetValue(
        fixture.dataContext,
        PropertyValue::NullObject(fixture.object)));
    context = element->GetValue(fixture.dataContext);
    CHECK(context && context.Value().IsNullObject());
    return true;
}

bool TestValidationCoercionAndReentrancy() {
    Fixture fixture;
    CHECK(fixture.Build());
    Dispatcher dispatcher;
    PresentationContextScope presentation(dispatcher, fixture.properties);

    Result<Ref<TestElement>> made = MakeRef<TestElement>(fixture.button);
    CHECK(made);
    Ref<TestElement> element = std::move(made).Value();

    CallbackState callbacks;
    callbacks.width = fixture.width;
    callbacks.doubleType = fixture.doubleType;
    gCallbacks = &callbacks;

    Result<void> invalid = element->SetValue(
        fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, -1.0));
    CHECK(!invalid);
    CHECK(invalid.GetStatus().code == ErrorCode::ValidationFailed);
    Result<PropertyValue> value = element->GetValue(fixture.width);
    CHECK(value && value.Value().AsDouble() == 10.0);
    CHECK(callbacks.changedCount == 0U);

    CHECK(element->SetValue(
        fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, 20.0)));
    CHECK(callbacks.changedCount == 1U);

    callbacks.reenter = true;
    CHECK(element->SetValue(
        fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, 30.0)));
    CHECK(callbacks.reentrantStatus == ErrorCode::InvalidState);
    value = element->GetValue(fixture.width);
    CHECK(value && value.Value().AsDouble() == 30.0);

    const std::uint32_t changedBeforeSameValue = callbacks.changedCount;
    CHECK(element->SetValue(
        fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, 30.0)));
    CHECK(callbacks.changedCount == changedBeforeSameValue);

    CHECK(element->SetValue(
        fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, 500.0)));
    value = element->GetValue(fixture.width);
    CHECK(value && value.Value().AsDouble() == 100.0);
    CHECK(element->CoerceValue(fixture.width));

    gCallbacks = nullptr;
    return true;
}

bool TestNestedChangeNotifications() {
    Fixture fixture;
    CHECK(fixture.Build());
    Dispatcher dispatcher;
    PresentationContextScope presentation(dispatcher, fixture.properties);
    Result<Ref<TestElement>> made = MakeRef<TestElement>(fixture.button);
    CHECK(made);
    Ref<TestElement> element = std::move(made).Value();

    NestedNotificationState state;
    state.height = fixture.height;
    state.width = fixture.width;
    state.doubleType = fixture.doubleType;
    NestedWidthHandler driverTarget{&state};
    RemovedWidthHandler victimTarget{&state};
    NestedHeightHandler heightTarget{&state};
    DependencyPropertyChangedEventHandler driver(&driverTarget);
    state.victim = DependencyPropertyChangedEventHandler(&victimTarget);
    DependencyPropertyChangedEventHandler height(&heightTarget);
    CHECK(element->TryAddValueChangedHandler(fixture.width, driver));
    CHECK(element->TryAddValueChangedHandler(fixture.width, state.victim));
    CHECK(element->TryAddValueChangedHandler(fixture.height, height));

    CHECK(element->SetValue(
        fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, 20.0)));
    CHECK(state.removeStatus == ErrorCode::Ok);
    CHECK(state.nestedSetStatus == ErrorCode::Ok);
    CHECK(state.count == 2U);
    CHECK(state.sequence[0] == 1U && state.sequence[1] == 2U);
    CHECK(!element->RemoveValueChangedHandler(
        fixture.width, state.victim).Value());
    return true;
}

bool TestWrongThread() {
    Fixture fixture;
    CHECK(fixture.Build());
    Dispatcher dispatcher;
    PresentationContextScope presentation(dispatcher, fixture.properties);

    Result<Ref<TestElement>> made = MakeRef<TestElement>(fixture.button);
    CHECK(made);
    Ref<TestElement> element = std::move(made).Value();

    std::atomic<std::uint32_t> status{
        static_cast<std::uint32_t>(ErrorCode::Ok)};
    std::thread worker([&]() {
        Result<void> result = element->SetValue(
            fixture.width,
            PropertyValue::FromDouble(fixture.doubleType, 42.0));
        status.store(
            static_cast<std::uint32_t>(result.GetStatus().code),
            std::memory_order_release);
    });
    worker.join();

    CHECK(status.load(std::memory_order_acquire) ==
        static_cast<std::uint32_t>(ErrorCode::WrongThread));
    Result<PropertyValue> value = element->GetValue(fixture.width);
    CHECK(value && value.Value().AsDouble() == 10.0);
    return true;
}

bool TestOomAndSparseStorage() {
    Fixture fixture;
    CHECK(fixture.Build());
    Dispatcher dispatcher;
    PresentationContextScope presentation(dispatcher, fixture.properties);

    Result<Ref<TestElement>> made = MakeRef<TestElement>(fixture.button);
    CHECK(made);
    Ref<TestElement> element = std::move(made).Value();

    fixture.allocator.FailAfter(0U);
    Result<void> failed = element->SetValue(
        fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, 20.0));
    CHECK(!failed);
    CHECK(failed.GetStatus().code == ErrorCode::OutOfMemory);
    CHECK(element->StoredValueCount() == 0U);
    Result<PropertyValue> value = element->GetValue(fixture.width);
    CHECK(value && value.Value().AsDouble() == 10.0);

    fixture.allocator.DisableFailures();
    CHECK(element->SetValue(
        fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, 20.0)));
    CHECK(element->StoredValueCount() == 1U);
    CHECK(element->ClearValue(fixture.width));
    CHECK(element->StoredValueCount() == 0U);
    return true;
}

bool TestRegistrationIsTransactionalOnOom() {
    bool observedFailure = false;
    bool observedSuccess = false;
    for (std::uint32_t allowance = 0U; allowance < 24U; ++allowance) {
        TrackingAllocator allocator;
        Aero::Tests::ScopedDefaultAllocator allocatorScope(allocator);
        TypeRegistry types;
        DependencyPropertyRegistry properties(types);
        const StringView ns("urn:transaction");
        const TypeId owner = MakeTypeId(ns, StringView("Owner"));
        const TypeId valueType = MakeTypeId(ns, StringView("Double"));
        CHECK(types.TryRegisterType({
            ns, StringView("Owner"), InvalidTypeId, TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({
            ns, StringView("Double"), InvalidTypeId,
            TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));

        DependencyPropertyRegistration registration;
        registration.name = StringView("Value");
        registration.ownerType = owner;
        registration.valueType = valueType;
        registration.metadata.defaultValue =
            PropertyValue::FromDouble(valueType, 0.0);
        allocator.FailAfter(allowance);
        Result<DependencyPropertyRegistrationResult> result =
            properties.TryRegister(registration);
        allocator.DisableFailures();

        if (!result) {
            observedFailure = true;
            CHECK(properties.PropertyCount() == 0U);
            CHECK(types.FindProperty(owner, StringView("Value"), false) == nullptr);
        } else {
            observedSuccess = true;
            CHECK(properties.PropertyCount() == 1U);
            CHECK(types.FindProperty(owner, StringView("Value"), false) != nullptr);
        }
    }
    CHECK(observedFailure && observedSuccess);
    return true;
}

bool TestRegistrationErrors() {
    TrackingAllocator allocator;
    Aero::Tests::ScopedDefaultAllocator allocatorScope(allocator);
    TypeRegistry types;
    DependencyPropertyRegistry properties(types);
    const StringView ns("urn:test");

    const TypeId object = MakeTypeId(ns, StringView("Object"));
    const TypeId number = MakeTypeId(ns, StringView("Number"));
    const TypeId owner = MakeTypeId(ns, StringView("Owner"));
    const TypeId unrelated = MakeTypeId(ns, StringView("Unrelated"));

    CHECK(types.TryRegisterType({
        ns, StringView("Object"), InvalidTypeId,
        TypeFlags::None, nullptr}));
    CHECK(types.TryRegisterType({
        ns, StringView("Number"), InvalidTypeId,
        TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
    CHECK(types.TryRegisterType({
        ns, StringView("Owner"), object,
        TypeFlags::None, nullptr}));
    CHECK(types.TryRegisterType({
        ns, StringView("Unrelated"), object,
        TypeFlags::None, nullptr}));

    Result<void> prematureFreeze = properties.Freeze();
    CHECK(!prematureFreeze);
    CHECK(prematureFreeze.GetStatus().code == ErrorCode::InvalidState);

    DependencyPropertyRegistration incomplete;
    incomplete.name = StringView("Incomplete");
    incomplete.ownerType = owner;
    incomplete.valueType = number;
    Result<DependencyPropertyRegistrationResult> incompleteResult =
        properties.TryRegister(incomplete);
    CHECK(!incompleteResult);
    CHECK(incompleteResult.GetStatus().code == ErrorCode::InvalidArgument);

    DependencyPropertyRegistration registration;
    registration.name = StringView("Value");
    registration.ownerType = owner;
    registration.valueType = number;
    registration.metadata.defaultValue = PropertyValue::FromDouble(number, 1.0);
    Result<DependencyPropertyRegistrationResult> registered =
        properties.TryRegister(registration);
    CHECK(registered);

    Result<DependencyPropertyRegistrationResult> duplicate =
        properties.TryRegister(registration);
    CHECK(!duplicate);
    CHECK(duplicate.GetStatus().code == ErrorCode::AlreadyExists);

    Result<void> duplicateOwner = properties.TryAddOwner(
        registered.Value().property,
        owner,
        registration.metadata);
    CHECK(!duplicateOwner);
    CHECK(duplicateOwner.GetStatus().code == ErrorCode::AlreadyExists);

    Result<void> unrelatedOverride = properties.TryOverrideMetadata(
        registered.Value().property,
        unrelated,
        registration.metadata);
    CHECK(!unrelatedOverride);
    CHECK(unrelatedOverride.GetStatus().code == ErrorCode::NotFound);

    DependencyPropertyRegistration wrongDefault = registration;
    wrongDefault.name = StringView("WrongDefault");
    wrongDefault.metadata.defaultValue = PropertyValue::NullObject(object);
    Result<DependencyPropertyRegistrationResult> wrongDefaultResult =
        properties.TryRegister(wrongDefault);
    CHECK(!wrongDefaultResult);
    CHECK(wrongDefaultResult.GetStatus().code == ErrorCode::InvalidArgument);

    CHECK(types.Freeze());
    CHECK(properties.Freeze());
    return true;
}

bool TestCommittedValueSurvivesInvalidationFailure() {
    Fixture fixture;
    CHECK(fixture.Build());
    Dispatcher dispatcher;
    PresentationContextScope presentation(dispatcher, fixture.properties);
    TestElement element(fixture.uiElement);
    element.RejectInvalidation(true);
    Result<void> set = element.SetValue(fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, 37.0));
    CHECK(!set && set.GetStatus().code == ErrorCode::InvalidState);
    Result<PropertyValue> committed = element.GetValue(fixture.width);
    CHECK(committed && committed.Value().AsDouble() == 37.0);
    element.RejectInvalidation(false);
    CHECK(element.ClearValue(fixture.width));
    CHECK(element.GetValue(fixture.width).Value().AsDouble() == 0.0);
    return true;
}

struct TestCase final {
    const char* name;
    bool (*run)();
};

} // namespace

int main() {
    const TestCase tests[] = {
        {"registration and metadata", &TestRegistrationAndMetadata},
        {"effective values and invalidation", &TestEffectiveValuesAndInvalidation},
        {"read-only and inheritance", &TestReadOnlyAndInheritance},
        {"validation, coercion, and reentrancy", &TestValidationCoercionAndReentrancy},
        {"nested change notifications", &TestNestedChangeNotifications},
        {"wrong thread", &TestWrongThread},
        {"OOM and sparse storage", &TestOomAndSparseStorage},
        {"transactional registration OOM", &TestRegistrationIsTransactionalOnOom},
        {"registration errors", &TestRegistrationErrors},
        {"committed value survives invalidation failure",
            &TestCommittedValueSurvivesInvalidationFailure},
    };

    for (const TestCase& test : tests) {
        if (!test.run()) {
            std::printf("[FAIL] %s\n", test.name);
            return 1;
        }
        std::printf("[PASS] %s\n", test.name);
    }
    return 0;
}
