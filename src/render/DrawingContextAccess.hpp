#pragma once

#include "DisplayList.hpp"

#include <Aero/DrawingContext.hpp>
#include <Aero/Rendering.hpp>

namespace Aero::Detail {

class DrawingContextAccess final {
public:
    static Render::DisplayListBuilder& Builder(
        DrawingContext& context) noexcept {
        return *static_cast<Render::DisplayListBuilder*>(
            context.implementation_);
    }
};

} // namespace Aero::Detail
