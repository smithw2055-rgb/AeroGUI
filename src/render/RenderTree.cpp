#include "DisplayList.hpp"
#include "DrawingInternals.hpp"
#include "RenderTree.hpp"

#include "gui/StyleInternal.hpp"

#include <Aero/Base/Assert.hpp>
#include "gui/MetadataInternal.hpp"
#include <Aero/Media/Effects.hpp>
#include <Aero/Media/Transforms.hpp>

#include <cmath>
#include <cstring>
#include <utility>

namespace Aero::Render {

using namespace Aero::Core;
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
    return HashScalar(hash, command.scalar);
}

bool IsValidColorComponent(float value) noexcept {
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
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
    return list_.commands_.TryPushBack(command);
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
        !IsValidLayoutRect(sourceUv) || sourceUv.x < 0.0 || sourceUv.y < 0.0 ||
        sourceUv.x + sourceUv.width > 1.0 ||
        sourceUv.y + sourceUv.height > 1.0 || !IsFinite(tint)) {
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

using namespace Aero::Core;
using Media::Transform;
using Media::TransformOwnerRole;
using Media::TransformBounds;
using Media::ComposeTransforms;
using Render::DisplayListBuilder;

FrameworkElement::FrameworkElement(TypeId runtimeType) noexcept
    : UIElement(runtimeType) {}

FrameworkElement::~FrameworkElement() {
    AERO_ASSERT(renderRuntime_ == nullptr);
    AERO_ASSERT(!renderAttached_);
    Base::Ref<Transform> renderTransform =
        GetRenderTransform();
    if (renderTransform) {
        renderTransform->DetachOwner(
            this,
            TransformOwnerRole::Render);
    }
    Base::Ref<Transform> layoutTransform =
        GetLayoutTransform();
    if (layoutTransform) {
        layoutTransform->DetachOwner(
            this,
            TransformOwnerRole::Layout);
    }
}

Base::Ref<Transform>
FrameworkElement::GetLayoutTransform() const noexcept {
    Base::Result<Base::Ref<Transform>> value =
        GetValue(LayoutTransformProperty);
    return value
        ? std::move(value).Value()
        : Base::Ref<Transform>{};
}

Base::Result<void> FrameworkElement::SetLayoutTransform(
    Base::Ref<Transform> value) noexcept {
    return SetValue(
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
        result = layoutTransform->Matrix();
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
                    renderTransform->Matrix()),
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

Base::Result<void> FrameworkElement::SetDataContext(
    Base::Ref<Base::Object> value) noexcept {
    return SetValue(DataContextProperty, std::move(value));
}

Base::Result<void> FrameworkElement::ClearDataContext() noexcept {
    return ClearValue(DataContextProperty);
}

Base::Result<void> FrameworkElement::OnPropertyInvalidated(
    PropertyInvalidationFlags flags) noexcept {
    Base::Result<void> layout = UIElement::OnPropertyInvalidated(flags);
    if (!layout) return layout;
    if (HasFlag(flags, PropertyInvalidationFlags::Render)) {
        return InvalidateVisual();
    }
    return {};
}

Base::Result<void> FrameworkElement::InvalidateVisual() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access;
    }
    if (renderRuntime_ == nullptr) {
        renderValid_ = false;
        return {};
    }
    return static_cast<Render::RenderTree*>(renderRuntime_)->Invalidate(*this);
}

Base::Result<void> FrameworkElement::OnRender(
    DrawingContext&) noexcept {
    return {};
}

} // namespace Aero

namespace Aero::Render {

using Aero::FrameworkElement;
using Aero::Media::Effect;
using Aero::Media::BlurEffect;
using Aero::Media::DropShadowEffect;

std::uint64_t RenderFrame::StableHash() const noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    hash = HashScalar(hash, version_);
    hash = HashScalar(hash, nodes_.Size());
    hash = HashScalar(hash, commands_.Size());
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
    return hash;
}


Base::Result<void> ValidateRenderFrame(const RenderFrame& frame) noexcept {
    std::uint32_t clipDepth = 0U;
    std::uint32_t opacityDepth = 0U;
    std::uint32_t transformDepth = 0U;
    const Base::Span<const RenderCommand> commands = frame.Commands();
    const Base::Span<const RenderNodeSnapshot> nodes = frame.Nodes();

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
            node.commandOffset > commands.Size() ||
            node.commandCount > commands.Size() - node.commandOffset) {
            return InvalidArgument("RenderFrame node snapshot is invalid");
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
                !IsValidLayoutRect(command.sourceUv) ||
                command.sourceUv.x < 0.0 || command.sourceUv.y < 0.0 ||
                command.sourceUv.x + command.sourceUv.width > 1.0 ||
                command.sourceUv.y + command.sourceUv.height > 1.0 ||
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

RenderTree::RenderTree(Dispatcher& dispatcher) noexcept
    : dispatcher_(&dispatcher), dirty_(), currentFrame_() {}

RenderTree::~RenderTree() noexcept {
    if (phaseHook_.IsValid() && dispatcher_->CheckAccess()) {
        (void)dispatcher_->RemoveFrameHook(phaseHook_);
    }
    if (root_ != nullptr && dispatcher_->CheckAccess()) {
        auto clear = [&](auto&& self, FrameworkElement& element) noexcept -> void {
            for (FrameworkElement* child : Aero::Detail::ElementPrivate::RenderChildren(element)) {
                self(self, *child);
            }
            element.renderAttached_ = false;
            element.renderRuntime_ = nullptr;
            element.renderQueued_ = false;
            element.renderValid_ = false;
            element.nodeId_ = InvalidRenderNodeId;
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
    const FrameworkElement& element) const noexcept {
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
            "FrameworkElement belongs to another Dispatcher");
    }
    if (committing_) {
        return InvalidState("Render tree mutation during commit is not allowed");
    }
    return {};
}

Base::Result<void> RenderTree::SetRoot(
    FrameworkElement* root) noexcept {
    if (root == nullptr) {
        Base::Result<void> access = dispatcher_->VerifyAccess();
        if (!access) return access.GetStatus();
        if (committing_) {
            return InvalidState(
                "Render root cannot change during commit");
        }
        if (root_ != nullptr) {
            auto clear = [&](auto&& self,
                             FrameworkElement& element) noexcept -> void {
                for (FrameworkElement* child : Aero::Detail::ElementPrivate::RenderChildren(element)) {
                    self(self, *child);
                }
                RemoveQueued(element);
                element.renderAttached_ = false;
                element.renderRuntime_ = nullptr;
                element.renderValid_ = false;
                element.nodeId_ = InvalidRenderNodeId;
            };
            clear(clear, *root_);
        }
        root_ = nullptr;
        dirty_.Clear();
        overlays_.Clear();
        return {};
    }

    Base::Result<void> verified = VerifyElement(*root);
    if (!verified) return verified.GetStatus();
    if (root_ == root) return {};
    if (root_ != nullptr || root->renderRuntime_ != nullptr ||
        root->renderAttached_ || root->GetVisualParent() != nullptr) {
        return InvalidState("Render root must be detached and unique");
    }
    if (nextNodeId_ == InvalidRenderNodeId) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Render node ID space exhausted");
    }

    Base::Result<Aero::Detail::VisualLease> lease =
        Aero::Detail::VisualLease::Acquire(*root);
    if (!lease) return lease.GetStatus();
    Base::Result<void> reserved =
        dirty_.TryReserve(dirty_.Size() + 1U);
    if (!reserved) return reserved.GetStatus();

    root_ = root;
    root->renderRuntime_ = this;
    root->nodeId_ = nextNodeId_++;
    root->renderValid_ = false;
    Base::Result<void> queued =
        dirty_.TryPushBack(std::move(lease).Value());
    AERO_ASSERT(queued);
    (void)queued;
    root->renderQueued_ = true;
    return {};
}

Base::Result<void> RenderTree::Attach(
    FrameworkElement& parent,
    FrameworkElement& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) return verified.GetStatus();
    verified = VerifyElement(child);
    if (!verified) return verified.GetStatus();
    if (parent.renderRuntime_ != this ||
        child.renderRuntime_ != nullptr || child.renderAttached_ ||
        Aero::Detail::ElementPrivate::RenderParent(child) != &parent) {
        return InvalidState(
            "Render attachment must match the visual-tree parent");
    }
    if (nextNodeId_ == InvalidRenderNodeId) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Render node ID space exhausted");
    }

    Base::Result<Aero::Detail::VisualLease> childLease =
        Aero::Detail::VisualLease::Acquire(child);
    if (!childLease) return childLease.GetStatus();

    std::uint32_t required = 1U;
    for (FrameworkElement* current = &parent; current != nullptr;
         current = current->renderAttached_
             ? Aero::Detail::ElementPrivate::RenderParent(*current) : nullptr) {
        if (!current->renderQueued_) ++required;
    }
    Base::Result<void> reserved =
        dirty_.TryReserve(dirty_.Size() + required);
    if (!reserved) return reserved.GetStatus();

    Base::Result<void> invalidated = Invalidate(parent);
    if (!invalidated) return invalidated.GetStatus();

    child.renderAttached_ = true;
    child.renderRuntime_ = this;
    child.nodeId_ = nextNodeId_++;
    child.renderValid_ = false;
    Base::Result<void> queued = dirty_.TryPushBack(
        std::move(childLease).Value());
    AERO_ASSERT(queued);
    (void)queued;
    child.renderQueued_ = true;
    return {};
}

Base::Result<void> RenderTree::Detach(
    FrameworkElement& parent,
    FrameworkElement& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) return verified.GetStatus();
    if (parent.renderRuntime_ != this || !child.renderAttached_ ||
        Aero::Detail::ElementPrivate::RenderParent(child) != &parent ||
        child.renderRuntime_ != this) {
        return NotFound(
            "Render parent-child relationship was not found");
    }

    Base::Result<void> invalidated = Invalidate(parent);
    if (!invalidated) return invalidated.GetStatus();

    auto clear = [&](auto&& self,
                     FrameworkElement& element) noexcept -> void {
        for (FrameworkElement* descendant : Aero::Detail::ElementPrivate::RenderChildren(element)) {
            self(self, *descendant);
        }
        RemoveQueued(element);
        element.renderAttached_ = false;
        element.renderRuntime_ = nullptr;
        element.renderValid_ = false;
        element.nodeId_ = InvalidRenderNodeId;
    };
    clear(clear, child);
    return {};
}

Base::Result<void> RenderTree::QueueDirty(
    FrameworkElement& element) noexcept {
    if (element.renderQueued_) return {};
    Base::Result<Aero::Detail::VisualLease> lease =
        Aero::Detail::VisualLease::Acquire(element);
    if (!lease) return lease.GetStatus();
    Base::Result<void> appended =
        dirty_.TryPushBack(std::move(lease).Value());
    if (!appended) return appended.GetStatus();
    element.renderQueued_ = true;
    return {};
}

void RenderTree::RemoveQueued(FrameworkElement& element) noexcept {
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
    element.renderQueued_ = false;
}

void RenderTree::MarkCommittedSubtree(
    FrameworkElement& element) noexcept {
    ++element.renderRevision_;
    element.renderValid_ = true;
    element.renderQueued_ = false;
    for (FrameworkElement* child : Aero::Detail::ElementPrivate::RenderChildren(element)) {
        MarkCommittedSubtree(*child);
    }
}

Base::Result<void> RenderTree::Invalidate(
    FrameworkElement& element) noexcept {
    Base::Result<void> verified = VerifyElement(element);
    if (!verified) return verified.GetStatus();
    if (element.renderRuntime_ != this) {
        return InvalidState(
            "FrameworkElement is not attached to this RenderTree");
    }

    Base::Vector<FrameworkElement*> path;
    for (FrameworkElement* current = &element; current != nullptr;
         current = current->renderAttached_
             ? Aero::Detail::ElementPrivate::RenderParent(*current) : nullptr) {
        Base::Result<void> currentVerified = VerifyElement(*current);
        if (!currentVerified) return currentVerified.GetStatus();
        Base::Result<void> appended = path.TryPushBack(current);
        if (!appended) return appended.GetStatus();
    }

    Base::Vector<Aero::Detail::VisualLease> leases;
    Base::Result<void> reserved = leases.TryReserve(path.Size());
    if (!reserved) return reserved.GetStatus();
    for (FrameworkElement* current : path) {
        if (current->renderQueued_) continue;
        Base::Result<Aero::Detail::VisualLease> lease =
            Aero::Detail::VisualLease::Acquire(*current);
        if (!lease) return lease.GetStatus();
        Base::Result<void> staged =
            leases.TryPushBack(std::move(lease).Value());
        if (!staged) return staged.GetStatus();
    }
    reserved = dirty_.TryReserve(dirty_.Size() + leases.Size());
    if (!reserved) return reserved.GetStatus();

    std::uint32_t leaseIndex = 0U;
    for (FrameworkElement* current : path) {
        current->renderValid_ = false;
        if (current->renderQueued_) continue;
        Base::Result<void> queued = dirty_.TryPushBack(
            std::move(leases[leaseIndex++]));
        AERO_ASSERT(queued);
        (void)queued;
        current->renderQueued_ = true;
    }
    return {};
}

} // namespace Aero::Render

namespace Aero {

Base::Result<void> FrameworkElement::TryAddAuthoredTrigger(
    Base::Ref<Base::Object> trigger) noexcept {
    if (!trigger) {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "FrameworkElement trigger cannot be null");
    }
    return authoredTriggers_.TryPushBack(std::move(trigger));
}

Base::Result<void>
FrameworkElement::ClearAuthoredTriggers() noexcept {
    authoredTriggers_.Clear();
    return {};
}

} // namespace Aero

namespace Aero::Render {

bool RenderTree::IsOverlay(
    const FrameworkElement& element) const noexcept {
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
        next.TryReserve(overlays.Size());
    if (!reserved) return reserved.GetStatus();
    for (std::uint32_t index = 0U;
         index < overlays.Size();
         ++index) {
        FrameworkElement* overlay =
            overlays[index];
        if (overlay == nullptr ||
            overlay->renderRuntime_ != this) {
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
            next.TryPushBack(
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
        return Invalidate(*root_);
    }
    return {};
}

Base::Result<void> RenderTree::BuildSubtree(
    FrameworkElement& element,
    RenderNodeId parentId,
    RenderFrame& plan,
    bool overlayRoot) noexcept {
    const bool visible =
        element.GetVisibility() ==
        Visibility::Visible;
    if ((visible && !element.GetIsArrangeValid()) ||
        element.rendering_) {
        return InvalidState("FrameworkElement must be arranged and non-reentrant");
    }
    element.rendering_ = true;
    DisplayListBuilder builder;
    DrawingContext context =
        Aero::Detail::DrawingPrivate::Create(builder);
    Base::Result<void> built = visible
        ? element.OnRender(context)
        : Base::Result<void>();
    if (!built) {
        element.rendering_ = false;
        return built;
    }
    Base::Result<DisplayList> listResult = builder.Finish();
    element.rendering_ = false;
    if (!listResult) {
        return listResult.GetStatus();
    }
    DisplayList list = std::move(listResult).Value();

    if (plan.commands_.Size() > UINT32_MAX - list.CommandCount()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "RenderFrame command count exceeds 32-bit range");
    }
    if (element.renderRevision_ == UINT64_MAX) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Render element revision space exhausted");
    }
    RenderNodeSnapshot snapshot;
    snapshot.id = element.nodeId_;
    snapshot.parentId = parentId;
    snapshot.layoutSlot = element.GetLayoutSlot();
    if (overlayRoot) {
        for (const OverlayRecord& overlay :
             overlays_) {
            if (overlay.element == &element) {
                snapshot.layoutSlot.x =
                    overlay.origin.x;
                snapshot.layoutSlot.y =
                    overlay.origin.y;
                break;
            }
        }
    }
    snapshot.clip = element.GetLayoutClip();
    snapshot.clipsToBounds = element.GetClipToBounds();
    snapshot.renderSize = element.GetRenderSize();
    snapshot.renderTransform =
        element.GetLocalVisualTransform();
    snapshot.blendMode =
        element.GetBlendMode();
    Base::Ref<Effect> effect =
        element.GetEffect();
    if (effect) {
        if (effect->RuntimeType() ==
            BlurEffect::StaticTypeId()) {
            BlurEffect* blur =
                static_cast<BlurEffect*>(
                    effect.Get());
            snapshot.effect.kind =
                RenderEffectKind::Blur;
            snapshot.effect.radius =
                blur->Radius();
        } else if (effect->RuntimeType() ==
            DropShadowEffect::StaticTypeId()) {
            DropShadowEffect* shadow =
                static_cast<DropShadowEffect*>(
                    effect.Get());
            snapshot.effect.kind =
                RenderEffectKind::DropShadow;
            snapshot.effect.radius =
                shadow->BlurRadius();
            snapshot.effect.direction =
                shadow->Direction();
            snapshot.effect.depth =
                shadow->ShadowDepth();
            snapshot.effect.opacity =
                shadow->Opacity();
            snapshot.effect.color =
                shadow->Color();
        }
    }
    snapshot.commandOffset = plan.commands_.Size();
    snapshot.commandCount = list.CommandCount();
    snapshot.elementRevision = element.renderRevision_ + 1U;

    Base::Result<void> nodeAppend = plan.nodes_.TryPushBack(snapshot);
    if (!nodeAppend) {
        return nodeAppend;
    }
    Base::Result<void> commandAppend = plan.commands_.TryAppend(list.Commands());
    if (!commandAppend) {
        plan.nodes_.PopBack();
        return commandAppend;
    }

    if (!visible) return {};
    for (FrameworkElement* child : Aero::Detail::ElementPrivate::RenderChildren(element)) {
        if (IsOverlay(*child)) continue;
        Base::Result<void> childResult =
            BuildSubtree(
                *child, element.nodeId_, plan);
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
        for (const Aero::Detail::VisualLease& lease : dirty_) {
            Visual* visual = lease.Resolve();
            FrameworkElement* element = visual != nullptr
                ? visual->AsFrameworkElement() : nullptr;
            if (element != nullptr) element->renderQueued_ = false;
        }
        dirty_.Clear();
        return 0U;
    }
    if (dirty_.Empty() && currentFrame_.Version() != 0U) return 0U;

    committing_ = true;
    RenderFrame next;
    next.version_ = commitVersion_ + 1U;
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
            overlay == root_ ||
            !overlay->renderAttached_ ||
            overlay->GetVisibility() != Visibility::Visible ||
            !overlay->GetIsArrangeValid()) {
            continue;
        }
        built = BuildSubtree(
            *overlay, root_->nodeId_, next, true);
        if (!built) {
            committing_ = false;
            return built.GetStatus();
        }
    }
    const std::uint32_t committedNodes = next.nodes_.Size();
    currentFrame_ = std::move(next);
    commitVersion_ = currentFrame_.Version();
    MarkCommittedSubtree(*root_);
    dirty_.Clear();
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

Base::Result<void> FrameworkElement::SetResources(
    Base::Ref<ResourceDictionary> value) noexcept {
    return Aero::Detail::AssignResourceDictionary(
        resources_,
        std::move(value),
        "FrameworkElement Resources is already assigned");
}

} // namespace Aero
