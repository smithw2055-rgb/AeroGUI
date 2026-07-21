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
    src/core/DependencyProperty.cpp
    src/core/Dispatcher.cpp
    src/core/EffectiveValueEngine.cpp
    src/core/Layout.cpp
    src/core/ObjectTree.cpp
    src/core/Rendering.cpp
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

add_library(AeroRhi STATIC
    src/rhi/Rhi.cpp)

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

if(AERO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
