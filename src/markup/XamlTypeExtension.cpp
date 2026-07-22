#include <Aero/Markup/XamlTypeExtension.hpp>

namespace Aero::Markup {
namespace {

Base::Result<Core::TypeId> ResolveType(
    Base::StringView name,
    const XamlServiceProvider& services) noexcept {
    if (services.schema == nullptr || name.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "x:Type requires a type name");
    }

    std::uint32_t colon = name.SizeBytes();
    for (std::uint32_t index = 0U; index < name.SizeBytes(); ++index) {
        if (name[index] == ':') {
            if (colon != name.SizeBytes()) {
                return Base::Status::Failure(
                    Base::ErrorCode::InvalidArgument,
                    "x:Type contains multiple namespace prefixes");
            }
            colon = index;
        }
    }
    Base::StringView prefix;
    Base::StringView localName = name;
    if (colon != name.SizeBytes()) {
        if (colon == 0U || colon + 1U >= name.SizeBytes()) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "x:Type namespace prefix is malformed");
        }
        prefix = name.Substr(0U, colon);
        localName = name.Substr(colon + 1U, name.SizeBytes() - colon - 1U);
    }

    Base::Result<Base::StringView> uri = services.namespaces.Lookup(prefix);
    if (!uri) {
        return uri.GetStatus();
    }
    const Core::TypeInfo* type = services.schema->Types().FindType(
        uri.Value(), localName);
    if (type == nullptr ||
        (static_cast<std::uint32_t>(type->Flags()) &
            static_cast<std::uint32_t>(Core::TypeFlags::ValueType)) != 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            "x:Type target was not found or is not an object type");
    }
    return type->Id();
}

} // namespace

Base::Result<void> XamlTypeExtension::Register(
    XamlSchemaContext& schema,
    Core::TypeId markupExtensionType) noexcept {
    if (schema.IsFrozen() || typeReferenceType_ == Core::InvalidTypeId ||
        markupExtensionType == Core::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "x:Type extension registration is invalid");
    }
    const Core::TypeInfo* token = schema.Types().FindType(typeReferenceType_);
    if (token == nullptr ||
        (static_cast<std::uint32_t>(token->Flags()) &
            static_cast<std::uint32_t>(Core::TypeFlags::ValueType)) == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "x:Type reference token must be a value type");
    }
    return schema.TryRegisterMarkupExtension({
        markupExtensionType, &ProvideValue, this});
}

Base::Result<XamlValue> XamlTypeExtension::ProvideValue(
    Base::StringView arguments,
    const XamlServiceProvider& services,
    void* context) noexcept {
    XamlTypeExtension* extension = static_cast<XamlTypeExtension*>(context);
    if (extension == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "x:Type extension context is invalid");
    }
    Base::Result<Core::TypeId> type = ResolveType(arguments, services);
    if (!type) {
        return type.GetStatus();
    }
    return XamlValue::FromUnsignedInteger(
        extension->typeReferenceType_, type.Value(),
        services.schema != nullptr ? &services.schema->Allocator() : nullptr);
}

} // namespace Aero::Markup
