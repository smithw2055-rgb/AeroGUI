#pragma once

// Private registration bridge for Presentation authoring objects.

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>
#include <Aero/Core/Property/DependencyProperty.hpp>

namespace Aero::Markup {

class Schema;

struct PresentationObjectModelOptions final {
    PresentationObjectModelOptions() noexcept = default;
    PresentationObjectModelOptions(
        Core::MetadataRuntime* metadataRuntime,
        Core::DependencyPropertyRegistry* dependencyProperties,
        Base::IAllocator* programAllocator = nullptr) noexcept
        : runtime(metadataRuntime),
          properties(dependencyProperties),
          allocator(programAllocator) {}

    Core::MetadataRuntime* runtime = nullptr;
    Core::DependencyPropertyRegistry* properties = nullptr;
    Base::IAllocator* allocator = nullptr;
};

// Optional registration override used by schema hosts that expose a custom
// Style/Setter model. Product runtimes use Register(schema), which
// registers Aero's complete Style/Trigger/Template object model.
struct PresentationObjectModelTypes final {
    Core::TypeId style = Core::InvalidTypeId;
    Core::TypeId setter = Core::InvalidTypeId;
    Core::TypeId trigger = Core::InvalidTypeId;
    Core::DependencyPropertyHandle styleProperty;
    bool includeTemplates = true;
};

// Owns the schema adapters for the Presentation XAML object model. Parsing,
// compiled XAML, themes, and application resources all register this same
// object model instead of constructing independent Style or Template paths.
class AERO_API PresentationObjectModel final {
public:
    explicit PresentationObjectModel(
        const PresentationObjectModelOptions& options) noexcept;
    ~PresentationObjectModel() noexcept;

    PresentationObjectModel(
        const PresentationObjectModel&) = delete;
    PresentationObjectModel& operator=(
        const PresentationObjectModel&) = delete;

    Base::Result<void> Register(
        Schema& schema) noexcept;
    Base::Result<void> Register(
        Schema& schema,
        const PresentationObjectModelTypes& types) noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
    bool optionsValid_ = false;
};

} // namespace Aero::Markup
