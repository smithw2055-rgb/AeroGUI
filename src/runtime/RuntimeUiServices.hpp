#pragma once

#include "RuntimeFwd.hpp"

#include <Aero/Base/Allocator.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Meta/MetadataDomain.hpp>
#include "core/property/EffectiveValueEngine.hpp"
#include <Aero/Data.hpp>
#include <Aero/Resources.hpp>

namespace Aero::Controls {
class VisualStateManager;
}

namespace Aero { class Visual; }

namespace Aero::Detail {

// Internal policy service that applies resource-selected Style and Template
// objects to a mounted visual tree. ViewRuntime owns lifecycle and delegates the
// UI style and template rules to this service.
class RuntimeUiServices final {
public:
    void Configure(
        Base::IAllocator& allocator,
        Core::MetadataDomain& metadata,
        Core::EffectiveValueEngine& values,
        Aero::Detail::BindingManager& bindings,
        Aero::Detail::EventRouter& events,
        Aero::Detail::InputService& input,
        Aero::Detail::StyleManager& styles,
        Controls::TemplateManager& templates,
        Controls::VisualStateManager& visualStates,
        const Aero::ResourceEnvironment& resources) noexcept;
    void Reset() noexcept;

    Base::Result<void> Apply(
        Aero::Visual& root) noexcept;
    void Detach(
        Aero::Visual* root,
        Base::Span<Aero::Visual* const> declarationNodes) noexcept;

    bool IsConfigured() const noexcept {
        return allocator_ != nullptr;
    }

private:
    Base::IAllocator* allocator_ = nullptr;
    Core::MetadataDomain* metadata_ = nullptr;
    Core::EffectiveValueEngine* values_ = nullptr;
    Aero::Detail::BindingManager* bindings_ = nullptr;
    Aero::Detail::EventRouter* events_ = nullptr;
    Aero::Detail::InputService* input_ = nullptr;
    Aero::Detail::StyleManager* styles_ = nullptr;
    Controls::TemplateManager* templates_ = nullptr;
    Controls::VisualStateManager* visualStates_ = nullptr;
    Aero::ResourceEnvironment resources_;
};

} // namespace Aero::Detail
