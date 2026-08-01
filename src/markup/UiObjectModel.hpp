#pragma once

// Private registration bridge for UI authoring objects.

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Config.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Meta/Registry.hpp>
#include <Aero/DependencyProperty.hpp>

namespace Aero::Markup {

class Schema;

struct UiObjectModelOptions final {
    UiObjectModelOptions() noexcept = default;
    UiObjectModelOptions(
        ::Aero::Meta::Registry* metadata,
        Core::DependencyPropertyRegistry* dependencyProperties,
        Base::IAllocator* programAllocator = nullptr) noexcept
        : metadata(metadata),
          properties(dependencyProperties),
          allocator(programAllocator) {}

    ::Aero::Meta::Registry* metadata = nullptr;
    Core::DependencyPropertyRegistry* properties = nullptr;
    Base::IAllocator* allocator = nullptr;
};

// Optional registration override used by schema hosts that expose a custom
// Style/Setter model. Product runtimes use Register(schema), which
// registers Aero's complete Style/Trigger/Template object model.
struct UiObjectModelTypes final {
    Core::TypeId style = Core::InvalidTypeId;
    Core::TypeId setter = Core::InvalidTypeId;
    Core::TypeId trigger = Core::InvalidTypeId;
    Core::DependencyPropertyHandle styleProperty;
    bool includeTemplates = true;
};

// Owns the schema adapters for the UI XAML object model. Parsing,
// compiled XAML, themes, and application resources all register this same
// object model instead of constructing independent Style or Template paths.
class AERO_API UiObjectModel final {
public:
    explicit UiObjectModel(
        const UiObjectModelOptions& options) noexcept;
    ~UiObjectModel() noexcept;

    UiObjectModel(
        const UiObjectModel&) = delete;
    UiObjectModel& operator=(
        const UiObjectModel&) = delete;

    Base::Result<void> Register(
        Schema& schema) noexcept;
    Base::Result<void> Register(
        Schema& schema,
        const UiObjectModelTypes& types) noexcept;

private:
    struct Impl;
    Base::IAllocator* allocator_ = nullptr;
    Impl* impl_ = nullptr;
    bool optionsValid_ = false;
};

} // namespace Aero::Markup
