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


path = "CMakeLists.txt"
text = read(path)
text = replace_once(
    text,
    """    src/presentation/Metadata.cpp\n    src/presentation/ObjectTree.cpp\n""",
    """    src/presentation/Metadata.cpp\n    src/presentation/MountService.cpp\n    src/presentation/ObjectTree.cpp\n""",
    "AeroPresentation MountService source")
write(path, text)


path = "include/Aero/Markup/XamlVisualTree.hpp"
text = read(path)
text = replace_once(
    text,
    "#include <Aero/Presentation/ObjectTree.hpp>\n",
    "#include <Aero/Presentation/MountService.hpp>\n#include <Aero/Presentation/ObjectTree.hpp>\n",
    "XAML MountService include")
text = replace_once(
    text,
    """        void* contentContext = nullptr;\n        bool logicalAttached = false;\n        bool visualAttached = false;\n        bool layoutAttached = false;\n        bool renderAttached = false;\n""",
    """        void* contentContext = nullptr;\n        Presentation::MountEdgeState mount;\n""",
    "XAML edge mount state")
text = replace_once(
    text,
    """    Presentation::RenderManager* renderer_ = nullptr;\n    XamlSchemaContext* schema_ = nullptr;\n""",
    """    Presentation::RenderManager* renderer_ = nullptr;\n    Presentation::MountService mounts_;\n    Presentation::MountRootState rootMount_;\n    XamlSchemaContext* schema_ = nullptr;\n""",
    "XAML mount service fields")
write(path, text)


path = "src/markup/XamlVisualTree.cpp"
text = read(path)
text = replace_once(
    text,
    """    : tree_(&tree), layout_(&layout), values_(&values), renderer_(renderer),\n      types_(), singles_(), collections_(), edges_(), nodes_() {}\n""",
    """    : tree_(&tree), layout_(&layout), values_(&values), renderer_(renderer),\n      mounts_(tree, &layout, renderer), rootMount_(),\n      types_(), singles_(), collections_(), edges_(), nodes_() {}\n""",
    "XAML mount service construction")
text = replace_section(
    text,
    "Base::Result<void> XamlVisualTreeHost::AttachEdge",
    "void XamlVisualTreeHost::DetachEdge",
    """Base::Result<void> XamlVisualTreeHost::AttachEdge(Edge& edge) noexcept {\n    Base::Result<Presentation::MountEdgeState> mounted =\n        mounts_.Attach(*edge.parent, *edge.child);\n    if (!mounted) return mounted.GetStatus();\n    edge.mount = std::move(mounted).Value();\n\n    if (edge.configureCollectionChild != nullptr) {\n        Base::Result<void> configured =\n            edge.configureCollectionChild(\n                *edge.parentOwner.Get(),\n                *edge.parent,\n                *edge.child,\n                edge.contentContext);\n        if (!configured) {\n            (void)mounts_.Detach(edge.mount);\n            return configured.GetStatus();\n        }\n    }\n    return {};\n}\n""",
    "XAML unified AttachEdge")
text = replace_section(
    text,
    "void XamlVisualTreeHost::DetachEdge",
    "Base::Result<void> XamlVisualTreeHost::Mount",
    """void XamlVisualTreeHost::DetachEdge(Edge& edge) noexcept {\n    (void)mounts_.Detach(edge.mount);\n}\n""",
    "XAML unified DetachEdge")
text = replace_section(
    text,
    "Base::Result<void> XamlVisualTreeHost::Mount",
    "void XamlVisualTreeHost::ReleaseStagedContent",
    """Base::Result<void> XamlVisualTreeHost::Mount(\n    Base::Object& root,\n    Core::TypeId rootType,\n    Presentation::Size availableSize) noexcept {\n    if (mounted_ || schema_ == nullptr ||\n        !Presentation::IsValidLayoutSize(availableSize)) {\n        return InvalidVisualTreeState(\n            \"XAML visual tree cannot mount in its current state\");\n    }\n    Base::Result<Presentation::Visual*> rootNode =\n        ResolveVisual(root, rootType);\n    if (!rootNode) return rootNode.GetStatus();\n    Base::Result<Presentation::UIElement*> rootLayout =\n        ResolveUIElement(root, rootType);\n    if (!rootLayout) return rootLayout.GetStatus();\n    Base::Result<void> added = AddNode(*rootNode.Value());\n    if (!added) return added.GetStatus();\n\n    rootNode_ = rootNode.Value();\n    rootLayout_ = rootLayout.Value();\n    rootRender_ = ResolveFrameworkElement(root, rootType);\n\n    Base::Result<Presentation::MountRootState> rootMounted =\n        mounts_.AttachRoot(*rootNode_, availableSize);\n    if (!rootMounted) {\n        rootNode_ = nullptr;\n        rootLayout_ = nullptr;\n        rootRender_ = nullptr;\n        return rootMounted.GetStatus();\n    }\n    rootMount_ = std::move(rootMounted).Value();\n\n    std::uint32_t attached = 0U;\n    while (attached < edges_.Size()) {\n        bool progressed = false;\n        for (Edge& edge : edges_) {\n            if (edge.mount.logicalAttached ||\n                edge.parent->OwningTree() != tree_) {\n                continue;\n            }\n            Base::Result<void> result = AttachEdge(edge);\n            if (!result) {\n                (void)Unmount();\n                return result.GetStatus();\n            }\n            ++attached;\n            progressed = true;\n        }\n        if (!progressed) {\n            (void)Unmount();\n            return InvalidVisualTreeState(\n                \"XAML content graph is disconnected from its root\");\n        }\n    }\n    mounted_ = true;\n    return {};\n}\n""",
    "XAML unified root mount")
text = replace_section(
    text,
    "Base::Result<void> XamlVisualTreeHost::Unmount",
    "Base::Result<void> XamlVisualTreeHost::DiscardStaged",
    """Base::Result<void> XamlVisualTreeHost::Unmount() noexcept {\n    if (!mounted_ && rootNode_ == nullptr) return {};\n\n    Base::Status firstError;\n    for (std::uint32_t index = edges_.Size(); index > 0U; --index) {\n        Base::Result<void> detached =\n            mounts_.Detach(edges_[index - 1U].mount);\n        if (!detached && firstError.IsOk()) {\n            firstError = detached.GetStatus();\n        }\n    }\n    Base::Result<void> rootDetached = mounts_.DetachRoot(rootMount_);\n    if (!rootDetached && firstError.IsOk()) {\n        firstError = rootDetached.GetStatus();\n    }\n\n    for (Presentation::Visual* node : nodes_) {\n        if (node != nullptr) (void)values_->DetachObject(*node);\n    }\n    ReleaseStagedContent();\n    edges_.Clear();\n    nodes_.Clear();\n    rootNode_ = nullptr;\n    rootLayout_ = nullptr;\n    rootRender_ = nullptr;\n    rootMount_ = {};\n    mounted_ = false;\n    return firstError.IsOk()\n        ? Base::Result<void>()\n        : Base::Result<void>(firstError);\n}\n""",
    "XAML unified unmount")
write(path, text)
