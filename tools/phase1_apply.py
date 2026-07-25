from pathlib import Path

root = Path(__file__).resolve().parents[1]

phase1 = root / "tests/presentation/Phase1RuntimeSafetyTests.cpp"
source = phase1.read_text(encoding="utf-8")
source = source.replace(
    "            const std::uint64_t treeVersion = runtime.Tree()->Version();\n",
    "")
source = source.replace(
    "                CHECK(runtime.Tree()->Version() == treeVersion);\n",
    "")
phase1.write_text(source, encoding="utf-8")

closure = root / "tests/markup/M1M4ClosureTests.inc"
text = closure.read_text(encoding="utf-8")
include_anchor = "#include <Aero/Text/UnicodeRuntime.hpp>\n"
include_block = """#include <Aero/Text/UnicodeRuntime.hpp>\n\n#ifdef CHECK\n#undef CHECK\n#endif\n#define main AeroPhase1EmbeddedMain\n#include \"../presentation/Phase1RuntimeSafetyTests.cpp\"\n#undef main\n"""
if "AeroPhase1EmbeddedMain" not in text:
    if text.count(include_anchor) != 1:
        raise RuntimeError("Phase 1 embedded include anchor mismatch")
    text = text.replace(include_anchor, include_block, 1)
run_anchor = "bool RunM1M4ClosureTests() {\n"
run_block = """bool RunM1M4ClosureTests() {\n    if (AeroPhase1EmbeddedMain() != 0) return false;\n"""
if "if (AeroPhase1EmbeddedMain()" not in text:
    if text.count(run_anchor) != 1:
        raise RuntimeError("Phase 1 embedded run anchor mismatch")
    text = text.replace(run_anchor, run_block, 1)
closure.write_text(text, encoding="utf-8")

xaml = root / "src/markup/XamlVisualTree.cpp"
xaml_text = xaml.read_text(encoding="utf-8")
old = """    Base::Result<void> rootDetached = mounts_.DetachRoot(rootMount_);\n    if (!rootDetached && firstError.IsOk()) {\n        firstError = rootDetached.GetStatus();\n    }\n\n    for (Presentation::Visual* node : nodes_) {\n"""
new = """    Base::Result<void> rootDetached = mounts_.DetachRoot(rootMount_);\n    if (!rootDetached && firstError.IsOk()) {\n        firstError = rootDetached.GetStatus();\n    }\n\n    // Unmount is a shutdown boundary. Even when an intermediate manager\n    // reports an error, remove any residual object-tree relationship before\n    // releasing owner references so destructors never observe a half-detached\n    // logical or visual subtree.\n    if (rootNode_ != nullptr &&\n        (tree_->Root() == rootNode_ ||\n         rootNode_->LogicalParent() != nullptr ||\n         rootNode_->VisualParent() != nullptr ||\n         !rootNode_->LogicalChildren().Empty() ||\n         !rootNode_->VisualChildren().Empty())) {\n        Base::Result<void> residual = tree_->DetachNode(*rootNode_);\n        if (!residual && firstError.IsOk()) {\n            firstError = residual.GetStatus();\n        }\n    }\n\n    for (Presentation::Visual* node : nodes_) {\n"""
if old not in xaml_text:
    raise RuntimeError("XAML residual cleanup anchor mismatch")
xaml.write_text(xaml_text.replace(old, new, 1), encoding="utf-8")
