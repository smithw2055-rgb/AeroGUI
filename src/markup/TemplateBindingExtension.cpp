#include "Extensions.hpp"

#include "SchemaInternal.hpp"

#include "../controls/TemplateInternals.hpp"

#include <Aero/Styling.hpp>
#include <Aero/Meta/ValueConversion.hpp>


namespace Aero::Markup {
namespace {

Base::StringView PropertyLocalName(
    Base::StringView value) noexcept {
    value = Core::ValueConversion::Trim(value);
    std::uint32_t separator = UINT32_MAX;
    for (std::uint32_t index = 0U;
         index < value.SizeBytes();
         ++index) {
        if (value[index] == '.') {
            separator = index;
        }
    }
    return separator == UINT32_MAX
        ? value
        : value.Substr(
              separator + 1U,
              value.SizeBytes() - separator - 1U);
}

const char* MissingPropertyMessage(
    Base::StringView propertyName,
    bool source) noexcept {
    if (propertyName == Base::StringView("Content")) {
        return source
            ? "TemplateBinding source property Content was not found"
            : "TemplateBinding target property for Content was not found";
    }
    if (propertyName == Base::StringView("ContentTemplate")) {
        return source
            ? "TemplateBinding source property ContentTemplate was not found"
            : "TemplateBinding target property for ContentTemplate was not found";
    }
    if (propertyName == Base::StringView("ContentTemplateSelector")) {
        return source
            ? "TemplateBinding source property ContentTemplateSelector was not found"
            : "TemplateBinding target property for ContentTemplateSelector was not found";
    }
    if (propertyName == Base::StringView("CanContentScroll")) {
        return source
            ? "TemplateBinding source property CanContentScroll was not found"
            : "TemplateBinding target property for CanContentScroll was not found";
    }
    if (propertyName == Base::StringView("Padding")) {
        return source
            ? "TemplateBinding source property Padding was not found"
            : "TemplateBinding target property for Padding was not found";
    }
    return source
        ? "TemplateBinding source property was not found"
        : "TemplateBinding target property was not found";
}

} // namespace

Base::Result<void> TemplateBindingExtension::Register(
    Schema& schema,
    Core::TypeId markupExtensionType) noexcept {
    if (schema.IsFrozen() ||
        markupExtensionType == Core::InvalidTypeId) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TemplateBinding extension registration is invalid");
    }
    return Detail::SchemaPrivate::AddMarkupExtension(
        schema,
        {markupExtensionType, &ProvideValue, nullptr});
}

Base::Result<ProvidedValue>
TemplateBindingExtension::ProvideValue(
    Base::StringView arguments,
    const ExtensionServices& services,
    void* context) noexcept {
    if (context != nullptr ||
        services.schema == nullptr ||
        services.targetObject == nullptr ||
        services.deferredContentOwner == nullptr ||
        services.nameScope == nullptr ||
        services.targetMember ==
            Core::InvalidMemberId ||
        services.deferredContentOwner->RuntimeType() !=
            Controls::ControlTemplate::StaticTypeId()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "TemplateBinding requires an active ControlTemplate target");
    }

    const Base::StringView propertyName =
        PropertyLocalName(arguments);
    if (propertyName.Empty()) {
        return Base::Status::Failure(
            Base::ErrorCode::ValidationFailed,
            "TemplateBinding requires a source property");
    }

    auto& controlTemplate =
        static_cast<Controls::ControlTemplate&>(
            *services.deferredContentOwner);
    Base::Result<::Aero::DependencyObject*> target =
        Detail::SchemaPrivate::ResolvePropertyTarget(
            *services.schema,
            *services.targetObject);
    if (!target) return target.GetStatus();

    const Core::DependencyProperty* source =
        target.Value()->PropertyRegistry().Find(
            controlTemplate.GetTargetType(),
            propertyName);
    const Core::DependencyProperty* destination =
        target.Value()->PropertyRegistry().Find(
            Core::DependencyPropertyHandle{
                services.targetMember});
    if (source == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            MissingPropertyMessage(propertyName, true));
    }
    if (destination == nullptr ||
        destination->MetadataFor(
            target.Value()->RuntimeType()) == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::NotFound,
            MissingPropertyMessage(propertyName, false));
    }
    Base::String targetName;
    Base::StringView authoredName =
        services.nameScope->NameOf(
            *services.targetObject);
    if (authoredName.Empty()) {
        Base::Result<Base::String> generated =
            Controls::Detail::TemplatePrivate::EnsureAuthoredName(controlTemplate,
                *services.targetObject);
        if (!generated) {
            return generated.GetStatus();
        }
        targetName =
            std::move(generated).Value();
        authoredName = targetName.View();
    }
    Base::Result<void> added =
        Controls::Detail::TemplatePrivate::TryAddTemplateBinding(controlTemplate,
            authoredName,
            source->Handle(),
            destination->Handle());
    return added
        ? Base::Result<ProvidedValue>(
              ProvidedValue::Handled())
        : Base::Result<ProvidedValue>(
              added.GetStatus());
}

} // namespace Aero::Markup
