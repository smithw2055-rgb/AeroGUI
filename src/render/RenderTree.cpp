#include "DisplayList.hpp"
#include "RenderTree.hpp"
#include "gui/meta/MetadataState.hpp"
#include "gui/core/State.hpp"
#include "gui/core/State.hpp"
#include "gui/media/AnimationEngine.hpp"
#include "gui/styles/StyleState.hpp"
#include "gui/media/MediaState.hpp"
#include "gui/core/facets/RenderFacet.hpp"
#include "gui/core/facets/VisualFacet.hpp"

#include <Aero/Base/Assert.hpp>
#include <Aero/Controls/Image.hpp>
#include <Aero/Controls/Menu.hpp>
#include <Aero/Controls/Popup.hpp>
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Transforms.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

namespace Aero::Render {

using namespace Aero::Meta;
using namespace Aero::Threading;
namespace {

Base::Status InvalidArgument(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

Base::Status InvalidState(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidState, message);
}

Base::Status NotFound(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::NotFound, message);
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
    const Transform2D& value) noexcept {
    hash = HashScalar(hash, value.m11);
    hash = HashScalar(hash, value.m12);
    hash = HashScalar(hash, value.m21);
    hash = HashScalar(hash, value.m22);
    hash = HashScalar(hash, value.dx);
    return HashScalar(hash, value.dy);
}

std::uint64_t HashColor(std::uint64_t hash, const Color& value) noexcept {
    hash = HashScalar(hash, value.red);
    hash = HashScalar(hash, value.green);
    hash = HashScalar(hash, value.blue);
    return HashScalar(hash, value.alpha);
}

std::uint64_t HashCommand(
    std::uint64_t hash,
    const RenderCommand& command) noexcept {
    hash = HashScalar(hash, static_cast<std::uint8_t>(command.kind));
    hash = HashRect(hash, command.rect);
    hash = HashTransform(hash, command.transform);
    hash = HashColor(hash, command.color);
    hash = HashRect(hash, command.sourceUv);
    hash = HashScalar(hash, command.image);
    hash = HashScalar(hash, command.mesh);
    hash = HashScalar(hash, command.glyphRun);
    hash = HashScalar(hash, command.scalar);
    return HashScalar(hash, command.cornerRadius);
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

bool IsValidOpacity(double value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

std::uint64_t DisplayList::StableHash() const noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    hash = HashScalar(hash, commands_.Size());
    for (const RenderCommand& command : commands_) {
        hash = HashCommand(hash, command);
    }
    return hash;
}

Base::Result<void> DisplayListBuilder::Append(
    const RenderCommand& command) noexcept {
    if (finished_) {
        return InvalidState("DisplayListBuilder has already been finished");
    }
    return list_.commands_.PushBack(command);
}

Base::Result<void> DisplayListBuilder::PushClip(Rect clip) noexcept {
    if (!IsValidLayoutRect(clip)) {
        return InvalidArgument("Render clip must be finite and nonnegative");
    }
    RenderCommand command;
    command.kind = RenderCommandKind::PushClip;
    command.rect = clip;
    Base::Result<void> result = Append(command);
    if (result) {
        ++clipDepth_;
    }
    return result;
}

Base::Result<void> DisplayListBuilder::PopClip() noexcept {
    if (clipDepth_ == 0U) {
        return InvalidState("Render clip stack underflow");
    }
    RenderCommand command;
    command.kind = RenderCommandKind::PopClip;
    Base::Result<void> result = Append(command);
    if (result) {
        --clipDepth_;
    }
    return result;
}

Base::Result<void> DisplayListBuilder::PushOpacity(double opacity) noexcept {
    if (!IsValidOpacity(opacity)) {
        return InvalidArgument("Render opacity must be within [0, 1]");
    }
    RenderCommand command;
    command.kind = RenderCommandKind::PushOpacity;
    command.scalar = opacity;
    Base::Result<void> result = Append(command);
    if (result) {
        ++opacityDepth_;
    }
    return result;
}

Base::Result<void> DisplayListBuilder::PopOpacity() noexcept {
    if (opacityDepth_ == 0U) {
        return InvalidState("Render opacity stack underflow");
    }
    RenderCommand command;
    command.kind = RenderCommandKind::PopOpacity;
    Base::Result<void> result = Append(command);
    if (result) {
        --opacityDepth_;
    }
    return result;
}

Base::Result<void> DisplayListBuilder::PushTransform(
    Transform2D value) noexcept {
    if (!IsFinite(value)) {
        return InvalidArgument("Render transform must contain finite values");
    }
    RenderCommand command;
    command.kind = RenderCommandKind::PushTransform;
    command.transform = value;
    Base::Result<void> result = Append(command);
    if (result) {
        ++transformDepth_;
    }
    return result;
}

Base::Result<void> DisplayListBuilder::PopTransform() noexcept {
    if (transformDepth_ == 0U) {
        return InvalidState("Render transform stack underflow");
    }
    RenderCommand command;
    command.kind = RenderCommandKind::PopTransform;
    Base::Result<void> result = Append(command);
    if (result) {
        --transformDepth_;
    }
    return result;
}

Base::Result<void> DisplayListBuilder::FillRect(
    Rect rect,
    Color color) noexcept {
    if (!IsValidLayoutRect(rect) || !IsFinite(color)) {
        return InvalidArgument("FillRect requires valid geometry and color");
    }
    RenderCommand command;
    command.kind = RenderCommandKind::FillRect;
    command.rect = rect;
    command.color = color;
    return Append(command);
}

Base::Result<void> DisplayListBuilder::FillRoundedRect(
    Rect rect,
    Color color,
    double cornerRadius) noexcept {
    if (!IsValidLayoutRect(rect) || !IsFinite(color) ||
        !std::isfinite(cornerRadius) || cornerRadius < 0.0 ||
        cornerRadius * 2.0 > std::fmin(rect.width, rect.height)) {
        return InvalidArgument(
            "FillRoundedRect requires valid geometry, color, and corner radius");
    }
    RenderCommand command;
    command.kind = RenderCommandKind::FillRoundedRect;
    command.rect = rect;
    command.color = color;
    command.scalar = cornerRadius;
    return Append(command);
}

Base::Result<void> DisplayListBuilder::FillGradientQuad(
    const Point points[4],
    const Color colors[4]) noexcept {
    RenderCommand command;
    command.kind = RenderCommandKind::FillGradientQuad;
    for (int i = 0; i < 4; ++i) {
        command.points[i] = points[i];
        command.colors[i] = colors[i];
    }
    return Append(command);
}

Base::Result<void> DisplayListBuilder::StrokeRect(
    Rect rect,
    Color color,
    double thickness,
    double cornerRadius) noexcept {
    if (!IsValidLayoutRect(rect) || !IsFinite(color) ||
        !std::isfinite(thickness) || thickness < 0.0 ||
        !std::isfinite(cornerRadius) || cornerRadius < 0.0 ||
        cornerRadius * 2.0 > std::fmin(rect.width, rect.height)) {
        return InvalidArgument(
            "StrokeRect requires valid geometry, color, and thickness");
    }
    RenderCommand command;
    command.kind = RenderCommandKind::StrokeRect;
    command.rect = rect;
    command.color = color;
    command.scalar = thickness;
    command.cornerRadius = cornerRadius;
    return Append(command);
}

Base::Result<void> DisplayListBuilder::DrawImage(
    RenderImageId image,
    Rect destination,
    Rect sourceUv,
    Color tint) noexcept {
    if (image == InvalidRenderImageId || !IsValidLayoutRect(destination) ||
        !IsValidImageUv(sourceUv) || !IsFinite(tint)) {
        return InvalidArgument(
            "DrawImage requires a valid image, destination, UV rectangle, and tint");
    }
    RenderCommand command;
    command.kind = RenderCommandKind::DrawImage;
    command.rect = destination;
    command.sourceUv = sourceUv;
    command.color = tint;
    command.image = image;
    return Append(command);
}

Base::Result<void> DisplayListBuilder::DrawMesh(
    RenderMeshId mesh,
    Color tint) noexcept {
    if (mesh == InvalidRenderMeshId || !IsFinite(tint)) {
        return InvalidArgument("DrawMesh requires a valid mesh and tint");
    }
    RenderCommand command;
    command.kind = RenderCommandKind::DrawMesh;
    command.mesh = mesh;
    command.color = tint;
    return Append(command);
}

Base::Result<void> DisplayListBuilder::DrawGlyphRun(
    RenderGlyphRunId glyphRun,
    Color tint) noexcept {
    if (glyphRun == InvalidRenderGlyphRunId || !IsFinite(tint)) {
        return InvalidArgument("DrawGlyphRun requires a valid glyph run and tint");
    }
    RenderCommand command;
    command.kind = RenderCommandKind::DrawGlyphRun;
    command.glyphRun = glyphRun;
    command.color = tint;
    return Append(command);
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

} // namespace Aero::Render

namespace Aero {

using namespace Aero::Meta;
using namespace Aero::Threading;
using Media::Transform;
using Media::TransformBounds;
using Media::ComposeTransforms;
using Render::DisplayListBuilder;

FrameworkElement::FrameworkElement(TypeId runtimeType) noexcept
    : UIElement(runtimeType) {}

FrameworkElement::~FrameworkElement() = default;

Base::Ref<Transform>
FrameworkElement::GetLayoutTransform() const noexcept {
    Base::Result<Base::Ref<Transform>> value =
        GetValue(LayoutTransformProperty);
    return value
        ? std::move(value).Value()
        : Base::Ref<Transform>{};
}

void FrameworkElement::SetLayoutTransform(
    Base::Ref<Transform> value) noexcept {
    SetValue(
        LayoutTransformProperty,
        std::move(value));
}

Base::Transform2D
FrameworkElement::GetLocalVisualTransform() const noexcept {
    Base::Transform2D result;
    Size visualSize = GetRenderSize();
    Base::Ref<Transform> layoutTransform =
        GetLayoutTransform();
    if (layoutTransform) {
        result = layoutTransform->GetMatrix();
        const Rect bounds = TransformBounds(
            result,
            {0.0, 0.0,
             visualSize.width,
             visualSize.height});
        Base::Transform2D normalize;
        normalize.dx = -bounds.x;
        normalize.dy = -bounds.y;
        result = ComposeTransforms(
            result,
            normalize);
        visualSize = {
            bounds.width,
            bounds.height};
    }

    Base::Result<Base::Ref<Media::CompositeTransform3D>> transform3D =
        GetValue(Element::Transform3DProperty);
    if (transform3D && transform3D.Value()) {
        result = ComposeTransforms(
            result,
            transform3D.Value()->GetProjectedMatrix());
    }

    Base::Ref<Transform> renderTransform =
        GetRenderTransform();
    if (renderTransform) {
        const Point origin = GetRenderTransformOrigin();
        const double originX =
            origin.x * visualSize.width;
        const double originY =
            origin.y * visualSize.height;
        Base::Transform2D before;
        before.dx = -originX;
        before.dy = -originY;
        Base::Transform2D after;
        after.dx = originX;
        after.dy = originY;
        const Base::Transform2D render =
            ComposeTransforms(
                ComposeTransforms(
                    before,
                    renderTransform->GetMatrix()),
                after);
        result = ComposeTransforms(
            result,
            render);
    }
    if (hasViewboxTransform_) {
        result = ComposeTransforms(result, viewboxTransform_);
    }
    return result;
}

Base::Result<Value>
FrameworkElement::GetDataContextResult() const noexcept {
    return GetValue(DataContextProperty);
}

void FrameworkElement::SetDataContext(
    Value value) noexcept {
    SetValue(DataContextProperty, std::move(value));
}

void FrameworkElement::ClearDataContext() noexcept {
    ClearValue(DataContextProperty);
}

void FrameworkElement::OnPropertyInvalidated(
    PropertyInvalidationFlags flags) noexcept {
    UIElement::OnPropertyInvalidated(flags);
    if (HasFlag(flags, PropertyInvalidationFlags::Render)) {
        (void)InvalidateVisual();
    }
}

Base::Result<void> FrameworkElement::InvalidateVisual() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access;
    }
    return Aero::Core::RenderFacet::
        InvalidateRenderDrawing(*this);
}

void FrameworkElement::OnRender(
    ::Aero::Media::DrawingContext&) noexcept {
    return;
}

} // namespace Aero

namespace Aero::Render {

using namespace ::Aero::Render;
using Aero::FrameworkElement;

std::uint64_t RenderFrame::StableHash() const noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    hash = HashScalar(hash, version_);
    hash = HashSize(hash, logicalSize_);
    hash = HashScalar(hash, pixelWidth_);
    hash = HashScalar(hash, pixelHeight_);
    hash = HashScalar(hash, dpiScale_);
    hash = HashScalar(hash, nodes_.Size());
    hash = HashScalar(hash, commands_.Size());
    hash = HashScalar(hash, gradientRamps_.Size());
    for (const RenderNodeSnapshot& node : nodes_) {
        hash = HashScalar(hash, node.id);
        hash = HashScalar(hash, node.parentId);
        hash = HashRect(hash, node.layoutSlot);
        hash = HashRect(hash, node.clip);
        hash = HashScalar(hash, node.clipsToBounds);
        hash = HashSize(hash, node.renderSize);
        hash = HashTransform(hash, node.renderTransform);
        hash = HashScalar(
            hash,
            static_cast<std::uint8_t>(
                node.blendMode));
        hash = HashScalar(hash, node.opacity);
        hash = HashScalar(
            hash,
            static_cast<std::uint8_t>(
                node.mask.kind));
        hash = HashScalar(
            hash,
            static_cast<std::uint8_t>(
                node.effect.kind));
        hash = HashScalar(hash, node.commandOffset);
        hash = HashScalar(hash, node.commandCount);
        hash = HashScalar(hash, node.elementRevision);
    }
    for (const RenderCommand& command : commands_) {
        hash = HashCommand(hash, command);
    }
    for (const RenderGradientRampSnapshot& ramp : gradientRamps_) {
        // brushIdentity is intentionally omitted: it is a process-local cache
        // key and must not make a frame hash depend on an address.
        hash = HashScalar(hash, ramp.revision);
        for (const std::uint8_t value : ramp.pixels) {
            hash = HashByte(hash, value);
        }
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
    const Base::Span<const RenderCommand> commands = frame.Commands();
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
        if (!IsValidLayoutRect(node.layoutSlot) ||
            !IsValidLayoutRect(node.clip) ||
            !IsValidLayoutSize(node.renderSize) ||
            !Base::IsFiniteTransform(node.renderTransform) ||
            !IsValidOpacity(node.opacity) ||
            node.commandOffset > commands.Size() ||
            node.commandCount > commands.Size() - node.commandOffset) {
            return InvalidArgument("RenderFrame node snapshot is invalid");
        }
        if (static_cast<std::uint8_t>(node.mask.kind) >
                static_cast<std::uint8_t>(RenderMaskKind::RadialGradient) ||
            !IsFinite(node.mask.color)) {
            return InvalidArgument("RenderFrame node mask is invalid");
        }
        if (node.mask.kind == RenderMaskKind::Image) {
            const bool viewboxIsRelative =
                node.mask.viewboxUnits == static_cast<std::uint8_t>(
                    Media::BrushMappingMode::RelativeToBoundingBox);
            const bool validViewbox =
                IsValidLayoutRect(node.mask.sourceUv) &&
                node.mask.sourceUv.x >= 0.0 &&
                node.mask.sourceUv.y >= 0.0 &&
                (!viewboxIsRelative ||
                 (node.mask.sourceUv.x + node.mask.sourceUv.width <= 1.0 &&
                  node.mask.sourceUv.y + node.mask.sourceUv.height <= 1.0));
            if (node.mask.image == InvalidRenderImageId ||
                node.mask.imageWidth == 0U || node.mask.imageHeight == 0U ||
                !validViewbox || !IsValidLayoutRect(node.mask.viewport) ||
                !IsFinite(node.mask.relativeTransform) ||
                node.mask.viewboxUnits > static_cast<std::uint8_t>(
                    Media::BrushMappingMode::Absolute) ||
                node.mask.viewportUnits > static_cast<std::uint8_t>(
                    Media::BrushMappingMode::Absolute) ||
                node.mask.stretch > static_cast<std::uint8_t>(
                    Media::Stretch::UniformToFill) ||
                node.mask.tileMode > static_cast<std::uint8_t>(
                    Media::TileMode::FlipXY) ||
                node.mask.alignmentX > static_cast<std::uint8_t>(
                    HorizontalAlignment::Right) ||
                node.mask.alignmentY > static_cast<std::uint8_t>(
                    VerticalAlignment::Bottom)) {
                return InvalidArgument("RenderFrame image mask is invalid");
            }
        }
        if (node.mask.kind == RenderMaskKind::LinearGradient ||
            node.mask.kind == RenderMaskKind::RadialGradient) {
            if (node.mask.gradientRamp >= ramps.Size() ||
                node.mask.mappingMode > static_cast<std::uint8_t>(
                    Media::BrushMappingMode::Absolute) ||
                !Aero::IsFinite(node.mask.startPoint) ||
                !Aero::IsFinite(node.mask.endPoint) ||
                !Aero::IsFinite(node.mask.center) ||
                !Aero::IsFinite(node.mask.gradientOrigin) ||
                !IsFinite(node.mask.relativeTransform) ||
                !std::isfinite(node.mask.radiusX) ||
                !std::isfinite(node.mask.radiusY) ||
                (node.mask.kind == RenderMaskKind::RadialGradient &&
                 (node.mask.radiusX <= 0.0 || node.mask.radiusY <= 0.0))) {
                return InvalidArgument("RenderFrame gradient mask is invalid");
            }
        }
        if (static_cast<std::uint8_t>(node.effect.kind) >
                static_cast<std::uint8_t>(RenderEffectKind::DropShadow) ||
            node.effect.radius < 0.0 ||
            !std::isfinite(node.effect.radius) ||
            !std::isfinite(node.effect.direction) ||
            node.effect.depth < 0.0 ||
            !std::isfinite(node.effect.depth) ||
            !IsValidOpacity(node.effect.opacity) ||
            !IsFinite(node.effect.color)) {
            return InvalidArgument("RenderFrame node effect is invalid");
        }
    }

    for (const RenderCommand& command : commands) {
        switch (command.kind) {
        case RenderCommandKind::PushClip:
            if (!IsValidLayoutRect(command.rect)) {
                return InvalidArgument("RenderFrame contains an invalid clip");
            }
            ++clipDepth;
            break;
        case RenderCommandKind::PopClip:
            if (clipDepth == 0U) {
                return InvalidState("RenderFrame clip stack underflow");
            }
            --clipDepth;
            break;
        case RenderCommandKind::PushOpacity:
            if (!IsValidOpacity(command.scalar)) {
                return InvalidArgument("RenderFrame contains invalid opacity");
            }
            ++opacityDepth;
            break;
        case RenderCommandKind::PopOpacity:
            if (opacityDepth == 0U) {
                return InvalidState("RenderFrame opacity stack underflow");
            }
            --opacityDepth;
            break;
        case RenderCommandKind::PushTransform:
            if (!IsFinite(command.transform)) {
                return InvalidArgument("RenderFrame contains invalid transform");
            }
            ++transformDepth;
            break;
        case RenderCommandKind::PopTransform:
            if (transformDepth == 0U) {
                return InvalidState("RenderFrame transform stack underflow");
            }
            --transformDepth;
            break;
        case RenderCommandKind::FillRect:
            if (!IsValidLayoutRect(command.rect) || !IsFinite(command.color)) {
                return InvalidArgument("RenderFrame contains invalid FillRect");
            }
            break;
        case RenderCommandKind::FillRoundedRect:
            if (!IsValidLayoutRect(command.rect) || !IsFinite(command.color) ||
                !std::isfinite(command.scalar) || command.scalar < 0.0 ||
                command.scalar * 2.0 > std::fmin(command.rect.width, command.rect.height)) {
                return InvalidArgument("RenderFrame contains invalid FillRoundedRect");
            }
            break;
        case RenderCommandKind::StrokeRect:
            if (!IsValidLayoutRect(command.rect) || !IsFinite(command.color) ||
                !std::isfinite(command.scalar) || command.scalar < 0.0 ||
                !std::isfinite(command.cornerRadius) ||
                command.cornerRadius < 0.0 ||
                command.cornerRadius * 2.0 >
                    std::fmin(command.rect.width, command.rect.height)) {
                return InvalidArgument("RenderFrame contains invalid StrokeRect");
            }
            break;
        case RenderCommandKind::FillGradientQuad:
            for (int i = 0; i < 4; ++i) {
                if (!::Aero::IsFinite(command.points[i]) || !IsFinite(command.colors[i])) {
                    return InvalidArgument("RenderFrame contains invalid FillGradientQuad");
                }
            }
            break;
        case RenderCommandKind::DrawImage:
            if (command.image == InvalidRenderImageId ||
                !IsValidLayoutRect(command.rect) ||
                !IsValidImageUv(command.sourceUv) ||
                !IsFinite(command.color)) {
                return InvalidArgument("RenderFrame contains invalid DrawImage");
            }
            break;
        case RenderCommandKind::DrawMesh:
            if (command.mesh == InvalidRenderMeshId || !IsFinite(command.color)) {
                return InvalidArgument("RenderFrame contains invalid DrawMesh");
            }
            break;
        case RenderCommandKind::DrawGlyphRun:
            if (command.glyphRun == InvalidRenderGlyphRunId || !IsFinite(command.color)) {
                return InvalidArgument("RenderFrame contains invalid DrawGlyphRun");
            }
            break;
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
using namespace ::Aero::Meta;
using namespace ::Aero::Render;
using namespace ::Aero::Threading;

RenderTree::RenderTree(Dispatcher& dispatcher) noexcept
    : dispatcher_(&dispatcher), dirty_(), drawings_(), currentFrame_() {}

RenderTree::~RenderTree() noexcept {
    if (phaseHook_.IsValid() && dispatcher_->CheckAccess()) {
        (void)dispatcher_->RemoveFrameHook(phaseHook_);
    }
    if (root_ != nullptr && dispatcher_->CheckAccess()) {
        auto clear = [&](auto&& self, ::Aero::Media::Visual& visual) noexcept -> void {
            for (::Aero::Media::Visual* child :
                 Aero::Core::RenderFacet::
                     RenderChildren(visual)) {
                if (child == nullptr) continue;
                self(self, *child);
            }
            Aero::Core::RenderFacet::RenderAttached(visual) = false;
            Aero::Core::RenderFacet::RenderQueued(visual) = false;
            Aero::Core::RenderFacet::RenderValid(visual) = false;
            Aero::Core::RenderFacet::NodeId(visual) = InvalidRenderNodeId;
            RemoveDrawing(visual);
        };
        clear(clear, *root_);
        root_ = nullptr;
    }
}

Base::Result<void> RenderTree::Initialize() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (phaseHook_.IsValid()) {
        return {};
    }
    Base::Result<DispatcherFrameHookHandle> hook = dispatcher_->RegisterFrameHook(
        DispatcherFramePhase::RenderCommit,
        &RenderTree::RenderCommitHook,
        this);
    if (!hook) {
        return hook.GetStatus();
    }
    phaseHook_ = hook.Value();
    return {};
}

Base::Result<void> RenderTree::VerifyElement(
    const ::Aero::Media::Visual& element) const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (!phaseHook_.IsValid()) {
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
                     Aero::Core::RenderFacet::
                         RenderChildren(element)) {
                    if (child == nullptr) continue;
                    self(self, *child);
                }
                RemoveQueued(element);
                RemoveDrawing(element);
                Aero::Core::RenderFacet::RenderAttached(element) = false;
                Aero::Core::RenderFacet::RenderValid(element) = false;
                Aero::Core::RenderFacet::RenderDirtyFlags(element) =
                    static_cast<std::uint8_t>(
                        RenderInvalidation::All);
                Aero::Core::RenderFacet::NodeId(element) = InvalidRenderNodeId;
            };
            clear(clear, *root_);
        }
        root_ = nullptr;
        dirty_.Clear();
        drawings_.Clear();
        overlays_.Clear();
        return {};
    }

    Base::Result<void> verified = VerifyElement(*root);
    if (!verified) return verified.GetStatus();
    if (root_ == root) return {};
    if (root_ != nullptr || Aero::Core::RenderFacet::RenderRuntime(*root) != nullptr ||
        Aero::Core::RenderFacet::RenderAttached(*root) || root->GetVisualParent() != nullptr) {
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
    Base::Result<void> reserved =
        dirty_.Reserve(dirty_.Size() + 1U);
    if (!reserved) return reserved.GetStatus();

    root_ = root;
    Aero::Core::RenderFacet::NodeId(*root) = nextNodeId_++;
    Aero::Core::RenderFacet::RenderValid(*root) = false;
    Aero::Core::RenderFacet::RenderDirtyFlags(*root) =
        static_cast<std::uint8_t>(
            RenderInvalidation::All);
    Base::Result<void> queued =
        dirty_.PushBack(std::move(lease).Value());
    AERO_ASSERT(queued);
    (void)queued;
    Aero::Core::RenderFacet::RenderQueued(*root) = true;
    return {};
}

Base::Result<void> RenderTree::Attach(
    ::Aero::Media::Visual& parent,
    ::Aero::Media::Visual& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) return verified.GetStatus();
    verified = VerifyElement(child);
    if (!verified) return verified.GetStatus();
    if (Aero::Core::RenderFacet::RenderRuntime(parent) != this ||
        Aero::Core::RenderFacet::RenderRuntime(child) != nullptr || Aero::Core::RenderFacet::RenderAttached(child) ||
        Aero::Core::RenderFacet::RenderParent(child) != &parent) {
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
         current = Aero::Core::RenderFacet::RenderAttached(*current)
             ? Aero::Core::RenderFacet::RenderParent(*current) : nullptr) {
        if (!Aero::Core::RenderFacet::RenderQueued(*current)) ++required;
    }
    Base::Result<void> reserved =
        dirty_.Reserve(dirty_.Size() + required);
    if (!reserved) return reserved.GetStatus();

    Base::Result<void> invalidated = Invalidate(
        parent, RenderInvalidation::Children);
    if (!invalidated) return invalidated.GetStatus();

    Aero::Core::RenderFacet::RenderAttached(child) = true;
    Aero::Core::RenderFacet::NodeId(child) = nextNodeId_++;
    Aero::Core::RenderFacet::RenderValid(child) = false;
    Aero::Core::RenderFacet::RenderDirtyFlags(child) =
        static_cast<std::uint8_t>(
            RenderInvalidation::All);
    Base::Result<void> queued = dirty_.PushBack(
        std::move(childLease).Value());
    AERO_ASSERT(queued);
    (void)queued;
    Aero::Core::RenderFacet::RenderQueued(child) = true;
    return {};
}

Base::Result<void> RenderTree::Detach(
    ::Aero::Media::Visual& parent,
    ::Aero::Media::Visual& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) return verified.GetStatus();
    if (Aero::Core::RenderFacet::RenderRuntime(parent) != this || !Aero::Core::RenderFacet::RenderAttached(child) ||
        Aero::Core::RenderFacet::RenderParent(child) != &parent ||
        Aero::Core::RenderFacet::RenderRuntime(child) != this) {
        return NotFound(
            "Render parent-child relationship was not found");
    }

    Base::Result<void> invalidated = Invalidate(
        parent, RenderInvalidation::Children);
    if (!invalidated) return invalidated.GetStatus();

    auto clear = [&](auto&& self,
                     ::Aero::Media::Visual& element) noexcept -> void {
        for (::Aero::Media::Visual* descendant :
             Aero::Core::RenderFacet::
                 RenderChildren(element)) {
            if (descendant == nullptr) continue;
            self(self, *descendant);
        }
        RemoveQueued(element);
        RemoveDrawing(element);
        Aero::Core::RenderFacet::RenderAttached(element) = false;
        Aero::Core::RenderFacet::RenderValid(element) = false;
        Aero::Core::RenderFacet::RenderDirtyFlags(element) =
            static_cast<std::uint8_t>(
                RenderInvalidation::All);
        Aero::Core::RenderFacet::NodeId(element) = InvalidRenderNodeId;
    };
    clear(clear, child);
    return {};
}

Base::Result<void> RenderTree::QueueDirty(
    ::Aero::Media::Visual& element) noexcept {
    if (Aero::Core::RenderFacet::RenderQueued(element)) return {};
    Base::Result<Aero::VisualLease> lease =
        Aero::VisualLease::Acquire(element);
    if (!lease) return lease.GetStatus();
    Base::Result<void> appended =
        dirty_.PushBack(std::move(lease).Value());
    if (!appended) return appended.GetStatus();
    Aero::Core::RenderFacet::RenderQueued(element) = true;
    return {};
}

void RenderTree::RemoveQueued(::Aero::Media::Visual& element) noexcept {
    for (std::uint32_t index = 0U; index < dirty_.Size();) {
        if (dirty_[index].Resolve() != &element) {
            ++index;
            continue;
        }
        for (std::uint32_t next = index + 1U;
             next < dirty_.Size(); ++next) {
            dirty_[next - 1U] = std::move(dirty_[next]);
        }
        dirty_.PopBack();
    }
    Aero::Core::RenderFacet::RenderQueued(element) = false;
}

RenderTree::DrawingRecord*
RenderTree::FindDrawing(::Aero::Media::Visual& visual) noexcept {
    for (DrawingRecord& record : drawings_) {
        if (record.visual == &visual) {
            return &record;
        }
    }
    return nullptr;
}

void RenderTree::RemoveDrawing(::Aero::Media::Visual& visual) noexcept {
    for (std::uint32_t index = 0U;
         index < drawings_.Size(); ++index) {
        if (drawings_[index].visual != &visual) continue;
        for (std::uint32_t next = index + 1U;
             next < drawings_.Size(); ++next) {
            drawings_[next - 1U] =
                std::move(drawings_[next]);
        }
        drawings_.PopBack();
        return;
    }
}

void RenderTree::MarkCommittedSubtree(
    ::Aero::Media::Visual& visual,
    bool ancestorVisible) noexcept {
    UIElement* element = visual.AsUIElement();
    FrameworkElement* framework =
        visual.AsFrameworkElement();
    const bool visible =
        ancestorVisible &&
        (element == nullptr ||
         element->GetVisibility() ==
             Visibility::Visible);
    std::uint8_t processed =
        Aero::Core::RenderFacet::RenderDirtyFlags(visual);
    if (!visible && framework != nullptr) {
        processed &= static_cast<std::uint8_t>(
            ~static_cast<std::uint8_t>(
                RenderInvalidation::Drawing));
    }
    if (processed != 0U &&
        Aero::Core::RenderFacet::RenderRevision(visual) !=
            UINT64_MAX) {
        ++Aero::Core::RenderFacet::RenderRevision(visual);
    }
    Aero::Core::RenderFacet::RenderDirtyFlags(visual) &=
        static_cast<std::uint8_t>(~processed);
    Aero::Core::RenderFacet::RenderValid(visual) =
        Aero::Core::RenderFacet::RenderDirtyFlags(visual) == 0U;
    Aero::Core::RenderFacet::RenderQueued(visual) = false;
    for (::Aero::Media::Visual* child :
         Aero::Core::RenderFacet::RenderChildren(visual)) {
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
    if (Aero::Core::RenderFacet::RenderRuntime(element) != this) {
        return InvalidState(
            "Visual is not attached to this RenderTree");
    }

    Aero::Core::RenderFacet::RenderDirtyFlags(element) |=
        static_cast<std::uint8_t>(invalidation);
    Aero::Core::RenderFacet::RenderValid(element) = false;
    if (HasRenderInvalidation(
            invalidation,
            RenderInvalidation::Drawing) &&
        HasRenderInvalidation(
            invalidation,
            RenderInvalidation::Children)) {
        auto dirtySubtree = [&](auto&& self,
                                ::Aero::Media::Visual& visual) noexcept -> void {
            for (::Aero::Media::Visual* child :
                 Aero::Core::RenderFacet::RenderChildren(visual)) {
                if (child == nullptr) continue;
                Aero::Core::RenderFacet::RenderDirtyFlags(*child) |=
                    static_cast<std::uint8_t>(
                        RenderInvalidation::State |
                        RenderInvalidation::Drawing);
                Aero::Core::RenderFacet::RenderValid(*child) = false;
                self(self, *child);
            }
        };
        dirtySubtree(dirtySubtree, element);
    }

    Base::Vector<::Aero::Media::Visual*> path;
    for (::Aero::Media::Visual* current = &element; current != nullptr;
         current = Aero::Core::RenderFacet::RenderAttached(*current)
             ? Aero::Core::RenderFacet::RenderParent(*current) : nullptr) {
        Base::Result<void> currentVerified = VerifyElement(*current);
        if (!currentVerified) return currentVerified.GetStatus();
        Base::Result<void> appended = path.PushBack(current);
        if (!appended) return appended.GetStatus();
    }

    Base::Vector<Aero::VisualLease> leases;
    Base::Result<void> reserved = leases.Reserve(path.Size());
    if (!reserved) return reserved.GetStatus();
    for (::Aero::Media::Visual* current : path) {
        if (Aero::Core::RenderFacet::RenderQueued(*current)) continue;
        Base::Result<Aero::VisualLease> lease =
            Aero::VisualLease::Acquire(*current);
        if (!lease) return lease.GetStatus();
        Base::Result<void> staged =
            leases.PushBack(std::move(lease).Value());
        if (!staged) return staged.GetStatus();
    }
    reserved = dirty_.Reserve(dirty_.Size() + leases.Size());
    if (!reserved) return reserved.GetStatus();

    std::uint32_t leaseIndex = 0U;
    for (::Aero::Media::Visual* current : path) {
        if (Aero::Core::RenderFacet::RenderQueued(*current)) continue;
        Base::Result<void> queued = dirty_.PushBack(
            std::move(leases[leaseIndex++]));
        AERO_ASSERT(queued);
        (void)queued;
        Aero::Core::RenderFacet::RenderQueued(*current) = true;
    }
    return {};
}

} // namespace Aero::Render

namespace Aero {

namespace Core {

void* RenderFacet::RenderRuntime(
    const ::Aero::Media::Visual& visual) noexcept {
    return visual.tree_ != nullptr &&
        visual.renderNodeId_ != Base::InvalidRenderNodeId
        ? static_cast<void*>(visual.tree_->Renderer())
        : nullptr;
}

Base::Result<void>
RenderFacet::InvalidateRenderDrawing(
    ::Aero::Media::Visual& visual) noexcept {
    using Render::RenderInvalidation;
    using Render::RenderTree;
    if (RenderRuntime(visual) == nullptr) {
        RenderDirtyFlags(visual) |=
            static_cast<std::uint8_t>(
                RenderInvalidation::Drawing);
        RenderValid(visual) = false;
        return {};
    }
    return static_cast<RenderTree*>(
        RenderRuntime(visual))->Invalidate(
            visual,
            RenderInvalidation::Drawing);
}

Base::Result<void>
RenderFacet::InvalidateRenderState(
    ::Aero::Media::Visual& visual) noexcept {
    using Render::RenderInvalidation;
    using Render::RenderTree;
    if (RenderRuntime(visual) == nullptr) {
        RenderDirtyFlags(visual) |=
            static_cast<std::uint8_t>(
                RenderInvalidation::State);
        RenderValid(visual) = false;
        return {};
    }
    return static_cast<RenderTree*>(
        RenderRuntime(visual))->Invalidate(
            visual,
            RenderInvalidation::State);
}

Base::Result<void> RenderFacet::SetImageRuntimeData(
    Aero::Controls::Image& image,
    std::uint64_t renderImage,
    std::uint32_t pixelWidth,
    std::uint32_t pixelHeight) noexcept {
    const bool measureChanged = image.pixelWidth_ != pixelWidth || image.pixelHeight_ != pixelHeight;
    const bool renderChanged = image.renderImage_ != renderImage;
    image.renderImage_ = renderImage;
    image.pixelWidth_ = pixelWidth;
    image.pixelHeight_ = pixelHeight;
    if (measureChanged) return image.InvalidateMeasure();
    return renderChanged ? image.InvalidateVisual() : Base::Result<void>();
}

} // namespace Core



Base::Result<void> FrameworkElement::AddAuthoredTrigger(
    Base::Ref<Base::Object> trigger) noexcept {
    if (!trigger) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "FrameworkElement trigger cannot be null");
    }
    return authoredTriggers_.PushBack(std::move(trigger));
}

void
FrameworkElement::ClearAuthoredTriggers() noexcept {
    authoredTriggers_.Clear();
}
Base::Result<void> FrameworkElement::AddAuthoredBehavior(
    Base::Ref<Base::Object> behavior) noexcept {
    if (!behavior) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "FrameworkElement behavior cannot be null");
    }
    return authoredBehaviors_.PushBack(std::move(behavior));
}

void FrameworkElement::ClearAuthoredBehaviors() noexcept {
    authoredBehaviors_.Clear();
}
Base::Result<void> FrameworkElement::AddStyleBehaviorPrototype(
    Base::Ref<Base::Object> behavior) noexcept {
    if (!behavior) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "FrameworkElement style behavior cannot be null");
    }
    return styleBehaviorPrototypes_.PushBack(std::move(behavior));
}

void FrameworkElement::ClearStyleBehaviorPrototypes() noexcept {
    styleBehaviorPrototypes_.Clear();
}
Base::Result<void> FrameworkElement::AddStyleTriggerPrototype(
    Base::Ref<Base::Object> trigger) noexcept {
    if (!trigger) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "FrameworkElement style trigger cannot be null");
    }
    return styleTriggerPrototypes_.PushBack(std::move(trigger));
}

void FrameworkElement::ClearStyleTriggerPrototypes() noexcept {
    styleTriggerPrototypes_.Clear();
}

} // namespace Aero

namespace Aero::Render {

using namespace ::Aero;
using namespace ::Aero::Meta;
using namespace ::Aero::Render;
using namespace ::Aero::Threading;

bool RenderTree::IsOverlay(
    const ::Aero::Media::Visual& element) const noexcept {
    for (const OverlayRecord& overlay :
         overlays_) {
        if (overlay.element == &element) {
            return true;
        }
    }
    return false;
}

Base::Result<void> RenderTree::SetOverlays(
    Base::Span<FrameworkElement* const> overlays,
    Base::Span<const Point> origins) noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (committing_) {
        return InvalidState(
            "Render overlays cannot change during commit");
    }

    if (overlays.Size() != origins.Size()) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Render overlay elements and origins must have equal lengths");
    }
    Base::Vector<OverlayRecord> next;
    Base::Result<void> reserved =
        next.Reserve(overlays.Size());
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t index = 0U;
         index < overlays.Size();
         ++index) {
        FrameworkElement* overlay =
            overlays[index];
        if (overlay == nullptr ||
            Aero::Core::RenderFacet::RenderRuntime(*overlay) != this) {
            return InvalidState(
                "Render overlay must belong to this render tree");
        }
        if (!Aero::IsFinite(origins[index])) {
            return Base::Status::Failure(
                Base::ErrorCode::InvalidArgument,
                "Render overlay origin must be finite");
        }
        bool duplicate = false;
        for (const OverlayRecord& current :
             next) {
            duplicate =
                duplicate ||
                current.element == overlay;
        }
        if (duplicate) continue;
        Base::Result<void> appended =
            next.PushBack(
                {overlay, origins[index]});
        if (!appended) return appended.GetStatus();
    }

    bool changed = next.Size() != overlays_.Size();
    if (!changed) {
        for (std::uint32_t index = 0U;
             index < next.Size();
             ++index) {
            if (next[index].element !=
                    overlays_[index].element ||
                next[index].origin.x !=
                    overlays_[index].origin.x ||
                next[index].origin.y !=
                    overlays_[index].origin.y) {
                changed = true;
                break;
            }
        }
    }
    if (!changed) return {};
    overlays_ = std::move(next);
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
    }
    return snapshot;
}

std::uint32_t RenderTree::AppendGradientRamp(
    RenderFrame& plan,
    const ::Aero::Media::GradientBrush& brush) noexcept {
    RenderGradientRampSnapshot ramp;
    ramp.brushIdentity =
        reinterpret_cast<std::uintptr_t>(&brush);
    ramp.revision =
        brush.GetRevision();
    for (std::uint32_t i = 0U; i < GradientRampWidth; ++i) {
        const double position =
            static_cast<double>(i) /
            static_cast<double>(GradientRampWidth - 1U);
        const Color color = SampleGradient(brush, position);
        const float alpha = color.alpha;
        ramp.pixels[i * 4U + 0U] = static_cast<std::uint8_t>(
            std::clamp(color.red * alpha * 255.0F + 0.5F, 0.0F, 255.0F));
        ramp.pixels[i * 4U + 1U] = static_cast<std::uint8_t>(
            std::clamp(color.green * alpha * 255.0F + 0.5F, 0.0F, 255.0F));
        ramp.pixels[i * 4U + 2U] = static_cast<std::uint8_t>(
            std::clamp(color.blue * alpha * 255.0F + 0.5F, 0.0F, 255.0F));
        ramp.pixels[i * 4U + 3U] = static_cast<std::uint8_t>(
            std::clamp(alpha * 255.0F + 0.5F, 0.0F, 255.0F));
    }
    const Base::Span<const RenderGradientRampSnapshot> existing =
        plan.GradientRamps();
    for (std::uint32_t i = 0U; i < existing.Size(); ++i) {
        if (existing[i].brushIdentity == ramp.brushIdentity) return i;
    }
    static_cast<void>(plan.gradientRamps_.PushBack(ramp));
    return plan.gradientRamps_.Size() - 1U;
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
        snapshot.gradientRamp = AppendGradientRamp(plan, *gradient);
    }
    return snapshot;
}

Base::Result<void> RenderTree::BuildSubtree(
    ::Aero::Media::Visual& visual,
    RenderNodeId parentId,
    RenderFrame& plan,
    bool overlayRoot) noexcept {
    UIElement* element = visual.AsUIElement();
    FrameworkElement* framework =
        visual.AsFrameworkElement();
    const bool visible =
        element == nullptr ||
        element->GetVisibility() ==
            Visibility::Visible;
    if ((visible && element != nullptr &&
         !element->GetIsArrangeValid()) ||
        Aero::Core::RenderFacet::Rendering(visual)) {
        thread_local char message[256];
        const TypeInfo* type = element != nullptr
            ? element->PropertyRegistry().Types().FindType(
                  visual.RuntimeType())
            : nullptr;
        const Base::StringView typeName = type != nullptr
            ? type->Name()
            : Base::StringView("<unknown>");
        std::snprintf(
            message,
            sizeof(message),
            "Visual '%.*s' must be arranged and non-reentrant",
            static_cast<int>(typeName.SizeBytes()),
            typeName.Data());
        return InvalidState(message);
    }
    DrawingRecord* record = FindDrawing(visual);
    const bool drawingDirty =
        HasRenderInvalidation(
            static_cast<RenderInvalidation>(
                Aero::Core::RenderFacet::
                    RenderDirtyFlags(visual)),
            RenderInvalidation::Drawing);
    if (visible && framework != nullptr &&
        (record == nullptr || !record->valid ||
         drawingDirty)) {
        Aero::Core::RenderFacet::Rendering(visual) = true;
        DisplayListBuilder builder;
        ::Aero::Media::DrawingContext context =
            Aero::Render::DrawingPrivate::
                Create(builder);
        Aero::Core::RenderFacet::Render(visual, context);
        Base::Result<DisplayList> recorded =
            builder.Finish();
        Aero::Core::RenderFacet::Rendering(visual) = false;
        if (!recorded) {
            return recorded.GetStatus();
        }
        if (record == nullptr) {
            Base::Result<DrawingRecord*> added =
                drawings_.EmplaceBack();
            if (!added) return added.GetStatus();
            record = added.Value();
            record->visual = &visual;
        }
        record->drawing =
            std::move(recorded).Value();
        record->valid = true;
    }
    if (record == nullptr) {
        Base::Result<DrawingRecord*> added =
            drawings_.EmplaceBack();
        if (!added) return added.GetStatus();
        record = added.Value();
        record->visual = &visual;
        record->valid = framework == nullptr;
    }
    const DisplayList& list = record->drawing;
    const std::uint32_t commandCount =
        visible && record->valid
        ? list.CommandCount()
        : 0U;

    if (plan.commands_.Size() > UINT32_MAX - commandCount) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "RenderFrame command count exceeds 32-bit range");
    }
    if (Aero::Core::RenderFacet::RenderRevision(visual) == UINT64_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Render element revision space exhausted");
    }
    RenderNodeSnapshot snapshot;
    snapshot.id = Aero::Core::RenderFacet::NodeId(visual);
    snapshot.parentId = parentId;
    snapshot.layoutSlot = element != nullptr
        ? element->GetLayoutSlot()
        : Rect{};
    if (overlayRoot) {
        for (const OverlayRecord& overlay :
             overlays_) {
            if (overlay.element == framework) {
                snapshot.layoutSlot.x =
                    overlay.origin.x;
                snapshot.layoutSlot.y =
                    overlay.origin.y;
                break;
            }
        }
    }
    snapshot.clip = element != nullptr
        ? element->GetLayoutClip()
        : Rect{};
    snapshot.clipsToBounds =
        element != nullptr &&
        element->GetClipToBounds();
    snapshot.renderSize = element != nullptr
        ? element->GetRenderSize()
        : Size{};
    snapshot.renderTransform = framework != nullptr
        ? framework->GetLocalVisualTransform()
        : Transform2D{};
    snapshot.blendMode = element != nullptr
        ? element->GetBlendMode()
        : BlendMode::Normal;
    snapshot.opacity = element != nullptr
        ? element->GetOpacity()
        : 1.0;
    snapshot.effect = element != nullptr
        ? BuildEffectSnapshot(element->GetEffect().Get())
        : RenderEffectSnapshot{};
    snapshot.mask = element != nullptr
        ? BuildMaskSnapshot(*element, plan)
        : RenderMaskSnapshot{};
    snapshot.commandOffset = plan.commands_.Size();
    snapshot.commandCount = commandCount;
    snapshot.elementRevision =
        Aero::Core::RenderFacet::RenderRevision(visual) +
        (Aero::Core::RenderFacet::RenderDirtyFlags(visual) != 0U
         ? 1U : 0U);

    Base::Result<void> nodeAppend = plan.nodes_.PushBack(snapshot);
    if (!nodeAppend) {
        return nodeAppend;
    }
    Base::Result<void> commandAppend =
        commandCount != 0U
        ? plan.commands_.Append(list.Commands())
        : Base::Result<void>();
    if (!commandAppend) {
        plan.nodes_.PopBack();
        return commandAppend;
    }

    if (!visible) return {};
    for (::Aero::Media::Visual* child :
         Aero::Core::RenderFacet::
             RenderChildren(visual)) {
        if (child == nullptr) continue;
        const Meta::TypeId childType = child->RuntimeType();
        const Meta::TypeRegistry& childTypes =
            child->PropertyRegistry().Types();
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
                Controls::ContextMenu::StaticTypeId());
        if (IsOverlay(*child) || overlayHost) continue;
        Base::Result<void> childResult =
            BuildSubtree(
                *child,
                Aero::Core::RenderFacet::NodeId(visual),
                plan);
        if (!childResult) {
            return childResult;
        }
    }
    return {};
}

Base::Result<std::uint32_t> RenderTree::Commit() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) return access.GetStatus();
    if (!phaseHook_.IsValid()) {
        return InvalidState(
            "RenderTree must be initialized before commit");
    }
    if (committing_) {
        return InvalidState("Nested render commit is not allowed");
    }
    if (root_ == nullptr) {
        for (const Aero::VisualLease& lease : dirty_) {
            ::Aero::Media::Visual* visual = lease.Resolve();
            if (visual != nullptr) Aero::Core::RenderFacet::RenderQueued(*visual) = false;
        }
        dirty_.Clear();
        return 0U;
    }
    if (dirty_.Empty() && !viewportDirty_ &&
        currentFrame_.Version() != 0U) return 0U;

    committing_ = true;
    RenderFrame next;
    next.version_ = commitVersion_ + 1U;
    next.logicalSize_ = logicalSize_;
    next.pixelWidth_ = pixelWidth_;
    next.pixelHeight_ = pixelHeight_;
    next.dpiScale_ = dpiScale_;
    Base::Result<void> built = BuildSubtree(
        *root_, InvalidRenderNodeId, next);
    if (!built) {
        committing_ = false;
        return built.GetStatus();
    }
    for (const OverlayRecord& record :
         overlays_) {
        FrameworkElement* overlay =
            record.element;
        if (overlay == nullptr ||
            static_cast<::Aero::Media::Visual*>(overlay) == root_ ||
            !Aero::Core::RenderFacet::RenderAttached(*overlay) ||
            overlay->GetVisibility() != Visibility::Visible ||
            !overlay->GetIsArrangeValid()) {
            continue;
        }
        built = BuildSubtree(
            *overlay, Aero::Core::RenderFacet::NodeId(*root_), next, true);
        if (!built) {
            committing_ = false;
            return built.GetStatus();
        }
    }
    const std::uint32_t committedNodes = next.nodes_.Size();
    currentFrame_ = std::move(next);
    commitVersion_ = currentFrame_.Version();
    MarkCommittedSubtree(*root_, true);
    dirty_.Clear();
    viewportDirty_ = false;
    committing_ = false;
    return committedNodes;
}

RenderDiagnostics RenderTree::Diagnostics() const noexcept {
    RenderDiagnostics diagnostics;
    diagnostics.commitVersion = commitVersion_;
    diagnostics.nodeCount = currentFrame_.Nodes().Size();
    diagnostics.commandCount = currentFrame_.Commands().Size();
    for (const RenderCommand& command : currentFrame_.Commands()) {
        if (command.kind == RenderCommandKind::DrawGlyphRun) {
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
        resources_,
        std::move(value),
        "FrameworkElement Resources is already assigned");
}

} // namespace Aero
