#pragma once

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Core/Metadata/MetadataDomain.hpp>
#include <Aero/Core/Property/EffectiveValueEngine.hpp>
#include <Aero/Presentation/Binding.hpp>
#include <Aero/Presentation/Resources.hpp>

namespace Aero::Controls {
class TemplateManager;
class VisualStateManager;
}

namespace Aero::Presentation {
class StyleManager;
class Visual;
}

namespace Aero::Detail {

// Internal policy service that applies resource-selected Style and Template
// objects to a mounted visual tree. ViewRuntime owns lifecycle and delegates the
// presentation rules to this service.
class RuntimePresentationServices final {
public:
    void Configure(
        Base::IAllocator& allocator,
        Core::MetadataDomain& metadata,
        Core::EffectiveValueEngine& values,
        Presentation::BindingManager& bindings,
        Presentation::StyleManager& styles,
        Controls::TemplateManager& templates,
        Controls::VisualStateManager& visualStates,
        const Presentation::ResourceEnvironment& resources) noexcept;
    void Reset() noexcept;

    Base::Result<void> Apply(
        Presentation::Visual& root) noexcept;
    void Detach(
        Presentation::Visual* root,
        Base::Span<Presentation::Visual* const> declarationNodes) noexcept;

    bool IsConfigured() const noexcept {
        return allocator_ != nullptr;
    }

private:
    Base::IAllocator* allocator_ = nullptr;
    Core::MetadataDomain* metadata_ = nullptr;
    Core::EffectiveValueEngine* values_ = nullptr;
    Presentation::BindingManager* bindings_ = nullptr;
    Presentation::StyleManager* styles_ = nullptr;
    Controls::TemplateManager* templates_ = nullptr;
    Controls::VisualStateManager* visualStates_ = nullptr;
    Presentation::ResourceEnvironment resources_;
};

} // namespace Aero::Detail
