#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Core/Metadata/Activation.hpp>
#include <Aero/Markup/Runtime/XamlLoadResult.hpp>

#include <cstdint>

namespace Aero::Markup {

class XamlNodeReader;
class XamlObjectWriter;
class XamlSchemaContext;
class XamlCompiledDocument;
inline constexpr std::uint32_t XamlActivationAbiVersion =
    Core::ObjectActivationAbiVersion;

using XamlActivationContext = Core::ObjectActivationContext;
using XamlActivateObjectCallback = Core::ObjectActivateCallback;
using XamlActivationProviderRegistration =
    Core::ObjectActivationProviderRegistration;
using XamlLoadFinalizeCallback = Base::Result<void> (*)(
    XamlLoadResult& result,
    void* context) noexcept;

struct XamlLoadContext final {
    const XamlActivationContext* activation = nullptr;
    // Activation facets are owned by Core and are shared by runtime, compiled
    // XAML, templates, and direct host creation.
    Core::ActivationProviderRegistry* activationFacets = nullptr;
    // Optional application/module resources. Local document scopes take
    // precedence; this dictionary is the final StaticResource fallback.
    const ResourceDictionary* resources = nullptr;
    const Base::ResourceUri* baseUri = nullptr;
    Base::Object* templatedParent = nullptr;
    Base::Ref<Base::Object> existingRoot;
    std::uint32_t maxObjects = UINT32_MAX;
    // Runs while the object-writer transaction is still reversible. URI,
    // resource dependency, and host validation failures therefore roll back
    // LoadComponent mutations through the same AbortInit boundary.
    XamlLoadFinalizeCallback finalize = nullptr;
    void* finalizeContext = nullptr;
};

AERO_API Base::Result<XamlLoadResult>
LoadXamlWithActivation(
    XamlObjectWriter& writer,
    XamlNodeReader& reader,
    Core::ActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept;

AERO_API Base::Result<XamlLoadResult>
LoadXamlWithActivation(
    XamlObjectWriter& writer,
    const XamlCompiledDocument& document,
    Core::ActivationProviderRegistry& providers,
    const XamlActivationContext& activation) noexcept;

} // namespace Aero::Markup
