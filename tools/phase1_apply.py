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


path = "src/presentation/ObjectTree.cpp"
text = read(path)
text = text.replace(
    "        AERO_ASSERT(appended);\n",
    "        AERO_ASSERT(appended);\n        (void)appended;\n")
text = text.replace(
    "        AERO_ASSERT(remembered);\n",
    "        AERO_ASSERT(remembered);\n        (void)remembered;\n")
text = text.replace(
    "    AERO_ASSERT(appended);\n    child.logicalParent_",
    "    AERO_ASSERT(appended);\n    (void)appended;\n    child.logicalParent_")
write(path, text)

path = "src/markup/XamlVisualTree.cpp"
text = read(path)
text = replace_once(
    text,
    """    if (renderer_ != nullptr && rootRender_ != nullptr)\n        (void)renderer_->SetRoot(nullptr);\n    if (rootLayout_ != nullptr) (void)layout_->SetRoot(nullptr, {});\n    if (rootNode_ != nullptr) (void)tree_->SetRoot(nullptr);\n""",
    """    if (renderer_ != nullptr && rootRender_ != nullptr)\n        (void)renderer_->SetRoot(nullptr);\n    if (rootLayout_ != nullptr) (void)layout_->SetRoot(nullptr, {});\n    // Edge teardown is deliberately best-effort so shutdown can continue. A\n    // final DetachNode pass removes any residual logical/visual edge before\n    // ownership references are released, preventing partially detached trees.\n    if (rootNode_ != nullptr && tree_->Root() == rootNode_)\n        (void)tree_->DetachNode(*rootNode_);\n""",
    "XAML residual tree cleanup")
write(path, text)
