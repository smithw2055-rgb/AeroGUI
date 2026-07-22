            PropertyValue::FromBoolean(booleanType, false);
        Result<DependencyPropertyRegistrationResult> lockedResult =
            properties.TryRegister(registration);
        CHECK(lockedResult);
        isLocked = lockedResult.Value().property;

        registration = {};
        registration.name = StringView("Row");
        registration.ownerType = gridType;
        registration.valueType = integerType;
        registration.flags = DependencyPropertyFlags::Attached;
        registration.metadata.defaultValue =
            PropertyValue::FromSignedInteger(integerType, 0);
        Result<DependencyPropertyRegistrationResult> rowResult =
            properties.TryRegister(registration);
        CHECK(rowResult);
        row = rowResult.Value().property;
        CHECK(properties.TryAddOwner(
            row,
            buttonType,
            registration.metadata));

        CHECK(types.Freeze());
        CHECK(properties.Freeze());
        CHECK(RegisterScalarTypes());

        CHECK(activations.TryRegister({
            elementType,
            &Fixture::Activate,
            this}));
        CHECK(bridge.TryRegisterType({
            elementType,
            &Fixture::CastDependencyObject,
            nullptr}));
        Result<std::uint32_t> bridged = bridge.TryRegisterProperties();
        CHECK(bridged);
        CHECK(bridged.Value() >= 8U);
        CHECK(bridge.IsTypeRegistered(buttonType));

        XamlTypeAdapterRegistration adapter;
        adapter.type = elementType;
        adapter.beginInit = &TestElement::Begin;
        adapter.endInit = &TestElement::End;
        adapter.abortInit = &TestElement::Abort;
        CHECK(schema.TryRegisterTypeAdapter(adapter));
        CHECK(schema.Freeze());
        CHECK(activations.Freeze());
        return true;
    }
};

Result<Ref<Object>> LoadDocument(
    Fixture& fixture,
    StringView xaml,
    DiagnosticBag& diagnostics,
    const XamlActivationContext& activation) noexcept {
    Utf8XmlTokenizer tokenizer;
    Result<void> reset = tokenizer.Reset(xaml, &diagnostics);
    if (!reset) {
        return reset.GetStatus();
    }
    XamlNodeReader reader(tokenizer, &diagnostics);
    XamlObjectWriter writer(fixture.schema, &diagnostics);
    return LoadXamlWithActivation(
        writer,
        reader,
        fixture.activations,
        activation);
}

bool TestActivationAndAutomaticDependencyProperties() {
    TestElement::ResetCounters();
    {
        Fixture fixture;
        CHECK(fixture.Build());
        DiagnosticBag diagnostics;
        XamlActivationContext activation = fixture.Activation();
        Result<Ref<Object>> loaded = LoadDocument(
            fixture,
            StringView(
                "<Button xmlns=\"urn:dp\" "
                "xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
                "Width=\"250\" Enabled=\"true\" Count=\"-7\" "
                "UnsignedValue=\"42\" Grid.Row=\"3\" "
                "Child=\"{x:Null}\">"
                "<Button.DataContext>"
                "<Button Width=\"25\"/>"
                "</Button.DataContext>"
                "</Button>"),
            diagnostics,
            activation);
        CHECK(loaded);
        CHECK(diagnostics.Size() == 0U);
        CHECK(fixture.activationCount == 2U);
        CHECK(fixture.lastRequestedType == fixture.buttonType);

        Ref<Object> rootObject = std::move(loaded).Value();
        TestElement* root = static_cast<TestElement*>(rootObject.Get());
        CHECK(root != nullptr);
        CHECK(root->RuntimeType() == fixture.buttonType);

        Result<PropertyValue> width = root->GetValue(fixture.width);
        Result<PropertyValue> localWidth = root->ReadLocalValue(fixture.width);
        CHECK(width && width.Value().AsDouble() == 100.0);
        CHECK(localWidth && localWidth.Value().AsDouble() == 250.0);

        Result<PropertyValue> enabled = root->GetValue(fixture.enabled);
        Result<PropertyValue> count = root->GetValue(fixture.count);
        Result<PropertyValue> unsignedValue = root->GetValue(
            fixture.unsignedValue);
        Result<PropertyValue> row = root->GetValue(fixture.row);
        Result<PropertyValue> dataContext = root->GetValue(fixture.dataContext);
        Result<PropertyValue> childValue = root->GetValue(fixture.child);
        CHECK(enabled && enabled.Value().AsBoolean());
        CHECK(count && count.Value().AsSignedInteger() == -7);
        CHECK(unsignedValue && unsignedValue.Value().AsUnsignedInteger() == 42U);
        CHECK(row && row.Value().AsSignedInteger() == 3);
        CHECK(dataContext && dataContext.Value().AsObject());
        CHECK(childValue && childValue.Value().IsNullObject());

        TestElement* child = static_cast<TestElement*>(
            dataContext.Value().AsObject().Get());
        CHECK(child != nullptr);
        CHECK(child->RuntimeType() == fixture.buttonType);
        Result<PropertyValue> childWidth = child->GetValue(fixture.width);
        CHECK(childWidth && childWidth.Value().AsDouble() == 25.0);

        Result<PropertyInvalidationFlags> invalidations =
            root->TakeInvalidations();
        CHECK(invalidations);
        CHECK(HasFlag(
            invalidations.Value(), PropertyInvalidationFlags::Measure));
        CHECK(HasFlag(
            invalidations.Value(), PropertyInvalidationFlags::Arrange));
        CHECK(HasFlag(
            invalidations.Value(), PropertyInvalidationFlags::Render));
        CHECK(HasFlag(
            invalidations.Value(), PropertyInvalidationFlags::Inheritance));

        CHECK(TestElement::BeginCount() == 2U);
        CHECK(TestElement::EndCount() == 2U);
        CHECK(TestElement::AbortCount() == 0U);
        CHECK(TestElement::LiveCount() == 2U);
    }
    CHECK(TestElement::LiveCount() == 0U);
    return true;
}

bool TestReadOnlyRollbackAfterCompletedChild() {
    TestElement::ResetCounters();
    Fixture fixture;
    CHECK(fixture.Build());
    DiagnosticBag diagnostics;
    XamlActivationContext activation = fixture.Activation();
    Result<Ref<Object>> loaded = LoadDocument(
        fixture,
        StringView(
            "<Button xmlns=\"urn:dp\">\n"
