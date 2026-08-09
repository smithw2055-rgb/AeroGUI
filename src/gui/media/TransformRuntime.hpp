#pragma once

#include <Aero/Media/Transforms.hpp>

#include <cstdint>

namespace Aero::Media { class Transform; }

namespace Aero::Media {

} // namespace Aero::Media

namespace Aero::Media {

struct Transform::Access {
public:
    static std::uint64_t Revision(
        const Aero::Media::Transform& transform) noexcept;
};

} // namespace Aero::Media

namespace Aero::Media {
using TransformPrivate = ::Aero::Media::Transform::Access;

} // namespace Aero::Media
