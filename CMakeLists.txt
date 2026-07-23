cmake_minimum_required(VERSION 3.20)

project(AeroGUI
    VERSION 0.1.0
    DESCRIPTION "C++17 retained-mode native GPU XAML UI runtime"
    LANGUAGES C CXX)

find_package(Threads REQUIRED)

option(AERO_BUILD_TESTS "Build AeroGUI unit tests" ON)
option(AERO_BUILD_SHARED "Build AeroGUI as shared libraries" OFF)
option(AERO_ENABLE_EXCEPTIONS "Enable C++ exceptions in AeroGUI targets" OFF)
option(AERO_ENABLE_RTTI "Enable C++ RTTI in AeroGUI targets" OFF)
option(AERO_WARNINGS_AS_ERRORS "Treat AeroGUI warnings as errors" OFF)
option(AERO_ENABLE_SOKOL_BACKEND
    "Compile a host-supplied sokol_gfx bridge into AeroRhi" OFF)
option(AERO_ENABLE_D3D11_BACKEND
    "Build the native Windows D3D11 graphics backend" ${WIN32})
set(AERO_SOKOL_BRIDGE_SOURCE "" CACHE FILEPATH
    "Private C++ source that populates Aero hosted callbacks with sokol_gfx")
set(AERO_SOKOL_INCLUDE_DIR "" CACHE PATH
    "Optional private include directory containing sokol_gfx.h")

include(cmake/AeroCompilerOptions.cmake)

if(AERO_BUILD_SHARED)
    set(AERO_LIBRARY_TYPE SHARED)
    # Keep test executables beside the DLLs on multi-config Windows builds so
    # CTest exercises the actual shared artifacts without PATH mutations.
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
else()
    set(AERO_LIBRARY_TYPE STATIC)
endif()

add_library(AeroBase ${AERO_LIBRARY_TYPE}
    src/base/Allocator.cpp
    src/base/CAPI.cpp
    src/base/Object.cpp
    src/base/String.cpp
    src/base/Utf8.cpp)

add_library(Aero::Base ALIAS AeroBase)

target_include_directories(AeroBase
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_compile_features(AeroBase PUBLIC cxx_std_17)
set_target_properties(AeroBase PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})

aero_apply_compiler_options(AeroBase)

if(MSVC)
    target_compile_options(AeroBase PUBLIC /wd4324)
endif()

target_compile_definitions(AeroBase
    PUBLIC
        $<$<BOOL:${AERO_BUILD_SHARED}>:AERO_BUILD_SHARED>
        AERO_CXX_STANDARD=17
        $<$<BOOL:${AERO_ENABLE_EXCEPTIONS}>:AERO_EXCEPTIONS_ENABLED=1>
        $<$<NOT:$<BOOL:${AERO_ENABLE_EXCEPTIONS}>>:AERO_EXCEPTIONS_ENABLED=0>
        $<$<BOOL:${AERO_ENABLE_RTTI}>:AERO_RTTI_ENABLED=1>
        $<$<NOT:$<BOOL:${AERO_ENABLE_RTTI}>>:AERO_RTTI_ENABLED=0>
    PRIVATE
        $<$<BOOL:${AERO_BUILD_SHARED}>:AERO_BASE_EXPORTS>)

add_library(AeroCore ${AERO_LIBRARY_TYPE}
    src/core/Binding.cpp
    src/core/Controls.cpp
    src/core/DependencyProperty.cpp
    src/core/Diagnostics.cpp
    src/core/Dispatcher.cpp
    src/core/EffectiveValueEngine.cpp
    src/core/Input.cpp
    src/core/Layout.cpp
    src/core/MemberAccessor.cpp
    src/core/Metadata.cpp
    src/core/MetadataDescriptors.cpp
    src/core/MetadataBehaviorRegistrationStore.cpp
    src/core/MetadataDomain.cpp
    src/core/MetadataValueFacets.cpp
    src/core/MetadataValueRegistrationStore.cpp
    src/core/ObjectTree.cpp
    src/core/Presentation.cpp
    src/core/Rendering.cpp
    src/core/Style.cpp
    src/core/TypeRegistry.cpp
    src/core/Value.cpp)

add_library(Aero::Core ALIAS AeroCore)

target_include_directories(AeroCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(AeroCore
    PUBLIC
        Aero::Base
        Threads::Threads)

target_compile_features(AeroCore PUBLIC cxx_std_17)
set_target_properties(AeroCore PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})

aero_apply_compiler_options(AeroCore)

add_library(AeroMarkup ${AERO_LIBRARY_TYPE}
    src/markup/XamlBinding.cpp
    src/markup/XamlCompiledCache.cpp
    src/markup/XamlDynamicResource.cpp
    src/markup/XamlStyle.cpp
    src/markup/XamlTypeExtension.cpp
    src/markup/XmlTokenizer.cpp
    src/markup/XamlActivation.cpp
    src/markup/XamlDependencyProperty.cpp
    src/markup/XamlNamesResources.cpp
    src/markup/XamlNodeReader.cpp
    src/markup/XamlRuntimeSchema.cpp
    src/markup/XamlObjectWriter.cpp
    src/markup/XamlSchemaContext.cpp
    src/markup/XamlVisualTree.cpp)

add_library(Aero::Markup ALIAS AeroMarkup)

target_include_directories(AeroMarkup
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(AeroMarkup PUBLIC Aero::Core)
target_compile_features(AeroMarkup PUBLIC cxx_std_17)
set_target_properties(AeroMarkup PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})

aero_apply_compiler_options(AeroMarkup)

add_library(AeroRhi ${AERO_LIBRARY_TYPE}
    src/rhi/Graphics.cpp
    src/rhi/Rhi.cpp
    src/rhi/Surface.cpp)

add_library(Aero::Rhi ALIAS AeroRhi)

target_include_directories(AeroRhi
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(AeroRhi PUBLIC Aero::Base)
target_compile_features(AeroRhi PUBLIC cxx_std_17)
set_target_properties(AeroRhi PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})

aero_apply_compiler_options(AeroRhi)

add_library(AeroRender ${AERO_LIBRARY_TYPE}
    src/render/RenderPlanTranslator.cpp)
add_library(Aero::Render ALIAS AeroRender)
target_include_directories(AeroRender
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroRender PUBLIC Aero::Core Aero::Rhi)
target_compile_features(AeroRender PUBLIC cxx_std_17)
set_target_properties(AeroRender PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroRender)

if(AERO_ENABLE_SOKOL_BACKEND)
    if(AERO_SOKOL_BRIDGE_SOURCE STREQUAL "")
        message(FATAL_ERROR
            "AERO_ENABLE_SOKOL_BACKEND requires AERO_SOKOL_BRIDGE_SOURCE")
    endif()
    if(NOT EXISTS "${AERO_SOKOL_BRIDGE_SOURCE}")
        message(FATAL_ERROR
            "AERO_SOKOL_BRIDGE_SOURCE does not exist: ${AERO_SOKOL_BRIDGE_SOURCE}")
    endif()

    target_sources(AeroRhi PRIVATE "${AERO_SOKOL_BRIDGE_SOURCE}")
    if(NOT AERO_SOKOL_INCLUDE_DIR STREQUAL "")
        target_include_directories(
            AeroRhi PRIVATE "${AERO_SOKOL_INCLUDE_DIR}")
    endif()
    target_compile_definitions(AeroRhi PUBLIC AERO_HAS_SOKOL_BACKEND=1)
else()
    target_compile_definitions(AeroRhi PUBLIC AERO_HAS_SOKOL_BACKEND=0)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC diagnoses a reference obtained through a temporary Span view even
    # though the view points into the longer-lived immutable RenderPlan.
    target_compile_options(AeroRhi PRIVATE -Wno-dangling-reference)
endif()

if(AERO_ENABLE_D3D11_BACKEND)
    if(NOT WIN32)
        message(FATAL_ERROR
            "AERO_ENABLE_D3D11_BACKEND is only supported on Windows")
    endif()

    set(_aero_d3d11_render_plan_shader_source
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rhi/d3d11/shaders/RenderPlanRect.hlsl")
    set(_aero_d3d11_render_plan_image_shader_source
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rhi/d3d11/shaders/RenderPlanImage.hlsl")
    set(_aero_d3d11_render_plan_mesh_shader_source
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rhi/d3d11/shaders/RenderPlanMesh.hlsl")
    set(_aero_d3d11_render_plan_glyph_shader_source
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rhi/d3d11/shaders/RenderPlanGlyph.hlsl")
    set(_aero_d3d11_render_plan_max_rectangle_instances 64)
    set(_aero_d3d11_shader_directory
        "${CMAKE_CURRENT_BINARY_DIR}/generated/d3d11-shaders")
    set(_aero_d3d11_render_plan_vs_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RenderPlanVertexShader.hpp")
    set(_aero_d3d11_render_plan_ps_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RenderPlanPixelShader.hpp")
    set(_aero_d3d11_render_plan_image_vs_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RenderPlanImageVertexShader.hpp")
    set(_aero_d3d11_render_plan_image_ps_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RenderPlanImagePixelShader.hpp")
    set(_aero_d3d11_render_plan_mesh_vs_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RenderPlanMeshVertexShader.hpp")
    set(_aero_d3d11_render_plan_mesh_ps_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RenderPlanMeshPixelShader.hpp")
    set(_aero_d3d11_render_plan_glyph_vs_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RenderPlanGlyphVertexShader.hpp")
    set(_aero_d3d11_render_plan_glyph_ps_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RenderPlanGlyphPixelShader.hpp")
    set(_aero_d3d11_fxc_hints
        "$ENV{WindowsSdkDir}bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64")
    file(GLOB _aero_d3d11_fxc_sdk_directories
        LIST_DIRECTORIES true
        "C:/Program Files (x86)/Windows Kits/10/bin/*/x64")
    list(APPEND _aero_d3d11_fxc_hints ${_aero_d3d11_fxc_sdk_directories})
    find_program(AERO_D3D11_FXC_EXECUTABLE
        NAMES fxc.exe
        HINTS ${_aero_d3d11_fxc_hints}
        DOC "Windows SDK fxc executable used to compile Aero D3D11 shaders")
    if(NOT AERO_D3D11_FXC_EXECUTABLE)
        message(FATAL_ERROR
            "AERO_ENABLE_D3D11_BACKEND requires the Windows SDK x64 fxc.exe")
    endif()
    add_custom_command(
        OUTPUT "${_aero_d3d11_render_plan_vs_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T vs_4_0 /E vs_main
            /D AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_plan_max_rectangle_instances}
            /Vn AeroD3D11RenderPlanVertexShader
            /Fh "${_aero_d3d11_render_plan_vs_header}"
            "${_aero_d3d11_render_plan_shader_source}"
        DEPENDS "${_aero_d3d11_render_plan_shader_source}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_aero_d3d11_render_plan_ps_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T ps_4_0 /E ps_main
            /D AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_plan_max_rectangle_instances}
            /Vn AeroD3D11RenderPlanPixelShader
            /Fh "${_aero_d3d11_render_plan_ps_header}"
            "${_aero_d3d11_render_plan_shader_source}"
        DEPENDS "${_aero_d3d11_render_plan_shader_source}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_aero_d3d11_render_plan_image_vs_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T vs_4_0 /E vs_main
            /D AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_plan_max_rectangle_instances}
            /Vn AeroD3D11RenderPlanImageVertexShader
            /Fh "${_aero_d3d11_render_plan_image_vs_header}"
            "${_aero_d3d11_render_plan_image_shader_source}"
        DEPENDS "${_aero_d3d11_render_plan_image_shader_source}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_aero_d3d11_render_plan_image_ps_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T ps_4_0 /E ps_main
            /D AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_plan_max_rectangle_instances}
            /Vn AeroD3D11RenderPlanImagePixelShader
            /Fh "${_aero_d3d11_render_plan_image_ps_header}"
            "${_aero_d3d11_render_plan_image_shader_source}"
        DEPENDS "${_aero_d3d11_render_plan_image_shader_source}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_aero_d3d11_render_plan_mesh_vs_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T vs_4_0 /E vs_main
            /D AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_plan_max_rectangle_instances}
            /Vn AeroD3D11RenderPlanMeshVertexShader
            /Fh "${_aero_d3d11_render_plan_mesh_vs_header}"
            "${_aero_d3d11_render_plan_mesh_shader_source}"
        DEPENDS "${_aero_d3d11_render_plan_mesh_shader_source}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_aero_d3d11_render_plan_mesh_ps_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T ps_4_0 /E ps_main
            /D AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_plan_max_rectangle_instances}
            /Vn AeroD3D11RenderPlanMeshPixelShader
            /Fh "${_aero_d3d11_render_plan_mesh_ps_header}"
            "${_aero_d3d11_render_plan_mesh_shader_source}"
        DEPENDS "${_aero_d3d11_render_plan_mesh_shader_source}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_aero_d3d11_render_plan_glyph_vs_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T vs_4_0 /E vs_main
            /D AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_plan_max_rectangle_instances}
            /Vn AeroD3D11RenderPlanGlyphVertexShader
            /Fh "${_aero_d3d11_render_plan_glyph_vs_header}"
            "${_aero_d3d11_render_plan_glyph_shader_source}"
        DEPENDS "${_aero_d3d11_render_plan_glyph_shader_source}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_aero_d3d11_render_plan_glyph_ps_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T ps_4_0 /E ps_main
            /D AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_plan_max_rectangle_instances}
            /Vn AeroD3D11RenderPlanGlyphPixelShader
            /Fh "${_aero_d3d11_render_plan_glyph_ps_header}"
            "${_aero_d3d11_render_plan_glyph_shader_source}"
        DEPENDS "${_aero_d3d11_render_plan_glyph_shader_source}"
        VERBATIM)
    add_custom_target(AeroD3D11RenderPlanShaders
        DEPENDS
            "${_aero_d3d11_render_plan_vs_header}"
            "${_aero_d3d11_render_plan_ps_header}"
            "${_aero_d3d11_render_plan_image_vs_header}"
            "${_aero_d3d11_render_plan_image_ps_header}"
            "${_aero_d3d11_render_plan_mesh_vs_header}"
            "${_aero_d3d11_render_plan_mesh_ps_header}"
            "${_aero_d3d11_render_plan_glyph_vs_header}"
            "${_aero_d3d11_render_plan_glyph_ps_header}")

    set(_aero_d3d11_backend_fragments
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rhi/d3d11/D3D11BackendPrivate.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rhi/d3d11/D3D11BackendDevice.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rhi/d3d11/D3D11BackendResources.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rhi/d3d11/D3D11BackendCommands1.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rhi/d3d11/D3D11BackendCommands2.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rhi/d3d11/D3D11BackendCommands3.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rhi/d3d11/D3D11BackendReadback.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/rhi/d3d11/D3D11BackendSurface.inc")
    set_source_files_properties(${_aero_d3d11_backend_fragments}
        PROPERTIES HEADER_FILE_ONLY TRUE)
    set_property(SOURCE src/rhi/d3d11/D3D11Backend.cpp APPEND
        PROPERTY OBJECT_DEPENDS "${_aero_d3d11_backend_fragments}")

    add_library(AeroRhiD3D11 ${AERO_LIBRARY_TYPE}
        src/rhi/d3d11/D3D11Backend.cpp
        src/rhi/d3d11/D3D11RenderPlanBackend.cpp
        ${_aero_d3d11_backend_fragments})
    add_library(Aero::RhiD3D11 ALIAS AeroRhiD3D11)
    add_dependencies(AeroRhiD3D11 AeroD3D11RenderPlanShaders)

    target_include_directories(AeroRhiD3D11
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE "${_aero_d3d11_shader_directory}")
    target_link_libraries(AeroRhiD3D11
        PUBLIC Aero::Rhi Aero::Core
        PRIVATE d3d11 dxgi d3dcompiler)
    target_compile_features(AeroRhiD3D11 PUBLIC cxx_std_17)
    set_target_properties(AeroRhiD3D11 PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
        WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
    target_compile_definitions(AeroRhiD3D11
        PRIVATE AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_plan_max_rectangle_instances}
        PUBLIC AERO_HAS_D3D11_BACKEND=1)
    aero_apply_compiler_options(AeroRhiD3D11)
else()
    target_compile_definitions(AeroRhi PUBLIC AERO_HAS_D3D11_BACKEND=0)
endif()

if(AERO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
