add_executable(AeroFrameworkConformanceTests
    FrameworkConformanceTests.cpp)
target_link_libraries(AeroFrameworkConformanceTests
    PRIVATE
        Aero::Gui)
target_compile_features(AeroFrameworkConformanceTests
    PRIVATE cxx_std_17)
set_target_properties(AeroFrameworkConformanceTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroFrameworkConformanceTests)

add_test(
    NAME AeroFrameworkConformanceTests
    COMMAND AeroFrameworkConformanceTests)
set_tests_properties(AeroFrameworkConformanceTests PROPERTIES
    LABELS "conformance;framework"
    TIMEOUT 60)

add_test(
    NAME AeroArchitectureChecks
    COMMAND "${CMAKE_COMMAND}"
        "-DAERO_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_SOURCE_DIR}/cmake/CheckArchitecture.cmake")
set_tests_properties(AeroArchitectureChecks PROPERTIES
    LABELS "architecture"
    TIMEOUT 30)
