#pragma once

#include <Aero/Media/Effects.hpp>

namespace Aero::Media {

// Effect ownership is a runtime attachment detail.  Keep it out of the SDK
// surface while allowing the metadata bridge to update it when an Effect
// property is assigned to a FrameworkElement.
struct Effect::Impl {
public:
    static Aero::FrameworkElement* Owner(
        const Aero::Media::Effect& effect) noexcept {
        return effect.owner_;
    }

    static void SetOwner(
        Aero::Media::Effect& effect,
        Aero::FrameworkElement* owner) noexcept {
        effect.owner_ = owner;
    }
};

} // namespace Aero::Media

namespace Aero::Internal {
using EffectPrivate = ::Aero::Media::Effect::Impl;

} // namespace Aero::Internal
