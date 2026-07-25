from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


def replace_section(text: str, start: str, end: str, replacement: str, label: str) -> str:
    begin = text.find(start)
    if begin < 0:
        raise RuntimeError(f"{label}: start marker not found")
    finish = text.find(end, begin)
    if finish < 0:
        raise RuntimeError(f"{label}: end marker not found")
    return text[:begin] + replacement.rstrip() + "\n\n" + text[finish:]


path = "include/Aero/Presentation/Rendering.hpp"
text = read(path)
text = replace_once(
    text,
    """    Base::Vector<FrameworkElement*> dirty_;\n""",
    """    Base::Vector<Detail::VisualLease> dirty_;\n""",
    "Render lease dirty queue")
text = replace_once(
    text,
    """    Base::Result<void> QueueDirty(\n        FrameworkElement& element) noexcept;\n    Base::Result<void> BuildSubtree(\n""",
    """    Base::Result<void> QueueDirty(\n        FrameworkElement& element) noexcept;\n    void RemoveQueued(FrameworkElement& element) noexcept;\n    void MarkCommittedSubtree(FrameworkElement& element) noexcept;\n    Base::Result<void> BuildSubtree(\n""",
    "Render helper declarations")
write(path, text)


path = "src/presentation/Rendering.cpp"
text = read(path)
text = replace_section(
    text,
    "Base::Result<void> RenderManager::SetRoot",
    "Base::Result<void> RenderManager::Attach",
    """Base::Result<void> RenderManager::SetRoot(\n    FrameworkElement* root) noexcept {\n    if (root == nullptr) {\n        Base::Result<void> access = dispatcher_->VerifyAccess();\n        if (!access) return access.GetStatus();\n        if (committing_) {\n            return InvalidState(\n                \"Render root cannot change during commit\");\n        }\n        if (root_ != nullptr) {\n            auto clear = [&](auto&& self,\n                             FrameworkElement& element) noexcept -> void {\n                for (FrameworkElement* child : element.RenderChildren()) {\n                    self(self, *child);\n                }\n                RemoveQueued(element);\n                element.renderAttached_ = false;\n                element.renderManager_ = nullptr;\n                element.renderValid_ = false;\n                element.nodeId_ = InvalidRenderNodeId;\n            };\n            clear(clear, *root_);\n        }\n        root_ = nullptr;\n        dirty_.Clear();\n        return {};\n    }\n\n    Base::Result<void> verified = VerifyElement(*root);\n    if (!verified) return verified.GetStatus();\n    if (root_ == root) return {};\n    if (root_ != nullptr || root->renderManager_ != nullptr ||\n        root->renderAttached_ || root->VisualParent() != nullptr) {\n        return InvalidState(\"Render root must be detached and unique\");\n    }\n    if (nextNodeId_ == InvalidRenderNodeId) {\n        return Base::Status::Failure(\n            Base::ErrorCode::OutOfRange,\n            \"Render node ID space exhausted\");\n    }\n\n    Base::Result<Detail::VisualLease> lease =\n        Detail::VisualLease::Acquire(*root);\n    if (!lease) return lease.GetStatus();\n    Base::Result<void> reserved =\n        dirty_.TryReserve(dirty_.Size() + 1U);\n    if (!reserved) return reserved.GetStatus();\n\n    root_ = root;\n    root->renderManager_ = this;\n    root->nodeId_ = nextNodeId_++;\n    root->renderValid_ = false;\n    Base::Result<void> queued =\n        dirty_.TryPushBack(std::move(lease).Value());\n    AERO_ASSERT(queued);\n    (void)queued;\n    root->renderQueued_ = true;\n    return {};\n}\n""",
    "atomic Render SetRoot")
text = replace_section(
    text,
    "Base::Result<void> RenderManager::Attach",
    "Base::Result<void> RenderManager::Detach",
    """Base::Result<void> RenderManager::Attach(\n    FrameworkElement& parent,\n    FrameworkElement& child) noexcept {\n    Base::Result<void> verified = VerifyElement(parent);\n    if (!verified) return verified.GetStatus();\n    verified = VerifyElement(child);\n    if (!verified) return verified.GetStatus();\n    if (parent.renderManager_ != this ||\n        child.renderManager_ != nullptr || child.renderAttached_ ||\n        child.RenderParent() != &parent) {\n        return InvalidState(\n            \"Render attachment must match the visual-tree parent\");\n    }\n    if (nextNodeId_ == InvalidRenderNodeId) {\n        return Base::Status::Failure(\n            Base::ErrorCode::OutOfRange,\n            \"Render node ID space exhausted\");\n    }\n\n    Base::Result<Detail::VisualLease> childLease =\n        Detail::VisualLease::Acquire(child);\n    if (!childLease) return childLease.GetStatus();\n\n    std::uint32_t required = 1U;\n    for (FrameworkElement* current = &parent; current != nullptr;\n         current = current->renderAttached_\n             ? current->RenderParent() : nullptr) {\n        if (!current->renderQueued_) ++required;\n    }\n    Base::Result<void> reserved =\n        dirty_.TryReserve(dirty_.Size() + required);\n    if (!reserved) return reserved.GetStatus();\n\n    Base::Result<void> invalidated = Invalidate(parent);\n    if (!invalidated) return invalidated.GetStatus();\n\n    child.renderAttached_ = true;\n    child.renderManager_ = this;\n    child.nodeId_ = nextNodeId_++;\n    child.renderValid_ = false;\n    Base::Result<void> queued = dirty_.TryPushBack(\n        std::move(childLease).Value());\n    AERO_ASSERT(queued);\n    (void)queued;\n    child.renderQueued_ = true;\n    return {};\n}\n""",
    "atomic Render Attach")
text = replace_section(
    text,
    "Base::Result<void> RenderManager::Detach",
    "Base::Result<void> RenderManager::QueueDirty",
    """Base::Result<void> RenderManager::Detach(\n    FrameworkElement& parent,\n    FrameworkElement& child) noexcept {\n    Base::Result<void> verified = VerifyElement(parent);\n    if (!verified) return verified.GetStatus();\n    if (parent.renderManager_ != this || !child.renderAttached_ ||\n        child.RenderParent() != &parent ||\n        child.renderManager_ != this) {\n        return NotFound(\n            \"Render parent-child relationship was not found\");\n    }\n\n    Base::Result<void> invalidated = Invalidate(parent);\n    if (!invalidated) return invalidated.GetStatus();\n\n    auto clear = [&](auto&& self,\n                     FrameworkElement& element) noexcept -> void {\n        for (FrameworkElement* descendant : element.RenderChildren()) {\n            self(self, *descendant);\n        }\n        RemoveQueued(element);\n        element.renderAttached_ = false;\n        element.renderManager_ = nullptr;\n        element.renderValid_ = false;\n        element.nodeId_ = InvalidRenderNodeId;\n    };\n    clear(clear, child);\n    return {};\n}\n""",
    "atomic Render Detach")
text = replace_section(
    text,
    "Base::Result<void> RenderManager::QueueDirty",
    "Base::Result<void> RenderManager::Invalidate",
    """Base::Result<void> RenderManager::QueueDirty(\n    FrameworkElement& element) noexcept {\n    if (element.renderQueued_) return {};\n    Base::Result<Detail::VisualLease> lease =\n        Detail::VisualLease::Acquire(element);\n    if (!lease) return lease.GetStatus();\n    Base::Result<void> appended =\n        dirty_.TryPushBack(std::move(lease).Value());\n    if (!appended) return appended.GetStatus();\n    element.renderQueued_ = true;\n    return {};\n}\n\nvoid RenderManager::RemoveQueued(FrameworkElement& element) noexcept {\n    for (std::uint32_t index = 0U; index < dirty_.Size();) {\n        if (dirty_[index].Resolve() != &element) {\n            ++index;\n            continue;\n        }\n        for (std::uint32_t next = index + 1U;\n             next < dirty_.Size(); ++next) {\n            dirty_[next - 1U] = std::move(dirty_[next]);\n        }\n        dirty_.PopBack();\n    }\n    element.renderQueued_ = false;\n}\n\nvoid RenderManager::MarkCommittedSubtree(\n    FrameworkElement& element) noexcept {\n    ++element.renderRevision_;\n    element.renderValid_ = true;\n    element.renderQueued_ = false;\n    for (FrameworkElement* child : element.RenderChildren()) {\n        MarkCommittedSubtree(*child);\n    }\n}\n""",
    "Render lease queue implementation")
text = replace_section(
    text,
    "Base::Result<void> RenderManager::Invalidate",
    "Base::Result<void> RenderManager::BuildSubtree",
    """Base::Result<void> RenderManager::Invalidate(\n    FrameworkElement& element) noexcept {\n    Base::Result<void> verified = VerifyElement(element);\n    if (!verified) return verified.GetStatus();\n    if (element.renderManager_ != this) {\n        return InvalidState(\n            \"FrameworkElement is not attached to this RenderManager\");\n    }\n\n    Base::Vector<FrameworkElement*> path;\n    for (FrameworkElement* current = &element; current != nullptr;\n         current = current->renderAttached_\n             ? current->RenderParent() : nullptr) {\n        Base::Result<void> currentVerified = VerifyElement(*current);\n        if (!currentVerified) return currentVerified.GetStatus();\n        Base::Result<void> appended = path.TryPushBack(current);\n        if (!appended) return appended.GetStatus();\n    }\n\n    Base::Vector<Detail::VisualLease> leases;\n    Base::Result<void> reserved = leases.TryReserve(path.Size());\n    if (!reserved) return reserved.GetStatus();\n    for (FrameworkElement* current : path) {\n        if (current->renderQueued_) continue;\n        Base::Result<Detail::VisualLease> lease =\n            Detail::VisualLease::Acquire(*current);\n        if (!lease) return lease.GetStatus();\n        Base::Result<void> staged =\n            leases.TryPushBack(std::move(lease).Value());\n        if (!staged) return staged.GetStatus();\n    }\n    reserved = dirty_.TryReserve(dirty_.Size() + leases.Size());\n    if (!reserved) return reserved.GetStatus();\n\n    std::uint32_t leaseIndex = 0U;\n    for (FrameworkElement* current : path) {\n        current->renderValid_ = false;\n        if (current->renderQueued_) continue;\n        Base::Result<void> queued = dirty_.TryPushBack(\n            std::move(leases[leaseIndex++]));\n        AERO_ASSERT(queued);\n        (void)queued;\n        current->renderQueued_ = true;\n    }\n    return {};\n}\n""",
    "atomic Render Invalidate")
text = replace_once(
    text,
    """    RenderNodeSnapshot snapshot;\n""",
    """    if (element.renderRevision_ == UINT64_MAX) {\n        return Base::Status::Failure(\n            Base::ErrorCode::OutOfRange,\n            \"Render element revision space exhausted\");\n    }\n    RenderNodeSnapshot snapshot;\n""",
    "Render revision preflight")
text = replace_once(
    text,
    """    element.renderRevision_ = snapshot.elementRevision;\n    element.renderValid_ = true;\n    element.renderQueued_ = false;\n    for (FrameworkElement* child : element.RenderChildren()) {\n""",
    """    for (FrameworkElement* child : element.RenderChildren()) {\n""",
    "Render deferred state commit")
text = replace_section(
    text,
    "Base::Result<std::uint32_t> RenderManager::Commit",
    "RenderDiagnostics RenderManager::Diagnostics",
    """Base::Result<std::uint32_t> RenderManager::Commit() noexcept {\n    Base::Result<void> access = dispatcher_->VerifyAccess();\n    if (!access) return access.GetStatus();\n    if (!phaseHook_.IsValid()) {\n        return InvalidState(\n            \"RenderManager must be initialized before commit\");\n    }\n    if (committing_) {\n        return InvalidState(\"Nested render commit is not allowed\");\n    }\n    if (root_ == nullptr) {\n        for (const Detail::VisualLease& lease : dirty_) {\n            Visual* visual = lease.Resolve();\n            FrameworkElement* element = visual != nullptr\n                ? visual->AsFrameworkElement() : nullptr;\n            if (element != nullptr) element->renderQueued_ = false;\n        }\n        dirty_.Clear();\n        return 0U;\n    }\n    if (dirty_.Empty() && currentPlan_.Version() != 0U) return 0U;\n\n    committing_ = true;\n    RenderPlan next;\n    next.version_ = commitVersion_ + 1U;\n    Base::Result<void> built = BuildSubtree(\n        *root_, InvalidRenderNodeId, next);\n    if (!built) {\n        committing_ = false;\n        return built.GetStatus();\n    }\n    Base::Result<void> submitted = backend_->Submit(next);\n    if (!submitted) {\n        committing_ = false;\n        return submitted.GetStatus();\n    }\n\n    const std::uint32_t committedNodes = next.nodes_.Size();\n    currentPlan_ = std::move(next);\n    commitVersion_ = currentPlan_.Version();\n    MarkCommittedSubtree(*root_);\n    dirty_.Clear();\n    committing_ = false;\n    return committedNodes;\n}\n""",
    "atomic Render Commit")
write(path, text)
