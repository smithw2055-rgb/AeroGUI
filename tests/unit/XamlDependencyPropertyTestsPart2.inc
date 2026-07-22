    static DependencyObject* CastDependencyObject(
        Object& object,
        void*) noexcept {
        return &static_cast<TestElement&>(object);
    }

    XamlActivationContext Activation() noexcept {
        XamlActivationContext activation = XamlActivationContext::Create();
        activation.dispatcher = &dispatcher;
        activation.dependencyProperties = &properties;
        activation.applicationServices = &applicationMarker;
        activation.hostContext = &hostMarker;
        return activation;
    }

    bool RegisterScalarTypes() {
        CHECK(schema.TryRegisterScalarType(
            booleanType,
            XamlScalarKind::Boolean));
        CHECK(schema.TryRegisterScalarType(
            integerType,
            XamlScalarKind::SignedInteger));
        CHECK(schema.TryRegisterScalarType(
            unsignedType,
            XamlScalarKind::UnsignedInteger));
        CHECK(schema.TryRegisterScalarType(
            doubleType,
            XamlScalarKind::Double));
        return true;
    }

    bool Build() {
        const StringView ns("urn:dp");
        objectType = MakeTypeId(ns, StringView("Object"));
        booleanType = MakeTypeId(ns, StringView("Boolean"));
        integerType = MakeTypeId(ns, StringView("Int64"));
        unsignedType = MakeTypeId(ns, StringView("UInt64"));
        doubleType = MakeTypeId(ns, StringView("Double"));
        elementType = MakeTypeId(ns, StringView("Element"));
        buttonType = MakeTypeId(ns, StringView("Button"));
        gridType = MakeTypeId(ns, StringView("Grid"));

        CHECK(types.TryRegisterType({
            ns, StringView("Object"), InvalidTypeId,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({
            ns, StringView("Boolean"), InvalidTypeId,
            TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
        CHECK(types.TryRegisterType({
            ns, StringView("Int64"), InvalidTypeId,
            TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
        CHECK(types.TryRegisterType({
            ns, StringView("UInt64"), InvalidTypeId,
            TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
        CHECK(types.TryRegisterType({
            ns, StringView("Double"), InvalidTypeId,
            TypeFlags::ValueType | TypeFlags::Sealed, nullptr}));
        CHECK(types.TryRegisterType({
            ns, StringView("Element"), objectType,
            TypeFlags::None, nullptr}));
        CHECK(types.TryRegisterType({
            ns, StringView("Button"), elementType,
            TypeFlags::Sealed, nullptr}));
        CHECK(types.TryRegisterType({
            ns, StringView("Grid"), objectType,
            TypeFlags::None, nullptr}));

        DependencyPropertyRegistration registration;
        registration.name = StringView("Width");
        registration.ownerType = elementType;
        registration.valueType = doubleType;
        registration.metadata.defaultValue =
            PropertyValue::FromDouble(doubleType, 10.0);
        registration.metadata.flags =
            PropertyMetadataFlags::AffectsMeasure |
            PropertyMetadataFlags::AffectsArrange |
            PropertyMetadataFlags::AffectsRender;
        registration.metadata.validate = &ValidateWidth;
        registration.metadata.coerce = &CoerceWidth;
        Result<DependencyPropertyRegistrationResult> widthResult =
            properties.TryRegister(registration);
        CHECK(widthResult);
        width = widthResult.Value().property;

        registration = {};
        registration.name = StringView("Enabled");
        registration.ownerType = elementType;
        registration.valueType = booleanType;
        registration.metadata.defaultValue =
            PropertyValue::FromBoolean(booleanType, false);
        Result<DependencyPropertyRegistrationResult> enabledResult =
            properties.TryRegister(registration);
        CHECK(enabledResult);
        enabled = enabledResult.Value().property;

        registration = {};
        registration.name = StringView("Count");
        registration.ownerType = elementType;
        registration.valueType = integerType;
        registration.metadata.defaultValue =
            PropertyValue::FromSignedInteger(integerType, 0);
        Result<DependencyPropertyRegistrationResult> countResult =
            properties.TryRegister(registration);
        CHECK(countResult);
        count = countResult.Value().property;

        registration = {};
        registration.name = StringView("UnsignedValue");
        registration.ownerType = elementType;
        registration.valueType = unsignedType;
        registration.metadata.defaultValue =
            PropertyValue::FromUnsignedInteger(unsignedType, 0U);
        Result<DependencyPropertyRegistrationResult> unsignedResult =
            properties.TryRegister(registration);
        CHECK(unsignedResult);
        unsignedValue = unsignedResult.Value().property;

        registration = {};
        registration.name = StringView("DataContext");
        registration.ownerType = elementType;
        registration.valueType = objectType;
        registration.metadata.defaultValue = PropertyValue::NullObject(objectType);
        registration.metadata.flags = PropertyMetadataFlags::Inherits;
        Result<DependencyPropertyRegistrationResult> contextResult =
            properties.TryRegister(registration);
        CHECK(contextResult);
        dataContext = contextResult.Value().property;

        registration = {};
        registration.name = StringView("Child");
        registration.ownerType = elementType;
        registration.valueType = objectType;
        registration.metadata.defaultValue = PropertyValue::NullObject(objectType);
        Result<DependencyPropertyRegistrationResult> childResult =
            properties.TryRegister(registration);
        CHECK(childResult);
        child = childResult.Value().property;

        registration = {};
        registration.name = StringView("IsLocked");
        registration.ownerType = elementType;
        registration.valueType = booleanType;
        registration.flags = DependencyPropertyFlags::ReadOnly;
        registration.metadata.defaultValue =
