#pragma once

#include "presentation/RenderingInternal.hpp"
#include "runtime/TextResourceContract.hpp"

namespace Aero::Render::Detail {

inline constexpr std::uint64_t TextBackendServiceId =
    UINT64_C(0x4145524F54455854);

class RenderBackendAccess final {
public:
    static Aero::Detail::TextBackendServices* TextServices(
        Presentation::IRenderBackend& backend) noexcept {
        return static_cast<Aero::Detail::TextBackendServices*>(
            backend.QueryInternalService(
                TextBackendServiceId));
    }
};

} // namespace Aero::Render::Detail
