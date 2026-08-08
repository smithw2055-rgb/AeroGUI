#pragma once

#include "render/RenderCommands.hpp"

#include <utility>

namespace Aero::Render::Detail {

// The one encoded UI submission unit exchanged between Renderer and the
// backend RenderDevice. It is intentionally source-private: applications see
// IRenderer and RenderDevice, while native backends consume a UI RenderBatch.
class RenderBatch final {
public:
    explicit RenderBatch(
        Graphics::CommandList&& commands) noexcept
        : commands_(std::move(commands)) {}

    RenderBatch(RenderBatch&&) noexcept = default;
    RenderBatch& operator=(RenderBatch&&) noexcept = default;

    RenderBatch(const RenderBatch&) = delete;
    RenderBatch& operator=(const RenderBatch&) = delete;

    const Graphics::CommandList& Commands() const noexcept {
        return commands_;
    }
    bool Empty() const noexcept {
        return commands_.CommandCount() == 0U;
    }

private:
    Graphics::CommandList commands_;
};

} // namespace Aero::Render::Detail
