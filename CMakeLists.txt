cmake_minimum_required(VERSION 3.20)

project(AeroGUI
    VERSION 0.1.0
    DESCRIPTION "C++17 retained-mode native GPU XAML UI runtime"
    LANGUAGES C CXX)

find_package(Threads REQUIRED)

option(AERO_BUILD_TESTS "Build AeroGUI unit tests" ON)
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

add_library(AeroBase STATIC
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
    POSITION_INDEPENDENT_CODE ON)

aero_apply_compiler_options(AeroBase)

if(MSVC)
    target_compile_options(AeroBase PUBLIC /wd4324)
endif()

target_compile_definitions(AeroBase
    PUBLIC
        AERO_CXX_STANDARD=17
        $<$<BOOL:${AERO_ENABLE_EXCEPTIONS}>:AERO_EXCEPTIONS_ENABLED=1>
        $<$<NOT:$<BOOL:${AERO_ENABLE_EXCEPTIONS}>>:AERO_EXCEPTIONS_ENABLED=0>
        $<$<BOOL:${AERO_ENABLE_RTTI}>:AERO_RTTI_ENABLED=1>
        $<$<NOT:$<BOOL:${AERO_ENABLE_RTTI}>>:AERO_RTTI_ENABLED=0>)

add_library(AeroCore STATIC
    src/core/Binding.cpp
    src/core/Controls.cpp
    src/core/DependencyProperty.cpp
    src/core/Diagnostics.cpp
    src/core/Dispatcher.cpp
    src/core/EffectiveValueEngine.cpp
    src/core/Input.cpp
    src/core/Layout.cpp
    src/core/ObjectTree.cpp
    src/core/Rendering.cpp
    src/core/Style.cpp
    src/core/TypeRegistry.cpp)

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
    POSITION_INDEPENDENT_CODE ON)

aero_apply_compiler_options(AeroCore)

add_library(AeroMarkup STATIC
    src/markup/XamlBinding.cpp
    src/markup/XamlDynamicResource.cpp
    src/markup/XamlLayout.cpp
    src/markup/XamlStyle.cpp
    src/markup/XamlTypeExtension.cpp
    src/markup/XmlTokenizer.cpp
    src/markup/XamlActivation.cpp
    src/markup/XamlDependencyProperty.cpp
    src/markup/XamlNamesResources.cpp
    src/markup/XamlNodeReader.cpp
    src/markup/XamlObjectWriterCompiled.cpp
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
    POSITION_INDEPENDENT_CODE ON)

aero_apply_compiler_options(AeroMarkup)

add_library(AeroRhi STATIC
    src/rhi/Graphics.cpp
    src/rhi/Rhi.cpp
    src/rhi/Surface.cpp)

add_library(Aero::Rhi ALIAS AeroRhi)

target_include_directories(AeroRhi
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(AeroRhi PUBLIC Aero::Core)
target_compile_features(AeroRhi PUBLIC cxx_std_17)
set_target_properties(AeroRhi PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON)

aero_apply_compiler_options(AeroRhi)

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

    add_library(AeroRhiD3D11 STATIC
        src/rhi/d3d11/D3D11Backend.cpp)
    add_library(Aero::RhiD3D11 ALIAS AeroRhiD3D11)

    target_include_directories(AeroRhiD3D11
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)
    target_link_libraries(AeroRhiD3D11
        PUBLIC Aero::Rhi
        PRIVATE d3d11 dxgi)
    target_compile_features(AeroRhiD3D11 PUBLIC cxx_std_17)
    set_target_properties(AeroRhiD3D11 PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON)
    target_compile_definitions(AeroRhiD3D11 PUBLIC AERO_HAS_D3D11_BACKEND=1)
    aero_apply_compiler_options(AeroRhiD3D11)
else()
    target_compile_definitions(AeroRhi PUBLIC AERO_HAS_D3D11_BACKEND=0)
endif()

if(AERO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
