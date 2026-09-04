// ===== StaticExtension =====






namespace Aero::Markup {
namespace {



} // namespace

Base::Result<void> StaticExtension::Register(
    Schema& schema,
    Meta::TypeId markupExtensionType) noexcept {
    if (schema.IsFrozen() ||
        markupExtensionType == Meta::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "x:Static extension registration is invalid");
    }
    return SchemaPrivate::AddMarkupExtension(schema, {
        markupExtensionType, &ProvideValue, nullptr});
}

Base::Result<ProvidedValue> StaticExtension::ProvideValue(
    Base::StringView arguments,
    const ExtensionServices& services,
    void* context) noexcept {
    if (context != nullptr ||
        services.schema == nullptr ||
        !services.namespaces.IsAvailable()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "x:Static extension context is invalid");
    }

    const Base::StringView expression = TrimAscii(arguments);
    std::uint32_t memberSeparator = expression.SizeBytes();
    for (std::uint32_t index = 0U;
         index < expression.SizeBytes(); ++index) {
        if (expression[index] == '.') memberSeparator = index;
    }
    if (memberSeparator == 0U ||
        memberSeparator + 1U >= expression.SizeBytes()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "x:Static requires Type.Member");
    }

    const Base::StringView qualifiedType =
        expression.Substr(0U, memberSeparator);
    const Base::StringView memberName =
        expression.Substr(
            memberSeparator + 1U,
            expression.SizeBytes() - memberSeparator - 1U);
    std::uint32_t prefixSeparator = qualifiedType.SizeBytes();
    for (std::uint32_t index = 0U;
         index < qualifiedType.SizeBytes(); ++index) {
        if (qualifiedType[index] == ':') {
            if (prefixSeparator != qualifiedType.SizeBytes()) {
                return Base::Status::Failure(
                    Base::ErrorCode::ValidationFailed,
                    "x:Static type has multiple namespace separators");
            }
            prefixSeparator = index;
        }
    }
    Base::StringView prefix;
    Base::StringView typeName = qualifiedType;
    if (prefixSeparator != qualifiedType.SizeBytes()) {
        if (prefixSeparator == 0U ||
            prefixSeparator + 1U >= qualifiedType.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::ValidationFailed,
                "x:Static type name is invalid");
        }
        prefix = qualifiedType.Substr(0U, prefixSeparator);
        typeName = qualifiedType.Substr(
            prefixSeparator + 1U,
            qualifiedType.SizeBytes() - prefixSeparator - 1U);
    }

    Base::Result<Base::StringView> xamlNamespace =
        services.namespaces.Lookup(prefix);
    if (!xamlNamespace) return xamlNamespace.GetStatus();
    Base::Result<const Meta::TypeInfo*> type =
        SchemaPrivate::ResolveType(
            *services.schema,
            xamlNamespace.Value(),
            typeName);
    if (!type) return type.GetStatus();
    if (type.Value()->Kind() == Meta::MetadataTypeKind::Enum) {
        const Meta::EnumValueInfo* value =
            services.schema->Types().FindEnumValue(
                type.Value()->Id(), memberName);
        if (value == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "x:Static enum member was not found");
        }

        const bool signedEnum =
            (static_cast<std::uint32_t>(type.Value()->Flags()) &
             static_cast<std::uint32_t>(Meta::TypeFlags::SignedEnum)) != 0U;
        Meta::Value result = signedEnum
            ? Meta::Value::FromSignedInteger(
                  type.Value()->Id(),
                  static_cast<std::int64_t>(value->RawValue()))
            : Meta::Value::FromUnsignedInteger(
                  type.Value()->Id(),
                  value->RawValue());
        return ProvidedValue::FromValue(std::move(result));
    }

    if (services.targetValueType != Meta::InvalidTypeId &&
        services.schema->Types().IsAssignableFrom(
            services.targetValueType,
            Input::RoutedCommand::StaticTypeId())) {
        Base::Result<Base::Ref<Input::RoutedCommand>> command =
            Input::RoutedCommand::ResolveStatic(
                type.Value()->Id(), memberName);
        if (!command) return command.GetStatus();
        Meta::Value result = Meta::Value::FromObject(
            services.targetValueType,
            Base::Ref<Base::Object>(command.Value()));
        return ProvidedValue::FromValue(std::move(result));
    }

    return Base::Status::Failure(
        Base::ErrorCode::Unsupported,
        "x:Static member is not a registered enum or routed command");
}

} // namespace Aero::Markup
