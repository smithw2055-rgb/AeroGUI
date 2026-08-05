if(NOT AERO_BUILD_CONFORMANCE)
    return()
endif()

add_executable(aero-conformance
    tools/conformance/main.cpp)
target_link_libraries(aero-conformance PRIVATE
    Aero::Integration)
if(WIN32)
    target_link_libraries(aero-conformance PRIVATE
        d3d11 dxgi d3dcompiler gdi32 opengl32 user32)
endif()
target_include_directories(aero-conformance PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(aero-conformance PRIVATE cxx_std_17)
set_target_properties(aero-conformance PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
aero_apply_compiler_options(aero-conformance)

add_executable(aero-control-gallery-conformance
    tools/control-gallery-conformance/main.cpp)
target_link_libraries(aero-control-gallery-conformance PRIVATE
    Aero::Integration)
target_include_directories(aero-control-gallery-conformance PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(aero-control-gallery-conformance PRIVATE cxx_std_17)
set_target_properties(aero-control-gallery-conformance PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
aero_apply_compiler_options(aero-control-gallery-conformance)
