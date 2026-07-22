#include <Aero/Core/Rendering.hpp>

#include <Aero/Base/Assert.hpp>

#include <cmath>
#include <cstring>
#include <utility>

namespace Aero::Core {
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

RenderElement::RenderElement(
    Dispatcher& dispatcher,
    DependencyPropertyRegistry& registry,
    TypeId runtimeType,
    Base::IAllocator* allocator) noexcept
    : LayoutElement(dispatcher, registry, runtimeType, allocator),
      renderChildren_(allocator) {}

RenderElement::~RenderElement() {
    AERO_ASSERT(renderManager_ == nullptr);
    AERO_ASSERT(renderParent_ == nullptr);
    AERO_ASSERT(renderChildren_.Empty());
}

Base::Result<void> RenderElement::InvalidateRender() noexcept {
    Base::Result<void> access = VerifyAccess();
    if (!access) {
        return access;
    }
    if (renderManager_ == nullptr) {
        renderValid_ = false;
        return {};
    }
    return renderManager_->Invalidate(*this);
}

Base::Result<void> RenderElement::BuildDisplayList(
    DisplayListBuilder&) noexcept {
    return {};
}

std::uint64_t RenderPlan::StableHash() const noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    hash = HashScalar(hash, version_);
    hash = HashScalar(hash, nodes_.Size());
    hash = HashScalar(hash, commands_.Size());
    for (const RenderNodeSnapshot& node : nodes_) {
        hash = HashScalar(hash, node.id);
        hash = HashScalar(hash, node.parentId);
        hash = HashRect(hash, node.layoutSlot);
        hash = HashRect(hash, node.clip);
        hash = HashSize(hash, node.renderSize);
        hash = HashScalar(hash, node.commandOffset);
        hash = HashScalar(hash, node.commandCount);
        hash = HashScalar(hash, node.elementRevision);
    }
    for (const RenderCommand& command : commands_) {
        hash = HashCommand(hash, command);
    }
    return hash;
}

Base::Result<void> NullRenderBackend::Submit(
    const RenderPlan& plan) noexcept {
    std::uint32_t clipDepth = 0U;
    std::uint32_t opacityDepth = 0U;
    std::uint32_t transformDepth = 0U;
    RenderNodeId previousId = InvalidRenderNodeId;

    const Base::Span<const RenderCommand> commands = plan.Commands();
    for (const RenderNodeSnapshot& node : plan.Nodes()) {
        if (node.id == InvalidRenderNodeId || node.id <= previousId) {
            return InvalidState("RenderPlan node IDs must be nonzero and ordered");
        }
        previousId = node.id;
        if (!IsValidLayoutRect(node.layoutSlot) ||
            !IsValidLayoutRect(node.clip) ||
            !IsValidLayoutSize(node.renderSize) ||
            node.commandOffset > commands.Size() ||
            node.commandCount > commands.Size() - node.commandOffset) {
            return InvalidArgument("RenderPlan node snapshot is invalid");
        }
    }

    for (const RenderCommand& command : commands) {
        switch (command.kind) {
        case RenderCommandKind::PushClip:
            if (!IsValidLayoutRect(command.rect)) {
                return InvalidArgument("RenderPlan contains an invalid clip");
            }
            ++clipDepth;
            break;
        case RenderCommandKind::PopClip:
            if (clipDepth == 0U) {
                return InvalidState("RenderPlan clip stack underflow");
            }
            --clipDepth;
            break;
        case RenderCommandKind::PushOpacity:
            if (!IsValidOpacity(command.scalar)) {
                return InvalidArgument("RenderPlan contains invalid opacity");
            }
            ++opacityDepth;
            break;
        case RenderCommandKind::PopOpacity:
            if (opacityDepth == 0U) {
                return InvalidState("RenderPlan opacity stack underflow");
            }
            --opacityDepth;
            break;
        case RenderCommandKind::PushTransform:
            if (!IsFinite(command.transform)) {
                return InvalidArgument("RenderPlan contains invalid transform");
            }
            ++transformDepth;
            break;
        case RenderCommandKind::PopTransform:
            if (transformDepth == 0U) {
                return InvalidState("RenderPlan transform stack underflow");
            }
            --transformDepth;
            break;
        case RenderCommandKind::FillRect:
            if (!IsValidLayoutRect(command.rect) || !IsFinite(command.color)) {
                return InvalidArgument("RenderPlan contains invalid FillRect");
            }
            break;
        case RenderCommandKind::FillRoundedRect:
            if (!IsValidLayoutRect(command.rect) || !IsFinite(command.color) ||
                !std::isfinite(command.scalar) || command.scalar < 0.0 ||
                command.scalar * 2.0 >
                    std::fmin(command.rect.width, command.rect.height)) {
                return InvalidArgument("RenderPlan contains invalid FillRoundedRect");
            }
            break;
        case RenderCommandKind::StrokeRect:
            if (!IsValidLayoutRect(command.rect) || !IsFinite(command.color) ||
                !std::isfinite(command.scalar) || command.scalar < 0.0) {
                return InvalidArgument("RenderPlan contains invalid StrokeRect");
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
                return InvalidArgument("RenderPlan contains invalid DrawImage");
            }
            break;
        case RenderCommandKind::DrawMesh:
            if (command.mesh == InvalidRenderMeshId || !IsFinite(command.color)) {
                return InvalidArgument("RenderPlan contains invalid DrawMesh");
            }
            break;
        case RenderCommandKind::DrawGlyphRun:
            if (command.glyphRun == InvalidRenderGlyphRunId ||
                !IsFinite(command.color)) {
                return InvalidArgument("RenderPlan contains invalid DrawGlyphRun");
            }
            break;
        }
    }
    if (clipDepth != 0U || opacityDepth != 0U || transformDepth != 0U) {
        return InvalidState("RenderPlan contains unbalanced state stacks");
    }

    lastVersion_ = plan.Version();
    lastHash_ = plan.StableHash();
    ++submissionCount_;
    return {};
}

RenderManager::RenderManager(
    Dispatcher& dispatcher,
    IRenderBackend& backend,
    Base::IAllocator* allocator) noexcept
    : dispatcher_(&dispatcher),
      backend_(&backend),
      allocator_(allocator != nullptr ? allocator : &dispatcher.Allocator()),
      dirty_(allocator_),
      currentPlan_(allocator_) {}

RenderManager::~RenderManager() noexcept {
    if (phaseHook_.IsValid() && dispatcher_->CheckAccess()) {
        (void)dispatcher_->RemoveFrameHook(phaseHook_);
    }
    if (root_ != nullptr && dispatcher_->CheckAccess()) {
        auto clear = [&](auto&& self, RenderElement& element) noexcept -> void {
            for (RenderElement* child : element.renderChildren_) {
                self(self, *child);
            }
            element.renderChildren_.Clear();
            element.renderParent_ = nullptr;
            element.renderManager_ = nullptr;
            element.renderQueued_ = false;
            element.renderValid_ = false;
            element.nodeId_ = InvalidRenderNodeId;
        };
        clear(clear, *root_);
        root_ = nullptr;
    }
}

Base::Result<void> RenderManager::Initialize() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (phaseHook_.IsValid()) {
        return {};
    }
    Base::Result<DispatcherFrameHookHandle> hook = dispatcher_->RegisterFrameHook(
        DispatcherFramePhase::RenderCommit,
        &RenderManager::RenderCommitHook,
        this);
    if (!hook) {
        return hook.GetStatus();
    }
    phaseHook_ = hook.Value();
    return {};
}

Base::Result<void> RenderManager::VerifyElement(
    const RenderElement& element) const noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access;
    }
    if (!phaseHook_.IsValid()) {
        return InvalidState("RenderManager must be initialized before use");
    }
    if (&element.GetDispatcher() != dispatcher_) {
        return Base::Status::Failure(
            Base::ErrorCode::WrongThread,
            "RenderElement belongs to another Dispatcher");
    }
    if (committing_) {
        return InvalidState("Render tree mutation during commit is not allowed");
    }
    return {};
}

Base::Result<void> RenderManager::SetRoot(RenderElement* root) noexcept {
    if (root == nullptr) {
        Base::Result<void> access = dispatcher_->VerifyAccess();
        if (!access) {
            return access;
        }
        if (committing_) {
            return InvalidState("Render root cannot change during commit");
        }
        if (root_ != nullptr) {
            auto clear = [&](auto&& self, RenderElement& element) noexcept -> void {
                for (RenderElement* child : element.renderChildren_) {
                    self(self, *child);
                }
                element.renderChildren_.Clear();
                element.renderParent_ = nullptr;
                element.renderManager_ = nullptr;
                element.renderQueued_ = false;
                element.renderValid_ = false;
                element.nodeId_ = InvalidRenderNodeId;
            };
            clear(clear, *root_);
        }
        root_ = nullptr;
        dirty_.Clear();
        return {};
    }

    Base::Result<void> verified = VerifyElement(*root);
    if (!verified) {
        return verified;
    }
    if (root_ == root) {
        return {};
    }
    if (root_ != nullptr || root->renderManager_ != nullptr ||
        root->renderParent_ != nullptr || root->VisualParent() != nullptr) {
        return InvalidState("Render root must be detached and unique");
    }
    if (nextNodeId_ == InvalidRenderNodeId) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Render node ID space exhausted");
    }
    root_ = root;
    root->renderManager_ = this;
    root->nodeId_ = nextNodeId_++;
    root->renderValid_ = false;
    return QueueDirty(*root);
}

Base::Result<void> RenderManager::Attach(
    RenderElement& parent,
    RenderElement& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) {
        return verified;
    }
    verified = VerifyElement(child);
    if (!verified) {
        return verified;
    }
    if (parent.renderManager_ != this || child.renderManager_ != nullptr ||
        child.renderParent_ != nullptr || child.VisualParent() != &parent) {
        return InvalidState("Render attachment must match the visual-tree parent");
    }
    if (nextNodeId_ == InvalidRenderNodeId) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "Render node ID space exhausted");
    }
    Base::Result<void> reserve = parent.renderChildren_.TryReserve(
        parent.renderChildren_.Size() + 1U);
    if (!reserve) {
        return reserve;
    }
    Base::Result<void> appended = parent.renderChildren_.TryPushBack(&child);
    AERO_ASSERT(appended);
    child.renderParent_ = &parent;
    child.renderManager_ = this;
    child.nodeId_ = nextNodeId_++;
    child.renderValid_ = false;
    Base::Result<void> queued = QueueDirty(child);
    if (!queued) {
        parent.renderChildren_.PopBack();
        child.renderParent_ = nullptr;
        child.renderManager_ = nullptr;
        child.nodeId_ = InvalidRenderNodeId;
        return queued;
    }
    return Invalidate(parent);
}

void RenderManager::RemoveChild(
    Base::Vector<RenderElement*>& children,
    RenderElement& child) noexcept {
    for (std::uint32_t index = 0U; index < children.Size(); ++index) {
        if (children[index] == &child) {
            for (std::uint32_t current = index + 1U;
                 current < children.Size(); ++current) {
                children[current - 1U] = children[current];
            }
            children.PopBack();
            return;
        }
    }
}

Base::Result<void> RenderManager::Detach(
    RenderElement& parent,
    RenderElement& child) noexcept {
    Base::Result<void> verified = VerifyElement(parent);
    if (!verified) {
        return verified;
    }
    if (parent.renderManager_ != this || child.renderParent_ != &parent ||
        child.renderManager_ != this) {
        return NotFound("Render parent-child relationship was not found");
    }
    RemoveChild(parent.renderChildren_, child);
    auto clear = [&](auto&& self, RenderElement& element) noexcept -> void {
        for (RenderElement* descendant : element.renderChildren_) {
            self(self, *descendant);
        }
        element.renderChildren_.Clear();
        element.renderParent_ = nullptr;
        element.renderManager_ = nullptr;
        element.renderQueued_ = false;
        element.renderValid_ = false;
        element.nodeId_ = InvalidRenderNodeId;
    };
    clear(clear, child);
    for (std::uint32_t index = 0U; index < dirty_.Size();) {
        if (dirty_[index]->renderManager_ != this) {
            for (std::uint32_t current = index + 1U;
                 current < dirty_.Size(); ++current) {
                dirty_[current - 1U] = dirty_[current];
            }
            dirty_.PopBack();
        } else {
            ++index;
        }
    }
    return Invalidate(parent);
}

Base::Result<void> RenderManager::QueueDirty(
    RenderElement& element) noexcept {
    if (element.renderQueued_) {
        return {};
    }
    Base::Result<void> appended = dirty_.TryPushBack(&element);
    if (!appended) {
        return appended;
    }
    element.renderQueued_ = true;
    return {};
}

Base::Result<void> RenderManager::Invalidate(
    RenderElement& element) noexcept {
    Base::Result<void> verified = VerifyElement(element);
    if (!verified) {
        return verified;
    }
    if (element.renderManager_ != this) {
        return InvalidState("RenderElement is not attached to this RenderManager");
    }
    RenderElement* current = &element;
    while (current != nullptr) {
        current->renderValid_ = false;
        Base::Result<void> queued = QueueDirty(*current);
        if (!queued) {
            return queued;
        }
        current = current->renderParent_;
    }
    return {};
}

Base::Result<void> RenderManager::BuildSubtree(
    RenderElement& element,
    RenderNodeId parentId,
    RenderPlan& plan) noexcept {
    if (!element.IsArrangeValid() || element.buildingDisplayList_) {
        return InvalidState("RenderElement must be arranged and non-reentrant");
    }
    element.buildingDisplayList_ = true;
    DisplayListBuilder builder(allocator_);
    Base::Result<void> built = element.BuildDisplayList(builder);
    if (!built) {
        element.buildingDisplayList_ = false;
        return built;
    }
    Base::Result<DisplayList> listResult = builder.Finish();
    element.buildingDisplayList_ = false;
    if (!listResult) {
        return listResult.GetStatus();
    }
    DisplayList list = std::move(listResult).Value();

    if (plan.commands_.Size() > UINT32_MAX - list.CommandCount()) {
        return Base::Status::Failure(
            Base::ErrorCode::OutOfRange,
            "RenderPlan command count exceeds 32-bit range");
    }
    RenderNodeSnapshot snapshot;
    snapshot.id = element.nodeId_;
    snapshot.parentId = parentId;
    snapshot.layoutSlot = element.LayoutSlot();
    snapshot.clip = element.LayoutClip();
    snapshot.renderSize = element.RenderSize();
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

    element.renderRevision_ = snapshot.elementRevision;
    element.renderValid_ = true;
    element.renderQueued_ = false;
    for (RenderElement* child : element.renderChildren_) {
        Base::Result<void> childResult = BuildSubtree(*child, element.nodeId_, plan);
        if (!childResult) {
            return childResult;
        }
    }
    return {};
}

Base::Result<std::uint32_t> RenderManager::Commit() noexcept {
    Base::Result<void> access = dispatcher_->VerifyAccess();
    if (!access) {
        return access.GetStatus();
    }
    if (!phaseHook_.IsValid()) {
        return InvalidState("RenderManager must be initialized before commit");
    }
    if (committing_) {
        return InvalidState("Nested render commit is not allowed");
    }
    if (root_ == nullptr) {
        dirty_.Clear();
        return 0U;
    }
    if (dirty_.Empty() && currentPlan_.Version() != 0U) {
        return 0U;
    }

    committing_ = true;
    RenderPlan next(allocator_);
    next.version_ = commitVersion_ + 1U;
    Base::Result<void> built = BuildSubtree(
        *root_, InvalidRenderNodeId, next);
    if (!built) {
        committing_ = false;
        return built.GetStatus();
    }
    Base::Result<void> submitted = backend_->Submit(next);
    if (!submitted) {
        committing_ = false;
        return submitted.GetStatus();
    }
    const std::uint32_t committedNodes = next.nodes_.Size();
    currentPlan_ = std::move(next);
    commitVersion_ = currentPlan_.Version();
    dirty_.Clear();
    committing_ = false;
    return committedNodes;
}

RenderDiagnostics RenderManager::Diagnostics() const noexcept {
    RenderDiagnostics diagnostics;
    diagnostics.commitVersion = commitVersion_;
    diagnostics.nodeCount = currentPlan_.Nodes().Size();
    diagnostics.commandCount = currentPlan_.Commands().Size();
    diagnostics.dirtyCount = dirty_.Size();
    diagnostics.planHash = currentPlan_.StableHash();
    return diagnostics;
}

void RenderManager::RenderCommitHook(void* context) noexcept {
    auto* manager = static_cast<RenderManager*>(context);
    (void)manager->Commit();
}

} // namespace Aero::Core
