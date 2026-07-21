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

add_executable(AeroContainerTests
    unit/ContainerTests.cpp)

target_link_libraries(AeroContainerTests PRIVATE Aero::Base)
target_compile_features(AeroContainerTests PRIVATE cxx_std_17)
set_target_properties(AeroContainerTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)

aero_apply_compiler_options(AeroContainerTests)
add_test(NAME AeroContainerTests COMMAND AeroContainerTests)

add_executable(AeroCoreTests
    unit/TypeRegistryTests.cpp)

target_link_libraries(AeroCoreTests PRIVATE Aero::Core)
target_compile_features(AeroCoreTests PRIVATE cxx_std_17)
set_target_properties(AeroCoreTests PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)

aero_apply_compiler_options(AeroCoreTests)
add_test(NAME AeroCoreTests COMMAND AeroCoreTests)

add_executable(AeroCAbiSmoke
    abi/CAbiSmoke.c)

target_link_libraries(AeroCAbiSmoke PRIVATE Aero::Base)
set_target_properties(AeroCAbiSmoke PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    LINKER_LANGUAGE CXX)

if(MSVC)
    target_compile_options(AeroCAbiSmoke PRIVATE
        /W4
        /utf-8
        $<$<BOOL:${AERO_WARNINGS_AS_ERRORS}>:/WX>)
else()
    target_compile_options(AeroCAbiSmoke PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        $<$<BOOL:${AERO_WARNINGS_AS_ERRORS}>:-Werror>)
endif()

add_test(NAME AeroCAbiSmoke COMMAND AeroCAbiSmoke)
