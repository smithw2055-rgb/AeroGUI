#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Hash.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Controls/Metadata.hpp>
#include <Aero/Markup/XamlActivation.hpp>
#include <Aero/Markup/XamlSchemaContext.hpp>

namespace Aero::Markup {

using XamlModuleConfigureCallback = Base::Result<void> (*)(
    XamlSchemaContext& schema,
    XamlActivationProviderRegistry& activation,
    void* context) noexcept;

struct XamlModuleManifest final {
    Base::StringView name;
    std::uint32_t metadataSchemaVersion = 1U;
    std::uint32_t xamlSchemaVersion = 1U;
    Core::MetadataModuleRegisterCallback registerMetadata = nullptr;
    XamlModuleConfigureCallback configureXaml = nullptr;
    void* context = nullptr;
};

// One catalog is shared by runtime startup and aero-xamlc. Its stable hash is
// embedded into compiled XAML so a tool/runtime module mismatch is rejected
// before replay.
class AERO_API XamlModuleCatalog final {
public:
    Base::Result<void> TryAdd(
        const XamlModuleManifest& manifest) noexcept;
    Base::Result<void> RegisterMetadata(
        Core::MetadataDomain& domain,
        bool includeBuiltInUi = true) const noexcept;
    Base::Result<void> ConfigureXaml(
        XamlSchemaContext& schema,
        XamlActivationProviderRegistry& activation) const noexcept;
    Base::HashCode ManifestHash() const noexcept;
    std::uint32_t ModuleCount() const noexcept {
        return modules_.Size();
    }

private:
    struct Module final {
        Base::String name;
        std::uint32_t metadataSchemaVersion = 1U;
        std::uint32_t xamlSchemaVersion = 1U;
        Core::MetadataModuleRegisterCallback registerMetadata = nullptr;
        XamlModuleConfigureCallback configureXaml = nullptr;
        void* context = nullptr;
    };
    Base::Vector<Module> modules_;
};

} // namespace Aero::Markup
