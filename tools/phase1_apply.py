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


path = "include/Aero/Presentation/MountService.hpp"
text = read(path)
text = replace_once(
    text,
    """    Base::Result<void> Detach(MountEdgeState& state) noexcept;\n\n    Base::Result<PresentationMountState> AttachPresentation(\n""",
    """    Base::Result<void> Detach(MountEdgeState& state) noexcept;\n    Base::Result<void> DetachPresentation(\n        MountEdgeState& state) noexcept;\n    Base::Result<void> AttachPresentation(\n        MountEdgeState& state,\n        Visual& newVisualParent) noexcept;\n\n    Base::Result<PresentationMountState> AttachPresentation(\n""",
    "MountEdge presentation operations")
write(path, text)

path = "src/presentation/MountService.cpp"
text = read(path)
marker = "Base::Result<PresentationMountState> MountService::AttachPresentation(\n"
insert = """Base::Result<void> MountService::DetachPresentation(\n    MountEdgeState& state) noexcept {\n    PresentationMountState presentation;\n    presentation.visualParent = state.visualParent;\n    presentation.child = state.child;\n    presentation.visualAttached = state.visualAttached;\n    presentation.layoutAttached = state.layoutAttached;\n    presentation.renderAttached = state.renderAttached;\n    Base::Result<void> detached = DetachPresentation(presentation);\n    state.visualAttached = presentation.visualAttached;\n    state.layoutAttached = presentation.layoutAttached;\n    state.renderAttached = presentation.renderAttached;\n    return detached;\n}\n\nBase::Result<void> MountService::AttachPresentation(\n    MountEdgeState& state,\n    Visual& newVisualParent) noexcept {\n    if (state.child == nullptr || state.visualAttached ||\n        state.layoutAttached || state.renderAttached) {\n        return InvalidState(\n            \"Mount edge is not ready for presentation attachment\");\n    }\n    Base::Result<PresentationMountState> attached =\n        AttachPresentation(newVisualParent, *state.child);\n    if (!attached) return attached.GetStatus();\n    PresentationMountState presentation =\n        std::move(attached).Value();\n    state.visualParent = presentation.visualParent;\n    state.visualAttached = presentation.visualAttached;\n    state.layoutAttached = presentation.layoutAttached;\n    state.renderAttached = presentation.renderAttached;\n    return {};\n}\n\n"""
text = replace_once(text, marker, insert + marker, "MountEdge presentation implementation")
write(path, text)


path = "include/Aero/Controls/Items.hpp"
text = read(path)
text = replace_once(
    text,
    "#include <Aero/Presentation/Style.hpp>\n",
    "#include <Aero/Presentation/MountService.hpp>\n#include <Aero/Presentation/Style.hpp>\n",
    "Items MountService include")
text = replace_once(
    text,
    """        Base::Ref<Base::Object> content;\n        const Style* appliedStyle = nullptr;\n""",
    """        Base::Ref<Base::Object> content;\n        MountEdgeState containerMount;\n        MountEdgeState contentMount;\n        const Style* appliedStyle = nullptr;\n""",
    "Items record mount states")
text = replace_once(
    text,
    """    RenderManager* renderer_ = nullptr;\n    ItemsControl* owner_ = nullptr;\n""",
    """    RenderManager* renderer_ = nullptr;\n    MountService mounts_;\n    ItemsControl* owner_ = nullptr;\n""",
    "Items mount service field")
write(path, text)


path = "src/controls/Items.cpp"
text = read(path)
text = replace_once(
    text,
    """       styles_(styles),\n       renderer_(renderer),\n       changedHandler_(\n""",
    """       styles_(styles),\n       renderer_(renderer),\n       mounts_(tree, &layout, renderer),\n       changedHandler_(\n""",
    "Items mount service construction")
text = replace_section(
    text,
    "Base::Result<void>\nItemContainerGenerator::AttachRecord",
    "Base::Result<void>\nItemContainerGenerator::DetachRecord",
    """Base::Result<void>\nItemContainerGenerator::AttachRecord(\n    Record& record,\n    std::uint32_t index) noexcept {\n    ItemContainer& container = *record.container;\n    auto& content =\n        *static_cast<UIElement*>(record.content.Get());\n\n    Base::Result<MountEdgeState> containerMounted =\n        mounts_.Attach(*owner_, *host_, container);\n    if (!containerMounted) return containerMounted.GetStatus();\n    record.containerMount = std::move(containerMounted).Value();\n\n    Base::Result<MountEdgeState> contentMounted =\n        mounts_.Attach(container, content);\n    if (!contentMounted) {\n        (void)mounts_.Detach(record.containerMount);\n        return contentMounted.GetStatus();\n    }\n    record.contentMount = std::move(contentMounted).Value();\n\n    Base::Result<void> selected =\n        container.SetOwnedContent(record.content, content);\n    if (!selected) {\n        (void)mounts_.Detach(record.contentMount);\n        (void)mounts_.Detach(record.containerMount);\n        return selected.GetStatus();\n    }\n\n    const Style* style = owner_->ItemContainerStyle();\n    if (style != nullptr && styles_ != nullptr) {\n        Base::Result<void> styled = styles_->Apply(container, *style);\n        if (!styled) {\n            (void)DetachRecord(record);\n            return styled.GetStatus();\n        }\n        record.appliedStyle = style;\n    }\n    Base::Result<void> prepared =\n        owner_->PrepareContainer(container, record.item, index);\n    if (!prepared) {\n        (void)DetachRecord(record);\n        return prepared.GetStatus();\n    }\n    return {};\n}\n""",
    "Items unified AttachRecord")
text = replace_section(
    text,
    "Base::Result<void>\nItemContainerGenerator::DetachRecord",
    "Base::Result<void>\nItemContainerGenerator::ReleaseRecycledContainers",
    """Base::Result<void>\nItemContainerGenerator::DetachRecord(\n    Record& record,\n    bool recycleContainer) noexcept {\n    if (!record.container) return {};\n    ItemContainer& container = *record.container;\n    Base::Status firstError;\n    const auto capture =\n        [&firstError](const Base::Result<void>& result) noexcept {\n            if (!result && firstError.IsOk()) {\n                firstError = result.GetStatus();\n            }\n        };\n\n    owner_->ClearContainer(container);\n    if (record.appliedStyle != nullptr && styles_ != nullptr) {\n        capture(styles_->Clear(container, *record.appliedStyle));\n        record.appliedStyle = nullptr;\n    }\n\n    UIElement* content = container.Content();\n    if (content != nullptr) {\n        capture(mounts_.Detach(record.contentMount));\n        capture(container.SetContent(nullptr));\n        capture(values_->DetachObject(*content));\n    }\n    capture(mounts_.Detach(record.containerMount));\n\n    if (recycleContainer && firstError.IsOk()) {\n        Base::Result<void> recycled =\n            recycledContainers_.TryPushBack(std::move(record.container));\n        if (!recycled) {\n            capture(values_->DetachObject(container));\n            if (firstError.IsOk()) firstError = recycled.GetStatus();\n        }\n    } else {\n        capture(values_->DetachObject(container));\n    }\n    record.item.Reset();\n    record.content.Reset();\n    record.containerMount = {};\n    record.contentMount = {};\n    return firstError.IsOk()\n        ? Base::Result<void>()\n        : Base::Result<void>(firstError);\n}\n""",
    "Items unified DetachRecord")
text = replace_section(
    text,
    "Base::Result<void>\nItemContainerGenerator::ReorderVisuals",
    "Base::Result<bool>\nItemContainerGenerator::SetRealizationRangeInternal",
    """Base::Result<void>\nItemContainerGenerator::ReorderVisuals() noexcept {\n    for (Record& record : records_) {\n        Base::Result<void> detached =\n            mounts_.DetachPresentation(record.containerMount);\n        if (!detached) return detached.GetStatus();\n    }\n    for (Record& record : records_) {\n        Base::Result<void> attached =\n            mounts_.AttachPresentation(\n                record.containerMount, *host_);\n        if (!attached) return attached.GetStatus();\n    }\n    return {};\n}\n""",
    "Items unified visual reorder")
write(path, text)
