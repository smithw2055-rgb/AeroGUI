#pragma once

#include "../render/DisplayList.hpp"

#include <Aero/Base/Result.hpp>
#include <Aero/Base/Span.hpp>
#include <Aero/Rendering.hpp>

#include <cstdint>

namespace Aero::Detail {

// Private, backend-neutral bridge. Controls hand immutable geometry to the
// endpoint and retain only the opaque RenderMeshId used by display lists.
struct MeshBackendServices final {
    std::uint64_t generation = 0U;
    void* context = nullptr;
    Base::Result<Render::RenderMeshId> (*createMesh)(
        void* context,
        Base::Span<const Aero::Point> vertices,
        Base::Span<const std::uint32_t> indices) noexcept = nullptr;
    void (*releaseMesh)(
        void* context,
        Render::RenderMeshId mesh) noexcept = nullptr;
};

} // namespace Aero::Detail
