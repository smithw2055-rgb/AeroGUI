cmake_minimum_required(VERSION 3.20)

include(cmake/AeroVersions.cmake)

project(AeroGUI
    VERSION ${AERO_VERSION}
    DESCRIPTION "C++17 retained-mode native GPU XAML UI runtime"
    LANGUAGES C CXX)

find_package(Threads REQUIRED)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(AERO_GENERATED_INCLUDE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
file(MAKE_DIRECTORY "${AERO_GENERATED_INCLUDE_DIR}/Aero")
configure_file(
    cmake/AeroVersion.hpp.in
    "${AERO_GENERATED_INCLUDE_DIR}/Aero/Version.hpp"
    @ONLY)
include_directories(BEFORE "${AERO_GENERATED_INCLUDE_DIR}")

option(AERO_BUILD_TESTS "Build AeroGUI unit tests when the test tree is present" OFF)
option(AERO_BUILD_SAMPLES "Build AeroGUI samples when the sample tree is present" OFF)
option(AERO_BUILD_SHARED "Build AeroGUI as shared libraries" OFF)
option(AERO_BUILD_TOOLS "Build AeroGUI offline tools" ON)
option(AERO_BUILD_CONFORMANCE
    "Build the non-installed Aero runtime conformance executable" OFF)
add_custom_target(AeroArchitectureCheck ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DAERO_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckArchitecture.cmake"
    VERBATIM)
add_custom_target(AeroConventionsCheck ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DAERO_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckConventions.cmake"
    VERBATIM)
option(AERO_PRECOMPILE_BUILTIN_THEMES
    "Precompile built-in XAML themes with a host aero-xamlc" ON)
set(AERO_HOST_XAMLC_EXECUTABLE "" CACHE FILEPATH
    "Host aero-xamlc executable used while cross-compiling or when tools are disabled")
set(AERO_HOST_SCHEMA_GEN_EXECUTABLE "" CACHE FILEPATH
    "Host aero-schema-gen executable used to create the built-in schema manifest")
set(AERO_BUILTIN_SCHEMA_MANIFEST "" CACHE FILEPATH
    "Pre-generated built-in .aeroschema file used instead of aero-schema-gen")
option(AERO_ENABLE_EXCEPTIONS "Enable C++ exceptions in AeroGUI targets" OFF)
option(AERO_ENABLE_RTTI "Enable C++ RTTI in AeroGUI targets" OFF)
option(AERO_WARNINGS_AS_ERRORS "Treat AeroGUI warnings as errors" OFF)
option(AERO_WITH_FREETYPE
    "Build the built-in FreeType text provider adapter" ON)
option(AERO_WITH_HARFBUZZ
    "Build the built-in HarfBuzz text shaping adapter" ON)
option(AERO_WITH_EXPAT
    "Build the recommended Expat XML tokenizer adapter" ON)
option(AERO_WITH_MINIAUDIO
    "Build the built-in miniAudio playback provider" ON)
option(AERO_ENABLE_SOKOL_BACKEND
    "Compile a host-supplied sokol_gfx bridge into AeroRendering" OFF)
option(AERO_ENABLE_D3D11_BACKEND
    "Build the native Windows D3D11 graphics backend" ${WIN32})
option(AERO_ENABLE_WGL_SURFACE
    "Build the native Windows WGL surface adapter" ${WIN32})
option(AERO_ENABLE_GLX_SURFACE
    "Build the optional native Linux/X11 GLX surface adapter" OFF)
set(AERO_SOKOL_BRIDGE_SOURCE "" CACHE FILEPATH
    "Private C++ source that populates Aero hosted callbacks with sokol_gfx")
set(AERO_SOKOL_INCLUDE_DIR "" CACHE PATH
    "Optional private include directory containing sokol_gfx.h")
set(AERO_THIRD_PARTY_ROOT "" CACHE PATH
    "Root containing the optional freetype and harfbuzz source directories")
set(AERO_TEXT_TEST_FONT "" CACHE FILEPATH
    "Font used by optional FreeType adapter conformance tests")
set(AERO_TEXT_TEST_CJK_FONT "" CACHE FILEPATH
    "CJK font used by optional HarfBuzz conformance tests")
set(AERO_TEXT_TEST_ARABIC_FONT "" CACHE FILEPATH
    "Arabic font used by optional HarfBuzz conformance tests")

include(cmake/AeroCompilerOptions.cmake)
include(cmake/AeroAddXaml.cmake)
include(cmake/AeroPublicHeaders.cmake)

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
    src/base/ResourceUri.cpp
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

# Public Aero object types are compiled without compiler RTTI. Propagate that
# ABI choice so installed-package consumers do not emit references to typeinfo
# records that the libraries intentionally do not provide.
if(NOT AERO_ENABLE_RTTI)
    if(MSVC)
        target_compile_options(AeroBase INTERFACE /GR-)
    else()
        target_compile_options(AeroBase INTERFACE -fno-rtti)
    endif()
endif()

if(AERO_WITH_MINIAUDIO AND NOT EXISTS
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio/miniaudio.h")
    message(FATAL_ERROR
        "AERO_WITH_MINIAUDIO requires the pinned "
        "third_party/miniaudio/miniaudio.h provider")
endif()

add_library(AeroAudio ${AERO_LIBRARY_TYPE}
    src/audio/Audio.cpp
    src/audio/MiniaudioImplementation.cpp)
add_library(Aero::Audio ALIAS AeroAudio)
target_include_directories(AeroAudio
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio")
target_link_libraries(AeroAudio PUBLIC Aero::Base)
if(UNIX AND NOT APPLE)
    target_link_libraries(AeroAudio PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
endif()
target_compile_definitions(AeroAudio PRIVATE
    AERO_AUDIO_MINIAUDIO=$<BOOL:${AERO_WITH_MINIAUDIO}>)
target_compile_features(AeroAudio PUBLIC cxx_std_17)
set_target_properties(AeroAudio PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroAudio)

add_library(AeroTextObjects OBJECT
    src/text/EditableText.cpp
    src/text/FontManager.cpp
    src/text/GlyphAtlas.cpp
    src/text/TextLayout.cpp
    src/text/TextTypes.cpp
    src/text/UnicodeAnalysis.cpp)
target_include_directories(AeroTextObjects
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroTextObjects PUBLIC Aero::Base)
target_compile_features(AeroTextObjects PUBLIC cxx_std_17)
set_target_properties(AeroTextObjects PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON)
aero_apply_compiler_options(AeroTextObjects)

if(NOT AERO_WITH_FREETYPE OR NOT AERO_WITH_HARFBUZZ)
    message(FATAL_ERROR
        "AeroRuntime requires the built-in FreeType and HarfBuzz text pipeline")
endif()

if(AERO_WITH_FREETYPE)
    if(AERO_THIRD_PARTY_ROOT STREQUAL "")
        set(_aero_repository_third_party
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
        if(EXISTS
                "${_aero_repository_third_party}/freetype/include/ft2build.h"
           AND EXISTS
                "${_aero_repository_third_party}/harfbuzz/CMakeLists.txt")
            set(AERO_THIRD_PARTY_ROOT
                "${_aero_repository_third_party}")
        else()
            get_filename_component(_aero_sibling_third_party
                "${CMAKE_CURRENT_SOURCE_DIR}/../AeroGUI/third_party"
                ABSOLUTE)
            if(EXISTS
                    "${_aero_sibling_third_party}/freetype/include/ft2build.h"
               AND EXISTS
                    "${_aero_sibling_third_party}/harfbuzz/CMakeLists.txt")
                set(AERO_THIRD_PARTY_ROOT
                    "${_aero_sibling_third_party}")
            endif()
        endif()
    endif()
    if(AERO_THIRD_PARTY_ROOT STREQUAL "")
        message(FATAL_ERROR
            "AeroRuntime requires FreeType and HarfBuzz sources. "
            "Set AERO_THIRD_PARTY_ROOT to a directory containing "
            "freetype/ and harfbuzz/, or initialize the repository "
            "submodules.")
    endif()
    if(AERO_TEXT_TEST_FONT STREQUAL "" AND
       EXISTS "${AERO_THIRD_PARTY_ROOT}/harfbuzz/perf/fonts/Roboto-Regular.ttf")
        set(AERO_TEXT_TEST_FONT
            "${AERO_THIRD_PARTY_ROOT}/harfbuzz/perf/fonts/Roboto-Regular.ttf")
    endif()

    set(_aero_freetype_source
        "${CMAKE_CURRENT_SOURCE_DIR}/src/text/freetype/FreeTypeSingleObject.c")
    set(_aero_freetype_root
        "${AERO_THIRD_PARTY_ROOT}/freetype")
    set(_aero_freetype_include
        "${_aero_freetype_root}/include")
    foreach(_aero_required_path IN ITEMS
            "${_aero_freetype_source}"
            "${_aero_freetype_include}/ft2build.h"
            "${_aero_freetype_root}/src/base/ftsystem.c"
            "${_aero_freetype_root}/src/truetype/truetype.c")
        if(NOT EXISTS "${_aero_required_path}")
            message(FATAL_ERROR
                "AERO_WITH_FREETYPE requires: "
                "${_aero_required_path}")
        endif()
    endforeach()

    add_library(freetype STATIC "${_aero_freetype_source}")
    target_include_directories(freetype
        PRIVATE
            "${_aero_freetype_root}"
            "${_aero_freetype_include}"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/text/freetype"
        INTERFACE
            "$<BUILD_INTERFACE:${_aero_freetype_include}>")
    set_target_properties(freetype PROPERTIES
        POSITION_INDEPENDENT_CODE ON)

    add_library(AeroTextFreeTypeObjects OBJECT
        src/text/freetype/FreeTypeAdapter.cpp)
    target_include_directories(AeroTextFreeTypeObjects
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)
    target_link_libraries(AeroTextFreeTypeObjects
        PUBLIC AeroTextObjects
        PRIVATE freetype)
    target_compile_features(AeroTextFreeTypeObjects PUBLIC cxx_std_17)
    set_target_properties(AeroTextFreeTypeObjects PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON)
    aero_apply_compiler_options(AeroTextFreeTypeObjects)
endif()

function(aero_add_vendored_harfbuzz source_directory binary_directory)
    set(HB_HAVE_CAIRO OFF)
    set(HB_HAVE_FREETYPE ON)
    set(HB_HAVE_GRAPHITE2 OFF)
    set(HB_HAVE_GLIB OFF)
    set(HB_HAVE_ICU OFF)
    set(HB_HAVE_GOBJECT OFF)
    set(HB_HAVE_INTROSPECTION OFF)
    set(HB_HAVE_UNISCRIBE OFF)
    set(HB_HAVE_GDI OFF)
    set(HB_HAVE_DIRECTWRITE OFF)
    set(HB_BUILD_UTILS OFF)
    set(HB_BUILD_SUBSET OFF)
    set(HB_BUILD_RASTER OFF)
    set(HB_BUILD_VECTOR OFF)
    set(HB_BUILD_GPU OFF)
    set(SKIP_INSTALL_ALL ON)
    set(BUILD_SHARED_LIBS OFF)
    add_subdirectory(
        "${source_directory}" "${binary_directory}"
        EXCLUDE_FROM_ALL)
    if(TARGET harfbuzz)
        set_target_properties(harfbuzz PROPERTIES
            POSITION_INDEPENDENT_CODE ON)
    endif()
endfunction()

if(AERO_WITH_HARFBUZZ)
    set(_aero_harfbuzz_source
        "${AERO_THIRD_PARTY_ROOT}/harfbuzz")
    if(NOT EXISTS "${_aero_harfbuzz_source}/CMakeLists.txt")
        message(FATAL_ERROR
            "AERO_WITH_HARFBUZZ requires: "
            "${_aero_harfbuzz_source}/CMakeLists.txt")
    endif()
    if(AERO_TEXT_TEST_CJK_FONT STREQUAL "" AND
       EXISTS "${_aero_harfbuzz_source}/test/api/fonts/Mplus1p-Regular.ttf")
        set(AERO_TEXT_TEST_CJK_FONT
            "${_aero_harfbuzz_source}/test/api/fonts/Mplus1p-Regular.ttf")
    endif()
    if(AERO_TEXT_TEST_ARABIC_FONT STREQUAL "" AND
       EXISTS "${_aero_harfbuzz_source}/perf/fonts/Amiri-Regular.ttf")
        set(AERO_TEXT_TEST_ARABIC_FONT
            "${_aero_harfbuzz_source}/perf/fonts/Amiri-Regular.ttf")
    endif()

    aero_add_vendored_harfbuzz(
        "${_aero_harfbuzz_source}"
        "${CMAKE_CURRENT_BINARY_DIR}/third_party/harfbuzz")

    add_library(AeroTextHarfBuzzObjects OBJECT
        src/text/harfbuzz/HarfBuzzAdapter.cpp)
    target_include_directories(AeroTextHarfBuzzObjects
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)
    target_link_libraries(AeroTextHarfBuzzObjects
        PUBLIC AeroTextFreeTypeObjects
        PRIVATE freetype harfbuzz)
    target_compile_features(AeroTextHarfBuzzObjects PUBLIC cxx_std_17)
    set_target_properties(AeroTextHarfBuzzObjects PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON)
    aero_apply_compiler_options(AeroTextHarfBuzzObjects)
endif()

include(cmake/AeroGuiTargets.cmake)
include(cmake/AeroToolsTargets.cmake)

set(_aero_schema_manifest "")
set(_aero_schema_manifest_dependency "")
set(_aero_generated_schema_manifest
    "${CMAKE_CURRENT_BINARY_DIR}/generated/Aero/Aero.aeroschema")
if(NOT "${AERO_BUILTIN_SCHEMA_MANIFEST}" STREQUAL "")
    if(NOT EXISTS "${AERO_BUILTIN_SCHEMA_MANIFEST}")
        message(FATAL_ERROR
            "AERO_BUILTIN_SCHEMA_MANIFEST does not exist: "
            "${AERO_BUILTIN_SCHEMA_MANIFEST}")
    endif()
    set(_aero_schema_manifest "${AERO_BUILTIN_SCHEMA_MANIFEST}")
    set(_aero_schema_manifest_dependency "${AERO_BUILTIN_SCHEMA_MANIFEST}")
elseif(NOT "${AERO_HOST_SCHEMA_GEN_EXECUTABLE}" STREQUAL "")
    if(NOT EXISTS "${AERO_HOST_SCHEMA_GEN_EXECUTABLE}")
        message(FATAL_ERROR
            "AERO_HOST_SCHEMA_GEN_EXECUTABLE does not exist: "
            "${AERO_HOST_SCHEMA_GEN_EXECUTABLE}")
    endif()
    add_custom_command(
        OUTPUT "${_aero_generated_schema_manifest}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/generated/Aero"
        COMMAND "${AERO_HOST_SCHEMA_GEN_EXECUTABLE}"
            "${_aero_generated_schema_manifest}"
        DEPENDS "${AERO_HOST_SCHEMA_GEN_EXECUTABLE}"
        VERBATIM)
    add_custom_target(AeroBuiltInSchema ALL
        DEPENDS "${_aero_generated_schema_manifest}")
    set(_aero_schema_manifest "${_aero_generated_schema_manifest}")
    set(_aero_schema_manifest_dependency AeroBuiltInSchema)
elseif(TARGET aero-schema-gen AND NOT CMAKE_CROSSCOMPILING)
    add_custom_command(
        OUTPUT "${_aero_generated_schema_manifest}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/generated/Aero"
        COMMAND "$<TARGET_FILE:aero-schema-gen>"
            "${_aero_generated_schema_manifest}"
        DEPENDS aero-schema-gen
        VERBATIM)
    add_custom_target(AeroBuiltInSchema ALL
        DEPENDS "${_aero_generated_schema_manifest}")
    set(_aero_schema_manifest "${_aero_generated_schema_manifest}")
    set(_aero_schema_manifest_dependency AeroBuiltInSchema)
endif()

set(_aero_xaml_schema_arguments "")
if(NOT _aero_schema_manifest STREQUAL "")
    set(_aero_xaml_schema_arguments
        --schema "${_aero_schema_manifest}")
endif()

set(_aero_xamlc_command "")
set(_aero_xamlc_dependency "")
if(AERO_PRECOMPILE_BUILTIN_THEMES)
    if(NOT "${AERO_HOST_XAMLC_EXECUTABLE}" STREQUAL "")
        if(NOT EXISTS "${AERO_HOST_XAMLC_EXECUTABLE}")
            message(FATAL_ERROR
                "AERO_HOST_XAMLC_EXECUTABLE does not exist: "
                "${AERO_HOST_XAMLC_EXECUTABLE}")
        endif()
        set(_aero_xamlc_command "${AERO_HOST_XAMLC_EXECUTABLE}")
        set(_aero_xamlc_dependency "${AERO_HOST_XAMLC_EXECUTABLE}")
    elseif(CMAKE_CROSSCOMPILING)
        message(STATUS
            "Cross-compiling without AERO_HOST_XAMLC_EXECUTABLE; "
            "built-in themes will use embedded source fallback")
        set(AERO_PRECOMPILE_BUILTIN_THEMES OFF)
    elseif(TARGET aero-xamlc)
        set(_aero_xamlc_command "$<TARGET_FILE:aero-xamlc>")
        set(_aero_xamlc_dependency aero-xamlc)
    else()
        message(STATUS
            "No host/in-tree xamlc is available; built-in themes will use "
            "the embedded source fallback")
        set(AERO_PRECOMPILE_BUILTIN_THEMES OFF)
    endif()
endif()

set(_aero_compiled_theme_directory
    "${CMAKE_CURRENT_BINARY_DIR}/generated/Aero")
set(_aero_light_theme_compiled
    "${_aero_compiled_theme_directory}/Light.axb")
set(_aero_dark_theme_compiled
    "${_aero_compiled_theme_directory}/Dark.axb")
set(_aero_generic_theme_compiled
    "${_aero_compiled_theme_directory}/Generic.axb")
set(_aero_generated_theme_header
    "${_aero_compiled_theme_directory}/BuiltinThemes.generated.hpp")
if(AERO_PRECOMPILE_BUILTIN_THEMES)
    add_custom_command(
        OUTPUT
            "${_aero_light_theme_compiled}"
            "${_aero_dark_theme_compiled}"
            "${_aero_generic_theme_compiled}"
            "${_aero_generated_theme_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_compiled_theme_directory}"
        COMMAND "${_aero_xamlc_command}"
            ${_aero_xaml_schema_arguments}
            --strip-source-map
            --origin
            "pack://application:,,,/Aero.Themes;component/Light.xaml"
            "${CMAKE_CURRENT_SOURCE_DIR}/themes/Light.xaml"
            "${_aero_light_theme_compiled}"
        COMMAND "${_aero_xamlc_command}"
            ${_aero_xaml_schema_arguments}
            --strip-source-map
            --origin
            "pack://application:,,,/Aero.Themes;component/Dark.xaml"
            "${CMAKE_CURRENT_SOURCE_DIR}/themes/Dark.xaml"
            "${_aero_dark_theme_compiled}"
        COMMAND "${_aero_xamlc_command}"
            ${_aero_xaml_schema_arguments}
            --strip-source-map
            --origin
            "pack://application:,,,/Aero.Themes;component/Generic.xaml"
            "${CMAKE_CURRENT_SOURCE_DIR}/themes/Generic.xaml"
            "${_aero_generic_theme_compiled}"
        COMMAND "${CMAKE_COMMAND}"
            "-DOUTPUT=${_aero_generated_theme_header}"
            "-DLIGHT_COMPILED=${_aero_light_theme_compiled}"
            "-DDARK_COMPILED=${_aero_dark_theme_compiled}"
            "-DGENERIC_COMPILED=${_aero_generic_theme_compiled}"
            "-DLIGHT_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/Light.xaml"
            "-DDARK_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/Dark.xaml"
            "-DGENERIC_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/Generic.xaml"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedXamlThemes.cmake"
        DEPENDS
            ${_aero_xamlc_dependency}
            ${_aero_schema_manifest_dependency}
            "${CMAKE_CURRENT_SOURCE_DIR}/themes/Light.xaml"
            "${CMAKE_CURRENT_SOURCE_DIR}/themes/Dark.xaml"
            "${CMAKE_CURRENT_SOURCE_DIR}/themes/Generic.xaml"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedXamlThemes.cmake"
        VERBATIM)
else()
    add_custom_command(
        OUTPUT "${_aero_generated_theme_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_compiled_theme_directory}"
        COMMAND "${CMAKE_COMMAND}"
            "-DOUTPUT=${_aero_generated_theme_header}"
            "-DLIGHT_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/Light.xaml"
            "-DDARK_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/Dark.xaml"
            "-DGENERIC_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/Generic.xaml"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedXamlThemes.cmake"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/themes/Light.xaml"
            "${CMAKE_CURRENT_SOURCE_DIR}/themes/Dark.xaml"
            "${CMAKE_CURRENT_SOURCE_DIR}/themes/Generic.xaml"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedXamlThemes.cmake"
        VERBATIM)
endif()

if(AERO_PRECOMPILE_BUILTIN_THEMES)
    add_custom_target(AeroCompiledThemes
        DEPENDS
            "${_aero_light_theme_compiled}"
            "${_aero_dark_theme_compiled}"
            "${_aero_generic_theme_compiled}"
            "${_aero_generated_theme_header}")
else()
    add_custom_target(AeroCompiledThemes
        DEPENDS "${_aero_generated_theme_header}")
endif()

include(cmake/AeroGuiCompositionTargets.cmake)
include(cmake/AeroRenderingTargets.cmake)
include(cmake/AeroProductTargets.cmake)
include(cmake/AeroConformanceTargets.cmake)
include(cmake/AeroInstall.cmake)

if(AERO_BUILD_TESTS)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        message(FATAL_ERROR
            "AERO_BUILD_TESTS=ON but this source distribution has no tests tree")
    endif()
    enable_testing()
    add_subdirectory(tests)
endif()

if(AERO_BUILD_SAMPLES)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/samples/CMakeLists.txt")
        message(FATAL_ERROR
            "AERO_BUILD_SAMPLES=ON but this source distribution has no samples tree")
    endif()
    add_subdirectory(samples)
endif()
