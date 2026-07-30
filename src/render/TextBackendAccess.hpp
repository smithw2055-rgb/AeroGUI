#pragma once

#include "presentation/RenderingInternal.hpp"
#include "runtime/MeshResourceContract.hpp"
#include "runtime/ImageResourceContract.hpp"
#include "runtime/TextResourceContract.hpp"

namespace Aero::Render::Detail {

inline constexpr std::uint64_t TextBackendServiceId =
    UINT64_C(0x4145524F54455854);
inline constexpr std::uint64_t MeshBackendServiceId =
    UINT64_C(0x4145524F4D455348);
inline constexpr std::uint64_t ImageBackendServiceId =
    UINT64_C(0x4145524F494D4147);

class RenderBackendAccess final {
public:
    static Aero::Detail::TextBackendServices* TextServices(
        Presentation::IRenderBackend& backend) noexcept {
        return static_cast<Aero::Detail::TextBackendServices*>(
            backend.QueryInternalService(
                TextBackendServiceId));
    }

    static Aero::Detail::MeshBackendServices* MeshServices(
        Presentation::IRenderBackend& backend) noexcept {
        return static_cast<Aero::Detail::MeshBackendServices*>(
            backend.QueryInternalService(
                MeshBackendServiceId));
    }

    static Aero::Detail::ImageBackendServices* ImageServices(
        Presentation::IRenderBackend& backend) noexcept {
        return static_cast<
            Aero::Detail::ImageBackendServices*>(
                backend.QueryInternalService(
                    ImageBackendServiceId));
    }

    static void* InternalService(
        Presentation::IRenderBackend& backend,
        std::uint64_t service) noexcept {
        return backend.QueryInternalService(service);
    }
};

} // namespace Aero::Render::Detail
