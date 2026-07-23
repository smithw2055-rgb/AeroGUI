#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/DependencyProperty.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

#include <cstdint>

namespace Aero::Markup {

class XamlActivationProviderRegistry;
class XamlVisualTreeHost;

using XamlAsDependencyObjectCallback = Core::DependencyObject* (*)(
    Base::Object& object,
    void* context) noexcept;

struct XamlDependencyObjectTypeRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    XamlAsDependencyObjectCallback cast = nullptr;
    void* context = nullptr;
};

class AERO_API XamlDependencyPropertyBridge final {
public:
    XamlDependencyPropertyBridge(
        XamlSchemaContext& schema,
        Core::DependencyPropertyRegistry& properties,
        Base::IAllocator* allocator = nullptr) noexcept;

    XamlDependencyPropertyBridge(const XamlDependencyPropertyBridge&) = delete;
    XamlDependencyPropertyBridge& operator=(
        const XamlDependencyPropertyBridge&) = delete;

    Base::Result<void> TryRegisterType(
        const XamlDependencyObjectTypeRegistration& registration) noexcept;
    Base::Result<std::uint32_t> TryRegisterProperties() noexcept;

    bool IsTypeRegistered(Core::TypeId type) const noexcept;
    std::uint32_t RegisteredTypeCount() const noexcept {
        return types_.Size();
    }
    std::uint32_t RegisteredPropertyCount() const noexcept {
        return registeredPropertyCount_;
    }

private:
    XamlSchemaContext* schema_ = nullptr;
    Core::DependencyPropertyRegistry* properties_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<XamlDependencyObjectTypeRegistration> types_;
    std::uint32_t registeredPropertyCount_ = 0U;
    bool providerRegistered_ = false;

    const XamlDependencyObjectTypeRegistration*
    FindTypeRegistration(Core::TypeId type) const noexcept;
    Base::Result<Core::PropertyValue> ConvertValue(
        const XamlValue& value,
        const Core::DependencyProperty& property) const noexcept;

    static bool HandlesDependencyProperty(
        const XamlResolvedMember& member,
        void* context) noexcept;

    static Base::Result<void> SetDependencyProperty(
        Base::Object& object,
        const XamlValue& value,
        const XamlServiceProvider& services,
        void* context) noexcept;
};

// Registers the canonical DependencyObject base cast and the single generic
// dependency-property member provider. Custom derived controls require no
// additional DP bridge registration.
AERO_API Base::Result<std::uint32_t>
TryRegisterCorePresentationXaml(
    XamlDependencyPropertyBridge& bridge) noexcept;

// Registers core control activation factories and, when supplied, the exact
// structural adapters for Content/Children ownership transactions.
AERO_API Base::Result<std::uint32_t>
TryRegisterCorePresentationXaml(
    XamlDependencyPropertyBridge& bridge,
    XamlActivationProviderRegistry& activation,
    XamlVisualTreeHost* visualTree = nullptr) noexcept;

} // namespace Aero::Markup
