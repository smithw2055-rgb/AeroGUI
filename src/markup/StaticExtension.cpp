#include "Extensions.hpp"

#include "SchemaInternal.hpp"


namespace Aero::Markup {
namespace {

Base::StringView TrimAscii(Base::StringView value) noexcept {
    std::uint32_t begin = 0U;
    std::uint32_t end = value.SizeBytes();
    while (begin < end &&
           (value[begin] == ' ' || value[begin] == '\t' ||
            value[begin] == '\r' || value[begin] == '\n')) {
        ++begin;
    }
    while (end > begin &&
           (value[end - 1U] == ' ' || value[end - 1U] == '\t' ||
            value[end - 1U] == '\r' || value[end - 1U] == '\n')) {
        --end;
    }
    return value.Substr(begin, end - begin);
}

} // namespace

Base::Result<void> StaticExtension::Register(
    Schema& schema,
    Core::TypeId markupExtensionType) noexcept {
    if (schema.IsFrozen() ||
        markupExtensionType == Core::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "x:Static extension registration is invalid");
    }
    return Detail::SchemaPrivate::AddMarkupExtension(schema, {
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
    Base::Result<const Core::TypeInfo*> type =
        Detail::SchemaPrivate::ResolveType(
            *services.schema,
            xamlNamespace.Value(),
            typeName);
    if (!type) return type.GetStatus();
    if (type.Value()->Kind() != Core::MetadataTypeKind::Enum) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "x:Static currently supports registered enum members");
    }
    const Core::EnumValueInfo* value =
        services.schema->Types().FindEnumValue(
            type.Value()->Id(), memberName);
    if (value == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "x:Static enum member was not found");
    }

    const bool signedEnum =
        (static_cast<std::uint32_t>(type.Value()->Flags()) &
         static_cast<std::uint32_t>(Core::TypeFlags::SignedEnum)) != 0U;
    Core::Value result = signedEnum
        ? Core::Value::FromSignedInteger(
              type.Value()->Id(),
              static_cast<std::int64_t>(value->RawValue()))
        : Core::Value::FromUnsignedInteger(
              type.Value()->Id(),
              value->RawValue());
    return ProvidedValue::FromValue(std::move(result));
}

} // namespace Aero::Markup
