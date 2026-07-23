#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Core/Activation.hpp>

#include <cstdint>

namespace Aero::Markup {

class XamlNodeReader;
class XamlObjectWriter;
class XamlSchemaContext;

inline constexpr std::uint32_t XamlActivationAbiVersion =
    Core::ObjectActivationAbiVersion;

using XamlActivationContext = Core::ObjectActivationContext;
using XamlActivateObjectCallback = Core::ObjectActivateCallback;
using XamlActivationProviderRegistration =
    Core::ObjectActivationProviderRegistration;

// XAML keeps a schema-bound façade for compatibility, while provider storage,
// inherited lookup and activation validation are owned by the shared Core
// ActivationProviderRegistry.
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

} // namespace Aero::Markup
