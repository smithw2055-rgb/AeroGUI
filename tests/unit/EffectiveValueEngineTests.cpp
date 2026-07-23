#include <Aero/Core/EffectiveValueEngine.hpp>
#include <Aero/Core/Presentation.hpp>
#include <Aero/Core/MetadataBehaviorRegistrationStore.hpp>

#include <cstdint>
#include <cstdio>

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

class TestElement final : public DependencyObject {
public:
    explicit TestElement(TypeId type) noexcept
        : DependencyObject(type) {}

    ~TestElement() override = default;
};

struct Fixture final {
    TypeRegistry types;
    MetadataBehaviorRegistrationStore typesBehaviors{types};
    MetadataRegistrationTypes typesRegistration{types, typesBehaviors};
    DependencyPropertyRegistry properties{types, typesBehaviors};

    TypeId objectType = InvalidTypeId;
    TypeId doubleType = InvalidTypeId;
    TypeId elementType = InvalidTypeId;
    DependencyPropertyHandle width;
    DependencyPropertyHandle theme;

    bool Build() {
        const StringView ns("urn:aero");
        objectType = MakeTypeId(ns, StringView("Object"));
        doubleType = MakeTypeId(ns, StringView("Double"));
        elementType = MakeTypeId(ns, StringView("Element"));

        CHECK(typesRegistration.TryRegisterType({
            ns, StringView("Object"), InvalidTypeId,
            TypeFlags::None, nullptr}));
        CHECK(typesRegistration.TryRegisterType({
            ns, StringView("Double"), InvalidTypeId,
            TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
        CHECK(typesRegistration.TryRegisterType({
            ns, StringView("Element"), objectType,
            TypeFlags::None, nullptr}));

        DependencyPropertyRegistration widthRegistration;
        widthRegistration.name = StringView("Width");
        widthRegistration.ownerType = elementType;
        widthRegistration.valueType = doubleType;
        widthRegistration.metadata.defaultValue =
            PropertyValue::FromDouble(doubleType, 1.0);
        Result<DependencyPropertyRegistrationResult> widthResult =
            properties.TryRegister(widthRegistration);
        CHECK(widthResult);
        width = widthResult.Value().property;

        DependencyPropertyRegistration themeRegistration;
        themeRegistration.name = StringView("ThemeValue");
        themeRegistration.ownerType = elementType;
        themeRegistration.valueType = doubleType;
        themeRegistration.metadata.defaultValue =
            PropertyValue::FromDouble(doubleType, 0.0);
        themeRegistration.metadata.flags = PropertyMetadataFlags::Inherits;
        Result<DependencyPropertyRegistrationResult> themeResult =
            properties.TryRegister(themeRegistration);
        CHECK(themeResult);
        theme = themeResult.Value().property;

        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        return true;
    }
};

double ReadDouble(
    DependencyObject& object,
    DependencyPropertyHandle property) {
    Result<PropertyValue> value = object.GetValue(property);
    return value ? value.Value().AsDouble() : -9999.0;
}

bool RunPropertyChanges(Dispatcher& dispatcher) {
    Result<std::uint32_t> result = dispatcher.RunFramePhase(
        DispatcherFramePhase::PropertyChanges);
    CHECK(result);
    return true;
}

struct ExpressionState final {
    TypeId type = InvalidTypeId;
    double value = 0.0;
    std::uint32_t evaluations = 0U;
    std::uint32_t cleanups = 0U;
};

Result<PropertyValue> EvaluateExpression(
    void* context,
    DependencyObject&,
    DependencyPropertyHandle) noexcept {
    auto* state = static_cast<ExpressionState*>(context);
    ++state->evaluations;
    return PropertyValue::FromDouble(state->type, state->value);
}

void CleanupExpression(void* context) noexcept {
    auto* state = static_cast<ExpressionState*>(context);
    ++state->cleanups;
}

bool TestProviderPrecedenceAndBatching() {
    Fixture fixture;
    CHECK(fixture.Build());

    Dispatcher dispatcher;
    PresentationContextScope presentation(dispatcher, fixture.properties);
    TestElement element(fixture.elementType);
    EffectiveValueEngine engine(dispatcher, fixture.properties);
    CHECK(engine.Initialize());

    CHECK(engine.SetStyleValue(
        element, fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, 2.0)));
    CHECK(engine.SetTemplateValue(
        element, fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, 3.0)));

    CHECK(ReadDouble(element, fixture.width) == 1.0);
    CHECK(engine.PendingPropertyCount() == 1U);
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(ReadDouble(element, fixture.width) == 3.0);

    Result<EffectiveValueDiagnostics> templateDiagnostics =
        engine.Diagnostics(element, fixture.width);
    CHECK(templateDiagnostics);
    CHECK(templateDiagnostics.Value().provider ==
        EffectiveValueProvider::Template);

    CHECK(element.SetValue(
        fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, 4.0)));
    CHECK(engine.Invalidate(element, fixture.width));
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(ReadDouble(element, fixture.width) == 4.0);

    ExpressionState expression;
    expression.type = fixture.doubleType;
    expression.value = 5.0;
    PropertyExpression descriptor;
    descriptor.context = &expression;
    descriptor.evaluate = &EvaluateExpression;
    descriptor.cleanup = &CleanupExpression;
    descriptor.kind = PropertyExpressionKind::Binding;

    CHECK(engine.SetLocalExpression(
        element, fixture.width, descriptor));
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(ReadDouble(element, fixture.width) == 5.0);
    CHECK(expression.evaluations == 1U);

    CHECK(engine.SetAnimationValue(
        element, fixture.width,
        PropertyValue::FromDouble(fixture.doubleType, 6.0)));
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(ReadDouble(element, fixture.width) == 6.0);

    Result<EffectiveValueDiagnostics> animated =
        engine.Diagnostics(element, fixture.width);
    CHECK(animated);
    CHECK(animated.Value().provider ==
        EffectiveValueProvider::Animation);
    CHECK(animated.Value().isAnimated);

    CHECK(engine.ClearAnimationValue(element, fixture.width));
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(ReadDouble(element, fixture.width) == 5.0);

    CHECK(engine.ClearLocalExpression(element, fixture.width));
    CHECK(expression.cleanups == 1U);
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(ReadDouble(element, fixture.width) == 4.0);

    Result<EffectiveValueDiagnostics> local =
        engine.Diagnostics(element, fixture.width);
    CHECK(local);
    CHECK(local.Value().provider == EffectiveValueProvider::Local);

    CHECK(engine.DetachObject(element));
    CHECK(engine.TrackedPropertyCount() == 0U);
    return true;
}

bool TestExpressionReplacementAndDiagnostics() {
    Fixture fixture;
    CHECK(fixture.Build());

    Dispatcher dispatcher;
    PresentationContextScope presentation(dispatcher, fixture.properties);
    TestElement element(fixture.elementType);
    EffectiveValueEngine engine(dispatcher, fixture.properties);
    CHECK(engine.Initialize());

    ExpressionState first;
    first.type = fixture.doubleType;
    first.value = 7.0;
    ExpressionState second;
    second.type = fixture.doubleType;
    second.value = 8.0;

    PropertyExpression firstExpression{
        &first,
        &EvaluateExpression,
        &CleanupExpression,
        PropertyExpressionKind::DynamicResource};
    PropertyExpression secondExpression{
        &second,
        &EvaluateExpression,
        &CleanupExpression,
        PropertyExpressionKind::Binding};

    CHECK(engine.SetLocalExpression(
        element, fixture.width, firstExpression));
    CHECK(engine.SetLocalExpression(
        element, fixture.width, secondExpression));
    CHECK(first.cleanups == 1U);
    CHECK(second.cleanups == 0U);

    CHECK(RunPropertyChanges(dispatcher));
    CHECK(ReadDouble(element, fixture.width) == 8.0);

    Result<EffectiveValueDiagnostics> diagnostics =
        engine.Diagnostics(element, fixture.width);
    CHECK(diagnostics);
    CHECK(diagnostics.Value().provider ==
        EffectiveValueProvider::LocalExpression);
    CHECK(diagnostics.Value().hasExpression);
    CHECK(diagnostics.Value().expressionKind ==
        PropertyExpressionKind::Binding);
    CHECK(diagnostics.Value().revision != 0U);

    CHECK(engine.DetachObject(element));
    CHECK(second.cleanups == 1U);
    return true;
}

bool TestInheritanceAndCycleDetection() {
    Fixture fixture;
    CHECK(fixture.Build());

    Dispatcher dispatcher;
    PresentationContextScope presentation(dispatcher, fixture.properties);
    TestElement parent(fixture.elementType);
    TestElement child(fixture.elementType);
    TestElement grandchild(fixture.elementType);
    EffectiveValueEngine engine(dispatcher, fixture.properties);
    CHECK(engine.Initialize());

    CHECK(engine.SetInheritanceParent(child, &parent));
    CHECK(engine.SetInheritanceParent(grandchild, &child));
    CHECK(engine.InheritanceParent(child) == &parent);
    CHECK(engine.InheritanceParent(grandchild) == &child);

    Result<void> cycle = engine.SetInheritanceParent(parent, &grandchild);
    CHECK(!cycle);
    CHECK(cycle.GetStatus().code == ErrorCode::CycleDetected);

    CHECK(parent.SetValue(
        fixture.theme,
        PropertyValue::FromDouble(fixture.doubleType, 10.0)));
    CHECK(engine.Invalidate(parent, fixture.theme));
    CHECK(engine.Invalidate(child, fixture.theme));
    CHECK(engine.Invalidate(grandchild, fixture.theme));
    CHECK(RunPropertyChanges(dispatcher));

    CHECK(ReadDouble(parent, fixture.theme) == 10.0);
    CHECK(ReadDouble(child, fixture.theme) == 10.0);
    CHECK(ReadDouble(grandchild, fixture.theme) == 10.0);

    Result<EffectiveValueDiagnostics> inherited =
        engine.Diagnostics(grandchild, fixture.theme);
    CHECK(inherited);
    CHECK(inherited.Value().provider ==
        EffectiveValueProvider::Inherited);
    CHECK(inherited.Value().isInherited);

    CHECK(parent.SetValue(
        fixture.theme,
        PropertyValue::FromDouble(fixture.doubleType, 20.0)));
    CHECK(engine.Invalidate(parent, fixture.theme));
    CHECK(RunPropertyChanges(dispatcher));
    CHECK(ReadDouble(child, fixture.theme) == 20.0);
    CHECK(ReadDouble(grandchild, fixture.theme) == 20.0);

    CHECK(engine.DetachObject(grandchild));
    CHECK(engine.DetachObject(child));
    CHECK(engine.DetachObject(parent));
    return true;
}

bool TestInitializationAndValidation() {
    Fixture fixture;
    CHECK(fixture.Build());

    Dispatcher dispatcher;
    PresentationContextScope presentation(dispatcher, fixture.properties);
    TestElement element(fixture.elementType);
    EffectiveValueEngine engine(dispatcher, fixture.properties);

    Result<void> beforeInit = engine.Invalidate(element, fixture.width);
    CHECK(!beforeInit);
    CHECK(beforeInit.GetStatus().code == ErrorCode::NotInitialized);

    CHECK(engine.Initialize());
    CHECK(engine.Initialize());

    PropertyExpression invalid;
    Result<void> invalidExpression = engine.SetLocalExpression(
        element, fixture.width, invalid);
    CHECK(!invalidExpression);
    CHECK(invalidExpression.GetStatus().code ==
        ErrorCode::InvalidArgument);

    CHECK(engine.ClearStyleValue(element, fixture.width));
    CHECK(engine.ClearTemplateValue(element, fixture.width));
    CHECK(engine.ClearAnimationValue(element, fixture.width));
    CHECK(engine.ClearLocalExpression(element, fixture.width));
    CHECK(engine.PendingPropertyCount() == 0U);
    return true;
}

struct TestCase final {
    const char* name;
    bool (*run)();
};

} // namespace

int main() {
    const TestCase tests[] = {
        {"Provider precedence and batching",
            &TestProviderPrecedenceAndBatching},
        {"Expression replacement and diagnostics",
            &TestExpressionReplacementAndDiagnostics},
        {"Inheritance and cycle detection",
            &TestInheritanceAndCycleDetection},
        {"Initialization and validation",
            &TestInitializationAndValidation},
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
