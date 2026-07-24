#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Core/MetadataBehaviorRegistrationStore.hpp>
#include <Aero/Core/MetadataDescriptors.hpp>
#include <Aero/Core/MetadataRegistrationValues.hpp>
#include <Aero/Core/ObjectTree.hpp>
#include <Aero/Core/TypeRegistry.hpp>
#include "TestAllocatorScope.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
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
        if (failAfter_ == 0U) return nullptr;
        if (failAfter_ != UINT32_MAX) --failAfter_;
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
            if (active_ == 0U) std::abort();
            --active_;
        }
        upstream_.Deallocate(memory, size, alignment, tag);
    }

    void FailAfter(std::uint32_t successfulAllocations) noexcept {
        failAfter_ = successfulAllocations;
    }
    void DisableFailures() noexcept { failAfter_ = UINT32_MAX; }
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

Result<Ref<Object>> SnapshotFactoryOne() noexcept {
    return Status::Failure(
        ErrorCode::Unsupported,
        "Snapshot test factory one is not executable");
}

Result<Ref<Object>> SnapshotFactoryTwo() noexcept {
    return Status::Failure(
        ErrorCode::Unsupported,
        "Snapshot test factory two is not executable");
}

bool PopulateRegistry(
    MetadataRegistrationTypes registration,
    bool reverseOrder,
    CommonIds& ids) {
    const StringView ns("urn:aero");
    ids.object = MakeTypeId(ns, "Object");
    ids.number = MakeTypeId(ns, "Double");
    ids.eventArgs = MakeTypeId(ns, "EventArgs");
    ids.uiElement = MakeTypeId(ns, "UIElement");
    ids.button = MakeTypeId(ns, "Button");

    const TypeRegistration object =
        TypeRegistration::Object(ns, "Object");
    const TypeRegistration number =
        TypeRegistration::Primitive(ns, "Double");
    const TypeRegistration eventArgs =
        TypeRegistration::Object(ns, "EventArgs", ids.object);
    const TypeRegistration uiElement =
        TypeRegistration::Object(ns, "UIElement", ids.object);
    const TypeRegistration button =
        TypeRegistration::Object(
            ns, "Button", ids.uiElement, TypeFlags::Sealed);

    if (!reverseOrder) {
        CHECK(registration.TryRegisterType(object));
        CHECK(registration.TryRegisterType(number));
        CHECK(registration.TryRegisterType(eventArgs));
        CHECK(registration.TryRegisterType(uiElement));
        CHECK(registration.TryRegisterType(button));
    } else {
        CHECK(registration.TryRegisterType(number));
        CHECK(registration.TryRegisterType(eventArgs));
        CHECK(registration.TryRegisterType(button));
        CHECK(registration.TryRegisterType(uiElement));
        CHECK(registration.TryRegisterType(object));
    }

    if (!reverseOrder) {
        Result<MemberId> width = registration.TryRegisterProperty(
            ids.uiElement,
            {"Width", ids.number,
             PropertyFlags::AffectsMeasure | PropertyFlags::AffectsArrange});
        CHECK(width);
        ids.width = width.Value();
        Result<MemberId> loaded = registration.TryRegisterEvent(
            ids.uiElement,
            {"Loaded", ids.eventArgs, EventFlags::Routed});
        CHECK(loaded);
        ids.loaded = loaded.Value();
    } else {
        Result<MemberId> loaded = registration.TryRegisterEvent(
            ids.uiElement,
            {"Loaded", ids.eventArgs, EventFlags::Routed});
        CHECK(loaded);
        ids.loaded = loaded.Value();
        Result<MemberId> width = registration.TryRegisterProperty(
            ids.uiElement,
            {"Width", ids.number,
             PropertyFlags::AffectsMeasure | PropertyFlags::AffectsArrange});
        CHECK(width);
        ids.width = width.Value();
    }
    return true;
}

bool TestStableIds() {
    const StringView ns("urn:aero");
    const TypeId object = MakeTypeId(ns, "Object");
    const TypeId uiElement = MakeTypeId(ns, "UIElement");
    CHECK(object == UINT64_C(0x6563B5703AEB39E0));
    CHECK(uiElement == UINT64_C(0x0376178515911073));
    CHECK(AeroNamespaceUri() == ns);
    CHECK(MakeTypeId("Object") == object);
    CHECK(Aero::Base::Object::StaticTypeId() == object);
    CHECK(EventArgs::StaticTypeId() == MakeTypeId("EventArgs"));
    CHECK(MakeTypeId("urn:other", "Object") != object);
    CHECK(MakeTypeId(ns, "object") != object);
    CHECK(MakeMemberId(uiElement, MemberKind::Property, "Width") ==
        UINT64_C(0x3CC42934E991D0BE));
    CHECK(MakeMemberId(uiElement, MemberKind::Event, "Loaded") ==
        UINT64_C(0x69EAE11AEB0F97AA));
    CHECK(MakeMemberId(uiElement, MemberKind::Field, "Width") !=
        MakeMemberId(uiElement, MemberKind::Property, "Width"));
    return true;
}

bool TestRegistrationLookupAndFreeze() {
    TypeRegistry registry;
    MetadataBehaviorRegistrationStore behaviors{registry};
    MetadataRegistrationTypes registration{registry, behaviors};
    CommonIds ids;
    CHECK(PopulateRegistry(registration, false, ids));
    CHECK(registry.TypeCount() == 5U);
    const TypeInfo* uiElement = registry.FindType("urn:aero", "UIElement");
    CHECK(uiElement != nullptr);
    CHECK(uiElement->BaseType() == ids.object);
    CHECK(uiElement->Properties().Size() == 1U);
    CHECK(uiElement->Events().Size() == 1U);
    CHECK(registry.FindProperty(ids.button, "Width", true) != nullptr);
    CHECK(registry.FindProperty(ids.button, "Width", false) == nullptr);
    CHECK(registry.IsDerivedFrom(ids.button, ids.object));
    CHECK(!registry.IsDerivedFrom(ids.object, ids.button));

    Result<TypeId> duplicate = registration.TryRegisterType(TypeRegistration::Object("urn:aero", "Object"));
    CHECK(!duplicate);
    CHECK(duplicate.GetStatus().code == ErrorCode::AlreadyExists);
    CHECK(registry.Freeze());
    CHECK(registry.Freeze());
    Result<TypeId> late = registration.TryRegisterType(TypeRegistration::Object("urn:aero", "Late", ids.object));
    CHECK(!late && late.GetStatus().code == ErrorCode::InvalidState);
    return true;
}

bool TestFreezeValidation() {
    const StringView ns("urn:validation");
    {
        TypeRegistry registry;
        MetadataBehaviorRegistrationStore behaviors{registry};
        MetadataRegistrationTypes registration{registry, behaviors};
        CHECK(registration.TryRegisterType(TypeRegistration::Object(ns, "Child", MakeTypeId(ns, "Missing"))));
        Result<void> frozen = registry.Freeze();
        CHECK(!frozen && frozen.GetStatus().code == ErrorCode::NotFound);
    }
    {
        TypeRegistry registry;
        MetadataBehaviorRegistrationStore behaviors{registry};
        MetadataRegistrationTypes registration{registry, behaviors};
        const TypeId typeA = MakeTypeId(ns, "A");
        const TypeId typeB = MakeTypeId(ns, "B");
        CHECK(registration.TryRegisterType(TypeRegistration::Object(ns, "A", typeB)));
        CHECK(registration.TryRegisterType(TypeRegistration::Object(ns, "B", typeA)));
        Result<void> frozen = registry.Freeze();
        CHECK(!frozen && frozen.GetStatus().code == ErrorCode::CycleDetected);
    }
    return true;
}

bool TestDeterministicSnapshot() {
    TypeRegistry first;
    MetadataBehaviorRegistrationStore firstBehaviors{first};
    MetadataRegistrationTypes firstRegistration{first, firstBehaviors};
    TypeRegistry second;
    MetadataBehaviorRegistrationStore secondBehaviors{second};
    MetadataRegistrationTypes secondRegistration{second, secondBehaviors};
    CommonIds firstIds;
    CommonIds secondIds;
    CHECK(PopulateRegistry(firstRegistration, false, firstIds));
    CHECK(PopulateRegistry(secondRegistration, true, secondIds));
    CHECK(first.Freeze());
    CHECK(second.Freeze());
    String firstSnapshot;
    String secondSnapshot;
    CHECK(first.BuildSnapshot(firstSnapshot));
    CHECK(second.BuildSnapshot(secondSnapshot));
    CHECK(firstSnapshot.View() == secondSnapshot.View());
    CHECK(std::strstr(firstSnapshot.CStr(),
        "AERO-TYPE-REGISTRY|4") != nullptr);
    CHECK(std::strstr(firstSnapshot.CStr(), "UIElement") != nullptr);
    Result<HashCode> firstHash = first.ComputeSnapshotHash();
    Result<HashCode> secondHash = second.ComputeSnapshotHash();
    CHECK(firstHash && secondHash);
    CHECK(firstHash.Value() == secondHash.Value());

    TypeRegistry firstFactory;
    MetadataBehaviorRegistrationStore firstFactoryBehaviors{firstFactory};
    MetadataRegistrationTypes firstFactoryRegistration{
        firstFactory, firstFactoryBehaviors};
    TypeRegistry secondFactory;
    MetadataBehaviorRegistrationStore secondFactoryBehaviors{secondFactory};
    MetadataRegistrationTypes secondFactoryRegistration{
        secondFactory, secondFactoryBehaviors};
    CHECK(firstFactoryRegistration.TryRegisterType(TypeRegistration::Object("urn:snapshot", "Object", InvalidTypeId, TypeFlags::None, &SnapshotFactoryOne)));
    CHECK(secondFactoryRegistration.TryRegisterType(TypeRegistration::Object("urn:snapshot", "Object", InvalidTypeId, TypeFlags::None, &SnapshotFactoryTwo)));
    CHECK(firstFactory.Freeze());
    CHECK(secondFactory.Freeze());
    String one;
    String two;
    CHECK(firstFactory.BuildSnapshot(one));
    CHECK(secondFactory.BuildSnapshot(two));
    CHECK(one.View() == two.View());
    return true;
}

Result<Value> GetStructCount(
    const void* object,
    MetadataRuntime&,
    void*) noexcept {
    return Value::FromUnsignedInteger(
        MakeTypeId("urn:meta", "UInt32"),
        *static_cast<const std::uint32_t*>(object));
}

Result<void> SetStructCount(
    void* object,
    const Value& value,
    MetadataRuntime&,
    void*) noexcept {
    *static_cast<std::uint32_t*>(object) =
        static_cast<std::uint32_t>(value.AsUnsignedInteger());
    return {};
}

bool TestInterfacesEnumsAndStructs() {
    const StringView ns("urn:meta");
    TypeRegistry registry;
    MetadataBehaviorRegistrationStore behaviors{registry};
    MetadataRegistrationTypes registration{registry, behaviors};
    const TypeId uintType = MakeTypeId(ns, "UInt32");
    const TypeId interfaceType = MakeTypeId(ns, "ICommandSource");
    const TypeId objectType = MakeTypeId(ns, "Object");
    const TypeId buttonType = MakeTypeId(ns, "Button");
    const TypeId enumType = MakeTypeId(ns, "Options");
    const TypeId structType = MakeTypeId(ns, "Counter");

    CHECK(registration.TryRegisterType(TypeRegistration::Primitive(ns, "UInt32", TypeFlags::ValueType | TypeFlags::Sealed)));
    CHECK(registration.TryRegisterType(TypeRegistration::Interface(ns, "ICommandSource", TypeFlags::Abstract)));
    CHECK(registration.TryRegisterType(TypeRegistration::Object(ns, "Object")));
    CHECK(registration.TryRegisterType(TypeRegistration::Object(ns, "Button", objectType)));
    CHECK(registration.TryRegisterInterface(buttonType, interfaceType));
    CHECK(registration.TryRegisterType(TypeRegistration::Enum(ns, "Options", uintType, TypeFlags::ValueType | TypeFlags::FlagsEnum)));
    CHECK(registration.TryRegisterEnumValue(enumType, {"None", 0U}));
    CHECK(registration.TryRegisterEnumValue(enumType, {"Fast", 1U}));
    CHECK(registration.TryRegisterEnumValue(enumType, {"Safe", 2U}));
    CHECK(registration.TryRegisterType(TypeRegistration::Struct(ns, "Counter", InvalidTypeId, TypeFlags::ValueType | TypeFlags::TriviallyCopyable)));
    Result<MemberId> count = registration.TryRegisterField(
        structType,
        {"Count", uintType, FieldFlags::None,
         &GetStructCount, &SetStructCount, nullptr});
    CHECK(count);
    CHECK(registry.Freeze());
    CHECK(behaviors.Freeze());
    CHECK(registry.Implements(buttonType, interfaceType));
    CHECK(registry.IsAssignableFrom(interfaceType, buttonType));
    CHECK(!registry.IsAssignableFrom(buttonType, interfaceType));

    MetadataDescriptorStore descriptors;
    CHECK(descriptors.Build(registry));
    const MetadataTypeDescriptor* enumDescriptor =
        descriptors.FindType(enumType);
    CHECK(enumDescriptor != nullptr);
    CHECK(enumDescriptor->Kind() == MetadataTypeKind::Enum);
    CHECK(enumDescriptor->UnderlyingType() == uintType);
    CHECK(enumDescriptor->IsFlagsEnum());
    CHECK(descriptors.FindEnumValue(enumType, "Fast") != nullptr);
    CHECK(descriptors.FindEnumValue(enumType, 2U) != nullptr);
    const MetadataFieldDescriptor* field =
        descriptors.FindField(structType, "Count");
    CHECK(field != nullptr && field->Id() == count.Value());
    CHECK(descriptors.Implements(buttonType, interfaceType));
    return true;
}

bool TestBehaviorRegistrationBoundaries() {
    TypeRegistry first;
    MetadataBehaviorRegistrationStore firstBehaviors{first};
    TypeRegistry second;
    MetadataBehaviorRegistrationStore secondBehaviors{second};
    MetadataRegistrationTypes mismatched{first, secondBehaviors};
    Result<TypeId> rejected = mismatched.TryRegisterType(TypeRegistration::Object("urn:behavior", "Rejected"));
    CHECK(!rejected &&
        rejected.GetStatus().code == ErrorCode::InvalidArgument);
    CHECK(!firstBehaviors.Freeze());
    MetadataRegistrationTypes registration{first, firstBehaviors};
    CHECK(registration.TryRegisterType(TypeRegistration::Object("urn:behavior", "Object", InvalidTypeId, TypeFlags::None, &SnapshotFactoryOne)));
    CHECK(first.Freeze());
    CHECK(firstBehaviors.Freeze());
    return true;
}

bool TestRegistrationRollbackOnOom() {
    TrackingAllocator allocator;
    Aero::Tests::ScopedDefaultAllocator scope(allocator);
    {
        TypeRegistry registry;
        MetadataBehaviorRegistrationStore behaviors{registry};
        MetadataRegistrationTypes registration{registry, behaviors};
        const TypeId expected = MakeTypeId("urn:oom", "Object");
        allocator.FailAfter(1U);
        Result<TypeId> failed = registration.TryRegisterType(TypeRegistration::Object("urn:oom", "Object"));
        CHECK(!failed && failed.GetStatus().code == ErrorCode::OutOfMemory);
        CHECK(registry.TypeCount() == 0U);
        CHECK(registry.FindType(expected) == nullptr);
        allocator.DisableFailures();
        CHECK(registration.TryRegisterType(TypeRegistration::Object("urn:oom", "Object")));
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

bool EqualSmall(const void* left, const void* right, void*) noexcept {
    const auto& a = *static_cast<const SmallValue*>(left);
    const auto& b = *static_cast<const SmallValue*>(right);
    return a.first == b.first && a.second == b.second;
}

Result<Value> ConvertSmall(
    TypeId type,
    StringView text,
    void* context) noexcept {
    auto* store = static_cast<MetadataValueRegistrationStore*>(context);
    if (text != StringView("7,9")) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Invalid SmallValue");
    }
    const SmallValue value{7U, 9U};
    return MetadataRegistrationValues(*store).TryCreateValue(type, &value);
}

bool TestUnifiedValueAndRegistrySemantics() {
    static_assert(noexcept(Value(std::declval<const Value&>())),
        "Value copies must remain noexcept");
    TypeRegistry registry;
    MetadataBehaviorRegistrationStore behaviors{registry};
    MetadataRegistrationTypes registration{registry, behaviors};
    MetadataValueRegistrationStore valueStore(registry);
    const TypeId smallType = MakeTypeId("urn:value-tests", "Small");
    CHECK(registration.TryRegisterType(TypeRegistration::Primitive("urn:value-tests", "Small", TypeFlags::ValueType | TypeFlags::Sealed)));
    MetadataRegistrationValues values(valueStore);
    CHECK(values.TryRegisterValueSemantics(
        smallType,
        {sizeof(SmallValue), alignof(SmallValue), nullptr, nullptr,
         &EqualSmall, nullptr, true}));
    CHECK(values.TryRegisterTextConverter(
        {smallType, &ConvertSmall, &valueStore}));
    const SmallValue first{7U, 9U};
    const SmallValue second{7U, 10U};
    Result<Value> inlineValue = values.TryCreateValue(smallType, &first);
    Result<Value> different = values.TryCreateValue(smallType, &second);
    CHECK(inlineValue && inlineValue.Value().IsInlineCustom());
    CHECK(different && inlineValue.Value() != different.Value());
    Result<Value> converted = values.TryConvertText(smallType, "7,9");
    CHECK(converted && converted.Value() == inlineValue.Value());
    CHECK(Value::FromBoolean(smallType, true).AsBoolean());
    CHECK(Value::FromSignedInteger(smallType, -8).AsSignedInteger() == -8);
    CHECK(Value::FromUnsignedInteger(smallType, 8U).AsUnsignedInteger() == 8U);
    CHECK(Value::FromDouble(smallType, 1.25).AsDouble() == 1.25);
    CHECK(registry.Freeze());
    CHECK(valueStore.Freeze());
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
        {"Deterministic snapshot v4", &TestDeterministicSnapshot},
        {"Interfaces, enums, and structs", &TestInterfacesEnumsAndStructs},
        {"Behavior registration boundaries", &TestBehaviorRegistrationBoundaries},
        {"Registration rollback on OOM", &TestRegistrationRollbackOnOom},
        {"Unified Value and registration service semantics",
         &TestUnifiedValueAndRegistrySemantics},
    };

    std::uint32_t passed = 0U;
    for (const TestCase& test : tests) {
        const bool ok = test.run();
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", test.name);
        if (!ok) return 1;
        ++passed;
    }
    std::printf("%u tests passed\n", passed);
    return 0;
}
