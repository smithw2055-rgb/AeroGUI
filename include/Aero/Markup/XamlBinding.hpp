#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Presentation/Binding.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Markup/XamlDependencyObjectResolver.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

namespace Aero::Markup {

struct XamlBindingExtensionOptions final {
    Presentation::BindingManager* bindings = nullptr;
    XamlDependencyObjectResolver targetResolver;
    Core::DependencyPropertyHandle dataContextProperty;
};

// Registers a {Binding ElementName=..., Path=..., Mode=...} provider. Explicit
// ElementName wins over DataContext. Paths are compiled to immutable metadata
// plans; DataContext paths are resolved after tree attachment and recompiled
// only when the concrete source type changes.
class AERO_API XamlBindingExtension final {
public:
    explicit XamlBindingExtension(
        const XamlBindingExtensionOptions& options) noexcept;

    XamlBindingExtension(const XamlBindingExtension&) = delete;
    XamlBindingExtension& operator=(const XamlBindingExtension&) = delete;

    Base::Result<void> Register(
        XamlSchemaContext& schema,
        Core::TypeId bindingExtensionType) noexcept;
    void SetDataContextProperty(
        Core::DependencyPropertyHandle property) noexcept {
        options_.dataContextProperty = property;
    }

private:
    XamlBindingExtensionOptions options_;

    static Base::Result<XamlValue> ProvideValue(
        Base::StringView arguments,
        const XamlServiceProvider& services,
        void* context) noexcept;
};

} // namespace Aero::Markup
