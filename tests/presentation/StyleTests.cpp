#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Presentation/Style.hpp>
#include <Aero/Presentation/Metadata.hpp>
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

class TestElement final : public DependencyObject {
public:
    explicit TestElement(TypeId type) noexcept
        : DependencyObject(type) {}
};

struct Fixture final {
    TypeRegistry types;
    MetadataBehaviorRegistrationStore typesBehaviors{types};
    MetadataRegistrationTypes typesRegistration{types, typesBehaviors};
    DependencyPropertyRegistry properties{types, typesBehaviors};
    TypeId objectType = InvalidTypeId;
    TypeId doubleType = InvalidTypeId;
    TypeId elementType = InvalidTypeId;
    TypeId buttonType = InvalidTypeId;
    DependencyPropertyHandle width;
    DependencyPropertyHandle height;

    bool Build() {
        const StringView ns("urn:style-tests");
        objectType = MakeTypeId(ns, StringView("Object"));
        doubleType = MakeTypeId(ns, StringView("Double"));
        elementType = MakeTypeId(ns, StringView("Element"));
        buttonType = MakeTypeId(ns, StringView("Button"));
        CHECK(typesRegistration.TryRegisterType(TypeRegistration::Object(ns, StringView("Object"), InvalidTypeId, TypeFlags::None, nullptr)));
        CHECK(typesRegistration.TryRegisterType(TypeRegistration::Primitive(ns, StringView("Double"), TypeFlags::ValueType)));
        CHECK(typesRegistration.TryRegisterType(TypeRegistration::Object(ns, StringView("Element"), objectType, TypeFlags::None, nullptr)));
        CHECK(typesRegistration.TryRegisterType(TypeRegistration::Object(ns, StringView("Button"), elementType, TypeFlags::None, nullptr)));
        DependencyPropertyRegistration widthRegistration;
        widthRegistration.name = StringView("Width");
        widthRegistration.ownerType = elementType;
        widthRegistration.valueType = doubleType;
        widthRegistration.metadata.defaultValue = PropertyValue::FromDouble(doubleType, 1.0);
        Result<DependencyPropertyRegistrationResult> widthResult = properties.TryRegister(widthRegistration);
        CHECK(widthResult);
        width = widthResult.Value().property;
        DependencyPropertyRegistration heightRegistration = widthRegistration;
        heightRegistration.name = StringView("Height");
        Result<DependencyPropertyRegistrationResult> heightResult = properties.TryRegister(heightRegistration);
        CHECK(heightResult);
        height = heightResult.Value().property;
        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        return true;
    }
};

bool RunPropertyChanges(Dispatcher& dispatcher) {
    CHECK(dispatcher.RunFramePhase(DispatcherFramePhase::PropertyChanges));
    return true;
}

bool TestBasedOnFlatteningAndPrecedence() {
    Fixture fixture;
    CHECK(fixture.Build());
    Dispatcher dispatcher;
    ObjectServicesScope presentation(dispatcher, fixture.properties);
    TestElement button(fixture.buttonType);
    EffectiveValueEngine values(dispatcher, fixture.properties);
    CHECK(values.Initialize());
    Style base(fixture.elementType);
    CHECK(base.TryAddSetter(fixture.width, PropertyValue::FromDouble(fixture.doubleType, 20.0)));
    CHECK(base.TryAddSetter(fixture.height, PropertyValue::FromDouble(fixture.doubleType, 30.0)));
    CHECK(base.Seal(fixture.properties));
    Style derived(fixture.buttonType, &base);
    CHECK(derived.TryAddSetter(fixture.width, PropertyValue::FromDouble(fixture.doubleType, 40.0)));
    CHECK(derived.Seal(fixture.properties));
    CHECK(derived.Setters().Size() == 2U);
    StyleManager manager(values, fixture.properties);
    CHECK(manager.Apply(button, derived));
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(button.GetValue(fixture.width).Value().AsDouble() == 40.0);
    CHECK(button.GetValue(fixture.height).Value().AsDouble() == 30.0);
    CHECK(button.SetValue(fixture.width, PropertyValue::FromDouble(fixture.doubleType, 50.0)));
    CHECK(values.Invalidate(button, fixture.width));
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(button.GetValue(fixture.width).Value().AsDouble() == 50.0);
    CHECK(manager.Clear(button, derived));
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(button.GetValue(fixture.width).Value().AsDouble() == 50.0);
    CHECK(button.GetValue(fixture.height).Value().AsDouble() == 1.0);
    Style replacement(fixture.buttonType);
    CHECK(replacement.TryAddSetter(
        fixture.height, PropertyValue::FromDouble(fixture.doubleType, 60.0)));
    CHECK(replacement.Seal(fixture.properties));
    CHECK(manager.Apply(button, replacement));
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(button.GetValue(fixture.width).Value().AsDouble() == 50.0);
    CHECK(button.GetValue(fixture.height).Value().AsDouble() == 60.0);
    CHECK(manager.DetachObject(button).Value());
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(button.GetValue(fixture.height).Value().AsDouble() == 1.0);
    return true;
}

bool TestStyleSealContracts() {
    Fixture fixture;
    CHECK(fixture.Build());
    Style base(fixture.elementType);
    CHECK(base.TryAddSetter(fixture.width, PropertyValue::FromDouble(fixture.doubleType, 2.0)));
    Style derived(fixture.buttonType, &base);
    CHECK(!derived.Seal(fixture.properties));
    CHECK(base.Seal(fixture.properties));
    CHECK(derived.Seal(fixture.properties));
    CHECK(!derived.TryAddSetter(fixture.height, PropertyValue::FromDouble(fixture.doubleType, 3.0)));
    Style incompatible(fixture.elementType, &derived);
    CHECK(!incompatible.Seal(fixture.properties));
    Style invalidValue(fixture.elementType);
    CHECK(invalidValue.TryAddSetter(
        fixture.width, PropertyValue::NullObject(fixture.objectType)));
    CHECK(!invalidValue.Seal(fixture.properties));
    return true;
}

} // namespace

int main() {
    if (!TestBasedOnFlatteningAndPrecedence()) return 1;
    if (!TestStyleSealContracts()) return 1;
    std::puts("Aero style tests passed");
    return 0;
}
