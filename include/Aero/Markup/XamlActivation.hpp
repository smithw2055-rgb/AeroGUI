#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Core/TypeRegistry.hpp>

#include <cstdint>

namespace Aero::Core {
class DependencyPropertyRegistry;
class Dispatcher;
}

namespace Aero::Markup {

class XamlNodeReader;
class XamlObjectWriter;
class XamlSchemaContext;

inline constexpr std::uint32_t XamlActivationAbiVersion = 1U;

struct XamlActivationContext final {
    std::uint32_t structSize = 0U;
    std::uint32_t abiVersion = XamlActivationAbiVersion;
    Core::Dispatcher* dispatcher = nullptr;
    Core::DependencyPropertyRegistry* dependencyProperties = nullptr;
    void* applicationServices = nullptr;
    void* hostContext = nullptr;

    static XamlActivationContext Create() noexcept {
        XamlActivationContext context;
        context.structSize = static_cast<std::uint32_t>(
            sizeof(XamlActivationContext));
        return context;
    }

    bool IsCompatible() const noexcept {
        return structSize >= static_cast<std::uint32_t>(
            sizeof(XamlActivationContext)) &&
            abiVersion == XamlActivationAbiVersion;
    }
};

using XamlActivateObjectCallback = Base::Result<Base::Ref<Base::Object>> (*)(
    Core::TypeId requestedType,
    const XamlActivationContext& activation,
    Base::IAllocator& allocator,
    void* context) noexcept;

struct XamlActivationProviderRegistration final {
    Core::TypeId type = Core::InvalidTypeId;
    XamlActivateObjectCallback activate = nullptr;
    void* context = nullptr;
};

class AERO_API XamlActivationProviderRegistry final {
public:
    explicit XamlActivationProviderRegistry(
        XamlSchemaContext& schema,
        Base::IAllocator* allocator = nullptr) noexcept;

    XamlActivationProviderRegistry(
        const XamlActivationProviderRegistry&) = delete;
    XamlActivationProviderRegistry& operator=(
        const XamlActivationProviderRegistry&) = delete;

    Base::Result<void> TryRegister(
        const XamlActivationProviderRegistration& registration) noexcept;
    Base::Result<void> Freeze() noexcept;

    bool IsFrozen() const noexcept { return frozen_; }
    std::uint32_t ProviderCount() const noexcept {
        return providers_.Size();
    }
    XamlSchemaContext& Schema() const noexcept {
        return *schema_;
    }

    Base::Result<Base::Ref<Base::Object>> CreateObject(
        Core::TypeId requestedType,
        const XamlActivationContext& activation) const noexcept;

private:
    XamlSchemaContext* schema_ = nullptr;
    Base::IAllocator* allocator_ = nullptr;
    Base::Vector<XamlActivationProviderRegistration> providers_;
    bool frozen_ = false;

    const XamlActivationProviderRegistration* FindExact(
        Core::TypeId type) const noexcept;
    const XamlActivationProviderRegistration* Find(
        Core::TypeId type) const noexcept;
};

AERO_API Base::Result<Base::Ref<Base::Object>>
LoadXamlWithActivation(
    XamlObjectWriter& writer,
    XamlNodeReader& reader,
    XamlActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept;

} // namespace Aero::Markup
