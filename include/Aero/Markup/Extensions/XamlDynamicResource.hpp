#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Markup/Resources/XamlNamesResources.hpp>
#include <Aero/Markup/Schema/XamlSchemaContext.hpp>

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
    static Base::Result<void> Attach(
        Core::EffectiveValueEngine& effectiveValues,
        Base::Span<const ResourceDictionary* const> resourceChain,
        ResourceDictionary* fallbackResources,
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        Base::StringView key) noexcept;
    static Base::Result<Core::PropertyExpression> CreateExpression(
        Core::EffectiveValueEngine& effectiveValues,
        Base::Span<const ResourceDictionary* const> resourceChain,
        ResourceDictionary* fallbackResources,
        Core::DependencyObject& target,
        Core::DependencyPropertyHandle property,
        Base::StringView key) noexcept;
};

struct XamlDynamicResourceExtensionOptions final {
    XamlDynamicResourceExtensionOptions() noexcept = default;
    XamlDynamicResourceExtensionOptions(
        Core::EffectiveValueEngine* effectiveValueEngine,
        ResourceDictionary* resourceDictionary) noexcept
        : effectiveValues(effectiveValueEngine),
          resources(resourceDictionary) {}

    Core::EffectiveValueEngine* effectiveValues = nullptr;
    ResourceDictionary* resources = nullptr;
};

// Registers {DynamicResource key}. Active local resource scopes are captured
// from the writer service provider and followed by the host-owned fallback
// environment, preserving element-to-application lookup order.
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

    static Base::Result<XamlProvidedValue> ProvideValue(
        Base::StringView arguments,
        const XamlServiceProvider& services,
        void* context) noexcept;
};

} // namespace Aero::Markup
