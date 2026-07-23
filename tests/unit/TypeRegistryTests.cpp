#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Core/TypeRegistry.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

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

struct CommonIds final {
    TypeId object = InvalidTypeId;
    TypeId number = InvalidTypeId;
    TypeId eventArgs = InvalidTypeId;
    TypeId uiElement = InvalidTypeId;
    TypeId button = InvalidTypeId;
    MemberId width = InvalidMemberId;
    MemberId loaded = InvalidMemberId;
};

bool PopulateRegistry(
    TypeRegistry& registry,
    bool reverseOrder,
    CommonIds& ids) {
    const StringView ns("urn:aero");
    ids.object = MakeTypeId(ns, StringView("Object"));
    ids.number = MakeTypeId(ns, StringView("Double"));
    ids.eventArgs = MakeTypeId(ns, StringView("EventArgs"));
    ids.uiElement = MakeTypeId(ns, StringView("UIElement"));
    ids.button = MakeTypeId(ns, StringView("Button"));

    if (!reverseOrder) {
        CHECK(registry.TryRegisterType({
            ns, StringView("Object"), InvalidTypeId,
            TypeFlags::None, nullptr}));
        CHECK(registry.TryRegisterType({
            ns, StringView("Double"), InvalidTypeId,
            TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
        CHECK(registry.TryRegisterType({
            ns, StringView("EventArgs"), ids.object,
            TypeFlags::None, nullptr}));
        CHECK(registry.TryRegisterType({
            ns, StringView("UIElement"), ids.object,
            TypeFlags::None, nullptr}));
        CHECK(registry.TryRegisterType({
            ns, StringView("Button"), ids.uiElement,
            TypeFlags::Sealed, nullptr}));
    } else {
        CHECK(registry.TryRegisterType({
            ns, StringView("Double"), InvalidTypeId,
            TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
        CHECK(registry.TryRegisterType({
            ns, StringView("EventArgs"), ids.object,
            TypeFlags::None, nullptr}));
        CHECK(registry.TryRegisterType({
            ns, StringView("Button"), ids.uiElement,
            TypeFlags::Sealed, nullptr}));
        CHECK(registry.TryRegisterType({
            ns, StringView("UIElement"), ids.object,
            TypeFlags::None, nullptr}));
        CHECK(registry.TryRegisterType({
            ns, StringView("Object"), InvalidTypeId,
            TypeFlags::None, nullptr}));
    }

    if (!reverseOrder) {
        Result<MemberId> width = registry.TryRegisterProperty(
            ids.uiElement,
            {StringView("Width"), ids.number,
             PropertyFlags::AffectsMeasure | PropertyFlags::AffectsArrange});
        CHECK(width);
        ids.width = width.Value();

        Result<MemberId> loaded = registry.TryRegisterEvent(
            ids.uiElement,
            {StringView("Loaded"), ids.eventArgs,
             EventFlags::Routed});
        CHECK(loaded);
        ids.loaded = loaded.Value();
    } else {
        Result<MemberId> loaded = registry.TryRegisterEvent(
            ids.uiElement,
            {StringView("Loaded"), ids.eventArgs,
             EventFlags::Routed});
        CHECK(loaded);
        ids.loaded = loaded.Value();

        Result<MemberId> width = registry.TryRegisterProperty(
            ids.uiElement,
            {StringView("Width"), ids.number,
             PropertyFlags::AffectsMeasure | PropertyFlags::AffectsArrange});
        CHECK(width);
        ids.width = width.Value();
    }
    return true;
}

bool TestStableIds() {
    const StringView ns("urn:aero");
    const TypeId object = MakeTypeId(ns, StringView("Object"));
    const TypeId uiElement = MakeTypeId(ns, StringView("UIElement"));

    CHECK(object == UINT64_C(0x6563B5703AEB39E0));
    CHECK(uiElement == UINT64_C(0x0376178515911073));
    CHECK(MakeTypeId(ns, StringView("Object")) == object);
    CHECK(MakeTypeId(StringView("urn:other"), StringView("Object")) != object);
    CHECK(MakeTypeId(ns, StringView("object")) != object);

    CHECK(MakeMemberId(
        uiElement, MemberKind::Property, StringView("Width")) ==
        UINT64_C(0x3CC42934E991D0BE));
    CHECK(MakeMemberId(
        uiElement, MemberKind::Event, StringView("Loaded")) ==
        UINT64_C(0x69EAE11AEB0F97AA));
    CHECK(MakeMemberId(
        uiElement, MemberKind::Property, StringView("Loaded")) !=
        MakeMemberId(
            uiElement, MemberKind::Event, StringView("Loaded")));
    const TypeId firstSignature[] = {object};
    const TypeId secondSignature[] = {uiElement};
    CHECK(MakeMethodId(uiElement, StringView("Find"),
        {firstSignature, 1U}) ==
        MakeMethodId(uiElement, StringView("Find"),
            {firstSignature, 1U}));
    CHECK(MakeMethodId(uiElement, StringView("Find"),
        {firstSignature, 1U}) !=
        MakeMethodId(uiElement, StringView("Find"),
            {secondSignature, 1U}));
    return true;
}

bool TestRegistrationLookupAndFreeze() {
    TypeRegistry registry;
    CommonIds ids;
    CHECK(PopulateRegistry(registry, false, ids));
    CHECK(registry.TypeCount() == 5U);
    CHECK(!registry.IsFrozen());

    const TypeInfo* uiElement = registry.FindType(
        StringView("urn:aero"), StringView("UIElement"));
    CHECK(uiElement != nullptr);
    CHECK(uiElement->Id() == ids.uiElement);
    CHECK(uiElement->BaseType() == ids.object);
    CHECK(uiElement->Properties().Size() == 1U);
    CHECK(uiElement->Events().Size() == 1U);

    const PropertyInfo* width = registry.FindProperty(ids.width);
    CHECK(width != nullptr);
    CHECK(width->ValueType() == ids.number);
    CHECK(width->Name() == StringView("Width"));

    const EventInfo* loaded = registry.FindEvent(ids.loaded);
    CHECK(loaded != nullptr);
    CHECK(loaded->EventArgsType() == ids.eventArgs);
    CHECK(loaded->Name() == StringView("Loaded"));

    CHECK(registry.FindProperty(
        ids.button, StringView("Width"), true) == width);
    CHECK(registry.FindProperty(
        ids.button, StringView("Width"), false) == nullptr);
    CHECK(registry.FindEvent(
        ids.button, StringView("Loaded"), true) == loaded);

    CHECK(registry.IsDerivedFrom(ids.button, ids.uiElement));
    CHECK(registry.IsDerivedFrom(ids.button, ids.object));
    CHECK(registry.IsDerivedFrom(ids.object, ids.object));
    CHECK(!registry.IsDerivedFrom(ids.object, ids.button));

    Result<TypeId> duplicate = registry.TryRegisterType({
        StringView("urn:aero"), StringView("Object"),
        InvalidTypeId, TypeFlags::None, nullptr});
    CHECK(!duplicate);
    CHECK(duplicate.GetStatus().code == ErrorCode::AlreadyExists);

    Result<MemberId> duplicateProperty = registry.TryRegisterProperty(
        ids.uiElement,
        {StringView("Width"), ids.number, PropertyFlags::None});
    CHECK(!duplicateProperty);
    CHECK(duplicateProperty.GetStatus().code == ErrorCode::AlreadyExists);

    CHECK(registry.Freeze());
    CHECK(registry.IsFrozen());
    CHECK(registry.Freeze());

    Result<TypeId> afterFreeze = registry.TryRegisterType({
        StringView("urn:aero"), StringView("LateType"),
        ids.object, TypeFlags::None, nullptr});
    CHECK(!afterFreeze);
    CHECK(afterFreeze.GetStatus().code == ErrorCode::InvalidState);
    return true;
}

bool TestFreezeValidation() {
    const StringView ns("urn:validation");

    {
        TypeRegistry registry;
        const TypeId missingBase = MakeTypeId(ns, StringView("Missing"));
        CHECK(registry.TryRegisterType({
            ns, StringView("Child"), missingBase,
            TypeFlags::None, nullptr}));
        Result<void> frozen = registry.Freeze();
        CHECK(!frozen);
        CHECK(frozen.GetStatus().code == ErrorCode::NotFound);
    }

    {
        TypeRegistry registry;
        Result<TypeId> owner = registry.TryRegisterType({
            ns, StringView("Owner"), InvalidTypeId,
            TypeFlags::None, nullptr});
        CHECK(owner);
        const TypeId missingValue = MakeTypeId(ns, StringView("Value"));
        CHECK(registry.TryRegisterProperty(
            owner.Value(),
            {StringView("Value"), missingValue, PropertyFlags::None}));
        Result<void> frozen = registry.Freeze();
        CHECK(!frozen);
        CHECK(frozen.GetStatus().code == ErrorCode::NotFound);
    }

    {
        TypeRegistry registry;
        const TypeId typeA = MakeTypeId(ns, StringView("A"));
        const TypeId typeB = MakeTypeId(ns, StringView("B"));
        CHECK(registry.TryRegisterType({
            ns, StringView("A"), typeB,
            TypeFlags::None, nullptr}));
        CHECK(registry.TryRegisterType({
            ns, StringView("B"), typeA,
            TypeFlags::None, nullptr}));
        Result<void> frozen = registry.Freeze();
        CHECK(!frozen);
        CHECK(frozen.GetStatus().code == ErrorCode::CycleDetected);
    }

    {
        TypeRegistry registry;
        Result<MemberId> missingOwner = registry.TryRegisterEvent(
            MakeTypeId(ns, StringView("Unknown")),
            {StringView("Changed"), MakeTypeId(ns, StringView("Args")),
             EventFlags::None});
        CHECK(!missingOwner);
        CHECK(missingOwner.GetStatus().code == ErrorCode::NotFound);
    }
    return true;
}

bool TestDeterministicSnapshot() {
    TypeRegistry first;
    TypeRegistry second;
    CommonIds firstIds;
    CommonIds secondIds;

    CHECK(PopulateRegistry(first, false, firstIds));
    CHECK(PopulateRegistry(second, true, secondIds));

    String beforeFreeze;
    Result<void> premature = first.BuildSnapshot(beforeFreeze);
    CHECK(!premature);
    CHECK(premature.GetStatus().code == ErrorCode::InvalidState);

    CHECK(first.Freeze());
    CHECK(second.Freeze());

    String firstSnapshot;
    String secondSnapshot;
    CHECK(first.BuildSnapshot(firstSnapshot));
    CHECK(second.BuildSnapshot(secondSnapshot));
    CHECK(firstSnapshot.View() == secondSnapshot.View());
    CHECK(std::strstr(
        firstSnapshot.CStr(), "AERO-TYPE-REGISTRY|2") != nullptr);
    CHECK(std::strstr(firstSnapshot.CStr(), "UIElement") != nullptr);
    CHECK(std::strstr(firstSnapshot.CStr(), "Width") != nullptr);
    CHECK(std::strstr(firstSnapshot.CStr(), "Loaded") != nullptr);

    Result<HashCode> firstHash = first.ComputeSnapshotHash();
    Result<HashCode> secondHash = second.ComputeSnapshotHash();
    CHECK(firstHash);
    CHECK(secondHash);
    CHECK(firstHash.Value() == secondHash.Value());
    CHECK(firstHash.Value() != 0U);
    return true;
}

bool TestRegistrationRollbackOnOom() {
    TrackingAllocator allocator;
    {
        TypeRegistry registry(&allocator);
        const TypeId expected = MakeTypeId(
            StringView("urn:oom"), StringView("Object"));

        allocator.FailAfter(1U);
        Result<TypeId> failed = registry.TryRegisterType({
            StringView("urn:oom"), StringView("Object"),
            InvalidTypeId, TypeFlags::None, nullptr});
        CHECK(!failed);
        CHECK(failed.GetStatus().code == ErrorCode::OutOfMemory);
        CHECK(registry.TypeCount() == 0U);
        CHECK(registry.FindType(expected) == nullptr);

        allocator.DisableFailures();
        Result<TypeId> registered = registry.TryRegisterType({
            StringView("urn:oom"), StringView("Object"),
            InvalidTypeId, TypeFlags::None, nullptr});
        CHECK(registered);
        CHECK(registered.Value() == expected);
        CHECK(registry.TypeCount() == 1U);
        CHECK(registry.Freeze());
    }
    CHECK(allocator.Active() == 0U);
    CHECK(allocator.Total() >= 2U);
    return true;
}

struct SmallValue final {
    std::uint64_t first = 0U;
    std::uint64_t second = 0U;
};

struct ManagedValue final {
    int* active = nullptr;
    int payload = 0;

    ManagedValue(int& count, int value) noexcept : active(&count), payload(value) {
        ++*active;
    }
    ManagedValue(const ManagedValue& other) noexcept
        : active(other.active), payload(other.payload) {
        ++*active;
    }
    ~ManagedValue() { --*active; }
};

struct LargeValue final {
    std::uint8_t bytes[40]{};
};

class ValueProbe final : public Object {};

bool EqualSmall(const void* left, const void* right, void*) noexcept {
    const auto& a = *static_cast<const SmallValue*>(left);
    const auto& b = *static_cast<const SmallValue*>(right);
    return a.first == b.first && a.second == b.second;
}

Result<void> CopyManaged(void* destination, const void* source, void*) noexcept {
    new (destination) ManagedValue(*static_cast<const ManagedValue*>(source));
    return {};
}

void DestroyManaged(void* value, void*) noexcept {
    static_cast<ManagedValue*>(value)->~ManagedValue();
}

bool EqualManaged(const void* left, const void* right, void*) noexcept {
    return static_cast<const ManagedValue*>(left)->payload ==
        static_cast<const ManagedValue*>(right)->payload;
}

Result<void> CopyLarge(void* destination, const void* source, void*) noexcept {
    std::memcpy(destination, source, sizeof(LargeValue));
    return {};
}

bool EqualLarge(const void* left, const void* right, void*) noexcept {
    return std::memcmp(left, right, sizeof(LargeValue)) == 0;
}

Result<Value> ConvertSmall(TypeId type, StringView text,
    IAllocator&, void* context) noexcept {
    auto* registry = static_cast<TypeRegistry*>(context);
    if (text != StringView("7,9")) {
        return Status::Failure(ErrorCode::InvalidArgument, "Invalid SmallValue");
    }
    const SmallValue value{7U, 9U};
    return registry->TryCreateValue(type, &value);
}

bool TestUnifiedValueAndRegistrySemantics() {
    static_assert(noexcept(Value(std::declval<const Value&>())),
        "Value copies must remain noexcept");
    static_assert(noexcept(Value(std::declval<Value&&>())),
        "Value moves must remain noexcept");

    TrackingAllocator allocator;
    TypeRegistry registry(&allocator);
    const StringView ns("urn:value-tests");
    const TypeId smallType = MakeTypeId(ns, StringView("Small"));
    const TypeId managedType = MakeTypeId(ns, StringView("Managed"));
    const TypeId largeType = MakeTypeId(ns, StringView("Large"));
    const TypeId objectType = MakeTypeId(ns, StringView("Object"));
    CHECK(registry.TryRegisterType({ns, StringView("Small"), InvalidTypeId,
        TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
    CHECK(registry.TryRegisterType({ns, StringView("Managed"), InvalidTypeId,
        TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
    CHECK(registry.TryRegisterType({ns, StringView("Large"), InvalidTypeId,
        TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
    CHECK(registry.TryRegisterType({ns, StringView("Object"), InvalidTypeId,
        TypeFlags::None, nullptr}));
    CHECK(registry.TryRegisterValueSemantics(smallType,
        {sizeof(SmallValue), alignof(SmallValue), nullptr, nullptr,
         &EqualSmall, nullptr, true}));
    CHECK(registry.TryRegisterValueSemantics(managedType,
        {sizeof(ManagedValue), alignof(ManagedValue), &CopyManaged,
         &DestroyManaged, &EqualManaged, nullptr, false}));
    CHECK(registry.TryRegisterValueSemantics(largeType,
        {sizeof(LargeValue), alignof(LargeValue), &CopyLarge, nullptr,
         &EqualLarge, nullptr, false}));
    Result<void> duplicate = registry.TryRegisterValueSemantics(smallType,
        {sizeof(SmallValue), alignof(SmallValue), nullptr, nullptr,
         &EqualSmall, nullptr, true});
    CHECK(!duplicate && duplicate.GetStatus().code == ErrorCode::AlreadyExists);
    Result<void> invalid = registry.TryRegisterValueSemantics(managedType,
        {sizeof(ManagedValue), 3U, &CopyManaged, &DestroyManaged,
         &EqualManaged, nullptr, false});
    CHECK(!invalid && invalid.GetStatus().code == ErrorCode::InvalidArgument);
    CHECK(registry.TryRegisterTextConverter({smallType, &ConvertSmall, &registry}));
    Result<void> duplicateConverter = registry.TryRegisterTextConverter(
        {smallType, &ConvertSmall, &registry});
    CHECK(!duplicateConverter &&
        duplicateConverter.GetStatus().code == ErrorCode::AlreadyExists);

    const SmallValue first{7U, 9U};
    const SmallValue second{7U, 10U};
    Result<Value> inlineValue = registry.TryCreateValue(smallType, &first);
    Result<Value> differentValue = registry.TryCreateValue(smallType, &second);
    CHECK(inlineValue && inlineValue.Value().IsInlineCustom());
    CHECK(differentValue && inlineValue.Value() != differentValue.Value());
    Value inlineCopy = inlineValue.Value();
    Value inlineMove = std::move(inlineCopy);
    CHECK(inlineMove == inlineValue.Value());
    Result<Value> converted = registry.TryConvertText(smallType, StringView("7,9"));
    CHECK(converted && converted.Value() == inlineValue.Value());
    Result<Value> failedConversion = registry.TryConvertText(
        smallType, StringView("bad"));
    CHECK(!failedConversion &&
        failedConversion.GetStatus().code == ErrorCode::InvalidArgument);

    Result<Value> empty = Value::TryFromString(smallType, StringView(), &allocator);
    Result<Value> text = Value::TryFromString(smallType, StringView("aero"), &allocator);
    Result<Value> sameText = Value::TryFromString(smallType, StringView("aero"), &allocator);
    CHECK(empty && empty.Value().AsString().Empty());
    CHECK(text && sameText && text.Value() == sameText.Value());

    CHECK(Value::Unset().IsUnset());
    CHECK(Value::FromBoolean(smallType, true).AsBoolean());
    CHECK(Value::FromSignedInteger(smallType, -8).AsSignedInteger() == -8);
    CHECK(Value::FromUnsignedInteger(smallType, 8U).AsUnsignedInteger() == 8U);
    CHECK(Value::FromDouble(smallType, 1.25).AsDouble() == 1.25);
    Result<Ref<ValueProbe>> firstObject = MakeRef<ValueProbe>();
    Result<Ref<ValueProbe>> secondObject = MakeRef<ValueProbe>();
    CHECK(firstObject && secondObject);
    Ref<Object> firstRef(firstObject.Value());
    Ref<Object> sameRef(firstObject.Value());
    Ref<Object> secondRef(secondObject.Value());
    const Value object = Value::FromObject(objectType, firstRef);
    CHECK(object == Value::FromObject(objectType, sameRef));
    CHECK(object != Value::FromObject(objectType, secondRef));
    CHECK(Value::NullObject(objectType).IsNullObject());

    LargeValue large{};
    large.bytes[39] = 91U;
    Result<Value> boxedLarge = registry.TryCreateValue(largeType, &large);
    CHECK(boxedLarge && !boxedLarge.Value().IsInlineCustom());
    Value largeCopy = boxedLarge.Value();
    CHECK(largeCopy == boxedLarge.Value());

    int activeManaged = 0;
    {
        ManagedValue source(activeManaged, 42);
        CHECK(activeManaged == 1);
        Result<Value> boxed = registry.TryCreateValue(managedType, &source);
        CHECK(boxed && !boxed.Value().IsInlineCustom() && activeManaged == 2);
        {
            Value copy = boxed.Value();
            Value moved = std::move(copy);
            CHECK(moved == boxed.Value() && activeManaged == 2);
        }
        CHECK(activeManaged == 2);
    }
    CHECK(activeManaged == 0);

    allocator.FailAfter(0U);
    Result<Value> stringOom = Value::TryFromString(
        smallType, StringView("allocation"), &allocator);
    CHECK(!stringOom && stringOom.GetStatus().code == ErrorCode::OutOfMemory);
    int failedActive = 0;
    ManagedValue failedSource(failedActive, 5);
    Result<Value> boxedOom = registry.TryCreateValue(
        managedType, &failedSource, &allocator);
    CHECK(!boxedOom && boxedOom.GetStatus().code == ErrorCode::OutOfMemory);
    allocator.DisableFailures();

    CHECK(registry.Freeze());
    Result<void> late = registry.TryRegisterTextConverter(
        {managedType, &ConvertSmall, &registry});
    CHECK(!late && late.GetStatus().code == ErrorCode::InvalidState);
    return true;
}

struct TestCase final {
    const char* name;
    bool (*run)();
};

} // namespace

int main() {
    const TestCase tests[] = {
        {"Stable metadata IDs", &TestStableIds},
        {"Registration, lookup, and freeze", &TestRegistrationLookupAndFreeze},
        {"Freeze validation", &TestFreezeValidation},
        {"Deterministic snapshot", &TestDeterministicSnapshot},
        {"Registration rollback on OOM", &TestRegistrationRollbackOnOom},
        {"Unified Value and registry semantics", &TestUnifiedValueAndRegistrySemantics},
    };

    std::uint32_t passed = 0U;
    for (const TestCase& test : tests) {
        const bool ok = test.run();
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", test.name);
        if (!ok) {
            return 1;
        }
        ++passed;
    }
    std::printf("%u tests passed\n", passed);
    return 0;
}
