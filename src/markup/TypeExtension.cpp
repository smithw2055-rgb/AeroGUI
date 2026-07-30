#include "Extensions.hpp"

#include <utility>
#include "SchemaInternal.hpp"
#include "../presentation/RuntimeManagers.hpp"

namespace Aero::Markup {
Base::Result<void> TypeExtension::Register(
    Schema& schema,
    Core::TypeId markupExtensionType) noexcept {
    if (schema.IsFrozen() ||
        markupExtensionType == Core::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "x:Type extension registration is invalid");
    }
    const Core::TypeInfo* token =
        schema.Types().FindType(
            Core::TypeOf<Core::TypeReference>());
    if (token == nullptr ||
        (static_cast<std::uint32_t>(token->Flags()) &
            static_cast<std::uint32_t>(Core::TypeFlags::ValueType)) == 0U) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "x:Type reference token must be a value type");
    }
    return Detail::SchemaAccess::AddMarkupExtension(schema, {
        markupExtensionType, &ProvideValue, nullptr});
}

Base::Result<ProvidedValue> TypeExtension::ProvideValue(
    Base::StringView arguments,
    const ExtensionContext& services,
    void* context) noexcept {
    if (context != nullptr || services.schema == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "x:Type extension context is invalid");
    }
    Base::Result<Core::Value> value =
        Detail::SchemaAccess::ConvertText(
            *services.schema,
            Core::TypeOf<Core::TypeReference>(),
            arguments,
            &services);
    return value
        ? ProvidedValue::FromValue(
              std::move(value).Value())
        : Base::Result<ProvidedValue>(
              value.GetStatus());
}

} // namespace Aero::Markup
