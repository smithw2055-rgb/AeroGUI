add_executable(AeroBaseTests
    unit/BaseTests.cpp)

target_link_libraries(AeroBaseTests PRIVATE Aero::Base)
target_compile_features(AeroBaseTests PRIVATE cxx_std_17)
set_target_properties(AeroBaseTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)

aero_apply_compiler_options(AeroBaseTests)

add_test(NAME AeroBaseTests COMMAND AeroBaseTests)
