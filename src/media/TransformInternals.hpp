#pragma once

#include <cstdint>

namespace Aero { class FrameworkElement; }

namespace Aero::Media { class Transform; }

namespace Aero::Internal {

enum class TransformOwnerRole : std::uint8_t {
    Render = 1U,
    Layout = 2U,
};

constexpr std::uint8_t OwnerRoleValue(
    TransformOwnerRole role) noexcept {
    return static_cast<std::uint8_t>(role);
}

class TransformPrivate final {
public:
    static Aero::FrameworkElement* Owner(
        const Aero::Media::Transform& transform) noexcept;
    static bool HasOwnerRole(
        const Aero::Media::Transform& transform,
        std::uint8_t role) noexcept;
    static void AttachOwner(
        Aero::Media::Transform& transform,
        Aero::FrameworkElement* owner,
        std::uint8_t role) noexcept;
    static void DetachOwner(
        Aero::Media::Transform& transform,
        Aero::FrameworkElement* owner,
        std::uint8_t role) noexcept;
};

} // namespace Aero::Internal
