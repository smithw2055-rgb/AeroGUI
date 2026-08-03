#include "DisplayList.hpp"
#include "render/RenderPrivate.hpp"
#include "RenderTree.hpp"
#include "../media/MediaPrivate.hpp"

#include "gui/GuiPrivate.hpp"

#include <Aero/Base/Assert.hpp>
#include "gui/GuiPrivate.hpp"
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Transforms.hpp>

#include <algorithm>
#include <cmath>
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

Base::Status Unsupported(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::Unsupported, message);
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
    return HashScalar(hash, command.scalar);
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

std::uint8_t ToUnorm8(float value) noexcept {
    if (!std::isfinite(value)) return 0U;
    return static_cast<std::uint8_t>(std::lround(
        std::clamp(value, 0.0F, 1.0F) * 255.0F));
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

Base::Result<void> DisplayListBuilder::StrokeRect(
    Rect rect,
    Color color,
    double thickness) noexcept {
    if (!IsValidLayoutRect(rect) || !IsFinite(color) ||
        !std::isfinite(thickness) || thickness < 0.0) {
        return InvalidArgument("StrokeRect requires valid geometry, color, and thickness");
    }
    RenderCommand command;
    command.kind = RenderCommandKind::StrokeRect;
    command.rect = rect;
    command.color = color;
    command.scalar = thickness;
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
    return result;
}

Base::Result<Base::Ref<Base::Object>>
FrameworkElement::GetDataContextResult() const noexcept {
    return GetValue(DataContextProperty);
}

void FrameworkElement::SetDataContext(
    Base::Ref<Base::Object> value) noexcept {
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
    return Aero::GuiPrivate::Detail::ElementPrivate::
        InvalidateRenderDrawing(*this);
}

void FrameworkElement::OnRender(
    DrawingContext&) noexcept {
    return;
}

} // namespace Aero

namespace Aero::Integration {

using namespace ::Aero::Render;
using Aero::FrameworkElement;
using Aero::Media::Effect;
using Aero::Media::BlurEffect;
using Aero::Media::DropShadowEffect;

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
        hash = HashColor(hash, node.mask.color);
        hash = HashScalar(hash, node.mask.image);
        hash = HashRect(hash, node.mask.sourceUv);
        hash = HashRect(hash, node.mask.viewport);
        hash = HashScalar(hash, node.mask.startPoint.x);
        hash = HashScalar(hash, node.mask.startPoint.y);
        hash = HashScalar(hash, node.mask.endPoint.x);
        hash = HashScalar(hash, node.mask.endPoint.y);
        hash = HashScalar(hash, node.mask.center.x);
        hash = HashScalar(hash, node.mask.center.y);
        hash = HashScalar(hash, node.mask.gradientOrigin.x);
        hash = HashScalar(hash, node.mask.gradientOrigin.y);
        hash = HashTransform(hash, node.mask.relativeTransform);
        hash = HashScalar(hash, node.mask.radiusX);
        hash = HashScalar(hash, node.mask.radiusY);
        hash = HashScalar(hash, node.mask.gradientRamp);
        hash = HashScalar(hash, node.mask.imageWidth);
        hash = HashScalar(hash, node.mask.imageHeight);
        hash = HashScalar(hash, node.mask.mappingMode);
        hash = HashScalar(hash, node.mask.viewboxUnits);
        hash = HashScalar(hash, node.mask.viewportUnits);
        hash = HashScalar(hash, node.mask.stretch);
        hash = HashScalar(hash, node.mask.tileMode);
        hash = HashScalar(hash, node.mask.alignmentX);
        hash = HashScalar(hash, node.mask.alignmentY);
        hash = HashScalar(
            hash,
            static_cast<std::uint8_t>(
                node.effect.kind));
        hash = HashScalar(hash, node.effect.radius);
        hash = HashScalar(hash, node.effect.direction);
        hash = HashScalar(hash, node.effect.depth);
        hash = HashScalar(hash, node.effect.opacity);
        hash = HashColor(hash, node.effect.color);
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
                !std::isfinite(command.scalar) || command.scalar < 0.0) {
                return InvalidArgument("RenderFrame contains invalid StrokeRect");
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

} // namespace Aero::Integration

namespace Aero::Render::Detail {

using namespace ::Aero;
using namespace ::Aero::Meta;
using namespace ::Aero::Integration;
using namespace ::Aero::Render;
using namespace ::Aero::Threading;

RenderTree::RenderTree(Dispatcher& dispatcher) noexcept
    : dispatcher_(&dispatcher), dirty_(), drawings_(), currentFrame_() {}

RenderTree::~RenderTree() noexcept {
    if (phaseHook_.IsValid() && dispatcher_->CheckAccess()) {
        (void)dispatcher_->RemoveFrameHook(phaseHook_);
    }
    if (root_ != nullptr && dispatcher_->CheckAccess()) {
        auto clear = [&](auto&& self, Visual& visual) noexcept -> void {
            for (Visual* child :
                 Aero::GuiPrivate::Detail::ElementPrivate::
                     RenderChildren(visual)) {
                if (child == nullptr) continue;
                self(self, *child);
            }
            ElementPrivate::RenderAttached(visual) = false;
            ElementPrivate::RenderRuntime(visual) = nullptr;
            ElementPrivate::RenderQueued(visual) = false;
            ElementPrivate::RenderValid(visual) = false;
            ElementPrivate::NodeId(visual) = InvalidRenderNodeId;
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
    const Visual& element) const noexcept {
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
    Visual* root) noexcept {
    if (root == nullptr) {
        Base::Result<void> access = dispatcher_->VerifyAccess();
        if (!access) return access.GetStatus();
        if (committing_) {
            return InvalidState(
                "Render root cannot change during commit");
        }
        if (root_ != nullptr) {
            auto clear = [&](auto&& self,
                             Visual& element) noexcept -> void {
                for (Visual* child :
                     Aero::GuiPrivate::Detail::ElementPrivate::
                         RenderChildren(element)) {
                    if (child == nullptr) continue;
                    self(self, *child);
                }
                RemoveQueued(element);
                RemoveDrawing(element);
                ElementPrivate::RenderAttached(element) = false;
                ElementPrivate::RenderRuntime(element) = nullptr;
                ElementPrivate::RenderValid(element) = false;
                ElementPrivate::RenderDirtyFlags(element) =
                    static_cast<std::uint8_t>(
                        RenderInvalidation::All);
                ElementPrivate::NodeId(element) = InvalidRenderNodeId;
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
    if (root_ != nullptr || ElementPrivate::RenderRuntime(*root) != nullptr ||
        ElementPrivate::RenderAttached(*root) || root->GetVisualParent() != nullptr) {
        return InvalidState("Render root must be detached and unique");
    }
    if (nextNodeId_ == InvalidRenderNodeId) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Render node ID space exhausted");
    }

    Base::Result<Aero::GuiPrivate::Detail::VisualLease> lease =
        Aero::GuiPrivate::Detail::VisualLease::Acquire(*root);
    if (!lease) return lease.GetStatus();
    Base::Result<void> reserved =
        dirty_.Reserve(dirty_.Size() + 1U);
    if (!reserved) return reserved.GetStatus();

    root_ = root;
    ElementPrivate::RenderRuntime(*root) = this;
    ElementPrivate::NodeId(*root) = nextNodeId_++;
    ElementPrivate::RenderValid(*root) = false;
    ElementPrivate::RenderDirtyFlags(*root) =
        static_cast<std::uint8_t>(
            RenderInvalidation::All);
    Base::Result<void> queued =
        dirty_.PushBack(std::move(lease).Value());
    AERO_ASSERT(queued);
    (void)queued;
    ElementPrivate::RenderQueued(*root) = true;
    return {};
}

Base::Result<void> RenderTree::Attach(
    Visual& parent,
    Visual& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) return verified.GetStatus();
    verified = VerifyElement(child);
    if (!verified) return verified.GetStatus();
    if (ElementPrivate::RenderRuntime(parent) != this ||
        ElementPrivate::RenderRuntime(child) != nullptr || ElementPrivate::RenderAttached(child) ||
        Aero::GuiPrivate::Detail::ElementPrivate::RenderParent(child) != &parent) {
        return InvalidState(
            "Render attachment must match the visual-tree parent");
    }
    if (nextNodeId_ == InvalidRenderNodeId) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Render node ID space exhausted");
    }

    Base::Result<Aero::GuiPrivate::Detail::VisualLease> childLease =
        Aero::GuiPrivate::Detail::VisualLease::Acquire(child);
    if (!childLease) return childLease.GetStatus();

    std::uint32_t required = 1U;
    for (Visual* current = &parent; current != nullptr;
         current = ElementPrivate::RenderAttached(*current)
             ? ElementPrivate::RenderParent(*current) : nullptr) {
        if (!ElementPrivate::RenderQueued(*current)) ++required;
    }
    Base::Result<void> reserved =
        dirty_.Reserve(dirty_.Size() + required);
    if (!reserved) return reserved.GetStatus();

    Base::Result<void> invalidated = Invalidate(
        parent, RenderInvalidation::Children);
    if (!invalidated) return invalidated.GetStatus();

    ElementPrivate::RenderAttached(child) = true;
    ElementPrivate::RenderRuntime(child) = this;
    ElementPrivate::NodeId(child) = nextNodeId_++;
    ElementPrivate::RenderValid(child) = false;
    ElementPrivate::RenderDirtyFlags(child) =
        static_cast<std::uint8_t>(
            RenderInvalidation::All);
    Base::Result<void> queued = dirty_.PushBack(
        std::move(childLease).Value());
    AERO_ASSERT(queued);
    (void)queued;
    ElementPrivate::RenderQueued(child) = true;
    return {};
}

Base::Result<void> RenderTree::Detach(
    Visual& parent,
    Visual& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) return verified.GetStatus();
    if (ElementPrivate::RenderRuntime(parent) != this || !ElementPrivate::RenderAttached(child) ||
        Aero::GuiPrivate::Detail::ElementPrivate::RenderParent(child) != &parent ||
        ElementPrivate::RenderRuntime(child) != this) {
        return NotFound(
            "Render parent-child relationship was not found");
    }

    Base::Result<void> invalidated = Invalidate(
        parent, RenderInvalidation::Children);
    if (!invalidated) return invalidated.GetStatus();

    auto clear = [&](auto&& self,
                     Visual& element) noexcept -> void {
        for (Visual* descendant :
             Aero::GuiPrivate::Detail::ElementPrivate::
                 RenderChildren(element)) {
            if (descendant == nullptr) continue;
            self(self, *descendant);
        }
        RemoveQueued(element);
        RemoveDrawing(element);
        ElementPrivate::RenderAttached(element) = false;
        ElementPrivate::RenderRuntime(element) = nullptr;
        ElementPrivate::RenderValid(element) = false;
        ElementPrivate::RenderDirtyFlags(element) =
            static_cast<std::uint8_t>(
                RenderInvalidation::All);
        ElementPrivate::NodeId(element) = InvalidRenderNodeId;
    };
    clear(clear, child);
    return {};
}

Base::Result<void> RenderTree::QueueDirty(
    Visual& element) noexcept {
    if (ElementPrivate::RenderQueued(element)) return {};
    Base::Result<Aero::GuiPrivate::Detail::VisualLease> lease =
        Aero::GuiPrivate::Detail::VisualLease::Acquire(element);
    if (!lease) return lease.GetStatus();
    Base::Result<void> appended =
        dirty_.PushBack(std::move(lease).Value());
    if (!appended) return appended.GetStatus();
    ElementPrivate::RenderQueued(element) = true;
    return {};
}

void RenderTree::RemoveQueued(Visual& element) noexcept {
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
    ElementPrivate::RenderQueued(element) = false;
}

RenderTree::DrawingRecord*
RenderTree::FindDrawing(Visual& visual) noexcept {
    for (DrawingRecord& record : drawings_) {
        if (record.visual == &visual) {
            return &record;
        }
    }
    return nullptr;
}

void RenderTree::RemoveDrawing(Visual& visual) noexcept {
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
    Visual& visual,
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
        ElementPrivate::RenderDirtyFlags(visual);
    if (!visible && framework != nullptr) {
        processed &= static_cast<std::uint8_t>(
            ~static_cast<std::uint8_t>(
                RenderInvalidation::Drawing));
    }
    if (processed != 0U &&
        ElementPrivate::RenderRevision(visual) !=
            UINT64_MAX) {
        ++ElementPrivate::RenderRevision(visual);
    }
    ElementPrivate::RenderDirtyFlags(visual) &=
        static_cast<std::uint8_t>(~processed);
    ElementPrivate::RenderValid(visual) =
        ElementPrivate::RenderDirtyFlags(visual) == 0U;
    ElementPrivate::RenderQueued(visual) = false;
    for (Visual* child :
         ElementPrivate::RenderChildren(visual)) {
        if (child != nullptr) {
            MarkCommittedSubtree(*child, visible);
        }
    }
}

Base::Result<void> RenderTree::Invalidate(
    Visual& element,
    RenderInvalidation invalidation) noexcept {
    Base::Result<void> verified = VerifyElement(element);
    if (!verified) return verified.GetStatus();
    if (ElementPrivate::RenderRuntime(element) != this) {
        return InvalidState(
            "Visual is not attached to this RenderTree");
    }

    ElementPrivate::RenderDirtyFlags(element) |=
        static_cast<std::uint8_t>(invalidation);
    ElementPrivate::RenderValid(element) = false;
    if (HasRenderInvalidation(
            invalidation,
            RenderInvalidation::Drawing) &&
        HasRenderInvalidation(
            invalidation,
            RenderInvalidation::Children)) {
        auto dirtySubtree = [&](auto&& self,
                                Visual& visual) noexcept -> void {
            for (Visual* child :
                 ElementPrivate::RenderChildren(visual)) {
                if (child == nullptr) continue;
                ElementPrivate::RenderDirtyFlags(*child) |=
                    static_cast<std::uint8_t>(
                        RenderInvalidation::State |
                        RenderInvalidation::Drawing);
                ElementPrivate::RenderValid(*child) = false;
                self(self, *child);
            }
        };
        dirtySubtree(dirtySubtree, element);
    }

    Base::Vector<Visual*> path;
    for (Visual* current = &element; current != nullptr;
         current = ElementPrivate::RenderAttached(*current)
             ? ElementPrivate::RenderParent(*current) : nullptr) {
        Base::Result<void> currentVerified = VerifyElement(*current);
        if (!currentVerified) return currentVerified.GetStatus();
        Base::Result<void> appended = path.PushBack(current);
        if (!appended) return appended.GetStatus();
    }

    Base::Vector<Aero::GuiPrivate::Detail::VisualLease> leases;
    Base::Result<void> reserved = leases.Reserve(path.Size());
    if (!reserved) return reserved.GetStatus();
    for (Visual* current : path) {
        if (ElementPrivate::RenderQueued(*current)) continue;
        Base::Result<Aero::GuiPrivate::Detail::VisualLease> lease =
            Aero::GuiPrivate::Detail::VisualLease::Acquire(*current);
        if (!lease) return lease.GetStatus();
        Base::Result<void> staged =
            leases.PushBack(std::move(lease).Value());
        if (!staged) return staged.GetStatus();
    }
    reserved = dirty_.Reserve(dirty_.Size() + leases.Size());
    if (!reserved) return reserved.GetStatus();

    std::uint32_t leaseIndex = 0U;
    for (Visual* current : path) {
        if (ElementPrivate::RenderQueued(*current)) continue;
        Base::Result<void> queued = dirty_.PushBack(
            std::move(leases[leaseIndex++]));
        AERO_ASSERT(queued);
        (void)queued;
        ElementPrivate::RenderQueued(*current) = true;
    }
    return {};
}

} // namespace Aero::Render::Detail

namespace Aero {

Base::Result<void>
Visual::Impl::InvalidateRenderDrawing(
    Visual& visual) noexcept {
    using Render::RenderInvalidation;
    using Render::Detail::RenderTree;
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
Visual::Impl::InvalidateRenderState(
    Visual& visual) noexcept {
    using Render::RenderInvalidation;
    using Render::Detail::RenderTree;
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

} // namespace Aero

namespace Aero::Render::Detail {

using namespace ::Aero;
using namespace ::Aero::Meta;
using namespace ::Aero::Integration;
using namespace ::Aero::Render;
using namespace ::Aero::Threading;

bool RenderTree::IsOverlay(
    const Visual& element) const noexcept {
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
            ElementPrivate::RenderRuntime(*overlay) != this) {
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

Base::Result<void> RenderTree::BuildSubtree(
    Visual& visual,
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
        ElementPrivate::Rendering(visual)) {
        return InvalidState(
            "Visual must be arranged and non-reentrant");
    }
    DrawingRecord* record = FindDrawing(visual);
    const bool drawingDirty =
        HasRenderInvalidation(
            static_cast<RenderInvalidation>(
                ElementPrivate::
                    RenderDirtyFlags(visual)),
            RenderInvalidation::Drawing);
    if (visible && framework != nullptr &&
        (record == nullptr || !record->valid ||
         drawingDirty)) {
        ElementPrivate::Rendering(visual) = true;
        DisplayListBuilder builder;
        DrawingContext context =
            Aero::Render::Detail::DrawingPrivate::
                Create(builder);
        ElementPrivate::Render(visual, context);
        Base::Result<DisplayList> recorded =
            builder.Finish();
        ElementPrivate::Rendering(visual) = false;
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
    if (ElementPrivate::RenderRevision(visual) == UINT64_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Render element revision space exhausted");
    }
    RenderNodeSnapshot snapshot;
    snapshot.id = ElementPrivate::NodeId(visual);
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
    Base::Ref<Media::Brush> opacityMask =
        element != nullptr
        ? element->GetOpacityMask()
        : Base::Ref<Media::Brush>{};
    if (opacityMask) {
        const Meta::TypeId maskType = opacityMask->RuntimeType();
        if (maskType == Media::SolidColorBrush::StaticTypeId()) {
            const Base::Color sampled =
                Media::Detail::SampleBrush(opacityMask, 0.5);
            snapshot.mask.kind = RenderMaskKind::Solid;
            snapshot.mask.color =
                {1.0F, 1.0F, 1.0F, sampled.alpha};
        } else if (maskType == Media::LinearGradientBrush::StaticTypeId() ||
                   maskType == Media::RadialGradientBrush::StaticTypeId()) {
            auto& gradient =
                static_cast<Media::GradientBrush&>(*opacityMask);
            snapshot.mask.kind = maskType ==
                    Media::LinearGradientBrush::StaticTypeId()
                ? RenderMaskKind::LinearGradient
                : RenderMaskKind::RadialGradient;
            snapshot.mask.mappingMode = static_cast<std::uint8_t>(
                gradient.GetMappingMode());
            if (const Base::Ref<Media::Transform> relative =
                    gradient.GetRelativeTransform()) {
                snapshot.mask.relativeTransform = relative->GetMatrix();
            }
            if (snapshot.mask.kind == RenderMaskKind::LinearGradient) {
                auto& linear =
                    static_cast<Media::LinearGradientBrush&>(gradient);
                snapshot.mask.startPoint = linear.GetStartPoint();
                snapshot.mask.endPoint = linear.GetEndPoint();
            } else {
                auto& radial =
                    static_cast<Media::RadialGradientBrush&>(gradient);
                snapshot.mask.center = radial.GetCenter();
                snapshot.mask.gradientOrigin = radial.GetGradientOrigin();
                snapshot.mask.radiusX = radial.GetRadiusX();
                snapshot.mask.radiusY = radial.GetRadiusY();
            }

            const std::uintptr_t identity =
                reinterpret_cast<std::uintptr_t>(opacityMask.Get());
            const std::uint64_t revision =
                Media::Detail::BrushPrivate::Revision(gradient);
            std::uint32_t rampIndex = UINT32_MAX;
            for (std::uint32_t index = 0U;
                 index < plan.gradientRamps_.Size(); ++index) {
                const RenderGradientRampSnapshot& candidate =
                    plan.gradientRamps_[index];
                if (candidate.brushIdentity == identity &&
                    candidate.revision == revision) {
                    rampIndex = index;
                    break;
                }
            }
            if (rampIndex == UINT32_MAX) {
                RenderGradientRampSnapshot ramp;
                ramp.brushIdentity = identity;
                ramp.revision = revision;
                for (std::uint32_t index = 0U;
                     index < GradientRampWidth; ++index) {
                    const double position = GradientRampWidth > 1U
                        ? static_cast<double>(index) /
                            static_cast<double>(GradientRampWidth - 1U)
                        : 0.0;
                    const Base::Color sampled =
                        Media::Detail::SampleGradient(gradient, position);
                    const std::uint32_t pixel = index * 4U;
                    ramp.pixels[pixel] = ToUnorm8(sampled.red);
                    ramp.pixels[pixel + 1U] = ToUnorm8(sampled.green);
                    ramp.pixels[pixel + 2U] = ToUnorm8(sampled.blue);
                    ramp.pixels[pixel + 3U] = ToUnorm8(sampled.alpha);
                }
                Base::Result<void> appended =
                    plan.gradientRamps_.PushBack(std::move(ramp));
                if (!appended) return appended.GetStatus();
                rampIndex = plan.gradientRamps_.Size() - 1U;
            }
            snapshot.mask.gradientRamp = rampIndex;
        } else if (maskType == Media::ImageBrush::StaticTypeId()) {
            auto& imageMask =
                static_cast<Media::ImageBrush&>(*opacityMask);
            if (!imageMask.GetSource()) {
                snapshot.mask.kind = RenderMaskKind::Solid;
                snapshot.mask.color =
                    {1.0F, 1.0F, 1.0F, 0.0F};
            } else {
                const RenderImageId image =
                    Media::Detail::BrushPrivate::RuntimeImage(imageMask);
                if (image == InvalidRenderImageId) {
                    return InvalidState(
                        "OpacityMask ImageBrush has no synchronized render image");
                }
                snapshot.mask.kind = RenderMaskKind::Image;
                snapshot.mask.image = image;
                snapshot.mask.sourceUv = imageMask.GetViewbox();
                snapshot.mask.viewport = imageMask.GetViewport();
                snapshot.mask.imageWidth =
                    Media::Detail::BrushPrivate::PixelWidth(imageMask);
                snapshot.mask.imageHeight =
                    Media::Detail::BrushPrivate::PixelHeight(imageMask);
                snapshot.mask.viewboxUnits = static_cast<std::uint8_t>(
                    imageMask.GetViewboxUnits());
                snapshot.mask.viewportUnits = static_cast<std::uint8_t>(
                    imageMask.GetViewportUnits());
                snapshot.mask.stretch = static_cast<std::uint8_t>(
                    imageMask.GetStretch());
                snapshot.mask.tileMode = static_cast<std::uint8_t>(
                    imageMask.GetTileMode());
                snapshot.mask.alignmentX = static_cast<std::uint8_t>(
                    imageMask.GetAlignmentX());
                snapshot.mask.alignmentY = static_cast<std::uint8_t>(
                    imageMask.GetAlignmentY());
                if (const Base::Ref<Media::Transform> relative =
                        imageMask.GetRelativeTransform()) {
                    snapshot.mask.relativeTransform = relative->GetMatrix();
                }
                snapshot.mask.color =
                    {1.0F, 1.0F, 1.0F,
                     static_cast<float>(imageMask.GetOpacity())};
            }
        } else {
            return Unsupported(
                "OpacityMask currently supports solid, gradient, and image brushes");
        }
    }
    Base::Ref<Effect> effect =
        element != nullptr
        ? element->GetEffect()
        : Base::Ref<Effect>{};
    if (effect) {
        if (effect->RuntimeType() ==
            BlurEffect::StaticTypeId()) {
            BlurEffect* blur =
                static_cast<BlurEffect*>(
                    effect.Get());
            snapshot.effect.kind =
                RenderEffectKind::Blur;
            snapshot.effect.radius =
                blur->GetRadius();
        } else if (effect->RuntimeType() ==
            DropShadowEffect::StaticTypeId()) {
            DropShadowEffect* shadow =
                static_cast<DropShadowEffect*>(
                    effect.Get());
            snapshot.effect.kind =
                RenderEffectKind::DropShadow;
            snapshot.effect.radius =
                shadow->GetBlurRadius();
            snapshot.effect.direction =
                shadow->GetDirection();
            snapshot.effect.depth =
                shadow->GetShadowDepth();
            snapshot.effect.opacity =
                shadow->GetOpacity();
            snapshot.effect.color =
                shadow->GetColor();
        }
    }
    snapshot.commandOffset = plan.commands_.Size();
    snapshot.commandCount = commandCount;
    snapshot.elementRevision =
        ElementPrivate::RenderRevision(visual) +
        (ElementPrivate::RenderDirtyFlags(visual) != 0U
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
    for (Visual* child :
         Aero::GuiPrivate::Detail::ElementPrivate::
             RenderChildren(visual)) {
        if (child == nullptr) continue;
        if (IsOverlay(*child)) continue;
        Base::Result<void> childResult =
            BuildSubtree(
                *child,
                ElementPrivate::NodeId(visual),
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
        for (const Aero::GuiPrivate::Detail::VisualLease& lease : dirty_) {
            Visual* visual = lease.Resolve();
            if (visual != nullptr) ElementPrivate::RenderQueued(*visual) = false;
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
            static_cast<Visual*>(overlay) == root_ ||
            !ElementPrivate::RenderAttached(*overlay) ||
            overlay->GetVisibility() != Visibility::Visible ||
            !overlay->GetIsArrangeValid()) {
            continue;
        }
        built = BuildSubtree(
            *overlay, ElementPrivate::NodeId(*root_), next, true);
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

} // namespace Aero::Render::Detail

namespace Aero {

void FrameworkElement::SetResources(
    Base::Ref<ResourceDictionary> value) noexcept {
    (void)Aero::GuiPrivate::Detail::AssignResourceDictionary(
        resources_,
        std::move(value),
        "FrameworkElement Resources is already assigned");
}

} // namespace Aero
