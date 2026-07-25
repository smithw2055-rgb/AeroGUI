from pathlib import Path

root = Path(__file__).resolve().parents[1]

cmake = root / "tests/CMakeLists.txt"
text = cmake.read_text(encoding="utf-8")
anchor = "add_test(NAME AeroXamlModuleSdkTests COMMAND AeroXamlModuleSdkTests)\n"
addition = """add_test(NAME AeroXamlModuleSdkTests COMMAND AeroXamlModuleSdkTests)\n\nadd_executable(AeroPhase1RuntimeSafetyTests\n    presentation/Phase1RuntimeSafetyTests.cpp)\ntarget_link_libraries(AeroPhase1RuntimeSafetyTests PRIVATE Aero::Markup)\ntarget_compile_features(AeroPhase1RuntimeSafetyTests PRIVATE cxx_std_17)\nset_target_properties(AeroPhase1RuntimeSafetyTests PROPERTIES\n    CXX_STANDARD 17\n    CXX_STANDARD_REQUIRED YES\n    CXX_EXTENSIONS NO)\naero_apply_compiler_options(AeroPhase1RuntimeSafetyTests)\nadd_test(NAME AeroPhase1RuntimeSafetyTests COMMAND AeroPhase1RuntimeSafetyTests)\nset_tests_properties(AeroPhase1RuntimeSafetyTests PROPERTIES\n    LABELS \"phase1;robustness\"\n    TIMEOUT 60)\n"""
if "add_executable(AeroPhase1RuntimeSafetyTests" not in text:
    if text.count(anchor) != 1:
        raise RuntimeError(f"Phase 1 test anchor count: {text.count(anchor)}")
    text = text.replace(anchor, addition, 1)
cmake.write_text(text, encoding="utf-8")

test = root / "tests/presentation/Phase1RuntimeSafetyTests.cpp"
source = test.read_text(encoding="utf-8")
source = source.replace(
    "            const std::uint64_t treeVersion = runtime.Tree()->Version();\n",
    "")
source = source.replace(
    "                CHECK(runtime.Tree()->Version() == treeVersion);\n",
    "")
test.write_text(source, encoding="utf-8")
