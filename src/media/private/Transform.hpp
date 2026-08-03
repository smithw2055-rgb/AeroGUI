#pragma once

#include <Aero/Media/Transforms.hpp>

#include <cstdint>

namespace Aero::Media { class Transform; }

namespace Aero::Media::Detail {

} // namespace Aero::Media::Detail

namespace Aero::Media {

struct Transform::Impl {
public:
    static std::uint64_t Revision(
        const Aero::Media::Transform& transform) noexcept;
};

} // namespace Aero::Media

namespace Aero::Media::Detail {
using TransformPrivate = ::Aero::Media::Transform::Impl;

} // namespace Aero::Media::Detail
