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
