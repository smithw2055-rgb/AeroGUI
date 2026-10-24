#include <cstdio>
#include "DisplayList.hpp"
#include "gui/styles/StyleEngine.hpp"
#include "RenderTree.hpp"
#include "gui/core/TypeRegistryCore.hpp"
#include "gui/core/ElementTree.hpp"
#include "gui/core/LayoutEngine.hpp"
#include "gui/core/EffectiveValueEngine.hpp"
#include "gui/core/RoutedEvents.hpp"
#include "gui/core/EventRouter.hpp"
#include "gui/media/BrushRendering.hpp"
#include <Aero/Base/Assert.hpp>
#include <Aero/Controls/Decorators.hpp>
#include <Aero/Controls/Menus.hpp>
#include <Aero/Controls/Popup.hpp>
#include <Aero/Documents.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Geometries.hpp>
#include <Aero/Media/Transform2D.hpp>
#include <Aero/Media/Transform3D.hpp>
#include "gui/media/GeometryFlatten.hpp"
#include "gui/media/Transform3DMath.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <new>
#include <utility>
#include <Aero/Base/Hash.hpp>
#include "gui/core/DependencyObjectAccess.hpp"

namespace Aero::Render {

namespace {

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

std::uint64_t HashByte(std::uint64_t hash, std::uint8_t value) noexcept {
    constexpr std::uint64_t Prime = 1099511628211ULL;
    return (hash ^ value) * Prime;
}

template <typename T>
std::uint64_t HashScalar(std::uint64_t hash, const T& value) noexcept {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    for (std::size_t index = 0U; index < sizeof(T); ++index) {
        hash = HashByte(hash, bytes[index]);
    }
    return hash;
}

std::uint64_t HashRect(std::uint64_t hash, const Rect& value) noexcept {
    hash = HashScalar(hash, value.x);
    hash = HashScalar(hash, value.y);
    hash = HashScalar(hash, value.width);
    return HashScalar(hash, value.height);
}

std::uint64_t HashSize(std::uint64_t hash, const Size& value) noexcept {
    hash = HashScalar(hash, value.width);
    return HashScalar(hash, value.height);
}

std::uint64_t HashTransform(
    std::uint64_t hash,
    const ProjectiveTransform2D& value) noexcept {
    hash = HashScalar(hash, value.m11);
    hash = HashScalar(hash, value.m12);
    hash = HashScalar(hash, value.m13);
    hash = HashScalar(hash, value.m21);
    hash = HashScalar(hash, value.m22);
    hash = HashScalar(hash, value.m23);
    hash = HashScalar(hash, value.m31);
    hash = HashScalar(hash, value.m32);
    return HashScalar(hash, value.m33);
}

std::uint64_t HashColor(std::uint64_t hash, const Color& value) noexcept {
    hash = HashScalar(hash, value.red);
    hash = HashScalar(hash, value.green);
    hash = HashScalar(hash, value.blue);
    return HashScalar(hash, value.alpha);
}

std::uint64_t HashCommandHeader(
    std::uint64_t hash,
    const RenderCommandHeader& h) noexcept {
    hash = HashScalar(hash, static_cast<std::uint8_t>(h.kind));
    hash = HashScalar(hash, h.paintKind);
    hash = HashScalar(hash, h.wrapMode);
    hash = HashScalar(hash, h.flags);
    return HashScalar(hash, h.payloadOffset);
}

bool IsValidColorComponent(float value) noexcept {
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
}

bool IsValidImageUv(Rect value) noexcept {
    if (!Base::IsFiniteRect(value)) return false;
    const double endX = value.x + value.width;
    const double endY = value.y + value.height;
    return std::fmin(value.x, endX) >= 0.0 &&
        std::fmax(value.x, endX) <= 1.0 &&
        std::fmin(value.y, endY) >= 0.0 &&
        std::fmax(value.y, endY) <= 1.0;
}

} // namespace

bool IsFinite(Color value) noexcept {
    return IsValidColorComponent(value.red) &&
        IsValidColorComponent(value.green) &&
        IsValidColorComponent(value.blue) &&
        IsValidColorComponent(value.alpha);
}

bool IsFinite(Transform2D value) noexcept {
    return std::isfinite(value.m11) && std::isfinite(value.m12) &&
        std::isfinite(value.m21) && std::isfinite(value.m22) &&
        std::isfinite(value.dx) && std::isfinite(value.dy);
}

bool IsFinite(ProjectiveTransform2D value) noexcept {
    return Base::IsFiniteTransform(value);
}

bool IsValidOpacity(double value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

std::uint64_t DisplayList::StableHash() const noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    hash = HashScalar(hash, headers_.Size());
    for (const RenderCommandHeader& h : headers_) {
        hash = HashCommandHeader(hash, h);
    }
    hash = HashScalar(hash, payload_.Size());
    for (std::uint8_t b : payload_) {
        hash = HashScalar(hash, b);
    }
    return hash;
}

Base::Result<void> DisplayListBuilder::PushClip(Rect clip) noexcept {
    if (!IsValidLayoutRect(clip)) {
        return InvalidArgument("Render clip must be finite and nonnegative");
    }
    if (finished_) return InvalidState("DisplayListBuilder has already been finished");
    CmdPushClip cmd{clip};
    RenderCommandHeader h{};
    h.kind = RenderCommandKind::PushClip;
    h.payloadOffset = AppendPayload(cmd);
    list_.headers_.PushBack(h);
    ++clipDepth_;
    return {};
}

Base::Result<void> DisplayListBuilder::PopClip() noexcept {
    if (clipDepth_ == 0U) {
        return InvalidState("Render clip stack underflow");
    }
    if (finished_) return InvalidState("DisplayListBuilder has already been finished");
    RenderCommandHeader h{};
    h.kind = RenderCommandKind::PopClip;
    h.payloadOffset = 0U;
    list_.headers_.PushBack(h);
    --clipDepth_;
    return {};
}

Base::Result<void> DisplayListBuilder::PushOpacity(double opacity) noexcept {
    if (!IsValidOpacity(opacity)) {
        return InvalidArgument("Render opacity must be within [0, 1]");
    }
    if (finished_) return InvalidState("DisplayListBuilder has already been finished");
    CmdPushOpacity cmd{opacity};
    RenderCommandHeader h{};
    h.kind = RenderCommandKind::PushOpacity;
    h.payloadOffset = AppendPayload(cmd);
    list_.headers_.PushBack(h);
    ++opacityDepth_;
    return {};
}

Base::Result<void> DisplayListBuilder::PopOpacity() noexcept {
    if (opacityDepth_ == 0U) {
        return InvalidState("Render opacity stack underflow");
    }
    if (finished_) return InvalidState("DisplayListBuilder has already been finished");
    RenderCommandHeader h{};
    h.kind = RenderCommandKind::PopOpacity;
    h.payloadOffset = 0U;
    list_.headers_.PushBack(h);
    --opacityDepth_;
    return {};
}

Base::Result<void> DisplayListBuilder::PushTransform(
    Transform2D value) noexcept {
    return PushTransform(Base::ToProjective(value));
}

Base::Result<void> DisplayListBuilder::PushTransform(
    ProjectiveTransform2D value) noexcept {
    if (!IsFinite(value)) {
        return InvalidArgument("Render transform must contain finite values");
    }
    if (finished_) return InvalidState("DisplayListBuilder has already been finished");
    CmdPushTransform cmd{value};
    RenderCommandHeader h{};
    h.kind = RenderCommandKind::PushTransform;
    h.payloadOffset = AppendPayload(cmd);
    list_.headers_.PushBack(h);
    ++transformDepth_;
    return {};
}

Base::Result<void> DisplayListBuilder::PopTransform() noexcept {
    if (transformDepth_ == 0U) {
        return InvalidState("Render transform stack underflow");
    }
    if (finished_) return InvalidState("DisplayListBuilder has already been finished");
    RenderCommandHeader h{};
    h.kind = RenderCommandKind::PopTransform;
    h.payloadOffset = 0U;
    list_.headers_.PushBack(h);
    --transformDepth_;
    return {};
}

Base::Result<void> DisplayListBuilder::FillRect(
    Rect rect,
    Color color) noexcept {
    if (!IsValidLayoutRect(rect) || !IsFinite(color)) {
        return InvalidArgument("FillRect requires valid geometry and color");
    }
    if (finished_) return InvalidState("DisplayListBuilder has already been finished");
    CmdFillRect cmd{rect, color};
    RenderCommandHeader h{};
    h.kind = RenderCommandKind::FillRect;
    h.payloadOffset = AppendPayload(cmd);
    list_.headers_.PushBack(h);
    return {};
}

Base::Result<void> DisplayListBuilder::FillRoundedRect(
    Rect rect,
    Color color,
    double cornerRadius) noexcept {
    if (!IsValidLayoutRect(rect) || !IsFinite(color) ||
        !std::isfinite(cornerRadius) || cornerRadius < 0.0) {
        return InvalidArgument(
            "FillRoundedRect requires valid geometry, color, and corner radius");
    }
    const double maxRadius = std::fmin(rect.width, rect.height) * 0.5;
    cornerRadius = std::clamp(cornerRadius, 0.0, maxRadius);
    if (finished_) return InvalidState("DisplayListBuilder has already been finished");
    CmdRoundedRect cmd{rect, color, static_cast<float>(cornerRadius), 0.0f};
    RenderCommandHeader h{};
    h.kind = RenderCommandKind::FillRoundedRect;
    h.payloadOffset = AppendPayload(cmd);
    list_.headers_.PushBack(h);
    return {};
}

Base::Result<void> DisplayListBuilder::FillGradientQuad(
    const Point points[4],
    const Color colors[4]) noexcept {
    if (finished_) return InvalidState("DisplayListBuilder has already been finished");
    CmdGradientQuad cmd{};
    for (int i = 0; i < 4; ++i) {
        cmd.points[i] = points[i];
        cmd.colors[i] = colors[i];
    }
    RenderCommandHeader h{};
    h.kind = RenderCommandKind::FillGradientQuad;
    h.paintKind = 0U;
    h.payloadOffset = AppendPayload(cmd);
    list_.headers_.PushBack(h);
    return {};
}

Base::Result<void> DisplayListBuilder::FillGradientQuad(
    const Point points[4],
    const Point uvs[4],
    std::uint8_t paintKind,
    const float uniforms[8],
    std::uintptr_t gradientBrush,
    std::uint8_t wrapMode) noexcept {
    if (paintKind == 0U || gradientBrush == 0U || uniforms == nullptr) {
        return InvalidArgument("GPU gradient fill requires paint and brush");
    }
    if (finished_) return InvalidState("DisplayListBuilder has already been finished");
    CmdGradientQuad cmd{};
    cmd.gradientBrush = gradientBrush;
    for (int i = 0; i < 4; ++i) {
        cmd.points[i] = points[i];
        cmd.uvs[i] = uvs[i];
        cmd.colors[i] = Color{1.0F, 1.0F, 1.0F, 1.0F};
    }
    std::memcpy(cmd.paintUniforms, uniforms, sizeof(cmd.paintUniforms));
    RenderCommandHeader h{};
    h.kind = RenderCommandKind::FillGradientQuad;
    h.paintKind = paintKind;
    h.wrapMode = wrapMode;
    h.payloadOffset = AppendPayload(cmd);
    list_.headers_.PushBack(h);
    return {};
}

Base::Result<void> DisplayListBuilder::StrokeRect(
    Rect rect,
    Color color,
    double thickness,
    double cornerRadius) noexcept {
    if (!IsValidLayoutRect(rect) || !IsFinite(color) ||
        !std::isfinite(thickness) || thickness < 0.0 ||
        !std::isfinite(cornerRadius) || cornerRadius < 0.0) {
        return InvalidArgument(
            "StrokeRect requires valid geometry, color, and thickness");
    }
    const double maxRadius = std::fmin(rect.width, rect.height) * 0.5;
    cornerRadius = std::clamp(cornerRadius, 0.0, maxRadius);
    if (finished_) return InvalidState("DisplayListBuilder has already been finished");
    CmdStrokeRect cmd{rect, color, static_cast<float>(thickness), static_cast<float>(cornerRadius)};
    RenderCommandHeader h{};
    h.kind = RenderCommandKind::StrokeRect;
    h.payloadOffset = AppendPayload(cmd);
    list_.headers_.PushBack(h);
    return {};
}

Base::Result<void> DisplayListBuilder::DrawAnalyticalShadow(
    Rect rect, Color color, float cornerRadius, float blurRadius, Point offset) noexcept {
    if (finished_) return InvalidState("DisplayListBuilder has already been finished");
    CmdAnalyticalShadow cmd{rect, color, cornerRadius, blurRadius, offset};
    RenderCommandHeader h{};
    h.kind = RenderCommandKind::DrawAnalyticalShadow;
    h.payloadOffset = AppendPayload(cmd);
    list_.headers_.PushBack(h);
    return {};
}

Base::Result<void> DisplayListBuilder::DrawImage(
    RenderImageId image,
    Rect destination,
    Rect sourceUv,
    Color tint,
    std::uint8_t wrapMode) noexcept {
    if (image == InvalidRenderImageId || !IsValidLayoutRect(destination) ||
        !IsValidImageUv(sourceUv) || !IsFinite(tint)) {
        return InvalidArgument(
            "DrawImage requires a valid image, destination, UV rectangle, and tint");
    }
    if (finished_) return InvalidState("DisplayListBuilder has already been finished");
    CmdDrawImage cmd{image, destination, sourceUv, tint};
    RenderCommandHeader h{};
    h.kind = RenderCommandKind::DrawImage;
    h.wrapMode = wrapMode;
    h.payloadOffset = AppendPayload(cmd);
    list_.headers_.PushBack(h);
    return {};
}

Base::Result<void> DisplayListBuilder::DrawMesh(
    RenderMeshId mesh,
    Color tint) noexcept {
    if (mesh == InvalidRenderMeshId || !IsFinite(tint)) {
        return InvalidArgument("DrawMesh requires a valid mesh and tint");
    }
    if (finished_) return InvalidState("DisplayListBuilder has already been finished");
    CmdDrawSolidMesh cmd{};
    cmd.mesh = mesh;
    cmd.tint = tint;
    RenderCommandHeader h{};
    h.kind = RenderCommandKind::DrawSolidMesh;
    h.payloadOffset = AppendPayload(cmd);
    list_.headers_.PushBack(h);
    return {};
}

Base::Result<void> DisplayListBuilder::DrawMesh(
    RenderMeshId mesh,
    std::uint8_t paintKind,
    const float uniforms[8],
    std::uintptr_t gradientBrush,
    Rect bounds,
    Point startOrOrigin,
    Point delta,
    Point radii,
    double len2,
    bool hasInverse,
    Transform2D inverse,
    std::uint8_t wrapMode) noexcept {
    if (mesh == InvalidRenderMeshId || paintKind == 0U ||
        gradientBrush == 0U || uniforms == nullptr) {
        return InvalidArgument("GPU DrawMesh requires paint and brush");
    }
    if (finished_) return InvalidState("DisplayListBuilder has already been finished");
    CmdDrawGradientMesh cmd{};
    cmd.mesh = mesh;
    cmd.tint = Color{1.0F, 1.0F, 1.0F, 1.0F};
    cmd.gradientBrush = gradientBrush;
    cmd.bounds = bounds;
    cmd.startOrOrigin = startOrOrigin;
    cmd.delta = delta;
    cmd.radii = radii;
    cmd.len2 = len2;
    cmd.hasInverse = hasInverse;
    cmd.inverse = inverse;
    std::memcpy(cmd.paintUniforms, uniforms, sizeof(cmd.paintUniforms));
    RenderCommandHeader h{};
    h.kind = RenderCommandKind::DrawGradientMesh;
    h.paintKind = paintKind;
    h.wrapMode = wrapMode;
    h.payloadOffset = AppendPayload(cmd);
    list_.headers_.PushBack(h);
    return {};
}

Base::Result<void> DisplayListBuilder::DrawGlyphRun(
    RenderGlyphRunId glyphRun,
    Color tint) noexcept {
    if (glyphRun == InvalidRenderGlyphRunId || !IsFinite(tint)) {
        return InvalidArgument("DrawGlyphRun requires a valid glyph run and tint");
    }
    if (finished_) return InvalidState("DisplayListBuilder has already been finished");
    CmdDrawGlyphRun cmd{glyphRun, tint};
    RenderCommandHeader h{};
    h.kind = RenderCommandKind::DrawGlyphRun;
    h.payloadOffset = AppendPayload(cmd);
    list_.headers_.PushBack(h);
    return {};
}

Base::Result<void> DisplayListBuilder::DrawTriangles(
    const Point* vertices,
    std::uint32_t vertexCount,
    const std::uint32_t* indices,
    std::uint32_t indexCount,
    Color color) noexcept {
    if (finished_) return InvalidState("DisplayListBuilder has already been finished");
    if (vertices == nullptr || vertexCount == 0U || indices == nullptr || indexCount == 0U) {
        return {};
    }
    CmdDrawTriangles cmd{};
    cmd.color = color;
    cmd.vertexCount = vertexCount;
    cmd.indexCount = indexCount;
    cmd.vertexOffset = AppendRawBytes(vertices, vertexCount * sizeof(Point));
    cmd.indexOffset = AppendRawBytes(indices, indexCount * sizeof(std::uint32_t));

    RenderCommandHeader h{};
    h.kind = RenderCommandKind::DrawTriangles;
    h.payloadOffset = AppendPayload(cmd);
    list_.headers_.PushBack(h);
    return {};
}

Base::Result<DisplayList> DisplayListBuilder::Finish() noexcept {
    if (finished_) {
        return InvalidState("DisplayListBuilder has already been finished");
    }
    if (clipDepth_ != 0U || opacityDepth_ != 0U || transformDepth_ != 0U) {
        return InvalidState("DisplayList contains unbalanced state stacks");
    }
    finished_ = true;
    return std::move(list_);
}

using Aero::FrameworkElement;

void RenderFrame::Clear() noexcept {
    nodes_.Clear();
    commandHeaders_.Clear();
    commandPayload_.Clear();
    gradientRamps_.Clear();
    masks_.Clear();
    effects_.Clear();
    geometryClipVertices_.Clear();
    geometryClipIndices_.Clear();
    version_ = 0U;
    logicalSize_ = {};
    pixelWidth_ = 0U;
    pixelHeight_ = 0U;
    dpiScale_ = 1.0;
}

std::uint64_t RenderFrame::StableHash() const noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    // P4.3: version_ is a generation marker, not content (see
    // elementRevision above). The descriptor version stays available via
    // Version()/Diagnostics; the hash identifies visuals only.
    hash = HashSize(hash, logicalSize_);
    hash = HashScalar(hash, pixelWidth_);
    hash = HashScalar(hash, pixelHeight_);
    hash = HashScalar(hash, dpiScale_);
    hash = HashScalar(hash, nodes_.Size());
    hash = HashScalar(hash, commandHeaders_.Size());
    hash = HashScalar(hash, commandPayload_.Size());
    hash = HashScalar(hash, gradientRamps_.Size());
    for (const RenderNodeSnapshot& node : nodes_) {
        hash = HashScalar(hash, node.id);
        hash = HashScalar(hash, node.parentId);
        hash = HashRect(hash, node.layoutSlot);
        hash = HashRect(hash, node.clip);
        hash = HashScalar(hash, node.clipsToBounds);
        hash = HashScalar(hash, node.hasLayoutClip);
        hash = HashSize(hash, node.renderSize);
        hash = HashTransform(hash, node.renderTransform);
        hash = HashScalar(
            hash,
            static_cast<std::uint8_t>(
                node.blendMode));
        hash = HashScalar(hash, node.opacity);
        const RenderMaskSnapshot* mask = MaskAt(node.maskIndex);
        hash = HashScalar(
            hash,
            mask != nullptr ? static_cast<std::uint8_t>(mask->kind) : 0U);
        const RenderEffectSnapshot* effect = EffectAt(node.effectIndex);
        hash = HashScalar(
            hash,
            effect != nullptr ? static_cast<std::uint8_t>(effect->kind) : 0U);
        hash = HashScalar(hash, node.commandOffset);
        hash = HashScalar(hash, node.commandCount);
        // P4.3: elementRevision is a commit-generation marker, not content.
        // Excluding it makes StableHash a true content hash: identical
        // visuals hash identically regardless of how many commits have
        // passed, which is what the content-stability gate compares.
    }
    for (const RenderCommandHeader& h : commandHeaders_) {
        hash = HashCommandHeader(hash, h);
    }
    for (std::uint8_t b : commandPayload_) {
        hash = HashScalar(hash, b);
    }
    for (const RenderGradientRampSnapshot& ramp : gradientRamps_) {
        // brushIdentity is intentionally omitted: it is a process-local cache
        // key and must not make a frame hash depend on an address.
        // P4.2: ramp pixels (1024 B) no longer participate per frame; the
        // revision already versions content changes, so hash it only.
        hash = HashScalar(hash, ramp.revision);
    }
    return hash;
}


Base::Result<void> ValidateRenderFrame(const RenderFrame& frame) noexcept {
    if (!IsValidLayoutSize(frame.LogicalSize()) ||
        !std::isfinite(frame.DpiScale()) || frame.DpiScale() <= 0.0 ||
        (frame.LogicalSize().width == 0.0 && frame.PixelWidth() != 0U) ||
        (frame.LogicalSize().height == 0.0 && frame.PixelHeight() != 0U)) {
        return InvalidArgument("RenderFrame viewport is invalid");
    }
    std::uint32_t clipDepth = 0U;
    std::uint32_t opacityDepth = 0U;
    std::uint32_t transformDepth = 0U;
    const Base::Span<const RenderCommandHeader> commands = frame.CommandHeaders();
    const Base::Span<const RenderNodeSnapshot> nodes = frame.Nodes();
    const Base::Span<const RenderGradientRampSnapshot> ramps =
        frame.GradientRamps();

    for (const RenderGradientRampSnapshot& ramp : ramps) {
        if (ramp.brushIdentity == 0U) {
            return InvalidArgument(
                "RenderFrame gradient ramp has no brush identity");
        }
    }

    for (std::uint32_t nodeIndex = 0U; nodeIndex < nodes.Size(); ++nodeIndex) {
        const RenderNodeSnapshot& node = nodes[nodeIndex];
        if (node.id == InvalidRenderNodeId) {
            return InvalidState("RenderFrame node IDs must be nonzero");
        }
        // P4.2: the ID-uniqueness and parent-precedes double loops were O(n^2)
        // per validated frame. Node IDs come from RenderTree's monotonic
        // nextNodeId_ counter and parents are appended before their children
        // in BuildSubtree, so both hold by construction; keep them as
        // Debug-only checks and validate fields in O(n) in all builds.
#ifndef NDEBUG
        for (std::uint32_t previous = 0U; previous < nodeIndex; ++previous) {
            if (nodes[previous].id == node.id) {
                return InvalidState("RenderFrame node IDs must be unique");
            }
        }
        if (node.parentId != InvalidRenderNodeId) {
            bool parentPrecedesChild = false;
            for (std::uint32_t previous = 0U; previous < nodeIndex; ++previous) {
                parentPrecedesChild = parentPrecedesChild || nodes[previous].id == node.parentId;
            }
            if (!parentPrecedesChild) {
                return InvalidState("RenderFrame parent must precede its child");
            }
        }
#endif
        if (!IsValidLayoutRect(node.layoutSlot) ||
            !IsValidLayoutRect(node.clip) ||
            !IsValidLayoutSize(node.renderSize) ||
            !Base::IsFiniteTransform(node.renderTransform) ||
            !IsValidOpacity(node.opacity) ||
            node.commandOffset > commands.Size() ||
            node.commandCount > commands.Size() - node.commandOffset ||
            node.payloadOffset > frame.CommandPayload().Size() ||
            node.payloadSize > frame.CommandPayload().Size() - node.payloadOffset) {
            return InvalidArgument("RenderFrame node snapshot is invalid");
        }
        if (node.maskIndex != UINT32_MAX) {
            const RenderMaskSnapshot* maskPtr = frame.MaskAt(node.maskIndex);
            if (maskPtr == nullptr) {
                return InvalidArgument("RenderFrame node mask index out of range");
            }
            const auto& mask = *maskPtr;
            if (static_cast<std::uint8_t>(mask.kind) >
                    static_cast<std::uint8_t>(RenderMaskKind::RadialGradient) ||
                !IsFinite(mask.color)) {
                return InvalidArgument("RenderFrame node mask is invalid");
            }
            if (mask.kind == RenderMaskKind::Image) {
                const bool viewboxIsRelative =
                    mask.viewboxUnits == static_cast<std::uint8_t>(
                        Media::BrushMappingMode::RelativeToBoundingBox);
                const bool validViewbox =
                    IsValidLayoutRect(mask.sourceUv) &&
                    mask.sourceUv.x >= 0.0 &&
                    mask.sourceUv.y >= 0.0 &&
                    (!viewboxIsRelative ||
                     (mask.sourceUv.x + mask.sourceUv.width <= 1.0 &&
                      mask.sourceUv.y + mask.sourceUv.height <= 1.0));
                if (mask.image == InvalidRenderImageId ||
                    mask.imageWidth == 0U || mask.imageHeight == 0U ||
                    !validViewbox || !IsValidLayoutRect(mask.viewport) ||
                    !IsFinite(mask.relativeTransform) ||
                    mask.viewboxUnits > static_cast<std::uint8_t>(
                        Media::BrushMappingMode::Absolute) ||
                    mask.viewportUnits > static_cast<std::uint8_t>(
                        Media::BrushMappingMode::Absolute) ||
                    mask.stretch > static_cast<std::uint8_t>(
                        Media::Stretch::UniformToFill) ||
                    mask.tileMode > static_cast<std::uint8_t>(
                        Media::TileMode::FlipXY) ||
                    mask.alignmentX > static_cast<std::uint8_t>(
                        HorizontalAlignment::Right) ||
                    mask.alignmentY > static_cast<std::uint8_t>(
                        VerticalAlignment::Bottom)) {
                    return InvalidArgument("RenderFrame image mask is invalid");
                }
            }
            if (mask.kind == RenderMaskKind::LinearGradient ||
                mask.kind == RenderMaskKind::RadialGradient) {
                if (mask.gradientRamp >= ramps.Size() ||
                    mask.mappingMode > static_cast<std::uint8_t>(
                        Media::BrushMappingMode::Absolute) ||
                    !Aero::IsFinite(mask.startPoint) ||
                    !Aero::IsFinite(mask.endPoint) ||
                    !Aero::IsFinite(mask.center) ||
                    !Aero::IsFinite(mask.gradientOrigin) ||
                    !IsFinite(mask.relativeTransform) ||
                    !std::isfinite(mask.radiusX) ||
                    !std::isfinite(mask.radiusY) ||
                    (mask.kind == RenderMaskKind::RadialGradient &&
                     (mask.radiusX <= 0.0 || mask.radiusY <= 0.0))) {
                    return InvalidArgument("RenderFrame gradient mask is invalid");
                }
            }
        }
        if (node.effectIndex != UINT32_MAX) {
            const RenderEffectSnapshot* effectPtr = frame.EffectAt(node.effectIndex);
            if (effectPtr == nullptr) {
                return InvalidArgument("RenderFrame node effect index out of range");
            }
            const auto& effect = *effectPtr;
            if (static_cast<std::uint8_t>(effect.kind) >
                    static_cast<std::uint8_t>(RenderEffectKind::Custom) ||
                effect.radius < 0.0 ||
                !std::isfinite(effect.radius) ||
                !std::isfinite(effect.direction) ||
                effect.depth < 0.0 ||
                !std::isfinite(effect.depth) ||
                !IsValidOpacity(effect.opacity) ||
                !IsFinite(effect.color)) {
                return InvalidArgument("RenderFrame node effect is invalid");
            }
        }
    }

    for (const RenderCommandHeader& h : frame.CommandHeaders()) {
        switch (h.kind) {
        case RenderCommandKind::PushClip: {
            const auto& cmd = frame.PayloadAt<CmdPushClip>(h.payloadOffset);
            if (!IsValidLayoutRect(cmd.rect)) {
                return InvalidArgument("RenderFrame contains an invalid clip");
            }
            ++clipDepth;
            break;
        }
        case RenderCommandKind::PopClip:
            if (clipDepth == 0U) {
                return InvalidState("RenderFrame clip stack underflow");
            }
            --clipDepth;
            break;
        case RenderCommandKind::PushOpacity: {
            const auto& cmd = frame.PayloadAt<CmdPushOpacity>(h.payloadOffset);
            if (!IsValidOpacity(cmd.opacity)) {
                return InvalidArgument("RenderFrame contains invalid opacity");
            }
            ++opacityDepth;
            break;
        }
        case RenderCommandKind::PopOpacity:
            if (opacityDepth == 0U) {
                return InvalidState("RenderFrame opacity stack underflow");
            }
            --opacityDepth;
            break;
        case RenderCommandKind::PushTransform: {
            const auto& cmd = frame.PayloadAt<CmdPushTransform>(h.payloadOffset);
            if (!IsFinite(cmd.transform)) {
                return InvalidArgument("RenderFrame contains invalid transform");
            }
            ++transformDepth;
            break;
        }
        case RenderCommandKind::PopTransform:
            if (transformDepth == 0U) {
                return InvalidState("RenderFrame transform stack underflow");
            }
            --transformDepth;
            break;
        case RenderCommandKind::FillRect: {
            const auto& cmd = frame.PayloadAt<CmdFillRect>(h.payloadOffset);
            if (!IsValidLayoutRect(cmd.rect) || !IsFinite(cmd.color)) {
                return InvalidArgument("RenderFrame contains invalid FillRect");
            }
            break;
        }
        case RenderCommandKind::FillRoundedRect: {
            const auto& cmd = frame.PayloadAt<CmdRoundedRect>(h.payloadOffset);
            if (!IsValidLayoutRect(cmd.rect) || !IsFinite(cmd.color) ||
                !std::isfinite(cmd.radius) || cmd.radius < 0.0f ||
                cmd.radius * 2.0f > std::fmin(cmd.rect.width, cmd.rect.height) + 1.0e-3f) {
                return InvalidArgument("RenderFrame contains invalid FillRoundedRect");
            }
            break;
        }
        case RenderCommandKind::StrokeRect: {
            const auto& cmd = frame.PayloadAt<CmdStrokeRect>(h.payloadOffset);
            if (!IsValidLayoutRect(cmd.rect) || !IsFinite(cmd.color) ||
                !std::isfinite(cmd.thickness) || cmd.thickness < 0.0f ||
                !std::isfinite(cmd.cornerRadius) ||
                cmd.cornerRadius < 0.0f ||
                cmd.cornerRadius * 2.0f >
                    std::fmin(cmd.rect.width, cmd.rect.height) + 1.0e-3f) {
                return InvalidArgument("RenderFrame contains invalid StrokeRect");
            }
            break;
        }
        case RenderCommandKind::DrawAnalyticalShadow: {
            const auto& cmd = frame.PayloadAt<CmdAnalyticalShadow>(h.payloadOffset);
            if (!IsValidLayoutRect(cmd.rect) || !IsFinite(cmd.color)) {
                return InvalidArgument("RenderFrame contains invalid DrawAnalyticalShadow");
            }
            break;
        }
        case RenderCommandKind::FillGradientQuad: {
            const auto& cmd = frame.PayloadAt<CmdGradientQuad>(h.payloadOffset);
            for (int i = 0; i < 4; ++i) {
                if (!::Aero::IsFinite(cmd.points[i]) || !IsFinite(cmd.colors[i])) {
                    return InvalidArgument("RenderFrame contains invalid FillGradientQuad");
                }
            }
            break;
        }
        case RenderCommandKind::DrawTriangles: {
            const auto& cmd = frame.PayloadAt<CmdDrawTriangles>(h.payloadOffset);
            if (cmd.vertexCount == 0U || cmd.indexCount == 0U || !IsFinite(cmd.color)) {
                return InvalidArgument("RenderFrame contains invalid DrawTriangles");
            }
            break;
        }
        case RenderCommandKind::DrawImage: {
            const auto& cmd = frame.PayloadAt<CmdDrawImage>(h.payloadOffset);
            if (cmd.image == InvalidRenderImageId ||
                !IsValidLayoutRect(cmd.destination) ||
                !IsValidImageUv(cmd.sourceUv) ||
                !IsFinite(cmd.tint)) {
                return InvalidArgument("RenderFrame contains invalid DrawImage");
            }
            break;
        }
        case RenderCommandKind::DrawSolidMesh: {
            const auto& cmd = frame.PayloadAt<CmdDrawSolidMesh>(h.payloadOffset);
            if (cmd.mesh == InvalidRenderMeshId || !IsFinite(cmd.tint)) {
                return InvalidArgument("RenderFrame contains invalid DrawSolidMesh");
            }
            break;
        }
        case RenderCommandKind::DrawGradientMesh: {
            const auto& cmd = frame.PayloadAt<CmdDrawGradientMesh>(h.payloadOffset);
            if (cmd.mesh == InvalidRenderMeshId || !IsFinite(cmd.tint)) {
                return InvalidArgument("RenderFrame contains invalid DrawGradientMesh");
            }
            break;
        }
        case RenderCommandKind::DrawGlyphRun: {
            const auto& cmd = frame.PayloadAt<CmdDrawGlyphRun>(h.payloadOffset);
            if (cmd.glyphRun == InvalidRenderGlyphRunId || !IsFinite(cmd.tint)) {
                return InvalidArgument("RenderFrame contains invalid DrawGlyphRun");
            }
            break;
        }
        }
    }

    if (clipDepth != 0U || opacityDepth != 0U || transformDepth != 0U) {
        return InvalidState("RenderFrame contains unbalanced state stacks");
    }
    return {};
}

} // namespace Aero::Render

namespace Aero::Render {

using namespace ::Aero;
using namespace ::Aero::Threading;

RenderTree::RenderTree(Dispatcher& dispatcher) noexcept
    : dispatcher_(&dispatcher), dirty_(), drawings_(), currentFrame_() {}

RenderTree::~RenderTree() noexcept {
    // P3.2: no frame-hook registration; nothing to unregister.
    if (root_ != nullptr && dispatcher_->CheckAccess()) {
        auto clear = [&](auto&& self, ::Aero::Media::Visual& visual) noexcept -> void {
            for (::Aero::Media::Visual* child :
                 (visual).RenderChildren()) {
                if (child == nullptr) continue;
                self(self, *child);
            }
            (visual).RenderAttached() = false;
            (visual).RenderQueued() = false;
            (visual).RenderValid() = false;
            (visual).NodeId() = InvalidRenderNodeId;
            RemoveDrawing(visual);
        };
        clear(clear, *root_);
        root_ = nullptr;
    }
    for (const OverlayRecord& overlay : overlays_) {
        if (overlay.element != nullptr) {
            overlay.element->SetOverlayFlag(false);
        }
    }
}

Base::Result<void> RenderTree::Initialize() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (initialized_) {
        return {};
    }
    // P3.2: ViewFrame drives RenderCommitHook() directly; no hook.
    initialized_ = true;
    return {};
}

Base::Result<void> RenderTree::VerifyElement(
    const ::Aero::Media::Visual& element) const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (!initialized_) {
        return InvalidState("RenderTree must be initialized before use");
    }
    if (&element.GetDispatcher() != dispatcher_) {
        return Base::Status::Failure(
            Base::ErrorCode::WrongThread,
            "Visual belongs to another Dispatcher");
    }
    if (committing_) {
        return InvalidState("Render tree mutation during commit is not allowed");
    }
    return {};
}

Base::Result<void> RenderTree::SetRoot(
    ::Aero::Media::Visual* root) noexcept {
    if (root == nullptr) {
        Base::Result<void> access = dispatcher_->VerifyAccess();
        if (!access) return access.GetStatus();
        if (committing_) {
            return InvalidState(
                "Render root cannot change during commit");
        }
        if (root_ != nullptr) {
            auto clear = [&](auto&& self,
                             ::Aero::Media::Visual& element) noexcept -> void {
                for (::Aero::Media::Visual* child :
                     (element).RenderChildren()) {
                    if (child == nullptr) continue;
                    self(self, *child);
                }
                RemoveQueued(element);
                RemoveDrawing(element);
                (element).RenderAttached() = false;
                (element).RenderValid() = false;
                (element).RenderDirtyFlags() =
                    static_cast<std::uint8_t>(
                        RenderInvalidation::All);
                (element).NodeId() = InvalidRenderNodeId;
            };
            clear(clear, *root_);
        }
        root_ = nullptr;
        dirty_.Clear();
        drawings_.Clear();
        overlays_.Clear();
        // P4.3: membership changed; committed node order is no longer valid.
        ++structureVersion_;
        return {};
    }

    Base::Result<void> verified = VerifyElement(*root);
    if (!verified) return verified.GetStatus();
    if (root_ == root) return {};
    if (root_ != nullptr || ElementTree::RenderTreeOf(*root) != nullptr ||
        (*root).RenderAttached() || root->GetVisualParent() != nullptr) {
        return InvalidState("Render root must be detached and unique");
    }
    if (nextNodeId_ == InvalidRenderNodeId) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Render node ID space exhausted");
    }

    Base::Result<Aero::VisualLease> lease =
        Aero::VisualLease::Acquire(*root);
    if (!lease) return lease.GetStatus();
    dirty_.Reserve(dirty_.Size() + 1U);

    root_ = root;
    (*root).NodeId() = nextNodeId_++;
    (*root).RenderValid() = false;
    (*root).RenderDirtyFlags() =
        static_cast<std::uint8_t>(
            RenderInvalidation::All);
    dirty_.PushBack(std::move(lease).Value());
    (*root).RenderQueued() = true;
    // P4.3: new root identity; committed node order is no longer valid.
    ++structureVersion_;
    return {};
}

Base::Result<void> RenderTree::Attach(
    ::Aero::Media::Visual& parent,
    ::Aero::Media::Visual& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) return verified.GetStatus();
    verified = VerifyElement(child);
    if (!verified) return verified.GetStatus();
    if ((child).RenderAttached() &&
        (child).GetVisualParent() == &parent &&
        ElementTree::RenderTreeOf(child) == this) {
        return {};
    }
    // Note: a stale RenderParent on a detached child is irrelevant and must
    // not block re-attachment; the RenderAttached(child) check above already
    // catches genuine double-attach conflicts.
    if (ElementTree::RenderTreeOf(parent) != this ||
        ElementTree::RenderTreeOf(child) != nullptr ||
        (child).RenderAttached()) {
        return InvalidState(
            "Render attachment must match the visual-tree parent");
    }
    if (nextNodeId_ == InvalidRenderNodeId) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Render node ID space exhausted");
    }

    Base::Result<Aero::VisualLease> childLease =
        Aero::VisualLease::Acquire(child);
    if (!childLease) return childLease.GetStatus();

    std::uint32_t required = 1U;
    for (::Aero::Media::Visual* current = &parent; current != nullptr;
         current = (*current).RenderAttached()
             ? (*current).GetVisualParent() : nullptr) {
        if (!(*current).RenderQueued()) ++required;
    }
    dirty_.Reserve(dirty_.Size() + required);

    Base::Result<void> invalidated = Invalidate(
        parent, RenderInvalidation::Children);
    if (!invalidated) return invalidated.GetStatus();

    (child).RenderAttached() = true;
    (child).NodeId() = nextNodeId_++;
    (child).RenderValid() = false;
    (child).RenderDirtyFlags() =
        static_cast<std::uint8_t>(
            RenderInvalidation::All);
    dirty_.PushBack(
        std::move(childLease).Value());
    (child).RenderQueued() = true;
    // P4.3: new node identity; committed node order is no longer valid.
    ++structureVersion_;
    return {};
}

Base::Result<void> RenderTree::Detach(
    ::Aero::Media::Visual& parent,
    ::Aero::Media::Visual& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) return verified.GetStatus();
    if (!(child).RenderAttached() ||
        ElementTree::RenderTreeOf(child) != this) {
        return {};
    }
    // The element-tree edge may name a logical parent that is not the render
    // (visual) parent. Detach from wherever the child is actually attached.
    ::Aero::Media::Visual* attachParent = (child).GetVisualParent();
    if (attachParent == nullptr) attachParent = &parent;

    if (ElementTree::RenderTreeOf(*attachParent) == this) {
        Base::Result<void> invalidated = Invalidate(
            *attachParent, RenderInvalidation::Children);
        if (!invalidated) return invalidated.GetStatus();
    }

    auto clear = [&](auto&& self,
                     ::Aero::Media::Visual& element) noexcept -> void {
        if (!element.RenderAttached()) {
            return;
        }
        element.RenderAttached() = false;
        for (::Aero::Media::Visual* descendant :
             element.RenderChildren()) {
            if (descendant == nullptr || descendant == &element) continue;
            self(self, *descendant);
        }
        RemoveQueued(element);
        RemoveDrawing(element);
        element.RenderValid() = false;
        element.RenderDirtyFlags() =
            static_cast<std::uint8_t>(
                RenderInvalidation::All);
        element.NodeId() = InvalidRenderNodeId;
    };
    clear(clear, child);
    // P4.3: node identities removed; committed node order is no longer valid.
    ++structureVersion_;
    return {};
}

Base::Result<void> RenderTree::QueueDirty(
    ::Aero::Media::Visual& element) noexcept {
    if ((element).RenderQueued()) return {};
    Base::Result<Aero::VisualLease> lease =
        Aero::VisualLease::Acquire(element);
    if (!lease) return lease.GetStatus();
    dirty_.PushBack(std::move(lease).Value());
    (element).RenderQueued() = true;
    return {};
}

void RenderTree::RemoveQueued(::Aero::Media::Visual& element) noexcept {
    for (std::uint32_t index = 0U; index < dirty_.Size();) {
        if (dirty_[index].Resolve() == &element) {
            dirty_[index] = std::move(dirty_[dirty_.Size() - 1U]);
            dirty_.PopBack();
        } else {
            ++index;
        }
    }
    (element).RenderQueued() = false;
}

RenderTree::DrawingRecord*
RenderTree::FindDrawing(::Aero::Media::Visual& visual) noexcept {
    const std::uint32_t index = visual.drawingIndex_;
    if (index < drawings_.Size() && drawings_[index].visual == &visual) {
        return &drawings_[index];
    }
    return nullptr;
}

void RenderTree::RemoveDrawing(::Aero::Media::Visual& visual) noexcept {
    const std::uint32_t index = visual.drawingIndex_;
    if (index < drawings_.Size() && drawings_[index].visual == &visual) {
        const std::uint32_t lastIndex = drawings_.Size() - 1U;
        if (index != lastIndex) {
            drawings_[index] = std::move(drawings_[lastIndex]);
            if (drawings_[index].visual != nullptr) {
                drawings_[index].visual->drawingIndex_ = index;
            }
        }
        drawings_.PopBack();
        visual.drawingIndex_ = UINT32_MAX;
    }
}

void RenderTree::MarkCommittedSubtree(
    ::Aero::Media::Visual& visual,
    bool ancestorVisible) noexcept {
    UIElement* element = ::Aero::TryCast<::Aero::UIElement>(&visual);
    FrameworkElement* framework =
        ::Aero::TryCast<::Aero::FrameworkElement>(&visual);
    const bool visible =
        ancestorVisible &&
        (element == nullptr ||
         element->GetVisibility() ==
             Visibility::Visible);
    std::uint8_t processed =
        (visual).RenderDirtyFlags();
    if (!visible && framework != nullptr) {
        processed &= static_cast<std::uint8_t>(
            ~static_cast<std::uint8_t>(
                RenderInvalidation::Drawing));
    }
    if (processed != 0U &&
        (visual).RenderRevision() !=
            UINT64_MAX) {
        ++(visual).RenderRevision();
    }
    (visual).RenderDirtyFlags() &=
        static_cast<std::uint8_t>(~processed);
    (visual).RenderValid() =
        (visual).RenderDirtyFlags() == 0U;
    (visual).RenderQueued() = false;
    for (::Aero::Media::Visual* child :
         (visual).RenderChildren()) {
        if (child != nullptr) {
            MarkCommittedSubtree(*child, visible);
        }
    }
}

Base::Result<void> RenderTree::Invalidate(
    ::Aero::Media::Visual& element,
    RenderInvalidation invalidation) noexcept {
    Base::Result<void> verified = VerifyElement(element);
    if (!verified) return verified.GetStatus();
    if (ElementTree::RenderTreeOf(element) != this) {
        return InvalidState(
            "Visual is not attached to this RenderTree");
    }

    (element).RenderDirtyFlags() |=
        static_cast<std::uint8_t>(invalidation);
    (element).RenderValid() = false;
    if (HasRenderInvalidation(
            invalidation,
            RenderInvalidation::Drawing) &&
        HasRenderInvalidation(
            invalidation,
            RenderInvalidation::Children)) {
        auto dirtySubtree = [&](auto&& self,
                                ::Aero::Media::Visual& visual) noexcept -> void {
            for (::Aero::Media::Visual* child :
                 (visual).RenderChildren()) {
                if (child == nullptr) continue;
                (*child).RenderDirtyFlags() |=
                    static_cast<std::uint8_t>(
                        RenderInvalidation::State |
                        RenderInvalidation::Drawing);
                (*child).RenderValid() = false;
                self(self, *child);
            }
        };
        dirtySubtree(dirtySubtree, element);
    }

    Base::Vector<::Aero::Media::Visual*> path;
    for (::Aero::Media::Visual* current = &element; current != nullptr;
         current = (*current).RenderAttached()
             ? (*current).GetVisualParent() : nullptr) {
        Base::Result<void> currentVerified = VerifyElement(*current);
        if (!currentVerified) return currentVerified.GetStatus();
        path.PushBack(current);
    }

    Base::Vector<Aero::VisualLease> leases;
    leases.Reserve(path.Size());
    for (::Aero::Media::Visual* current : path) {
        if ((*current).RenderQueued()) continue;
        Base::Result<Aero::VisualLease> lease =
            Aero::VisualLease::Acquire(*current);
        if (!lease) return lease.GetStatus();
        leases.PushBack(std::move(lease).Value());
    }
    dirty_.Reserve(dirty_.Size() + leases.Size());

    std::uint32_t leaseIndex = 0U;
    for (::Aero::Media::Visual* current : path) {
        if ((*current).RenderQueued()) continue;
        dirty_.PushBack(
            std::move(leases[leaseIndex++]));
        (*current).RenderQueued() = true;
    }
    return {};
}

} // namespace Aero::Render

namespace Aero {

void
Media::Visual::InvalidateRenderDrawing() noexcept {
    using Render::RenderInvalidation;
    using Render::RenderTree;
    if (ElementTree::RenderTreeOf(*this) == nullptr) {
        RenderDirtyFlags() |=
            static_cast<std::uint8_t>(
                RenderInvalidation::Drawing);
        RenderValid() = false;
        return;
    }
    Base::Result<void> invalidated =
        ElementTree::RenderTreeOf(*this)->Invalidate(
            *this,
            RenderInvalidation::Drawing);
    AERO_ASSERT(invalidated);
    (void)invalidated;
}

void
Media::Visual::InvalidateRenderState() noexcept {
    using Render::RenderInvalidation;
    using Render::RenderTree;
    if (ElementTree::RenderTreeOf(*this) == nullptr) {
        RenderDirtyFlags() |=
            static_cast<std::uint8_t>(
                RenderInvalidation::State);
        RenderValid() = false;
        return;
    }
    Base::Result<void> invalidated =
        ElementTree::RenderTreeOf(*this)->Invalidate(
            *this,
            RenderInvalidation::State);
    AERO_ASSERT(invalidated);
}

} // namespace Aero

namespace Aero::Render {

using namespace ::Aero;
using namespace ::Aero::Meta;

bool RenderTree::IsOverlay(
    const ::Aero::Media::Visual& element) const noexcept {
    return element.IsOverlayFlag();
}

Base::Result<void> RenderTree::SetOverlays(
    Base::Span<FrameworkElement* const> overlays,
    Base::Span<const Point> origins) noexcept {
    if (overlays.Size() != origins.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render overlay elements and origins must have equal lengths");
    }
    Base::Vector<Base::Transform2D> transforms;
    transforms.Reserve(origins.Size());
    for (std::uint32_t index = 0U; index < origins.Size(); ++index) {
        Base::Transform2D t{};
        t.dx = origins[index].x;
        t.dy = origins[index].y;
        transforms.PushBack(t);
    }
    return SetOverlays(overlays, transforms.AsSpan());
}

Base::Result<void> RenderTree::SetOverlays(
    Base::Span<FrameworkElement* const> overlays,
    Base::Span<const Base::Transform2D> transforms) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (committing_) {
        return InvalidState(
            "Render overlays cannot change during commit");
    }

    if (overlays.Size() != transforms.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render overlay elements and transforms must have equal lengths");
    }
    Base::Vector<OverlayRecord> next;
    next.Reserve(overlays.Size());
    for (std::uint32_t index = 0U;
         index < overlays.Size();
         ++index) {
        FrameworkElement* overlay =
            overlays[index];
        if (overlay == nullptr ||
            ElementTree::RenderTreeOf(*overlay) != this) {
            return InvalidState(
                "Render overlay must belong to this render tree");
        }
        if (!Aero::Base::IsFiniteTransform(transforms[index])) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Render overlay transform must be finite");
        }
        bool duplicate = false;
        for (const OverlayRecord& current :
             next) {
            duplicate =
                duplicate ||
                current.element == overlay;
        }
        if (duplicate) continue;
        next.PushBack(
                {overlay, transforms[index]});
    }

    bool changed = next.Size() != overlays_.Size();
    if (!changed) {
        for (std::uint32_t index = 0U;
             index < next.Size();
             ++index) {
            const auto& a = next[index].transform;
            const auto& b = overlays_[index].transform;
            if (next[index].element !=
                    overlays_[index].element ||
                a.m11 != b.m11 || a.m12 != b.m12 ||
                a.m21 != b.m21 || a.m22 != b.m22 ||
                a.dx != b.dx || a.dy != b.dy) {
                changed = true;
                break;
            }
        }
    }
    if (!changed) return {};
    for (const OverlayRecord& oldOverlay : overlays_) {
        if (oldOverlay.element != nullptr) {
            oldOverlay.element->SetOverlayFlag(false);
        }
    }
    for (const OverlayRecord& newOverlay : next) {
        if (newOverlay.element != nullptr) {
            newOverlay.element->SetOverlayFlag(true);
        }
    }
    overlays_ = std::move(next);
    // P4.3: overlay membership feeds the commit tail; committed node order
    // is no longer valid.
    ++structureVersion_;
    if (root_ != nullptr) {
        return Invalidate(
            *root_,
            RenderInvalidation::Children);
    }
    return {};
}

Base::Result<void> RenderTree::SetViewport(
    Aero::Size logicalSize,
    std::uint32_t pixelWidth,
    std::uint32_t pixelHeight,
    double dpiScale) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (committing_) {
        return InvalidState(
            "Render viewport cannot change during commit");
    }
    if (!IsValidLayoutSize(logicalSize) ||
        !std::isfinite(dpiScale) || dpiScale <= 0.0 ||
        (logicalSize.width == 0.0 && pixelWidth != 0U) ||
        (logicalSize.height == 0.0 && pixelHeight != 0U)) {
        return InvalidArgument("Render viewport is invalid");
    }
    if (logicalSize_.width == logicalSize.width &&
        logicalSize_.height == logicalSize.height &&
        pixelWidth_ == pixelWidth && pixelHeight_ == pixelHeight &&
        dpiScale_ == dpiScale) {
        return {};
    }
    logicalSize_ = logicalSize;
    pixelWidth_ = pixelWidth;
    pixelHeight_ = pixelHeight;
    dpiScale_ = dpiScale;
    viewportDirty_ = true;
    return {};
}

RenderEffectSnapshot RenderTree::BuildEffectSnapshot(
    const ::Aero::Media::Effect* effect) noexcept {
    RenderEffectSnapshot snapshot;
    if (effect == nullptr) return snapshot;
    if (effect->RuntimeType() ==
        ::Aero::Media::BlurEffect::StaticTypeId()) {
        snapshot.kind = RenderEffectKind::Blur;
        snapshot.radius = static_cast<
            const ::Aero::Media::BlurEffect*>(effect)->GetRadius();
    } else if (effect->RuntimeType() ==
        ::Aero::Media::DropShadowEffect::StaticTypeId()) {
        const auto* drop =
            static_cast<const ::Aero::Media::DropShadowEffect*>(effect);
        snapshot.kind = RenderEffectKind::DropShadow;
        snapshot.radius = drop->GetBlurRadius();
        snapshot.direction = drop->GetDirection();
        snapshot.depth = drop->GetShadowDepth();
        snapshot.opacity = drop->GetOpacity();
        snapshot.color = drop->GetColor();
    } else if (effect->RuntimeType() ==
        ::Aero::Media::PixelateEffect::StaticTypeId()) {
        snapshot.kind = RenderEffectKind::Pixelate;
        snapshot.size = static_cast<
            const ::Aero::Media::PixelateEffect*>(effect)->GetSize();
    } else if (effect->RuntimeType() ==
        ::Aero::Media::TintEffect::StaticTypeId()) {
        snapshot.kind = RenderEffectKind::Tint;
        snapshot.color = static_cast<
            const ::Aero::Media::TintEffect*>(effect)->GetColor();
    } else if (effect->RuntimeType() ==
        ::Aero::Media::DirectionalBlurEffect::StaticTypeId()) {
        const auto* blur =
            static_cast<const ::Aero::Media::DirectionalBlurEffect*>(effect);
        snapshot.kind = RenderEffectKind::DirectionalBlur;
        snapshot.radius = blur->GetRadius();
        snapshot.direction = blur->GetAngle();
    } else if (effect->RuntimeType() ==
        ::Aero::Media::ShaderEffect::StaticTypeId()) {
        const auto* shader =
            static_cast<const ::Aero::Media::ShaderEffect*>(effect);
        snapshot.kind = RenderEffectKind::Custom;
        snapshot.shaderId = shader->GetShaderId();
        snapshot.shaderSource = shader->GetPixelShader();
        snapshot.bytecode = shader->GetBytecode();
        const Base::Span<const float> uniforms = shader->GetUniforms();
        snapshot.uniformCount = std::min(uniforms.Size(), 16U);
        for (std::uint32_t index = 0U; index < snapshot.uniformCount; ++index) {
            snapshot.uniforms[index] = uniforms[index];
        }
    }
    return snapshot;
}

std::uint32_t RenderTree::AppendGradientRamp(
    RenderFrame& plan,
    const ::Aero::Media::GradientBrush& brush) noexcept {
    const std::uintptr_t brushIdentity =
        reinterpret_cast<std::uintptr_t>(&brush);
    std::uint64_t revision = brush.GetRevision();
    for (const auto& stop : brush.GetGradientStops()) {
        if (stop) {
            revision = Base::MixHash64(revision ^ stop->GetRevision());
            const Color c = stop->GetColor();
            const double offset = stop->GetOffset();
            std::uint32_t rBits = 0, gBits = 0, bBits = 0, aBits = 0;
            std::uint64_t oBits = 0;
            std::memcpy(&rBits, &c.red, sizeof(rBits));
            std::memcpy(&gBits, &c.green, sizeof(gBits));
            std::memcpy(&bBits, &c.blue, sizeof(bBits));
            std::memcpy(&aBits, &c.alpha, sizeof(aBits));
            std::memcpy(&oBits, &offset, sizeof(oBits));
            const std::uint64_t cBits =
                (static_cast<std::uint64_t>(rBits) << 32) ^
                (static_cast<std::uint64_t>(gBits)) ^
                (static_cast<std::uint64_t>(bBits) << 16) ^
                (static_cast<std::uint64_t>(aBits));
            revision = Base::MixHash64(revision ^ cBits ^ oBits);
        }
    }

    Base::Span<const RenderGradientRampSnapshot> existing =
        plan.GradientRamps();
    std::uint32_t targetIndex = UINT32_MAX;
    for (std::uint32_t i = 0U; i < existing.Size(); ++i) {
        if (existing[i].brushIdentity != brushIdentity) continue;
        if (existing[i].revision == revision) {
            return i;
        }
        targetIndex = i;
        break;
    }

    RenderGradientRampSnapshot* dst = nullptr;
    if (targetIndex != UINT32_MAX) {
        dst = &plan.gradientRamps_[targetIndex];
    } else {
        RenderGradientRampSnapshot newRamp;
        static_cast<void>(plan.gradientRamps_.PushBack(newRamp));
        targetIndex = plan.gradientRamps_.Size() - 1U;
        dst = &plan.gradientRamps_[targetIndex];
    }

    dst->brushIdentity = brushIdentity;
    dst->revision = revision;
    for (std::uint32_t i = 0U; i < GradientRampWidth; ++i) {
        const double position =
            static_cast<double>(i) /
            static_cast<double>(GradientRampWidth - 1U);
        const Color color = SampleGradient(brush, position);
        const float alpha = color.alpha;
        dst->pixels[i * 4U + 0U] = static_cast<std::uint8_t>(
            std::clamp(color.red * alpha * 255.0F + 0.5F, 0.0F, 255.0F));
        dst->pixels[i * 4U + 1U] = static_cast<std::uint8_t>(
            std::clamp(color.green * alpha * 255.0F + 0.5F, 0.0F, 255.0F));
        dst->pixels[i * 4U + 2U] = static_cast<std::uint8_t>(
            std::clamp(color.blue * alpha * 255.0F + 0.5F, 0.0F, 255.0F));
        dst->pixels[i * 4U + 3U] = static_cast<std::uint8_t>(
            std::clamp(alpha * 255.0F + 0.5F, 0.0F, 255.0F));
    }
    return targetIndex;
}

RenderMaskSnapshot RenderTree::BuildMaskSnapshot(
    const ::Aero::UIElement& element,
    RenderFrame& plan) noexcept {
    RenderMaskSnapshot snapshot;
    const Base::Ref<::Aero::Media::Brush> brush =
        element.GetOpacityMask();
    if (!brush) return snapshot;
    if (brush->RuntimeType() ==
        ::Aero::Media::SolidColorBrush::StaticTypeId()) {
        snapshot.kind = RenderMaskKind::Solid;
        snapshot.color = static_cast<
            const ::Aero::Media::SolidColorBrush*>(brush.Get())->GetColor();
        snapshot.color.alpha *=
            static_cast<float>(brush->GetOpacity());
    } else if (brush->RuntimeType() ==
        ::Aero::Media::ImageBrush::StaticTypeId()) {
        const auto* image =
            static_cast<const ::Aero::Media::ImageBrush*>(brush.Get());
        snapshot.kind = RenderMaskKind::Image;
        snapshot.image = image->GetRenderImageId();
        snapshot.imageWidth =
            image->GetPixelWidth();
        snapshot.imageHeight =
            image->GetPixelHeight();
        Rect source = image->GetViewbox();
        if (image->GetViewboxUnits() ==
            ::Aero::Media::BrushMappingMode::Absolute) {
            if (snapshot.imageWidth != 0U) {
                source.x /= static_cast<double>(snapshot.imageWidth);
                source.width /= static_cast<double>(snapshot.imageWidth);
            }
            if (snapshot.imageHeight != 0U) {
                source.y /= static_cast<double>(snapshot.imageHeight);
                source.height /= static_cast<double>(snapshot.imageHeight);
            }
        }
        snapshot.sourceUv = source;
        snapshot.viewport = image->GetViewport();
        snapshot.viewboxUnits = static_cast<std::uint8_t>(
            image->GetViewboxUnits());
        snapshot.viewportUnits = static_cast<std::uint8_t>(
            image->GetViewportUnits());
        snapshot.stretch = static_cast<std::uint8_t>(
            image->GetStretch());
        snapshot.tileMode = static_cast<std::uint8_t>(
            image->GetTileMode());
        snapshot.alignmentX = static_cast<std::uint8_t>(
            image->GetAlignmentX());
        snapshot.alignmentY = static_cast<std::uint8_t>(
            image->GetAlignmentY());
    } else if (brush->RuntimeType() ==
        ::Aero::Media::LinearGradientBrush::StaticTypeId()) {
        const auto* gradient =
            static_cast<const ::Aero::Media::LinearGradientBrush*>(
                brush.Get());
        snapshot.kind = RenderMaskKind::LinearGradient;
        snapshot.mappingMode = static_cast<std::uint8_t>(
            gradient->GetMappingMode());
        snapshot.startPoint = gradient->GetStartPoint();
        snapshot.endPoint = gradient->GetEndPoint();
        snapshot.tileMode = gradient->GetSpreadMethod() == ::Aero::Media::GradientSpreadMethod::Repeat ? 1U :
            (gradient->GetSpreadMethod() == ::Aero::Media::GradientSpreadMethod::Reflect ? 2U : 0U);
        snapshot.gradientRamp = AppendGradientRamp(plan, *gradient);
    } else if (brush->RuntimeType() ==
        ::Aero::Media::RadialGradientBrush::StaticTypeId()) {
        const auto* gradient =
            static_cast<const ::Aero::Media::RadialGradientBrush*>(
                brush.Get());
        snapshot.kind = RenderMaskKind::RadialGradient;
        snapshot.mappingMode = static_cast<std::uint8_t>(
            gradient->GetMappingMode());
        snapshot.center = gradient->GetCenter();
        snapshot.gradientOrigin = gradient->GetGradientOrigin();
        snapshot.radiusX = gradient->GetRadiusX();
        snapshot.radiusY = gradient->GetRadiusY();
        snapshot.tileMode = gradient->GetSpreadMethod() == ::Aero::Media::GradientSpreadMethod::Repeat ? 1U :
            (gradient->GetSpreadMethod() == ::Aero::Media::GradientSpreadMethod::Reflect ? 2U : 0U);
        snapshot.gradientRamp = AppendGradientRamp(plan, *gradient);
    }
    return snapshot;
}

bool RenderTree::IsEmittedChild(
    const ::Aero::Media::Visual& child) noexcept {
    const Meta::TypeId childType = child.RuntimeType();
    const Meta::TypeRegistry& childTypes =
        DependencyObjectAccess::PropertyRegistry((child)).Types();
    // Popup-style visuals remain logical/template children so bindings,
    // layout and routed events keep their WPF shape. They must never be
    // emitted inline, though: an open popup is committed exactly once via
    // the overlay list, and a closed popup is omitted altogether.
    const bool overlayHost =
        childTypes.IsDerivedFrom(
            childType,
            Controls::Primitives::Popup::StaticTypeId()) ||
        childTypes.IsDerivedFrom(
            childType,
            Controls::ContextMenu::StaticTypeId()) ||
        childTypes.IsDerivedFrom(
            childType,
            Documents::AdornerLayer::StaticTypeId());
    if (overlayHost) return false;
    // Open overlay instances additionally pass IsOverlay(); callers check
    // both so an open popup is committed exactly once via the overlay list.
    return true;
}

void RenderTree::ResolveCommandGradients(
    RenderFrame& plan,
    std::uint32_t start,
    std::uint32_t count) noexcept {
    for (std::uint32_t i = 0U; i < count; ++i) {
        RenderCommandHeader& h = plan.commandHeaders_[start + i];
        if (h.paintKind == 0U) {
            continue;
        }
        if (h.kind == RenderCommandKind::FillGradientQuad) {
            auto& quad = plan.MutablePayloadAt<CmdGradientQuad>(h.payloadOffset);
            if (quad.gradientBrush != 0U) {
                const auto* brush = reinterpret_cast<
                    const ::Aero::Media::GradientBrush*>(quad.gradientBrush);
                quad.gradientRamp = AppendGradientRamp(plan, *brush);
            }
        } else if (h.kind == RenderCommandKind::DrawGradientMesh) {
            auto& mesh = plan.MutablePayloadAt<CmdDrawGradientMesh>(h.payloadOffset);
            if (mesh.gradientBrush != 0U) {
                const auto* brush = reinterpret_cast<
                    const ::Aero::Media::GradientBrush*>(mesh.gradientBrush);
                mesh.gradientRamp = AppendGradientRamp(plan, *brush);
            }
        }
    }
}

Base::Result<void> RenderTree::DescribeVisual(
    ::Aero::Media::Visual& visual,
    RenderNodeId parentId,
    bool overlayRoot,
    const ::Aero::Media::Transform3DContext& transform3D,
    RenderFrame& plan,
    const DisplayList*& drawingOut,
    RenderNodeSnapshot& snapshotOut,
    bool& skipOut,
    bool& visibleOut,
    ::Aero::Media::Transform3DContext& childContextOut,
    bool& clippedOut) noexcept {
    drawingOut = nullptr;
    snapshotOut = RenderNodeSnapshot{};
    skipOut = false;
    visibleOut = false;
    childContextOut = transform3D;
    clippedOut = false;

    UIElement* element = ::Aero::TryCast<::Aero::UIElement>(&visual);
    FrameworkElement* framework =
        ::Aero::TryCast<::Aero::FrameworkElement>(&visual);
    const bool visible =
        element == nullptr ||
        element->GetVisibility() ==
            Visibility::Visible;
    const bool reentrant = (visual).Rendering();
    const bool unarranged =
        visible && element != nullptr && !element->GetIsArrangeValid();
    if (reentrant || unarranged) {
        // A visual can reach the render pass before its arrange has been
        // recomputed (e.g. a freshly mounted fragment whose layout flush has
        // not run yet) or while a nested render is in flight. Aborting the
        // entire frame commit on this is worse than rendering best-effort:
        // the visual retains its last valid layout slot, so a single
        // best-effort frame is harmless and self-corrects next pass.
        // Re-entrant OnRender must still skip non-root nodes; unarranged
        // nodes keep walking children so a stale arrangeValid flag cannot
        // wipe a realized ItemsControl subtree.
        const bool isRoot = parentId == InvalidRenderNodeId;
        if (reentrant && !isRoot) {
            skipOut = true;
            return {};
        }
        // First host frames can paint the Window before its measure/arrange
        // flush (DesktopHost Update(0) + show). Keep walking so the frame
        // self-corrects; do not spam stderr for this expected path.
    }
    visibleOut = visible;
    DrawingRecord* record = FindDrawing(visual);
    const Size currentRenderSize = framework != nullptr ? framework->GetRenderSize() : Size{};
    const bool sizeChanged = (record != nullptr && record->valid) &&
        (record->recordedSize.width != currentRenderSize.width ||
         record->recordedSize.height != currentRenderSize.height);
    if (sizeChanged) {
        (visual).RenderDirtyFlags() |=
            static_cast<std::uint8_t>(
                RenderInvalidation::Drawing);
        (visual).RenderValid() = false;
    }
    const bool drawingDirty =
        HasRenderInvalidation(
            static_cast<RenderInvalidation>(
                (visual).RenderDirtyFlags()),
            RenderInvalidation::Drawing);
    if (visible && framework != nullptr &&
        (record == nullptr || !record->valid ||
         drawingDirty)) {
        (visual).Rendering() = true;
        DisplayListBuilder builder;
        ::Aero::Media::DrawingContext context =
            Aero::Render::DrawingBridge::
                Create(builder);
        if (FrameworkElement* element = ::Aero::TryCast<FrameworkElement>(&visual)) {
            element->Render(context);
        }
        Base::Result<DisplayList> recorded =
            builder.Finish();
        (visual).Rendering() = false;
        if (!recorded) {
            return recorded.GetStatus();
        }
        if (record == nullptr) {
            record = drawings_.EmplaceBack();
            record->visual = &visual;
            visual.drawingIndex_ = drawings_.Size() - 1U;
        }
        record->drawing =
            std::move(recorded).Value();
        record->recordedSize = currentRenderSize;
        record->valid = true;
    }
    if (record == nullptr) {
        record = drawings_.EmplaceBack();
        record->visual = &visual;
        visual.drawingIndex_ = drawings_.Size() - 1U;
        record->recordedSize = currentRenderSize;
        record->valid = framework == nullptr;
    }
    const DisplayList& list = record->drawing;
    const std::uint32_t commandCount =
        visible && record->valid
        ? list.CommandCount()
        : 0U;

    if (plan.commandHeaders_.Size() > UINT32_MAX - commandCount) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "RenderFrame command count exceeds 32-bit range");
    }
    if ((visual).RenderRevision() == UINT64_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Render element revision space exhausted");
    }
    RenderNodeSnapshot snapshot;
    snapshot.id = (visual).NodeId();
    snapshot.parentId = parentId;
    snapshot.layoutSlot = element != nullptr
        ? element->GetLayoutSlot()
        : Rect{};
    snapshot.clip = element != nullptr
        ? element->GetLayoutClip()
        : Rect{};
    snapshot.clipsToBounds =
        element != nullptr &&
        element->GetClipToBounds();
    snapshot.hasLayoutClip =
        element != nullptr &&
        element->GetHasLayoutClip();
    snapshot.renderSize = element != nullptr
        ? element->GetRenderSize()
        : Size{};
    snapshot.blendMode = element != nullptr
        ? element->GetBlendMode()
        : BlendMode::Normal;
    snapshot.opacity = element != nullptr
        ? element->GetOpacity()
        : 1.0;
    if (element != nullptr) {
        RenderEffectSnapshot effect = BuildEffectSnapshot(element->GetEffect().Get());
        if (effect.kind != RenderEffectKind::None) {
            snapshot.effectIndex = plan.effects_.Size();
            plan.effects_.PushBack(std::move(effect));
        }
    }
    if (element != nullptr) {
        if (Base::Ref<Media::Geometry> clip = element->GetClip()) {
            const std::uint64_t currentRevision = clip->GetRevision();
            if (record->clipRevision != currentRevision || record->clipVertices.Empty()) {
                record->clipVertices.Clear();
                record->clipIndices.Clear();
                if (Media::TessellateGeometryFill(*clip, record->clipVertices, record->clipIndices)) {
                    record->clipRevision = currentRevision;
                }
            }
            if (!record->clipVertices.Empty() && !record->clipIndices.Empty()) {
                snapshot.geometryClipVertexOffset = plan.geometryClipVertices_.Size();
                snapshot.geometryClipIndexOffset = plan.geometryClipIndices_.Size();
                snapshot.geometryClipVertexCount = record->clipVertices.Size();
                snapshot.geometryClipIndexCount = record->clipIndices.Size();
                plan.geometryClipVertices_.Append(record->clipVertices.AsSpan());
                plan.geometryClipIndices_.Append(record->clipIndices.AsSpan());
                clippedOut = true;
            }
        } else {
            record->clipRevision = 0U;
            record->clipVertices.Clear();
            record->clipIndices.Clear();
        }
    }
    if (element != nullptr) {
        RenderMaskSnapshot mask = BuildMaskSnapshot(*element, plan);
        if (mask.kind != RenderMaskKind::None) {
            snapshot.maskIndex = plan.masks_.Size();
            plan.masks_.PushBack(std::move(mask));
        }
    }
    snapshot.commandOffset = plan.commandHeaders_.Size();
    snapshot.commandCount = commandCount;
    snapshot.elementRevision =
        (visual).RenderRevision() +
        ((visual).RenderDirtyFlags() != 0U
         ? 1U : 0U);
    if (element != nullptr) {
        if (Media::Transform3D* localTransform3D =
                element->GetTransform3D().Get()) {
            const std::uint64_t extra =
                (*localTransform3D).GetRevision();
            if (UINT64_MAX - snapshot.elementRevision < extra) {
                snapshot.elementRevision = UINT64_MAX;
            } else {
                snapshot.elementRevision += extra;
            }
        }
    }

    Base::ProjectiveTransform2D overlayPlacement = Base::IdentityProjective();
    if (overlayRoot) {
        for (const OverlayRecord& overlay : overlays_) {
            if (overlay.element == framework) {
                overlayPlacement = Base::ToProjective(overlay.transform);
                snapshot.layoutSlot.x = 0.0;
                snapshot.layoutSlot.y = 0.0;
                break;
            }
        }
    }
    const Base::ProjectiveTransform2D localVisual =
        framework != nullptr
        ? framework->GetLocalVisualTransform()
        : Base::IdentityProjective();
    Base::Transform2D viewboxMatrix{};
    const Base::Transform2D* viewbox =
        framework != nullptr && framework->TryGetViewboxTransform(viewboxMatrix)
            ? &viewboxMatrix
            : nullptr;
    if (viewbox != nullptr) {
        snapshot.hasViewboxTransform = true;
        snapshot.viewboxTransform = viewboxMatrix;
    }
    Base::Transform3 innerVisual;
    Base::Transform3 outerVisual;
    Media::LiftLocalVisualWithViewbox(
        localVisual, viewbox, innerVisual, outerVisual);
    const Media::Transform3DContext childContext =
        Media::AdvanceTransform3DContext(
            transform3D,
            element != nullptr ? element->GetTransform3D().Get() : nullptr,
            innerVisual,
            snapshot.layoutSlot,
            snapshot.renderSize,
            overlayRoot,
            outerVisual);
    Media::Transform3D* local3D =
        element != nullptr ? element->GetTransform3D().Get() : nullptr;
    const bool local3DActive =
        local3D != nullptr &&
        ::Aero::TryCast<Media::PerspectiveTransform3D>(local3D) == nullptr &&
        !Base::LeavesZ0PlaneUnchanged(local3D->GetTransform3D());
    const bool parent3DActive =
        !Base::LeavesZ0PlaneUnchanged(transform3D.accumulated);
    if (local3DActive) {
        // Collapse this element's CompositeTransform3D in local pixels
        // (CenterX/Y) and wrap Viewbox scale. Grid itself does not paint;
        // descendants inherit this multiply (see parent3DActive below).
        snapshot.renderTransform = Media::CollapseLocalTransform3D(
            local3D,
            snapshot.renderSize,
            innerVisual,
            outerVisual,
            childContext.depth);
    } else if (parent3DActive) {
        // Parent already collapsed RotationY into renderTransform. Collapsing
        // again with the window-sized camera sends a card-flip off-axis so
        // the board vanishes at 90° instead of turning.
        snapshot.renderTransform = localVisual;
    } else {
        snapshot.renderTransform =
            Media::CollapseRelativeToParent(
                transform3D,
                childContext,
                snapshot.layoutSlot,
                localVisual,
                false);
    }
    if (element != nullptr &&
        ::Aero::TryCast<Media::PerspectiveTransform3D>(
            element->GetTransform3D().Get()) != nullptr) {
        snapshot.renderTransform = localVisual;
    }
    if (overlayRoot) {
        snapshot.renderTransform =
            Base::Compose(snapshot.renderTransform, overlayPlacement);
    }

    drawingOut = &list;
    snapshotOut = snapshot;
    childContextOut = childContext;
    return {};
}

Base::Result<void> RenderTree::BuildSubtree(
    ::Aero::Media::Visual& visual,
    RenderNodeId parentId,
    RenderFrame& plan,
    bool overlayRoot,
    const ::Aero::Media::Transform3DContext& transform3D) noexcept {
    const DisplayList* drawing = nullptr;
    RenderNodeSnapshot snapshot;
    bool skipped = false;
    bool visible = false;
    Media::Transform3DContext childContext = transform3D;
    bool clipped = false;
    Base::Result<void> described = DescribeVisual(
        visual, parentId, overlayRoot, transform3D, plan, drawing, snapshot,
        skipped, visible, childContext, clipped);
    if (!described) return described.GetStatus();
    if (skipped) return {};
    static_cast<void>(clipped);

    snapshot.commandOffset = plan.commandHeaders_.Size();
    const std::uint32_t commandCount = snapshot.commandCount;
    if (commandCount != 0U && drawing != nullptr) {
        snapshot.payloadOffset =
            static_cast<std::uint32_t>(plan.commandPayload_.Size());
        snapshot.payloadSize =
            static_cast<std::uint32_t>(drawing->Payload().Size());
        for (const auto& h : drawing->Headers()) {
            RenderCommandHeader newHeader = h;
            newHeader.payloadOffset += snapshot.payloadOffset;
            plan.commandHeaders_.PushBack(newHeader);
        }
        plan.commandPayload_.Append(drawing->Payload());
        ResolveCommandGradients(plan, snapshot.commandOffset, commandCount);
    } else {
        snapshot.payloadOffset = 0U;
        snapshot.payloadSize = 0U;
    }
    plan.nodes_.PushBack(snapshot);

    if (!visible) return {};
    for (::Aero::Media::Visual* child :
         (visual).RenderChildren()) {
        if (child == nullptr || IsOverlay(*child) ||
            !IsEmittedChild(*child)) {
            continue;
        }
        Base::Result<void> childResult =
            BuildSubtree(
                *child,
                (visual).NodeId(),
                plan,
                false,
                childContext);
        if (!childResult) {
            return childResult;
        }
    }
    return {};
}

Base::Result<void> RenderTree::RefreshInPlace(
    bool& fallbackOut,
    std::uint32_t& committedNodesOut) noexcept {
    fallbackOut = true;
    committedNodesOut = 0U;
    // Caller guarantees: root present, frame built before, structure
    // generation matches, no overlays, viewport clean.
    const Size rootSize = [&]() noexcept {
        if (FrameworkElement* rootFramework =
                ::Aero::TryCast<::Aero::FrameworkElement>(root_)) {
            return rootFramework->GetRenderSize();
        }
        return logicalSize_;
    }();
    const Media::Transform3DContext rootContext =
        Media::MakeImplicitViewRootContext(rootSize);

    std::size_t validPayloadTotal = 0U;
    for (const auto& node : currentFrame_.nodes_) {
        validPayloadTotal += node.payloadSize;
    }
    if (currentFrame_.commandPayload_.Size() > (validPayloadTotal == 0U ? 0U : 2U * validPayloadTotal)) {
        fallbackOut = true;
        return {};
    }

    currentFrame_.effects_.Clear();
    currentFrame_.masks_.Clear();

    std::uint32_t cursor = 0U;
    bool fallback = false;
    Base::Status fatal;
    auto refresh = [&](auto&& self,
                       ::Aero::Media::Visual& visual,
                       RenderNodeId parentId,
                       const Media::Transform3DContext& transform3D) noexcept
        -> Base::Result<void> {
        if (fallback) return {};
        // Bounds and identity first: never bind a reference past the end.
        // Structural drift (missed attach/detach) falls back to the full
        // rebuild, which re-derives order from the live tree.
        if (cursor >= currentFrame_.nodes_.Size() ||
            currentFrame_.nodes_[cursor].id !=
                (visual).NodeId() ||
            currentFrame_.nodes_[cursor].parentId != parentId) {
            fallback = true;
            return {};
        }
        RenderNodeSnapshot& stored = currentFrame_.nodes_[cursor];
        // Layout writes sizes directly without render flags. A resized
        // visual needs its drawing re-recorded; force the Drawing bit so
        // DescribeVisual treats it exactly like a flagged visual (the full
        // path reaches the same state via the blanket invalidations).
        bool sizeChanged = false;
        if (UIElement* layoutElement =
                ::Aero::TryCast<::Aero::UIElement>(&visual)) {
            const Size nowSize = layoutElement->GetRenderSize();
            sizeChanged =
                nowSize.width != stored.renderSize.width ||
                nowSize.height != stored.renderSize.height;
            if (sizeChanged) {
                (visual).RenderDirtyFlags() |=
                    static_cast<std::uint8_t>(
                        RenderInvalidation::Drawing);
                (visual).RenderValid() = false;
            }
        }
        const bool drawingDirty = HasRenderInvalidation(
            static_cast<RenderInvalidation>(
                (visual).RenderDirtyFlags()),
            RenderInvalidation::Drawing);

        const DisplayList* drawing = nullptr;
        RenderNodeSnapshot snapshot;
        bool skipped = false;
        bool visible = false;
        Media::Transform3DContext childContext = transform3D;
        bool clipped = false;
        Base::Result<void> described = DescribeVisual(
            visual, parentId, false, transform3D, currentFrame_, drawing,
            snapshot, skipped, visible, childContext, clipped);
        if (!described) {
            fatal = described.GetStatus();
            return fatal;
        }
        if (skipped) return {};
        if (clipped) {
            // Geometry-clip ranges cannot be spliced in place.
            fallback = true;
            return {};
        }
        if (snapshot.commandCount != stored.commandCount) {
            // Command-range resize shifts every later offset: full rebuild.
            fallback = true;
            return {};
        }
        // Ranges are fixed: patch fields in place, refresh the command
        // bytes only when the drawing was actually re-recorded.
        snapshot.commandOffset = stored.commandOffset;
        snapshot.payloadOffset = stored.payloadOffset;
        snapshot.payloadSize = stored.payloadSize;
        if (snapshot.commandCount != 0U &&
            (drawingDirty || sizeChanged)) {
            const std::uint32_t newPayloadSize =
                drawing != nullptr ? static_cast<std::uint32_t>(drawing->Payload().Size()) : 0U;
            if (newPayloadSize != stored.payloadSize) {
                fallback = true;
                return {};
            }
            if (newPayloadSize != 0U) {
                std::memcpy(
                    currentFrame_.commandPayload_.Data() + stored.payloadOffset,
                    drawing->Payload().Data(),
                    newPayloadSize);
            }
            const auto fresh = drawing->Headers();
            for (std::uint32_t i = 0U; i < snapshot.commandCount; ++i) {
                RenderCommandHeader h = fresh[i];
                h.payloadOffset += stored.payloadOffset;
                currentFrame_.commandHeaders_[stored.commandOffset + i] = h;
            }
            ResolveCommandGradients(
                currentFrame_, stored.commandOffset,
                snapshot.commandCount);
        }
        stored = snapshot;
        ++cursor;

        if (!visible) return {};
        for (::Aero::Media::Visual* child :
             (visual).RenderChildren()) {
            if (child == nullptr || IsOverlay(*child) ||
                !IsEmittedChild(*child)) {
                continue;
            }
            Base::Result<void> childResult = self(
                self, *child, (visual).NodeId(),
                childContext);
            if (!childResult) return childResult.GetStatus();
            if (fallback) return {};
        }
        return {};
    };

    // Bounds-guard the first access: an empty frame cannot refresh.
    if (currentFrame_.nodes_.Empty()) {
        return {};
    }
    Base::Result<void> walked =
        refresh(refresh, *root_, InvalidRenderNodeId, rootContext);
    if (!walked) return walked.GetStatus();
    if (fallback || cursor != currentFrame_.nodes_.Size()) {
        return {};
    }
    currentFrame_.version_ = ++commitVersion_;
    MarkCommittedSubtree(*root_, true);
    dirty_.Clear();
    committedStructureVersion_ = structureVersion_;
    committedNodesOut = cursor;
    fallbackOut = false;
    return {};
}

Base::Result<std::uint32_t> RenderTree::RebuildFull() noexcept {
    stagedFrame_.Clear();
    stagedFrame_.version_ = commitVersion_ + 1U;
    stagedFrame_.logicalSize_ = logicalSize_;
    stagedFrame_.pixelWidth_ = pixelWidth_;
    stagedFrame_.pixelHeight_ = pixelHeight_;
    stagedFrame_.dpiScale_ = dpiScale_;
    const Size rootSize = [&]() noexcept {
        if (FrameworkElement* rootFramework =
                ::Aero::TryCast<::Aero::FrameworkElement>(root_)) {
            return rootFramework->GetRenderSize();
        }
        return logicalSize_;
    }();
    const Media::Transform3DContext rootContext =
        Media::MakeImplicitViewRootContext(rootSize);
    Base::Result<void> built = BuildSubtree(
        *root_, InvalidRenderNodeId, stagedFrame_, false, rootContext);
    if (!built) {
        return built.GetStatus();
    }
    for (const OverlayRecord& record :
         overlays_) {
        FrameworkElement* overlay =
            record.element;
        if (overlay == nullptr ||
            static_cast<::Aero::Media::Visual*>(overlay) == root_ ||
            !(*overlay).RenderAttached() ||
            overlay->GetVisibility() != Visibility::Visible ||
            !overlay->GetIsArrangeValid()) {
            continue;
        }
        const Size overlaySize = overlay->GetRenderSize();
        built = BuildSubtree(
            *overlay,
            (*root_).NodeId(),
            stagedFrame_,
            true,
            Media::MakeImplicitViewRootContext(overlaySize));
        if (!built) {
            return built.GetStatus();
        }
    }
    const std::uint32_t committedNodes = stagedFrame_.nodes_.Size();
    currentFrame_ = std::move(stagedFrame_);
    commitVersion_ = currentFrame_.Version();
    MarkCommittedSubtree(*root_, true);
    dirty_.Clear();
    viewportDirty_ = false;
    committedStructureVersion_ = structureVersion_;
    return committedNodes;
}

#ifndef NDEBUG
// P4.3: Debug-only agreement check. Independently rebuilds the frame from
// scratch (flags are still dirty, so drawings re-record fresh) and requires
// bit-identical content versus the in-place refresh. Gated by
// AERO_VERIFY_REFRESH (see Commit) so normal conformance pumps do not always
// double-rebuild. On disagreement it asserts and reports false so the caller
// heals via the full rebuild.
bool RenderTree::VerifyRefresh() noexcept {
    verifyFrame_.Clear();
    verifyFrame_.logicalSize_ = logicalSize_;
    verifyFrame_.pixelWidth_ = pixelWidth_;
    verifyFrame_.pixelHeight_ = pixelHeight_;
    verifyFrame_.dpiScale_ = dpiScale_;
    const Size rootSize = [&]() noexcept {
        if (FrameworkElement* rootFramework =
                ::Aero::TryCast<::Aero::FrameworkElement>(root_)) {
            return rootFramework->GetRenderSize();
        }
        return logicalSize_;
    }();
    const Media::Transform3DContext rootContext =
        Media::MakeImplicitViewRootContext(rootSize);
    // Overlays are empty whenever a refresh was attempted.
    Base::Result<void> built = BuildSubtree(
        *root_, InvalidRenderNodeId, verifyFrame_, false, rootContext);
    if (!built) return true;
    // Normalize the generation marker: StableHash covers the descriptor
    // version, which the refresh already advanced.
    verifyFrame_.version_ = currentFrame_.Version();
    const bool agree =
        verifyFrame_.nodes_.Size() == currentFrame_.nodes_.Size() &&
        verifyFrame_.commandHeaders_.Size() == currentFrame_.commandHeaders_.Size() &&
        verifyFrame_.commandPayload_.Size() == currentFrame_.commandPayload_.Size() &&
        verifyFrame_.gradientRamps_.Size() ==
            currentFrame_.gradientRamps_.Size() &&
        verifyFrame_.StableHash() == currentFrame_.StableHash();
    AERO_ASSERT(agree &&
        "P4.3: in-place refresh diverged from full rebuild");
    return agree;
}
#endif

Base::Result<std::uint32_t> RenderTree::Commit() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!initialized_) {
        return InvalidState(
            "RenderTree must be initialized before commit");
    }
    if (committing_) {
        return InvalidState("Nested render commit is not allowed");
    }
    if (root_ == nullptr) {
        for (const Aero::VisualLease& lease : dirty_) {
            ::Aero::Media::Visual* visual = lease.Resolve();
            if (visual != nullptr) (*visual).RenderQueued() = false;
        }
        dirty_.Clear();
        return 0U;
    }
    if (dirty_.Empty() && !viewportDirty_ &&
        currentFrame_.Version() != 0U) {
        return 0U;
    }

    // P4.3: viewport-only change with a stable structure and no dirty
    // visuals is descriptor-only: snapshots store element-relative state,
    // so no node needs to be revisited. A new version is still published
    // so hosts re-present at the new size/dpi.
    if (dirty_.Empty() && viewportDirty_ &&
        currentFrame_.Version() != 0U &&
        committedStructureVersion_ == structureVersion_) {
        committing_ = true;
        currentFrame_.version_ = ++commitVersion_;
        currentFrame_.logicalSize_ = logicalSize_;
        currentFrame_.pixelWidth_ = pixelWidth_;
        currentFrame_.pixelHeight_ = pixelHeight_;
        currentFrame_.dpiScale_ = dpiScale_;
        viewportDirty_ = false;
        committing_ = false;
        return currentFrame_.nodes_.Size();
    }

    committing_ = true;
    // P4.3: fast path. The live visual tree (+ cached drawings) is the
    // single fact source; when node order and command ranges are stable
    // the commit patches snapshot fields in place instead of materializing
    // a whole-tree mirror. Any doubt falls back to the full rebuild below.
    const bool canRefresh =
        currentFrame_.Version() != 0U &&
        committedStructureVersion_ == structureVersion_ &&
        overlays_.Empty() && !viewportDirty_;
    if (canRefresh) {
        bool fallback = true;
        std::uint32_t refreshedNodes = 0U;
        Base::Result<void> refreshed =
            RefreshInPlace(fallback, refreshedNodes);
        if (!refreshed) {
            committing_ = false;
            return refreshed.GetStatus();
        }
        if (!fallback) {
#ifndef NDEBUG
            // Opt-in / sampled: every Pump used to double-rebuild in Debug via
            // VerifyRefresh. Default off so conformance stays fast; set
            // AERO_VERIFY_REFRESH=1 (always) or =sample (every 16th success).
            static const int verifyMode = []() noexcept -> int {
                const char* env = std::getenv("AERO_VERIFY_REFRESH");
                if (env == nullptr || env[0] == '\0' || env[0] == '0') {
                    return 0;
                }
                if (env[0] == 's' || env[0] == 'S') {
                    return 2;
                }
                return 1;
            }();
            static std::uint32_t verifySampleCounter = 0U;
            bool runVerify = false;
            if (verifyMode == 1) {
                runVerify = true;
            } else if (verifyMode == 2) {
                runVerify = ((++verifySampleCounter) & 0xFU) == 0U;
            }
            if (runVerify && !VerifyRefresh()) {
                fallback = true;
            }
#endif
        }
        if (!fallback) {
            committing_ = false;
            return refreshedNodes;
        }
    }
    Base::Result<std::uint32_t> rebuilt = RebuildFull();
    committing_ = false;
    return rebuilt;
}

RenderDiagnostics RenderTree::Diagnostics() const noexcept {
    RenderDiagnostics diagnostics;
    diagnostics.commitVersion = commitVersion_;
    diagnostics.nodeCount = currentFrame_.Nodes().Size();
    diagnostics.commandCount = currentFrame_.CommandCount();
    for (const RenderCommandHeader& h : currentFrame_.CommandHeaders()) {
        if (h.kind == RenderCommandKind::DrawGlyphRun) {
            ++diagnostics.glyphCommandCount;
        }
    }
    diagnostics.dirtyCount = dirty_.Size();
    diagnostics.frameHash = currentFrame_.StableHash();
    return diagnostics;
}

void RenderTree::RenderCommitHook(void* context) noexcept {
    auto* manager = static_cast<RenderTree*>(context);
    Base::Result<std::uint32_t> committed =
        manager->Commit();
    manager->lastCommitStatus_ = committed
        ? Base::Status::Ok()
        : committed.GetStatus();
}

} // namespace Aero::Render

namespace Aero {

void FrameworkElement::SetResources(
    Base::Ref<ResourceDictionary> value) noexcept {
    (void)Aero::AssignResourceDictionary(
        EnsureOwnedResources(resources_),
        std::move(value),
        "FrameworkElement Resources is already assigned");
}

} // namespace Aero
