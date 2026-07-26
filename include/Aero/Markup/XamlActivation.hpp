#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Core/Metadata/Activation.hpp>
#include <Aero/Markup/XamlLoadResult.hpp>

#include <cstdint>

namespace Aero::Markup {

class XamlNodeReader;
class XamlObjectWriter;
class XamlSchemaContext;
class XamlCompiledDocument;
class ResourceDictionary;

inline constexpr std::uint32_t XamlActivationAbiVersion =
    Core::ObjectActivationAbiVersion;

using XamlActivationContext = Core::ObjectActivationContext;
using XamlActivateObjectCallback = Core::ObjectActivateCallback;
using XamlActivationProviderRegistration =
    Core::ObjectActivationProviderRegistration;

class XamlActivationProviderRegistry;

struct XamlLoadContext final {
    XamlActivationProviderRegistry* activationProviders = nullptr;
    const XamlActivationContext* activation = nullptr;
    // Optional application/module resources. Local document scopes take
    // precedence; this dictionary is the final StaticResource fallback.
    const ResourceDictionary* resources = nullptr;
};

// Schema-bound XAML integration over the shared Core activation registry.
// Provider storage, inheritance lookup and runtime-type validation remain
// single-sourced in Core::ActivationProviderRegistry.
class AERO_API XamlActivationProviderRegistry final {
public:
    explicit XamlActivationProviderRegistry(
        XamlSchemaContext& schema) noexcept;

    XamlActivationProviderRegistry(
        const XamlActivationProviderRegistry&) = delete;
    XamlActivationProviderRegistry& operator=(
        const XamlActivationProviderRegistry&) = delete;

    Base::Result<void> TryRegister(
        const XamlActivationProviderRegistration& registration) noexcept;
    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return providers_.IsFrozen(); }
    std::uint32_t ProviderCount() const noexcept {
        return providers_.ProviderCount();
    }
    XamlSchemaContext& Schema() const noexcept {
        return *schema_;
    }
    Core::ActivationProviderRegistry& CoreProviders() noexcept {
        return providers_;
    }
    const Core::ActivationProviderRegistry& CoreProviders() const noexcept {
        return providers_;
    }

    Base::Result<Base::Ref<Base::Object>> CreateObject(
        Core::TypeId requestedType,
        const XamlActivationContext& activation) const noexcept;

private:
    XamlSchemaContext* schema_ = nullptr;
    Core::ActivationProviderRegistry providers_;
};

AERO_API Base::Result<Base::Ref<Base::Object>>
LoadXamlWithActivation(
    XamlObjectWriter& writer,
    XamlNodeReader& reader,
    XamlActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept;

AERO_API Base::Result<Base::Ref<Base::Object>>
LoadXamlWithActivation(
    XamlObjectWriter& writer,
    const XamlCompiledDocument& document,
    XamlActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept;

AERO_API Base::Result<XamlLoadResult>
LoadXamlDocumentWithActivation(
    XamlObjectWriter& writer,
    XamlNodeReader& reader,
    XamlActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept;

AERO_API Base::Result<XamlLoadResult>
LoadXamlDocumentWithActivation(
    XamlObjectWriter& writer,
    const XamlCompiledDocument& document,
    XamlActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept;

} // namespace Aero::Markup
