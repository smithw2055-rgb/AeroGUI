#pragma once

#include "runtime/MeshResourceContract.hpp"

#include <Aero/Controls/Controls.hpp>

namespace Aero::Controls::Detail {

class PathServicesAccess final {
public:
    static void InvalidateGeometry(
        Path& path) noexcept {
        path.ResetGeometry();
    }

    static void Attach(
        Path& path,
        Aero::Detail::MeshBackendServices* services,
        bool invalidate = false) noexcept {
        path.AttachMeshServices(
            services, invalidate);
        if (invalidate) {
            static_cast<void>(path.InvalidateRender());
        }
    }
};

} // namespace Aero::Controls::Detail
