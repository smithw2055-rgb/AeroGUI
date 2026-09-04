// ===== TypeExtension =====






namespace Aero::Markup {
Base::Result<void> TypeExtension::Register(
    Schema& schema,
    Meta::TypeId markupExtensionType) noexcept {
    if (schema.IsFrozen() ||
        markupExtensionType == Meta::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "x:Type extension registration is invalid");
    }
    const Meta::TypeInfo* token =
        schema.Types().FindType(
            Meta::TypeOf<Meta::TypeReference>());
    if (token == nullptr ||
        (static_cast<std::uint32_t>(token->Flags()) &
            static_cast<std::uint32_t>(Meta::TypeFlags::ValueType)) == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "x:Type reference token must be a value type");
    }
    return SchemaPrivate::AddMarkupExtension(schema, {
        markupExtensionType, &ProvideValue, nullptr});
}

Base::Result<ProvidedValue> TypeExtension::ProvideValue(
    Base::StringView arguments,
    const ExtensionServices& services,
    void* context) noexcept {
    if (context != nullptr || services.schema == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "x:Type extension context is invalid");
    }
    Base::Result<Meta::Value> value =
        SchemaPrivate::ConvertText(
            *services.schema,
            Meta::TypeOf<Meta::TypeReference>(),
            arguments,
            &services);
    return value
        ? ProvidedValue::FromValue(
              std::move(value).Value())
        : Base::Result<ProvidedValue>(
              value.GetStatus());
}

} // namespace Aero::Markup
