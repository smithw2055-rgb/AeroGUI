#pragma once

#include "DisplayList.hpp"

#include <Aero/DrawingContext.hpp>
#include <Aero/FrameworkElement.hpp>

namespace Aero::Internal {

class DrawingPrivate {
public:
    static DrawingContext Create(
        Render::DisplayListBuilder& builder) noexcept {
        return DrawingContext(&builder);
    }

    static Render::DisplayListBuilder& Builder(
        DrawingContext& context) noexcept {
        return *static_cast<Render::DisplayListBuilder*>(
            context.implementation_);
    }
};

} // namespace Aero::Internal
