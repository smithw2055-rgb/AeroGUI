#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Markup/XamlDependencyObjectResolver.hpp>
#include <Aero/Markup/XamlNamesResources.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

namespace Aero::Markup {

// Installs a local DynamicResource expression. The dictionary must outlive the
// expression, or the host must clear/detach the target property first.
class AERO_API DynamicResource final {
public:
    static Base::Result<void> Attach(
        Core::EffectiveValueEngine& effectiveValues,
        ResourceDictionary& resources,
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        Base::StringView key) noexcept;
};

struct XamlDynamicResourceExtensionOptions final {
    XamlDynamicResourceExtensionOptions() noexcept = default;
    XamlDynamicResourceExtensionOptions(
        Core::EffectiveValueEngine* effectiveValueEngine,
        ResourceDictionary* resourceDictionary,
        XamlDependencyObjectResolver resolver) noexcept
        : effectiveValues(effectiveValueEngine),
          resources(resourceDictionary),
          targetResolver(resolver) {}
    XamlDynamicResourceExtensionOptions(
        Core::EffectiveValueEngine* effectiveValueEngine,
        ResourceDictionary* resourceDictionary,
        XamlDependencyObjectCastCallback cast,
        void* castContext) noexcept
        : XamlDynamicResourceExtensionOptions(
            effectiveValueEngine,
            resourceDictionary,
            XamlDependencyObjectResolver{cast, castContext}) {}

    Core::EffectiveValueEngine* effectiveValues = nullptr;
    ResourceDictionary* resources = nullptr;
    XamlDependencyObjectResolver targetResolver;
};

// Registers {DynamicResource key}. This initial application-resource slice
// intentionally resolves against the supplied live dictionary; nested/local
// resource scope selection will extend the same expression contract later.
class AERO_API XamlDynamicResourceExtension final {
public:
    explicit XamlDynamicResourceExtension(
        const XamlDynamicResourceExtensionOptions& options) noexcept;

    XamlDynamicResourceExtension(const XamlDynamicResourceExtension&) = delete;
    XamlDynamicResourceExtension& operator=(
        const XamlDynamicResourceExtension&) = delete;

    Base::Result<void> Register(
        XamlSchemaContext& schema,
        Core::TypeId dynamicResourceExtensionType) noexcept;

private:
    XamlDynamicResourceExtensionOptions options_;

    static Base::Result<XamlValue> ProvideValue(
        Base::StringView arguments,
        const XamlServiceProvider& services,
        void* context) noexcept;
};

} // namespace Aero::Markup
