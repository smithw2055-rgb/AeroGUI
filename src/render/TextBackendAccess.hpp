#pragma once

#include "render/RenderingInternal.hpp"
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
        Render::IRenderBackend& backend) noexcept {
        return static_cast<Aero::Detail::TextBackendServices*>(
            backend.QueryInternalService(
                TextBackendServiceId));
    }

    static Aero::Detail::MeshBackendServices* MeshServices(
        Render::IRenderBackend& backend) noexcept {
        return static_cast<Aero::Detail::MeshBackendServices*>(
            backend.QueryInternalService(
                MeshBackendServiceId));
    }

    static Aero::Detail::ImageBackendServices* ImageServices(
        Render::IRenderBackend& backend) noexcept {
        return static_cast<
            Aero::Detail::ImageBackendServices*>(
                backend.QueryInternalService(
                    ImageBackendServiceId));
    }

    static void* InternalService(
        Render::IRenderBackend& backend,
        std::uint64_t service) noexcept {
        return backend.QueryInternalService(service);
    }
};

} // namespace Aero::Render::Detail
