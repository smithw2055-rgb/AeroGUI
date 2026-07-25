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


path = "include/Aero/Controls/Templates.hpp"
text = read(path)
text = replace_once(text,
    "#include <Aero/Presentation/ObjectTree.hpp>\n",
    "#include <Aero/Presentation/MountService.hpp>\n#include <Aero/Presentation/ObjectTree.hpp>\n",
    "Template MountService include")
text = replace_once(text,
    """    FrameworkElement* frameworkElement = nullptr;\n};\n""",
    """    FrameworkElement* frameworkElement = nullptr;\n    MountEdgeState mount;\n};\n""",
    "Template part mount state")
text = replace_once(text,
    """    Visual* originalVisualParent = nullptr;\n    bool attachedLogical = false;\n    bool detachedOriginalLayout = false;\n    bool detachedOriginalRender = false;\n    bool attachedProjectedLayout = false;\n    bool attachedProjectedRender = false;\n};\n""",
    """    Visual* originalVisualParent = nullptr;\n    PresentationMountState projectedMount;\n    bool attachedLogical = false;\n    bool detachedOriginalPresentation = false;\n};\n""",
    "Template projection mount state")
text = replace_once(text,
    """        : tree_(&tree),\n          layout_(layout),\n          renderer_(renderer),\n          parent_(&parent) {}\n""",
    """        : tree_(&tree),\n          layout_(layout),\n          renderer_(renderer),\n          mounts_(tree, layout, renderer),\n          parent_(&parent) {}\n""",
    "TemplateBuildContext MountService construction")
text = replace_once(text,
    """        Base::StringView name,\n        Base::Ref<Base::Object> owner,\n        Visual& visual) noexcept;\n""",
    """        Base::StringView name,\n        Base::Ref<Base::Object> owner,\n        Visual& visual,\n        MountEdgeState mount) noexcept;\n""",
    "Template AddOwnedPart state")
text = replace_once(text,
    """    RenderManager* renderer_ = nullptr;\n    Control* parent_ = nullptr;\n""",
    """    RenderManager* renderer_ = nullptr;\n    MountService mounts_;\n    Control* parent_ = nullptr;\n""",
    "TemplateBuildContext MountService field")
text = replace_once(text,
    """          layout_(layout),\n          renderer_(renderer),\n          propertyChangedHandler_(\n""",
    """          layout_(layout),\n          renderer_(renderer),\n          mounts_(tree, layout, renderer),\n          propertyChangedHandler_(\n""",
    "TemplateManager MountService construction")
text = replace_once(text,
    """    RenderManager* renderer_ = nullptr;\n    Base::Vector<Instance> instances_;\n""",
    """    RenderManager* renderer_ = nullptr;\n    MountService mounts_;\n    Base::Vector<Instance> instances_;\n""",
    "TemplateManager MountService field")
write(path, text)


path = "src/controls/Templates.cpp"
text = read(path)
text = replace_section(text,
    "Base::Result<void> TemplateBuildContext::SetRoot",
    "Base::Result<void> TemplateBuildContext::AddPart",
    """Base::Result<void> TemplateBuildContext::SetRoot(\n    Base::Ref<Base::Object> owner,\n    Visual& root) noexcept {\n    if (tree_ == nullptr || parent_ == nullptr ||\n        rootVisual_ != nullptr || !owner ||\n        owner.Get() != &root || root.AsUIElement() == nullptr) {\n        return Base::Status::Failure(\n            Base::ErrorCode::InvalidArgument,\n            \"Template root registration is invalid\");\n    }\n\n    Base::Result<MountEdgeState> mounted =\n        mounts_.Attach(*parent_, root);\n    if (!mounted) return mounted.GetStatus();\n    MountEdgeState mount = std::move(mounted).Value();\n\n    Base::Result<void> selected =\n        parent_->SetTemplateChild(root.AsUIElement());\n    if (!selected) {\n        (void)mounts_.Detach(mount);\n        return selected.GetStatus();\n    }\n    if (root.AsFrameworkElement() != nullptr) {\n        Base::Result<void> templated =\n            root.AsFrameworkElement()->SetTemplatedParent(parent_);\n        if (!templated) {\n            (void)parent_->SetTemplateChild(nullptr);\n            (void)mounts_.Detach(mount);\n            return templated.GetStatus();\n        }\n    }\n    Base::Result<void> added = AddOwnedPart(\n        {}, std::move(owner), root, mount);\n    if (!added) {\n        if (root.AsFrameworkElement() != nullptr) {\n            (void)root.AsFrameworkElement()->SetTemplatedParent(nullptr);\n        }\n        (void)parent_->SetTemplateChild(nullptr);\n        (void)mounts_.Detach(mount);\n        return added.GetStatus();\n    }\n    rootVisual_ = &root;\n    rootElement_ = root.AsUIElement();\n    return {};\n}\n""",
    "Template unified SetRoot")
text = replace_section(text,
    "Base::Result<void> TemplateBuildContext::AddPart",
    "Base::Result<bool> TemplateBuildContext::ProjectContent",
    """Base::Result<void> TemplateBuildContext::AddPart(\n    Base::StringView name,\n    Visual& parent,\n    Base::Ref<Base::Object> owner,\n    Visual& part) noexcept {\n    if (tree_ == nullptr || parent_ == nullptr ||\n        rootVisual_ == nullptr || name.Empty() ||\n        !owner || owner.Get() != &part ||\n        FindObject(name) != nullptr) {\n        return Base::Status::Failure(\n            Base::ErrorCode::InvalidArgument,\n            \"Template part registration is invalid\");\n    }\n\n    Base::Result<MountEdgeState> mounted = mounts_.Attach(parent, part);\n    if (!mounted) return mounted.GetStatus();\n    MountEdgeState mount = std::move(mounted).Value();\n\n    if (part.AsFrameworkElement() != nullptr) {\n        Base::Result<void> templated =\n            part.AsFrameworkElement()->SetTemplatedParent(parent_);\n        if (!templated) {\n            (void)mounts_.Detach(mount);\n            return templated.GetStatus();\n        }\n    }\n    Base::Result<void> added = AddOwnedPart(\n        name, std::move(owner), part, mount);\n    if (!added) {\n        if (part.AsFrameworkElement() != nullptr) {\n            (void)part.AsFrameworkElement()->SetTemplatedParent(nullptr);\n        }\n        (void)mounts_.Detach(mount);\n        return added.GetStatus();\n    }\n    return {};\n}\n""",
    "Template unified AddPart")
text = replace_section(text,
    "Base::Result<bool> TemplateBuildContext::ProjectContent",
    "DependencyObject* TemplateBuildContext::FindObject",
    """Base::Result<bool> TemplateBuildContext::ProjectContent(\n    ContentControl& owner,\n    ContentPresenter& presenter) noexcept {\n    if (tree_ == nullptr || parent_ == nullptr ||\n        &owner != parent_ || rootVisual_ == nullptr) {\n        return Base::Status::Failure(\n            Base::ErrorCode::InvalidArgument,\n            \"Template content projection owner is invalid\");\n    }\n    UIElement* content = owner.Content();\n    if (content == nullptr) return false;\n\n    bool presenterIsPart = false;\n    for (const TemplatePart& part : parts_) {\n        presenterIsPart = presenterIsPart || part.visual == &presenter;\n    }\n    if (!presenterIsPart ||\n        (content->LogicalParent() != nullptr &&\n         content->LogicalParent() != &owner) ||\n        (content->VisualParent() != nullptr &&\n         content->VisualParent() != &owner)) {\n        return Base::Status::Failure(\n            Base::ErrorCode::InvalidState,\n            \"Template content cannot be projected\");\n    }\n\n    TemplateContentProjection projection;\n    projection.owner = &owner;\n    projection.presenter = &presenter;\n    projection.content = content;\n    projection.originalVisualParent = content->VisualParent();\n\n    auto restore = [&]() noexcept {\n        (void)presenter.SetContent(nullptr);\n        (void)mounts_.DetachPresentation(projection.projectedMount);\n        if (projection.detachedOriginalPresentation &&\n            projection.originalVisualParent != nullptr) {\n            (void)mounts_.AttachPresentation(\n                *projection.originalVisualParent, *content);\n        }\n        if (projection.attachedLogical) {\n            (void)tree_->DetachLogical(owner, *content);\n        }\n    };\n\n    if (content->LogicalParent() == nullptr) {\n        Base::Result<void> logical = tree_->AttachLogical(owner, *content);\n        if (!logical) return logical.GetStatus();\n        projection.attachedLogical = true;\n    }\n    if (projection.originalVisualParent != nullptr) {\n        PresentationMountState original;\n        original.visualParent = projection.originalVisualParent;\n        original.child = content;\n        original.visualAttached = true;\n        original.layoutAttached = layout_ != nullptr &&\n            projection.originalVisualParent->AsUIElement() != nullptr;\n        original.renderAttached = renderer_ != nullptr &&\n            projection.originalVisualParent->AsFrameworkElement() != nullptr &&\n            content->AsFrameworkElement() != nullptr;\n        Base::Result<void> detached = mounts_.DetachPresentation(original);\n        if (!detached) {\n            if (projection.attachedLogical) {\n                (void)tree_->DetachLogical(owner, *content);\n            }\n            return detached.GetStatus();\n        }\n        projection.detachedOriginalPresentation = true;\n    }\n\n    Base::Result<PresentationMountState> projected =\n        mounts_.AttachPresentation(presenter, *content);\n    if (!projected) {\n        restore();\n        return projected.GetStatus();\n    }\n    projection.projectedMount = std::move(projected).Value();\n\n    Base::Result<void> selected = presenter.SetContent(content);\n    if (!selected) {\n        restore();\n        return selected.GetStatus();\n    }\n    Base::Result<void> tracked =\n        projections_.TryPushBack(std::move(projection));\n    if (!tracked) {\n        restore();\n        return tracked.GetStatus();\n    }\n    return true;\n}\n""",
    "Template unified ProjectContent")
text = replace_section(text,
    "Base::Result<void> TemplateBuildContext::AddOwnedPart",
    "void TemplateBuildContext::Rollback",
    """Base::Result<void> TemplateBuildContext::AddOwnedPart(\n    Base::StringView name,\n    Base::Ref<Base::Object> owner,\n    Visual& visual,\n    MountEdgeState mount) noexcept {\n    TemplatePart part;\n    Base::Result<void> assigned = part.name.TryAssign(name);\n    if (!assigned) return assigned.GetStatus();\n    part.owner = std::move(owner);\n    part.visual = &visual;\n    part.object = &visual;\n    part.frameworkElement = visual.AsFrameworkElement();\n    part.mount = mount;\n    return parts_.TryPushBack(std::move(part));\n}\n""",
    "Template AddOwnedPart mount state")
text = replace_section(text,
    "void TemplateBuildContext::Rollback",
    "Base::Result<void> FrameworkTemplate::TryAddTemplateBinding",
    """void TemplateBuildContext::Rollback() noexcept {\n    for (std::uint32_t index = projections_.Size();\n         index > 0U; --index) {\n        TemplateContentProjection& projection = projections_[index - 1U];\n        if (projection.presenter == nullptr || projection.content == nullptr) {\n            continue;\n        }\n        (void)projection.presenter->SetContent(nullptr);\n        (void)mounts_.DetachPresentation(projection.projectedMount);\n        if (projection.detachedOriginalPresentation &&\n            projection.originalVisualParent != nullptr) {\n            (void)mounts_.AttachPresentation(\n                *projection.originalVisualParent, *projection.content);\n        }\n        if (projection.attachedLogical && projection.owner != nullptr) {\n            (void)tree_->DetachLogical(\n                *projection.owner, *projection.content);\n        }\n    }\n    projections_.Clear();\n\n    for (std::uint32_t index = parts_.Size(); index > 0U; --index) {\n        TemplatePart& part = parts_[index - 1U];\n        if (part.frameworkElement != nullptr) {\n            (void)part.frameworkElement->SetTemplatedParent(nullptr);\n        }\n        (void)mounts_.Detach(part.mount);\n    }\n    if (parent_ != nullptr) (void)parent_->SetTemplateChild(nullptr);\n    parts_.Clear();\n    rootVisual_ = nullptr;\n    rootElement_ = nullptr;\n}\n""",
    "Template unified Rollback")
text = replace_section(text,
    "Base::Result<void> TemplateManager::ClearAt",
    "void TemplateManager::OnPropertyChanged",
    """Base::Result<void> TemplateManager::ClearAt(\n    std::uint32_t index) noexcept {\n    Instance& instance = instances_[index];\n    Unsubscribe(instance);\n    Base::Result<void> providers = ClearProviders(instance);\n    if (!providers) return providers.GetStatus();\n\n    for (TemplatePart& part : instance.parts) {\n        if (part.frameworkElement != nullptr) {\n            Base::Result<void> cleared =\n                part.frameworkElement->SetTemplatedParent(nullptr);\n            if (!cleared) return cleared.GetStatus();\n        }\n    }\n    Base::Result<void> child = instance.parent->SetTemplateChild(nullptr);\n    if (!child) return child.GetStatus();\n\n    for (std::uint32_t projectionIndex = instance.projections.Size();\n         projectionIndex > 0U; --projectionIndex) {\n        TemplateContentProjection& projection =\n            instance.projections[projectionIndex - 1U];\n        if (projection.presenter == nullptr || projection.content == nullptr) {\n            continue;\n        }\n        Base::Result<void> presenterCleared =\n            projection.presenter->SetContent(nullptr);\n        if (!presenterCleared) return presenterCleared.GetStatus();\n        Base::Result<void> projectedDetached =\n            mounts_.DetachPresentation(projection.projectedMount);\n        if (!projectedDetached) return projectedDetached.GetStatus();\n        if (projection.detachedOriginalPresentation &&\n            projection.originalVisualParent != nullptr) {\n            Base::Result<PresentationMountState> restored =\n                mounts_.AttachPresentation(\n                    *projection.originalVisualParent, *projection.content);\n            if (!restored) return restored.GetStatus();\n        }\n        if (projection.attachedLogical && projection.owner != nullptr) {\n            Base::Result<void> logicalDetached = tree_->DetachLogical(\n                *projection.owner, *projection.content);\n            if (!logicalDetached) return logicalDetached.GetStatus();\n        }\n    }\n\n    for (std::uint32_t partIndex = instance.parts.Size();\n         partIndex > 0U; --partIndex) {\n        Base::Result<void> detached =\n            mounts_.Detach(instance.parts[partIndex - 1U].mount);\n        if (!detached) return detached.GetStatus();\n    }\n    for (TemplatePart& part : instance.parts) {\n        if (part.object == nullptr) continue;\n        Base::Result<void> untracked = values_->DetachObject(*part.object);\n        if (!untracked) return untracked.GetStatus();\n    }\n    if (index + 1U != instances_.Size()) {\n        instances_[index] = std::move(instances_[instances_.Size() - 1U]);\n    }\n    instances_.PopBack();\n    return {};\n}\n""",
    "Template unified ClearAt")
write(path, text)
