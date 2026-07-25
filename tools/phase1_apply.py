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


path = "include/Aero/Presentation/Layout.hpp"
text = read(path)
text = replace_once(
    text,
    """    Base::Vector<UIElement*> measureQueue_;\n    Base::Vector<UIElement*> arrangeQueue_;\n""",
    """    Base::Vector<Detail::VisualLease> measureQueue_;\n    Base::Vector<Detail::VisualLease> arrangeQueue_;\n""",
    "Layout lease queues")
text = replace_once(
    text,
    """    Base::Result<void> QueueArrange(UIElement& element) noexcept;\n    Base::Result<void> MeasureElement(UIElement& element, Size constraint) noexcept;\n""",
    """    Base::Result<void> QueueArrange(UIElement& element) noexcept;\n    void RemoveQueued(UIElement& element) noexcept;\n    Base::Result<void> MeasureElement(UIElement& element, Size constraint) noexcept;\n""",
    "Layout queue removal declaration")
write(path, text)


path = "src/presentation/Layout.cpp"
text = read(path)
text = replace_section(
    text,
    "Base::Result<void> LayoutManager::Attach",
    "Base::Result<void> LayoutManager::Detach",
    """Base::Result<void> LayoutManager::Attach(\n    UIElement& parent,\n    UIElement& child) noexcept {\n    Base::Result<void> verified = VerifyElement(parent);\n    if (!verified) return verified.GetStatus();\n    verified = VerifyElement(child);\n    if (!verified) return verified.GetStatus();\n    if (&parent == &child || child.layoutAttached_) {\n        return InvalidState(\n            \"Layout child is already attached or self-referential\");\n    }\n    if (child.LayoutParent() != &parent) {\n        return InvalidState(\n            \"Layout attachment must match the visual tree parent\");\n    }\n\n    // Queue all parent invalidation work before publishing the child state.\n    Base::Result<void> invalidated = InvalidateMeasure(parent);\n    if (!invalidated) return invalidated.GetStatus();\n\n    parent.manager_ = this;\n    child.manager_ = this;\n    child.layoutAttached_ = true;\n    child.measureValid_ = false;\n    child.arrangeValid_ = false;\n    return {};\n}\n""",
    "atomic Layout Attach")
text = replace_section(
    text,
    "Base::Result<void> LayoutManager::Detach",
    "Base::Result<void> LayoutManager::SetRoot",
    """Base::Result<void> LayoutManager::Detach(\n    UIElement& parent,\n    UIElement& child) noexcept {\n    Base::Result<void> verified = VerifyElement(parent);\n    if (!verified) return verified.GetStatus();\n    if (!child.layoutAttached_ || child.LayoutParent() != &parent ||\n        child.manager_ != this) {\n        return Base::Status::Failure(\n            Base::ErrorCode::NotFound,\n            \"Layout parent-child relationship was not found\");\n    }\n\n    Base::Result<void> invalidated = InvalidateMeasure(parent);\n    if (!invalidated) return invalidated.GetStatus();\n\n    RemoveQueued(child);\n    child.layoutAttached_ = false;\n    child.manager_ = nullptr;\n    child.measureValid_ = false;\n    child.arrangeValid_ = false;\n    return {};\n}\n""",
    "atomic Layout Detach")
text = replace_section(
    text,
    "Base::Result<void> LayoutManager::SetRoot",
    "Base::Result<void> LayoutManager::QueueMeasure",
    """Base::Result<void> LayoutManager::SetRoot(\n    UIElement* root,\n    Size availableSize) noexcept {\n    Base::Result<void> access = dispatcher_->VerifyAccess();\n    if (!access) return access.GetStatus();\n    if (!IsValidLayoutSize(availableSize)) {\n        return InvalidArgument(\n            \"Root layout size must be finite and nonnegative\");\n    }\n    if (root != nullptr) {\n        Base::Result<void> verified = VerifyElement(*root);\n        if (!verified) return verified.GetStatus();\n        if (root->layoutAttached_ || root->VisualParent() != nullptr) {\n            return InvalidState(\n                \"Layout root cannot have a visual or layout parent\");\n        }\n        Base::Result<void> invalidated = InvalidateMeasure(*root);\n        if (!invalidated) return invalidated.GetStatus();\n    }\n\n    if (root_ != nullptr && root_ != root) {\n        RemoveQueued(*root_);\n        root_->manager_ = nullptr;\n    }\n    root_ = root;\n    rootAvailableSize_ = availableSize;\n    if (root_ != nullptr) root_->manager_ = this;\n    return {};\n}\n""",
    "atomic Layout SetRoot")
text = replace_section(
    text,
    "Base::Result<void> LayoutManager::QueueMeasure",
    "Base::Result<void> LayoutManager::InvalidateMeasure",
    """Base::Result<void> LayoutManager::QueueMeasure(\n    UIElement& element) noexcept {\n    if (element.measureQueued_) return {};\n    Base::Result<Detail::VisualLease> lease =\n        Detail::VisualLease::Acquire(element);\n    if (!lease) return lease.GetStatus();\n    Base::Result<void> appended =\n        measureQueue_.TryPushBack(std::move(lease).Value());\n    if (!appended) return appended.GetStatus();\n    element.measureQueued_ = true;\n    return {};\n}\n\nBase::Result<void> LayoutManager::QueueArrange(\n    UIElement& element) noexcept {\n    if (element.arrangeQueued_) return {};\n    Base::Result<Detail::VisualLease> lease =\n        Detail::VisualLease::Acquire(element);\n    if (!lease) return lease.GetStatus();\n    Base::Result<void> appended =\n        arrangeQueue_.TryPushBack(std::move(lease).Value());\n    if (!appended) return appended.GetStatus();\n    element.arrangeQueued_ = true;\n    return {};\n}\n\nvoid LayoutManager::RemoveQueued(UIElement& element) noexcept {\n    auto remove = [&](Base::Vector<Detail::VisualLease>& queue) noexcept {\n        for (std::uint32_t index = 0U; index < queue.Size();) {\n            if (queue[index].Resolve() != &element) {\n                ++index;\n                continue;\n            }\n            for (std::uint32_t next = index + 1U;\n                 next < queue.Size(); ++next) {\n                queue[next - 1U] = std::move(queue[next]);\n            }\n            queue.PopBack();\n        }\n    };\n    remove(measureQueue_);\n    remove(arrangeQueue_);\n    element.measureQueued_ = false;\n    element.arrangeQueued_ = false;\n}\n""",
    "Layout lease queue implementation")
text = replace_section(
    text,
    "Base::Result<void> LayoutManager::InvalidateMeasure",
    "Base::Result<void> LayoutManager::InvalidateArrange",
    """Base::Result<void> LayoutManager::InvalidateMeasure(\n    UIElement& element) noexcept {\n    Base::Vector<UIElement*> path;\n    UIElement* current = &element;\n    while (current != nullptr) {\n        Base::Result<void> verified = VerifyElement(*current);\n        if (!verified) return verified.GetStatus();\n        Base::Result<void> appended = path.TryPushBack(current);\n        if (!appended) return appended.GetStatus();\n        current = current->layoutAttached_\n            ? current->LayoutParent() : nullptr;\n    }\n\n    Base::Vector<Detail::VisualLease> leases;\n    Base::Result<void> reserved = leases.TryReserve(path.Size());\n    if (!reserved) return reserved.GetStatus();\n    for (UIElement* item : path) {\n        if (item->measureQueued_) continue;\n        Base::Result<Detail::VisualLease> lease =\n            Detail::VisualLease::Acquire(*item);\n        if (!lease) return lease.GetStatus();\n        Base::Result<void> staged =\n            leases.TryPushBack(std::move(lease).Value());\n        if (!staged) return staged.GetStatus();\n    }\n    reserved = measureQueue_.TryReserve(\n        measureQueue_.Size() + leases.Size());\n    if (!reserved) return reserved.GetStatus();\n\n    std::uint32_t leaseIndex = 0U;\n    for (UIElement* item : path) {\n        item->measureValid_ = false;\n        item->arrangeValid_ = false;\n        if (item->measureQueued_) continue;\n        Base::Result<void> queued = measureQueue_.TryPushBack(\n            std::move(leases[leaseIndex++]));\n        AERO_ASSERT(queued);\n        (void)queued;\n        item->measureQueued_ = true;\n    }\n    return {};\n}\n""",
    "atomic InvalidateMeasure")
text = replace_section(
    text,
    "Base::Result<void> LayoutManager::InvalidateArrange",
    "Base::Result<void> LayoutManager::MeasureElement",
    """Base::Result<void> LayoutManager::InvalidateArrange(\n    UIElement& element) noexcept {\n    Base::Vector<UIElement*> path;\n    UIElement* current = &element;\n    while (current != nullptr) {\n        Base::Result<void> verified = VerifyElement(*current);\n        if (!verified) return verified.GetStatus();\n        Base::Result<void> appended = path.TryPushBack(current);\n        if (!appended) return appended.GetStatus();\n        current = current->layoutAttached_\n            ? current->LayoutParent() : nullptr;\n    }\n\n    Base::Vector<Detail::VisualLease> leases;\n    Base::Result<void> reserved = leases.TryReserve(path.Size());\n    if (!reserved) return reserved.GetStatus();\n    for (UIElement* item : path) {\n        if (item->arrangeQueued_) continue;\n        Base::Result<Detail::VisualLease> lease =\n            Detail::VisualLease::Acquire(*item);\n        if (!lease) return lease.GetStatus();\n        Base::Result<void> staged =\n            leases.TryPushBack(std::move(lease).Value());\n        if (!staged) return staged.GetStatus();\n    }\n    reserved = arrangeQueue_.TryReserve(\n        arrangeQueue_.Size() + leases.Size());\n    if (!reserved) return reserved.GetStatus();\n\n    std::uint32_t leaseIndex = 0U;\n    for (UIElement* item : path) {\n        item->arrangeValid_ = false;\n        if (item->arrangeQueued_) continue;\n        Base::Result<void> queued = arrangeQueue_.TryPushBack(\n            std::move(leases[leaseIndex++]));\n        AERO_ASSERT(queued);\n        (void)queued;\n        item->arrangeQueued_ = true;\n    }\n    return {};\n}\n""",
    "atomic InvalidateArrange")
text = replace_once(
    text,
    """    if (element.measureValid_ && SameSize(element.previousMeasureConstraint_, constraint)) {\n        return {};\n    }\n    const FrameworkElement* framework = element.AsFrameworkElement();\n""",
    """    if (element.measureValid_ && SameSize(element.previousMeasureConstraint_, constraint)) {\n        return {};\n    }\n\n    Detail::VisualLease pendingArrange;\n    const bool queueArrange = !element.arrangeQueued_;\n    if (queueArrange) {\n        Base::Result<Detail::VisualLease> lease =\n            Detail::VisualLease::Acquire(element);\n        if (!lease) return lease.GetStatus();\n        pendingArrange = std::move(lease).Value();\n        Base::Result<void> reserved = arrangeQueue_.TryReserve(\n            arrangeQueue_.Size() + 1U);\n        if (!reserved) return reserved.GetStatus();\n    }\n\n    const FrameworkElement* framework = element.AsFrameworkElement();\n""",
    "Measure arrange preflight")
text = replace_once(
    text,
    """    ++element.layoutRevision_;\n    ++measuredCount_;\n    return QueueArrange(element);\n""",
    """    ++element.layoutRevision_;\n    ++measuredCount_;\n    if (queueArrange) {\n        Base::Result<void> queued = arrangeQueue_.TryPushBack(\n            std::move(pendingArrange));\n        AERO_ASSERT(queued);\n        (void)queued;\n        element.arrangeQueued_ = true;\n    }\n    return {};\n""",
    "Measure arrange commit")
text = replace_section(
    text,
    "Base::Result<std::uint32_t> LayoutManager::Flush",
    "LayoutDiagnostics LayoutManager::Diagnostics",
    """Base::Result<std::uint32_t> LayoutManager::Flush() noexcept {\n    Base::Result<void> access = dispatcher_->VerifyAccess();\n    if (!access) return access.GetStatus();\n    if (flushing_) return InvalidState(\"Nested layout flush is not allowed\");\n\n    flushing_ = true;\n    measuredCount_ = 0U;\n    arrangedCount_ = 0U;\n\n    if (root_ != nullptr &&\n        (!root_->measureValid_ || !root_->arrangeValid_)) {\n        Base::Result<void> measured =\n            MeasureElement(*root_, rootAvailableSize_);\n        if (!measured) {\n            flushing_ = false;\n            return measured.GetStatus();\n        }\n        Base::Result<void> arranged = ArrangeElement(\n            *root_, {0.0, 0.0,\n                     rootAvailableSize_.width, rootAvailableSize_.height});\n        if (!arranged) {\n            flushing_ = false;\n            return arranged.GetStatus();\n        }\n    }\n\n    Base::Vector<Detail::VisualLease> measure =\n        std::move(measureQueue_);\n    measureQueue_ = Base::Vector<Detail::VisualLease>();\n    for (const Detail::VisualLease& lease : measure) {\n        Visual* visual = lease.Resolve();\n        UIElement* element = visual != nullptr\n            ? visual->AsUIElement() : nullptr;\n        if (element != nullptr) element->measureQueued_ = false;\n    }\n    for (const Detail::VisualLease& lease : measure) {\n        Visual* visual = lease.Resolve();\n        UIElement* element = visual != nullptr\n            ? visual->AsUIElement() : nullptr;\n        if (element == nullptr || element == root_ ||\n            element->manager_ != this || element->measureValid_) {\n            continue;\n        }\n        UIElement* parent = element->layoutAttached_\n            ? element->LayoutParent() : nullptr;\n        const Size constraint = parent != nullptr\n            ? parent->renderSize_ : rootAvailableSize_;\n        Base::Result<void> measured =\n            MeasureElement(*element, constraint);\n        if (!measured) {\n            (void)QueueMeasure(*element);\n            flushing_ = false;\n            return measured.GetStatus();\n        }\n    }\n\n    Base::Vector<Detail::VisualLease> arrange =\n        std::move(arrangeQueue_);\n    arrangeQueue_ = Base::Vector<Detail::VisualLease>();\n    for (const Detail::VisualLease& lease : arrange) {\n        Visual* visual = lease.Resolve();\n        UIElement* element = visual != nullptr\n            ? visual->AsUIElement() : nullptr;\n        if (element != nullptr) element->arrangeQueued_ = false;\n    }\n    for (const Detail::VisualLease& lease : arrange) {\n        Visual* visual = lease.Resolve();\n        UIElement* element = visual != nullptr\n            ? visual->AsUIElement() : nullptr;\n        if (element == nullptr || element == root_ ||\n            element->manager_ != this || element->arrangeValid_) {\n            continue;\n        }\n        Rect slot = element->layoutSlot_;\n        if (slot.width == 0.0 && slot.height == 0.0) {\n            slot.width = element->desiredSize_.width;\n            slot.height = element->desiredSize_.height;\n        }\n        Base::Result<void> arranged = ArrangeElement(*element, slot);\n        if (!arranged) {\n            (void)QueueArrange(*element);\n            flushing_ = false;\n            return arranged.GetStatus();\n        }\n    }\n\n    ++passVersion_;\n    flushing_ = false;\n    return measuredCount_ + arrangedCount_;\n}\n""",
    "safe Layout Flush")
write(path, text)
