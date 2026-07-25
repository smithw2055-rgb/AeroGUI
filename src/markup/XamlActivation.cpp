#include <Aero/Markup/XamlActivation.hpp>

#include <Aero/Core/ObjectServices.hpp>
#include <Aero/Presentation/Metadata.hpp>
#include <Aero/Markup/XamlCompiledDocument.hpp>
#include <Aero/Markup/XamlObjectWriter.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

namespace Aero::Markup {
namespace {

struct ActiveActivation final {
    XamlActivationProviderRegistry* providers = nullptr;
    const XamlActivationContext* context = nullptr;
};

thread_local ActiveActivation gActiveActivation;

class ActiveActivationScope final {
public:
    ActiveActivationScope(
        XamlActivationProviderRegistry& providers,
        const XamlActivationContext& context) noexcept
        : previous_(gActiveActivation) {
        gActiveActivation.providers = &providers;
        gActiveActivation.context = &context;
    }

    ~ActiveActivationScope() {
        gActiveActivation = previous_;
    }

    ActiveActivationScope(const ActiveActivationScope&) = delete;
    ActiveActivationScope& operator=(const ActiveActivationScope&) = delete;

private:
    ActiveActivation previous_;
};

} // namespace

XamlActivationProviderRegistry::XamlActivationProviderRegistry(
    XamlSchemaContext& schema) noexcept
    : schema_(&schema),
      providers_(schema.Descriptors()) {}

Base::Result<void> XamlActivationProviderRegistry::TryRegister(
    const XamlActivationProviderRegistration& registration) noexcept {
    return providers_.TryRegister(registration);
}

Base::Result<void> XamlActivationProviderRegistry::Freeze() noexcept {
    if (providers_.IsFrozen()) {
        return {};
    }
    if (!schema_->IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML schema context must be frozen before activation facets");
    }
    return providers_.Freeze();
}

Base::Result<Base::Ref<Base::Object>>
XamlActivationProviderRegistry::CreateObject(
    Core::TypeId requestedType,
    const XamlActivationContext& activation) const noexcept {
    if (!providers_.IsFrozen()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "XAML activation facet registry is not frozen");
    }
    if (!activation.IsCompatible()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML activation context is incompatible");
    }
    if (activation.dispatcher == nullptr ||
        activation.dependencyProperties == nullptr) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "XAML activation context has no presentation services");
    }

    Core::ObjectServicesScope presentationScope(
        *activation.dispatcher,
        *activation.dependencyProperties,
        schema_->Runtime());

    if (providers_.Find(requestedType) == nullptr) {
        return schema_->CreateObject(requestedType);
    }
    return providers_.CreateObject(requestedType, activation);
}

Base::Result<Base::Ref<Base::Object>> XamlSchemaContext::CreateObjectActivated(
    Core::TypeId type) const noexcept {
    if (gActiveActivation.providers == nullptr) {
        return CreateObject(type);
    }
    if (gActiveActivation.context == nullptr ||
        &gActiveActivation.providers->Schema() != this) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidState,
            "Active XAML activation provider belongs to another schema");
    }
    return gActiveActivation.providers->CreateObject(
        type,
        *gActiveActivation.context);
}

Base::Result<Base::Ref<Base::Object>> LoadXamlWithActivation(
    XamlObjectWriter& writer,
    XamlNodeReader& reader,
    XamlActivationProviderRegistry& providers,
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
    Core::ObjectServicesScope objectServices(
        *activation.dispatcher,
        *activation.dependencyProperties,
        providers.Schema().Runtime());
    ActiveActivationScope scope(providers, activation);
    return writer.Load(reader);
}

Base::Result<Base::Ref<Base::Object>> LoadXamlWithActivation(
    XamlObjectWriter& writer,
    const XamlCompiledDocument& document,
    XamlActivationProviderRegistry& providers,
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
    Core::ObjectServicesScope objectServices(
        *activation.dispatcher,
        *activation.dependencyProperties,
        providers.Schema().Runtime());
    ActiveActivationScope scope(providers, activation);
    return writer.Load(document);
}

} // namespace Aero::Markup
