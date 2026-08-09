#pragma once

#include <Aero/Media/Effects.hpp>

namespace Aero::Media {

// Effect ownership is a runtime attachment detail.  Keep it out of the SDK
// surface while allowing the metadata bridge to update it when an Effect
// property is assigned to a FrameworkElement.
struct Effect::Access {
public:
    static std::uint64_t Revision(
        const Aero::Media::Effect& effect) noexcept;
};

} // namespace Aero::Media

namespace Aero::Media {
using EffectPrivate = ::Aero::Media::Effect::Access;

} // namespace Aero::Media
