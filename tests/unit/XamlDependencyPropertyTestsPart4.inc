            "  <Button.Child><Button/></Button.Child>\n"
            "  <Button.IsLocked>true</Button.IsLocked>\n"
            "</Button>"),
        diagnostics,
        activation);
    CHECK(!loaded);
    CHECK(loaded.GetStatus().code == ErrorCode::ReadOnly);
    CHECK(diagnostics.Size() == 1U);
    CHECK(diagnostics.Items()[0].Code() ==
        XamlObjectWriterDiagnosticCodes::InvalidValue);
    CHECK(diagnostics.Items()[0].Source().begin.line == 3U);
    CHECK(TestElement::AbortCount() == 2U);
    CHECK(TestElement::LiveCount() == 0U);
    return true;
}

bool TestValidationDiagnosticAndRollback() {
    TestElement::ResetCounters();
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    XamlActivationContext activation = fixture.Activation();
    Result<Ref<Object>> loaded = LoadDocument(
        fixture,
        StringView("<Button xmlns=\"urn:dp\" Width=\"-1\"/>"),
        diagnostics,
        activation);
    CHECK(!loaded);
    CHECK(loaded.GetStatus().code == ErrorCode::ValidationFailed);
    CHECK(diagnostics.Size() == 1U);
    CHECK(diagnostics.Items()[0].Code() ==
        XamlObjectWriterDiagnosticCodes::InvalidValue);
    CHECK(diagnostics.Items()[0].Source().begin.line == 1U);
    CHECK(TestElement::AbortCount() == 1U);
    CHECK(TestElement::LiveCount() == 0U);
    return true;
}

bool TestActivationValidationAndProviderFailure() {
    TestElement::ResetCounters();
    Fixture fixture;
    CHECK(fixture.Build());

    XamlActivationContext invalid = fixture.Activation();
    invalid.structSize = static_cast<std::uint32_t>(
        sizeof(XamlActivationContext) - 1U);
    DiagnosticBag invalidDiagnostics;
    Result<Ref<Object>> invalidLoad = LoadDocument(
        fixture,
        StringView("<Button xmlns=\"urn:dp\"/>"),
        invalidDiagnostics,
        invalid);
    CHECK(!invalidLoad);
    CHECK(invalidLoad.GetStatus().code == ErrorCode::InvalidArgument);
    CHECK(invalidDiagnostics.Size() == 0U);

    fixture.failActivation = true;
    DiagnosticBag failureDiagnostics;
    XamlActivationContext activation = fixture.Activation();
    Result<Ref<Object>> failed = LoadDocument(
        fixture,
        StringView("<Button xmlns=\"urn:dp\"/>"),
        failureDiagnostics,
        activation);
    CHECK(!failed);
    CHECK(failed.GetStatus().code == ErrorCode::InternalError);
    CHECK(failureDiagnostics.Size() == 1U);
    CHECK(failureDiagnostics.Items()[0].Code() ==
        XamlObjectWriterDiagnosticCodes::FactoryFailed);
    CHECK(TestElement::LiveCount() == 0U);
    return true;
}

bool TestActivationRegistrationGuards() {
    TypeRegistry types;
    XamlSchemaContext schema(types);
    XamlActivationProviderRegistry activations(schema);
    const StringView ns("urn:guard");
    const TypeId objectType = MakeTypeId(ns, StringView("Object"));
    CHECK(types.TryRegisterType({
        ns, StringView("Object"), InvalidTypeId,
        TypeFlags::None, nullptr}));

    XamlActivationProviderRegistration invalid;
    invalid.type = objectType;
    Result<void> bad = activations.TryRegister(invalid);
    CHECK(!bad && bad.GetStatus().code == ErrorCode::InvalidArgument);

    CHECK(activations.TryRegister({
        objectType,
        &Fixture::Activate,
        nullptr}));
    Result<void> duplicate = activations.TryRegister({
        objectType,
        &Fixture::Activate,
        nullptr});
    CHECK(!duplicate && duplicate.GetStatus().code == ErrorCode::AlreadyExists);

    Result<void> premature = activations.Freeze();
    CHECK(!premature && premature.GetStatus().code == ErrorCode::InvalidState);
    CHECK(types.Freeze());
    CHECK(schema.Freeze());
    CHECK(activations.Freeze());
    Result<void> late = activations.TryRegister({
        objectType,
        &Fixture::Activate,
        nullptr});
    CHECK(!late && late.GetStatus().code == ErrorCode::InvalidState);
    return true;
}

} // namespace

int main() {
    if (!TestActivationAndAutomaticDependencyProperties()) return 1;
    if (!TestReadOnlyRollbackAfterCompletedChild()) return 1;
    if (!TestValidationDiagnosticAndRollback()) return 1;
    if (!TestActivationValidationAndProviderFailure()) return 1;
    if (!TestActivationRegistrationGuards()) return 1;
    std::puts("Aero XAML dependency-property tests passed");
    return 0;
}
