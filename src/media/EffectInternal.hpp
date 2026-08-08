#pragma once

#include <Aero/Media/Effects.hpp>

namespace Aero::Media {

// Effect ownership is a runtime attachment detail.  Keep it out of the SDK
// surface while allowing the metadata bridge to update it when an Effect
// property is assigned to a FrameworkElement.
struct Effect::Impl {
public:
    static std::uint64_t Revision(
        const Aero::Media::Effect& effect) noexcept;
};

} // namespace Aero::Media

namespace Aero::Media::Detail {
using EffectPrivate = ::Aero::Media::Effect::Impl;

} // namespace Aero::Media::Detail
