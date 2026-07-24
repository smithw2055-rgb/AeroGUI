#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Presentation/Binding.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

namespace Aero::Markup {

// XAML does not depend on RTTI. Applications register this explicit bridge for
// each family of XAML-created objects that derives from DependencyObject.
using XamlDependencyObjectCastCallback = Core::DependencyObject* (*)(
    Base::Object& object,
    void* context) noexcept;

struct XamlBindingExtensionOptions final {
    Presentation::BindingManager* bindings = nullptr;
    XamlDependencyObjectCastCallback asDependencyObject = nullptr;
    void* castContext = nullptr;
    Core::DependencyPropertyHandle dataContextProperty;
};

// Registers a {Binding ElementName=..., Path=..., Mode=...} provider. When a
// DataContext property is configured, ElementName may be omitted. This initial
// slice resolves one DependencyObject source and a single dependency-property
// path; nested paths, converters, and TwoWay propagation build on this
// type-safe bridge later.
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
