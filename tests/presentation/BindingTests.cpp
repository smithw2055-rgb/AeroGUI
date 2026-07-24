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

Result<PropertyValue> ScaleDouble(
    const PropertyValue& value,
    TypeId targetType,
    void* context) noexcept {
    if (value.Kind() != ValueKind::Double ||
        context == nullptr) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Scale converter requires a double");
    }
    return PropertyValue::FromDouble(
        targetType,
        value.AsDouble() * *static_cast<double*>(context));
}

Result<PropertyValue> UnscaleDouble(
    const PropertyValue& value,
    TypeId targetType,
    void* context) noexcept {
    if (value.Kind() != ValueKind::Double ||
        context == nullptr ||
        *static_cast<double*>(context) == 0.0) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Scale converter cannot convert back");
    }
    return PropertyValue::FromDouble(
        targetType,
        value.AsDouble() / *static_cast<double*>(context));
}

Result<void> RequireNonNegative(
    const PropertyValue& value,
    void*) noexcept {
    if (value.Kind() != ValueKind::Double ||
        value.AsDouble() < 0.0) {
        return Status::Failure(
            ErrorCode::ValidationFailed,
            "Binding value must be non-negative");
    }
    return {};
}

struct DiagnosticProbe final {
    std::uint32_t count = 0U;
    BindingDiagnostic diagnostic;
};

void RecordDiagnostic(
    const BindingDiagnostic& diagnostic,
    void* context) noexcept {
    auto* probe = static_cast<DiagnosticProbe*>(context);
    ++probe->count;
    probe->diagnostic = diagnostic;
}

BindingDescriptor MakeBindingDescriptor(
    DependencyObject& source,
    DependencyPropertyHandle sourceProperty,
    DependencyObject& target,
    DependencyPropertyHandle targetProperty,
    BindingMode mode,
    UpdateSourceTrigger updateSourceTrigger =
        UpdateSourceTrigger::PropertyChanged) noexcept {
    BindingDescriptor descriptor;
    descriptor.source = &source;
    descriptor.sourceProperty = sourceProperty;
    descriptor.target = &target;
    descriptor.targetProperty = targetProperty;
    descriptor.mode = mode;
    descriptor.updateSourceTrigger = updateSourceTrigger;
    return descriptor;
}

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
    Result<BindingHandle> attached = bindings.Attach(
        MakeBindingDescriptor(
            source,
            fixture.source,
            target,
            fixture.target,
            BindingMode::OneWay));
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
    Result<BindingHandle> attached = bindings.Attach(
        MakeBindingDescriptor(
            source,
            fixture.source,
            target,
            fixture.target,
            BindingMode::OneTime));
    CHECK(attached);
    CHECK(bindings.Flush().Value() == 1U);
    CHECK(source.SetValue(
        fixture.source,
        PropertyValue::FromDouble(fixture.doubleType, 9.0)));
    CHECK(bindings.Flush().Value() == 0U);
    CHECK(target.GetValue(fixture.target).Value().AsDouble() == 7.0);

    Result<BindingHandle> mismatch = bindings.Attach(
        MakeBindingDescriptor(
            source,
            fixture.source,
            target,
            fixture.boolean,
            BindingMode::OneWay));
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
    Result<BindingHandle> twoWay = bindings.Attach(
        MakeBindingDescriptor(
            source,
            fixture.source,
            target,
            fixture.target,
            BindingMode::TwoWay));
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
    Result<BindingHandle> targetToSource = bindings.Attach(
        MakeBindingDescriptor(
            source,
            fixture.source,
            target,
            fixture.target,
            BindingMode::OneWayToSource));
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
    Result<BindingHandle> binding = bindings.Attach(
        MakeBindingDescriptor(
            source,
            fixture.source,
            target,
            fixture.target,
            BindingMode::TwoWay,
            UpdateSourceTrigger::Explicit));
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

bool TestConversionFallbackDiagnosticsAndIsolation() {
    Fixture fixture;
    CHECK(fixture.Build());
    TestObject invalidSource(fixture.elementType);
    TestObject invalidTarget(fixture.elementType);
    TestObject validSource(fixture.elementType);
    TestObject validTarget(fixture.elementType);
    BindingManager bindings(fixture.dispatcher);
    CHECK(bindings.Initialize());
    CHECK(invalidSource.SetValue(
        fixture.source,
        PropertyValue::FromDouble(fixture.doubleType, -2.0)));
    CHECK(validSource.SetValue(
        fixture.source,
        PropertyValue::FromDouble(fixture.doubleType, 3.0)));

    double scale = 2.0;
    DiagnosticProbe diagnostic;
    BindingDescriptor invalid;
    invalid.source = &invalidSource;
    invalid.sourceProperty = fixture.source;
    invalid.target = &invalidTarget;
    invalid.targetProperty = fixture.target;
    invalid.mode = BindingMode::OneWay;
    invalid.convert = &ScaleDouble;
    invalid.validate = &RequireNonNegative;
    invalid.conversionContext = &scale;
    invalid.fallbackValue =
        PropertyValue::FromDouble(fixture.doubleType, 99.0);
    invalid.diagnostic = &RecordDiagnostic;
    invalid.diagnosticContext = &diagnostic;
    Result<BindingHandle> failed = bindings.Attach(invalid);
    CHECK(failed);

    BindingDescriptor valid = invalid;
    valid.source = &validSource;
    valid.target = &validTarget;
    valid.fallbackValue = {};
    valid.diagnostic = nullptr;
    valid.diagnosticContext = nullptr;
    Result<BindingHandle> succeeded = bindings.Attach(valid);
    CHECK(succeeded);
    CHECK(bindings.Flush().Value() == 2U);
    CHECK(invalidTarget.GetValue(
        fixture.target).Value().AsDouble() == 99.0);
    CHECK(validTarget.GetValue(
        fixture.target).Value().AsDouble() == 6.0);
    CHECK(diagnostic.count == 1U);
    CHECK(diagnostic.diagnostic.handle.value == failed.Value().value);
    CHECK(diagnostic.diagnostic.stage ==
        BindingDiagnosticStage::Validate);
    CHECK(diagnostic.diagnostic.status.code ==
        ErrorCode::ValidationFailed);
    CHECK(bindings.LastError().code == ErrorCode::ValidationFailed);

    CHECK(bindings.Detach(failed.Value()).Value());
    CHECK(bindings.Detach(succeeded.Value()).Value());
    return true;
}

bool TestConvertBackAndTargetNullValue() {
    Fixture fixture;
    CHECK(fixture.Build());
    TestObject source(fixture.elementType);
    TestObject target(fixture.elementType);
    BindingManager bindings(fixture.dispatcher);
    CHECK(bindings.Initialize());
    CHECK(source.SetValue(
        fixture.source,
        PropertyValue::FromDouble(fixture.doubleType, 4.0)));

    double scale = 2.0;
    BindingDescriptor converted;
    converted.source = &source;
    converted.sourceProperty = fixture.source;
    converted.target = &target;
    converted.targetProperty = fixture.target;
    converted.mode = BindingMode::TwoWay;
    converted.convert = &ScaleDouble;
    converted.convertBack = &UnscaleDouble;
    converted.conversionContext = &scale;
    Result<BindingHandle> attached = bindings.Attach(converted);
    CHECK(attached);
    CHECK(bindings.Flush().Value() == 1U);
    CHECK(target.GetValue(
        fixture.target).Value().AsDouble() == 8.0);
    CHECK(target.SetValue(
        fixture.target,
        PropertyValue::FromDouble(fixture.doubleType, 14.0)));
    CHECK(bindings.Flush().Value() == 1U);
    CHECK(source.GetValue(
        fixture.source).Value().AsDouble() == 7.0);
    CHECK(bindings.Detach(attached.Value()).Value());

    TestObject nullSource(fixture.elementType);
    TestObject nullTarget(fixture.elementType);
    Result<Ref<TestObject>> replacement =
        MakeRef<TestObject>(fixture.objectType);
    CHECK(replacement);
    Ref<TestObject> replacementTyped =
        std::move(replacement).Value();
    Ref<Object> replacementObject(replacementTyped);
    BindingDescriptor nullBinding;
    nullBinding.source = &nullSource;
    nullBinding.sourceProperty = fixture.boolean;
    nullBinding.target = &nullTarget;
    nullBinding.targetProperty = fixture.boolean;
    nullBinding.targetNullValue = PropertyValue::FromObject(
        fixture.objectType, replacementObject);
    Result<BindingHandle> nullAttached =
        bindings.Attach(nullBinding);
    CHECK(nullAttached);
    CHECK(bindings.Flush().Value() == 1U);
    CHECK(nullTarget.GetValue(
        fixture.boolean).Value().AsObject().Get() ==
        replacementTyped.Get());
    return true;
}

} // namespace

int main() {
    if (!TestOneWayBindsAtDataBindPhase()) return 1;
    if (!TestOneTimeAndValidation()) return 1;
    if (!TestTwoWayAndOneWayToSource()) return 1;
    if (!TestExplicitUpdateSourceTrigger()) return 1;
    if (!TestConversionFallbackDiagnosticsAndIsolation()) return 1;
    if (!TestConvertBackAndTargetNullValue()) return 1;
    std::puts("Aero binding tests passed");
    return 0;
}
