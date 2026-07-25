from pathlib import Path

root = Path(__file__).resolve().parents[1]

path = root / "src/controls/Templates.cpp"
text = path.read_text(encoding="utf-8")

old = """        (void)presenter.SetContent(nullptr);\n        (void)mounts_.DetachPresentation(projection.projectedMount);\n"""
new = """        (void)mounts_.DetachPresentation(projection.projectedMount);\n        (void)presenter.SetContent(nullptr);\n"""
count = text.count(old)
if count != 1:
    raise RuntimeError(f"ProjectContent restore order count: {count}")
text = text.replace(old, new, 1)

old = """        (void)projection.presenter->SetContent(nullptr);\n        (void)mounts_.DetachPresentation(projection.projectedMount);\n"""
new = """        (void)mounts_.DetachPresentation(projection.projectedMount);\n        (void)projection.presenter->SetContent(nullptr);\n"""
count = text.count(old)
if count != 1:
    raise RuntimeError(f"Template rollback order count: {count}")
text = text.replace(old, new, 1)

old = """        Base::Result<void> presenterCleared =\n            projection.presenter->SetContent(nullptr);\n        if (!presenterCleared) return presenterCleared.GetStatus();\n        Base::Result<void> projectedDetached =\n            mounts_.DetachPresentation(projection.projectedMount);\n        if (!projectedDetached) return projectedDetached.GetStatus();\n"""
new = """        Base::Result<void> projectedDetached =\n            mounts_.DetachPresentation(projection.projectedMount);\n        if (!projectedDetached) return projectedDetached.GetStatus();\n        Base::Result<void> presenterCleared =\n            projection.presenter->SetContent(nullptr);\n        if (!presenterCleared) return presenterCleared.GetStatus();\n"""
count = text.count(old)
if count != 1:
    raise RuntimeError(f"Template ClearAt order count: {count}")
text = text.replace(old, new, 1)
path.write_text(text, encoding="utf-8")

path = root / "src/markup/XamlVisualTree.cpp"
text = path.read_text(encoding="utf-8")
old = """    Base::Result<void> rootDetached = mounts_.DetachRoot(rootMount_);\n    if (!rootDetached && firstError.IsOk()) {\n        firstError = rootDetached.GetStatus();\n    }\n\n    for (Presentation::Visual* node : nodes_) {\n"""
new = """    Base::Result<void> rootDetached = mounts_.DetachRoot(rootMount_);\n    if (!rootDetached && firstError.IsOk()) {\n        firstError = rootDetached.GetStatus();\n    }\n\n    if (rootNode_ != nullptr &&\n        (tree_->Root() == rootNode_ ||\n         rootNode_->LogicalParent() != nullptr ||\n         rootNode_->VisualParent() != nullptr ||\n         !rootNode_->LogicalChildren().Empty() ||\n         !rootNode_->VisualChildren().Empty())) {\n        Base::Result<void> residual = tree_->DetachNode(*rootNode_);\n        if (!residual && firstError.IsOk()) {\n            firstError = residual.GetStatus();\n        }\n    }\n\n    for (Presentation::Visual* node : nodes_) {\n"""
if old in text:
    text = text.replace(old, new, 1)
elif "Base::Result<void> residual = tree_->DetachNode(*rootNode_);" not in text:
    raise RuntimeError("XAML residual cleanup anchor mismatch")
path.write_text(text, encoding="utf-8")
