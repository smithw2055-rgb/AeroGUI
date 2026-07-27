#include <Aero/Markup/Runtime/XamlActivation.hpp>

#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Presentation/Metadata.hpp>
#include <Aero/Markup/Compiled/XamlCompiledDocument.hpp>
#include <Aero/Markup/Runtime/XamlObjectWriter.hpp>
#include <Aero/Markup/Schema/XamlSchemaContext.hpp>

namespace Aero::Markup {
namespace {

Base::Result<void> ValidateLoadActivation(
    Core::ActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept {
    if (!activation.IsCompatible()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML activation context is incompatible");
    }
    if (!providers.IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML activation facet registry is not frozen");
    }
    if (activation.dispatcher == nullptr ||
        activation.dependencyProperties == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML activation context has no presentation services");
    }
    return {};
}

} // namespace

Base::Result<XamlLoadResult> LoadXamlWithActivation(
    XamlObjectWriter& writer,
    XamlNodeReader& reader,
    Core::ActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept {
    Base::Result<void> valid = ValidateLoadActivation(providers, activation);
    if (!valid) return valid.GetStatus();
    Core::ObjectServicesScope objectServices(
        *activation.dispatcher,
        *activation.dependencyProperties,
        writer.Schema().Runtime());
    XamlLoadContext context;
    context.activation = &activation;
    context.activationFacets = &providers;
    return writer.LoadDocument(reader, context);
}

Base::Result<XamlLoadResult> LoadXamlWithActivation(
    XamlObjectWriter& writer,
    const XamlCompiledDocument& document,
    Core::ActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept {
    Base::Result<void> valid = ValidateLoadActivation(providers, activation);
    if (!valid) return valid.GetStatus();
    Core::ObjectServicesScope objectServices(
        *activation.dispatcher,
        *activation.dependencyProperties,
        writer.Schema().Runtime());
    XamlLoadContext context;
    context.activation = &activation;
    context.activationFacets = &providers;
    return writer.LoadDocument(document, context);
}

} // namespace Aero::Markup
