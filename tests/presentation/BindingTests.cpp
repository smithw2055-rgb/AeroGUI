#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Presentation/Binding.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Presentation/Metadata.hpp>
#include <Aero/Core/Metadata/TypeRegistry.hpp>
#include <Aero/Core/Metadata/MetadataBehaviorRegistrationStore.hpp>

#include <cstdio>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Presentation;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

class TestObject final : public DependencyObject {
public:
    explicit TestObject(TypeId type) noexcept
        : DependencyObject(type) {}
};

struct Fixture final {
    Dispatcher dispatcher;
    TypeRegistry types;
    MetadataBehaviorRegistrationStore typesBehaviors{types};
    MetadataRegistrationTypes typesRegistration{types, typesBehaviors};
    DependencyPropertyRegistry properties{types, typesBehaviors};
    ObjectServicesScope presentation{dispatcher, properties};
    TypeId objectType = InvalidTypeId;
    TypeId doubleType = InvalidTypeId;
    TypeId elementType = InvalidTypeId;
    DependencyPropertyHandle source;
    DependencyPropertyHandle target;
    DependencyPropertyHandle boolean;

    bool Build() {
        const StringView ns("urn:binding-tests");
        objectType = MakeTypeId(ns, StringView("Object"));
        doubleType = MakeTypeId(ns, StringView("Double"));
        elementType = MakeTypeId(ns, StringView("Element"));
        CHECK(typesRegistration.TryRegisterType(TypeRegistration::Object(ns, StringView("Object"), InvalidTypeId, TypeFlags::None, nullptr)));
        CHECK(typesRegistration.TryRegisterType(TypeRegistration::Primitive(ns, StringView("Double"), TypeFlags::ValueType | TypeFlags::Sealed)));
        CHECK(typesRegistration.TryRegisterType(TypeRegistration::Object(ns, StringView("Element"), objectType, TypeFlags::Sealed, nullptr)));
        DependencyPropertyRegistration sourceRegistration;
        sourceRegistration.name = StringView("Source");
        sourceRegistration.ownerType = elementType;
        sourceRegistration.valueType = doubleType;
        sourceRegistration.metadata.defaultValue =
            PropertyValue::FromDouble(doubleType, 0.0);
        Result<DependencyPropertyRegistrationResult> sourceResult =
            properties.TryRegister(sourceRegistration);
        CHECK(sourceResult);
        source = sourceResult.Value().property;

        DependencyPropertyRegistration targetRegistration;
        targetRegistration.name = StringView("Target");
        targetRegistration.ownerType = elementType;
        targetRegistration.valueType = doubleType;
        targetRegistration.metadata.defaultValue =
            PropertyValue::FromDouble(doubleType, -1.0);
        Result<DependencyPropertyRegistrationResult> targetResult =
            properties.TryRegister(targetRegistration);
        CHECK(targetResult);
        target = targetResult.Value().property;

        DependencyPropertyRegistration booleanRegistration;
        booleanRegistration.name = StringView("Boolean");
        booleanRegistration.ownerType = elementType;
        booleanRegistration.valueType = objectType;
        booleanRegistration.metadata.defaultValue =
            PropertyValue::NullObject(objectType);
        Result<DependencyPropertyRegistrationResult> booleanResult =
            properties.TryRegister(booleanRegistration);
        CHECK(booleanResult);
        boolean = booleanResult.Value().property;

        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        return true;
    }
};

bool TestOneWayBindsAtDataBindPhase() {
    Fixture fixture;
    CHECK(fixture.Build());
    TestObject source(fixture.elementType);
    TestObject target(fixture.elementType);
    BindingManager bindings(fixture.dispatcher);
    CHECK(bindings.Initialize());
    CHECK(source.SetValue(
        fixture.source,
        PropertyValue::FromDouble(fixture.doubleType, 12.5)));
    Result<BindingHandle> attached = bindings.Attach({
        &source, fixture.source, &target, fixture.target, BindingMode::OneWay});
    CHECK(attached);
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::DataBind));
    Result<PropertyValue> first = target.GetValue(fixture.target);
    CHECK(first && first.Value().AsDouble() == 12.5);
    CHECK(target.GetValueSource(fixture.target).Value() ==
        EffectiveValueSource::Current);

    CHECK(source.SetValue(
        fixture.source,
        PropertyValue::FromDouble(fixture.doubleType, 33.0)));
    CHECK(fixture.dispatcher.RunFramePhase(DispatcherFramePhase::DataBind));
    Result<PropertyValue> second = target.GetValue(fixture.target);
    CHECK(second && second.Value().AsDouble() == 33.0);
    CHECK(bindings.Detach(attached.Value()).Value());
    CHECK(!bindings.Detach(attached.Value()).Value());
    return true;
}

bool TestOneTimeAndValidation() {
    Fixture fixture;
    CHECK(fixture.Build());
    TestObject source(fixture.elementType);
    TestObject target(fixture.elementType);
    BindingManager bindings(fixture.dispatcher);
    CHECK(bindings.Initialize());
    CHECK(source.SetValue(
        fixture.source,
        PropertyValue::FromDouble(fixture.doubleType, 7.0)));
    Result<BindingHandle> attached = bindings.Attach({
        &source, fixture.source, &target, fixture.target, BindingMode::OneTime});
    CHECK(attached);
    CHECK(bindings.Flush().Value() == 1U);
    CHECK(source.SetValue(
        fixture.source,
        PropertyValue::FromDouble(fixture.doubleType, 9.0)));
    CHECK(bindings.Flush().Value() == 0U);
    CHECK(target.GetValue(fixture.target).Value().AsDouble() == 7.0);

    Result<BindingHandle> mismatch = bindings.Attach({
        &source, fixture.source, &target, fixture.boolean, BindingMode::OneWay});
    CHECK(!mismatch);
    CHECK(mismatch.GetStatus().code == ErrorCode::InvalidArgument);
    return true;
}

bool TestTwoWayAndOneWayToSource() {
    Fixture fixture;
    CHECK(fixture.Build());
    TestObject source(fixture.elementType);
    TestObject target(fixture.elementType);
    BindingManager bindings(fixture.dispatcher);
    CHECK(bindings.Initialize());
    CHECK(source.SetValue(
        fixture.source,
        PropertyValue::FromDouble(fixture.doubleType, 2.0)));
    Result<BindingHandle> twoWay = bindings.Attach({
        &source, fixture.source, &target, fixture.target, BindingMode::TwoWay});
    CHECK(twoWay);
    CHECK(bindings.Flush().Value() == 1U);
    CHECK(target.GetValue(fixture.target).Value().AsDouble() == 2.0);

    CHECK(target.SetValue(
        fixture.target,
        PropertyValue::FromDouble(fixture.doubleType, 5.0)));
    CHECK(bindings.Flush().Value() == 1U);
    CHECK(source.GetValue(fixture.source).Value().AsDouble() == 5.0);

    CHECK(bindings.Detach(twoWay.Value()).Value());
    CHECK(target.SetValue(
        fixture.target,
        PropertyValue::FromDouble(fixture.doubleType, 8.0)));
    Result<BindingHandle> targetToSource = bindings.Attach({
        &source, fixture.source, &target, fixture.target,
        BindingMode::OneWayToSource});
    CHECK(targetToSource);
    CHECK(bindings.Flush().Value() == 1U);
    CHECK(source.GetValue(fixture.source).Value().AsDouble() == 8.0);
    CHECK(bindings.DetachObject(source).Value() == 1U);
    CHECK(bindings.BindingCount() == 0U);
    return true;
}

bool TestExplicitUpdateSourceTrigger() {
    Fixture fixture;
    CHECK(fixture.Build());
    TestObject source(fixture.elementType);
    TestObject target(fixture.elementType);
    BindingManager bindings(fixture.dispatcher);
    CHECK(bindings.Initialize());
    CHECK(source.SetValue(
        fixture.source, PropertyValue::FromDouble(fixture.doubleType, 3.0)));
    Result<BindingHandle> binding = bindings.Attach({
        &source, fixture.source, &target, fixture.target, BindingMode::TwoWay,
        UpdateSourceTrigger::Explicit});
    CHECK(binding);
    CHECK(bindings.Flush().Value() == 1U);
    CHECK(target.GetValue(fixture.target).Value().AsDouble() == 3.0);
    CHECK(target.SetValue(
        fixture.target, PropertyValue::FromDouble(fixture.doubleType, 9.0)));
    CHECK(bindings.Flush().Value() == 0U);
    CHECK(source.GetValue(fixture.source).Value().AsDouble() == 3.0);
    CHECK(bindings.UpdateSource(binding.Value()).Value());
    CHECK(bindings.Flush().Value() == 1U);
    CHECK(source.GetValue(fixture.source).Value().AsDouble() == 9.0);
    return true;
}

} // namespace

int main() {
    if (!TestOneWayBindsAtDataBindPhase()) return 1;
    if (!TestOneTimeAndValidation()) return 1;
    if (!TestTwoWayAndOneWayToSource()) return 1;
    if (!TestExplicitUpdateSourceTrigger()) return 1;
    std::puts("Aero binding tests passed");
    return 0;
}
