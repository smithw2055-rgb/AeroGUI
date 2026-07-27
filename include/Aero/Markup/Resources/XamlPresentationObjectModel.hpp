#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Core/Metadata/Activation.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>

namespace Aero::Markup {

class XamlSchemaContext;

struct XamlPresentationObjectModelOptions final {
    XamlPresentationObjectModelOptions() noexcept = default;
    XamlPresentationObjectModelOptions(
        Core::MetadataRuntime* metadataRuntime,
        Core::DependencyPropertyRegistry* dependencyProperties,
        Core::TypeId typeReference = Core::InvalidTypeId,
        Base::IAllocator* programAllocator = nullptr) noexcept
        : runtime(metadataRuntime),
          properties(dependencyProperties),
          typeReferenceType(typeReference),
          allocator(programAllocator) {}

    Core::MetadataRuntime* runtime = nullptr;
    Core::DependencyPropertyRegistry* properties = nullptr;
    Core::TypeId typeReferenceType = Core::InvalidTypeId;
    Base::IAllocator* allocator = nullptr;
};

// Optional registration override used by schema hosts that expose a custom
// Style/Setter model. Product runtimes use Register(schema, activation), which
// registers Aero's complete Style/Trigger/Template object model.
struct XamlPresentationObjectModelTypes final {
    Core::TypeId style = Core::InvalidTypeId;
    Core::TypeId setter = Core::InvalidTypeId;
    Core::TypeId trigger = Core::InvalidTypeId;
    Core::DependencyPropertyHandle styleProperty;
    bool includeTemplates = true;
};

// Owns the schema adapters for the Presentation XAML object model. Parsing,
// compiled XAML, themes, and application resources all register this same
// object model instead of constructing independent Style or Template paths.
class AERO_API XamlPresentationObjectModel final {
public:
    explicit XamlPresentationObjectModel(
        const XamlPresentationObjectModelOptions& options) noexcept;
    ~XamlPresentationObjectModel() noexcept;

    XamlPresentationObjectModel(
        const XamlPresentationObjectModel&) = delete;
    XamlPresentationObjectModel& operator=(
        const XamlPresentationObjectModel&) = delete;

    Base::Result<void> Register(
        XamlSchemaContext& schema,
        Core::ActivationProviderRegistry& activation) noexcept;
    Base::Result<void> Register(
        XamlSchemaContext& schema,
        Core::ActivationProviderRegistry& activation,
        const XamlPresentationObjectModelTypes& types) noexcept;

    void SetTypeReferenceType(Core::TypeId type) noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
    bool optionsValid_ = false;
};

} // namespace Aero::Markup
