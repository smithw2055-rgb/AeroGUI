from pathlib import Path

root = Path(__file__).resolve().parents[1]
path = root / "tests/CMakeLists.txt"
text = path.read_text(encoding="utf-8")
anchor = """add_test(NAME AeroXamlModuleSdkTests COMMAND AeroXamlModuleSdkTests)\n"""
addition = """add_test(NAME AeroXamlModuleSdkTests COMMAND AeroXamlModuleSdkTests)\n\nadd_executable(AeroPhase1RuntimeSafetyTests\n    presentation/Phase1RuntimeSafetyTests.cpp)\ntarget_link_libraries(AeroPhase1RuntimeSafetyTests\n    PRIVATE Aero::Markup)\ntarget_compile_features(AeroPhase1RuntimeSafetyTests PRIVATE cxx_std_17)\nset_target_properties(AeroPhase1RuntimeSafetyTests PROPERTIES\n    CXX_STANDARD 17\n    CXX_STANDARD_REQUIRED YES\n    CXX_EXTENSIONS NO)\naero_apply_compiler_options(AeroPhase1RuntimeSafetyTests)\nadd_test(NAME AeroPhase1RuntimeSafetyTests\n    COMMAND AeroPhase1RuntimeSafetyTests)\nset_tests_properties(AeroPhase1RuntimeSafetyTests PROPERTIES\n    LABELS \"phase1;robustness\"\n    TIMEOUT 60)\n"""
if text.count(anchor) != 1:
    raise RuntimeError(f"Phase 1 test anchor count: {text.count(anchor)}")
path.write_text(text.replace(anchor, addition, 1), encoding="utf-8")
