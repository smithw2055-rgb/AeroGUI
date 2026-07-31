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

option(AERO_BUILD_TESTS "Build AeroGUI unit tests" ON)
option(AERO_BUILD_SAMPLES "Build AeroGUI sample applications" ON)
option(AERO_BUILD_SHARED "Build AeroGUI as shared libraries" OFF)
option(AERO_BUILD_TOOLS "Build AeroGUI offline tools" ON)
add_custom_target(AeroArchitectureCheck ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DAERO_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckArchitecture.cmake"
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
    "Compile a host-supplied sokol_gfx bridge into AeroRhi" OFF)
option(AERO_ENABLE_D3D11_BACKEND
    "Build the native Windows D3D11 graphics backend" ${WIN32})
option(AERO_ENABLE_WGL_SURFACE
    "Build the native Windows WGL surface adapter" ${WIN32})
set(_aero_glx_default OFF)
if(UNIX AND NOT APPLE)
    set(_aero_glx_default ON)
endif()
option(AERO_ENABLE_GLX_SURFACE
    "Build the native Linux/X11 GLX surface adapter"
    ${_aero_glx_default})
unset(_aero_glx_default)
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

if(AERO_WITH_MINIAUDIO AND NOT EXISTS
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio/miniaudio.h")
    message(FATAL_ERROR
        "AERO_WITH_MINIAUDIO requires the pinned "
        "third_party/miniaudio/miniaudio.h provider")
endif()

add_library(AeroAudio ${AERO_LIBRARY_TYPE}
    src/audio/Audio.cpp)
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

add_library(AeroPlatform ${AERO_LIBRARY_TYPE}
    src/platform/Clipboard.cpp
    src/platform/Ime.cpp)

add_library(Aero::Platform ALIAS AeroPlatform)

target_include_directories(AeroPlatform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroPlatform PUBLIC Aero::Base)
target_compile_features(AeroPlatform PUBLIC cxx_std_17)
set_target_properties(AeroPlatform PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroPlatform)
if(WIN32)
    target_link_libraries(AeroPlatform PRIVATE imm32)
endif()

add_library(AeroText ${AERO_LIBRARY_TYPE}
    src/text/EditableText.cpp
    src/text/FontManager.cpp
    src/text/GlyphAtlas.cpp
    src/text/TextLayout.cpp
    src/text/TextTypes.cpp)

add_library(Aero::Text ALIAS AeroText)

target_include_directories(AeroText
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(AeroText PUBLIC Aero::Base)
target_compile_features(AeroText PUBLIC cxx_std_17)
set_target_properties(AeroText PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})

aero_apply_compiler_options(AeroText)

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

    add_library(AeroTextFreeType ${AERO_LIBRARY_TYPE}
        src/text/freetype/FreeTypeAdapter.cpp)
    add_library(Aero::TextFreeType ALIAS AeroTextFreeType)
    target_include_directories(AeroTextFreeType
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)
    target_link_libraries(AeroTextFreeType
        PUBLIC Aero::Text
        PRIVATE freetype)
    target_compile_features(AeroTextFreeType PUBLIC cxx_std_17)
    set_target_properties(AeroTextFreeType PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
        WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
    aero_apply_compiler_options(AeroTextFreeType)
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

    add_library(AeroTextHarfBuzz ${AERO_LIBRARY_TYPE}
        src/text/harfbuzz/HarfBuzzAdapter.cpp)
    add_library(Aero::TextHarfBuzz ALIAS AeroTextHarfBuzz)
    target_include_directories(AeroTextHarfBuzz
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)
    target_link_libraries(AeroTextHarfBuzz
        PUBLIC Aero::TextFreeType
        PRIVATE freetype harfbuzz)
    target_compile_features(AeroTextHarfBuzz PUBLIC cxx_std_17)
    set_target_properties(AeroTextHarfBuzz PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
        WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
    aero_apply_compiler_options(AeroTextHarfBuzz)
endif()

add_library(AeroCore ${AERO_LIBRARY_TYPE}
    src/core/metadata/BindingPath.cpp
    src/core/property/DependencyProperty.cpp
    src/core/Diagnostics.cpp
    src/core/Dispatcher.cpp
    src/core/property/EffectiveValueEngine.cpp
    src/core/metadata/RoutedEventCatalog.cpp
    src/core/metadata/CoreMetadata.cpp
    src/core/metadata/MetadataAuthoring.cpp
    src/core/metadata/MetadataContext.cpp
    src/core/metadata/MetadataRuntimeData.cpp
    src/core/metadata/MetadataBehaviorRegistrationStore.cpp
    src/core/metadata/MetadataDomain.cpp
    src/core/metadata/MetadataRegistrationValues.cpp
    src/core/metadata/MetadataRuntime.cpp
    src/core/metadata/MetadataValueFacets.cpp
    src/core/metadata/MetadataValueRegistrationStore.cpp
    src/core/metadata/ValueConversion.cpp
    src/core/ObjectServices.cpp
    src/core/metadata/TypeRegistry.cpp
    src/core/metadata/Value.cpp
    src/media/AnimationRuntime.cpp
    src/media/Animation.cpp
    src/render/BatchPlanner.cpp
    src/data/Binding.cpp
    src/media/Brushes.cpp
    src/input/Commands.cpp
    src/media/Effects.cpp
    src/input/Input.cpp
    src/media/Images.cpp
    src/ui/Layout.cpp
    src/ui/Metadata.cpp
    src/ui/MountService.cpp
    src/ui/ObjectTree.cpp
    src/render/QueuedRenderBackend.cpp
    src/ui/Resources.cpp
    src/render/Rendering.cpp
    src/media/Transforms.cpp
    src/ui/VisualTreeMount.cpp
    src/ui/Style.cpp)

add_library(Aero::Core ALIAS AeroCore)

target_include_directories(AeroCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_link_libraries(AeroCore
    PUBLIC
        Aero::Base
        Threads::Threads)

target_compile_definitions(
    AeroCore PUBLIC AERO_UI_RESOURCE_MODEL=2)
target_compile_features(AeroCore PUBLIC cxx_std_17)
set_target_properties(AeroCore PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})

aero_apply_compiler_options(AeroCore)



# Application object model and metadata are shared by the default App
# product and the built-in schema catalog without pulling in native hosts.
add_library(AeroAppModel ${AERO_LIBRARY_TYPE}
    src/app/Application.cpp
    src/app/Metadata.cpp)
add_library(Aero::DetailAppModel ALIAS AeroAppModel)
target_include_directories(AeroAppModel
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroAppModel PUBLIC Aero::Core)
target_compile_features(AeroAppModel PUBLIC cxx_std_17)
set_target_properties(AeroAppModel PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroAppModel)

add_library(AeroControls ${AERO_LIBRARY_TYPE}
    src/controls/Bars.cpp
    src/controls/Buttons.cpp
    src/controls/ContentControls.cpp
    src/controls/Controls.cpp
    src/controls/DocumentInteraction.cpp
    src/controls/Documents.cpp
    src/controls/Images.cpp
    src/controls/Items.cpp
    src/controls/ListView.cpp
    src/controls/Menus.cpp
    src/controls/Metadata.cpp
    src/controls/Path.cpp
    src/controls/Scroll.cpp
    src/controls/Selection.cpp
    src/controls/Shapes.cpp
    src/controls/TextBox.cpp
    src/controls/Templates.cpp
    src/controls/Trees.cpp
    src/controls/Virtualization.cpp
    src/controls/VisualStates.cpp)

add_library(Aero::Controls ALIAS AeroControls)

target_include_directories(AeroControls
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AeroControls
    PUBLIC
        Aero::Core
        Aero::Platform
        Aero::Text)
target_compile_definitions(
    AeroControls PUBLIC AERO_CONTROLS_TEMPLATE_ABI=10)
target_compile_features(AeroControls PUBLIC cxx_std_17)
set_target_properties(AeroControls PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroControls)

add_library(AeroInspector ${AERO_LIBRARY_TYPE}
    src/diagnostics/Inspector.cpp)
add_library(Aero::Inspector ALIAS
    AeroInspector)
target_include_directories(AeroInspector
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AeroInspector
    PUBLIC Aero::Controls)
target_compile_features(AeroInspector
    PUBLIC cxx_std_17)
set_target_properties(AeroInspector PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroInspector)

add_library(AeroMarkupKernel ${AERO_LIBRARY_TYPE}
    src/markup/CompiledCache.cpp
    src/markup/CompiledDocument.cpp
    src/markup/XmlTokenizer.cpp
    src/markup/NodeReader.cpp)
add_library(Aero::MarkupKernel ALIAS AeroMarkupKernel)

target_include_directories(AeroMarkupKernel
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroMarkupKernel PUBLIC Aero::Core)
target_compile_features(AeroMarkupKernel PUBLIC cxx_std_17)
set_target_properties(AeroMarkupKernel PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroMarkupKernel)

add_library(AeroMarkup ${AERO_LIBRARY_TYPE}
    src/markup/BindingExtension.cpp
    src/markup/CompiledSchema.cpp
    src/markup/Metadata.cpp
    src/markup/FacetStore.cpp
    src/markup/SchemaManifest.cpp
    src/markup/DynamicResourceExtension.cpp
    src/markup/StyleSupport.cpp
    src/markup/TemplateSupport.cpp
    src/markup/TemplateBindingExtension.cpp
    src/markup/TemplateCompiler.cpp
    src/markup/TypeExtension.cpp
    src/markup/StaticExtension.cpp
    src/markup/Scopes.cpp
    src/markup/LoaderResult.cpp
    src/markup/ObjectWriterState.cpp
    src/markup/DocumentCache.cpp
    src/markup/Loader.cpp
    src/markup/Resources.cpp
    src/markup/Schema.cpp
    src/markup/ObjectWriter.cpp
    src/markup/SchemaServices.cpp)

if(AERO_WITH_EXPAT)
    set(_aero_vendored_expat_target "")
    if(AERO_THIRD_PARTY_ROOT STREQUAL "")
        get_filename_component(_aero_sibling_third_party
            "${CMAKE_CURRENT_SOURCE_DIR}/../AeroGUI/third_party"
            ABSOLUTE)
        if(EXISTS
           "${_aero_sibling_third_party}/expat/expat/CMakeLists.txt")
            set(AERO_THIRD_PARTY_ROOT
                "${_aero_sibling_third_party}")
        endif()
    endif()
    set(_aero_expat_source
        "${AERO_THIRD_PARTY_ROOT}/expat/expat")
    if(EXISTS "${_aero_expat_source}/CMakeLists.txt")
        set(EXPAT_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
        set(EXPAT_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        add_subdirectory(
            "${_aero_expat_source}"
            "${CMAKE_CURRENT_BINARY_DIR}/third_party/expat"
            EXCLUDE_FROM_ALL)
        set(_aero_expat_target expat::expat)
        set(_aero_vendored_expat_target expat)
    else()
        find_package(EXPAT QUIET)
        if(TARGET EXPAT::EXPAT)
            set(_aero_expat_target EXPAT::EXPAT)
        else()
            message(FATAL_ERROR
                "AERO_WITH_EXPAT requires Expat or "
                "${_aero_expat_source}")
        endif()
    endif()
    target_sources(AeroMarkupKernel PRIVATE
        src/markup/ExpatXmlTokenizer.cpp)
    if(_aero_vendored_expat_target)
        target_link_libraries(AeroMarkupKernel PRIVATE
            $<BUILD_INTERFACE:${_aero_expat_target}>
            $<INSTALL_INTERFACE:Aero::RuntimeDetailExpat>)
    else()
        target_link_libraries(AeroMarkupKernel PRIVATE
            $<BUILD_INTERFACE:${_aero_expat_target}>
            $<INSTALL_INTERFACE:EXPAT::EXPAT>)
    endif()
endif()

add_library(Aero::Markup ALIAS AeroMarkup)

target_include_directories(AeroMarkup
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_compile_definitions(
    AeroMarkupKernel
    PUBLIC
        $<$<BOOL:${AERO_WITH_EXPAT}>:AERO_WITH_EXPAT=1>
        $<$<NOT:$<BOOL:${AERO_WITH_EXPAT}>>:AERO_WITH_EXPAT=0>)
target_link_libraries(AeroMarkup
    PUBLIC Aero::MarkupKernel Aero::Controls)
target_compile_definitions(
    AeroMarkup PRIVATE AERO_MARKUP_UI_RESOURCES=1)
target_compile_features(AeroMarkup PUBLIC cxx_std_17)
set_target_properties(AeroMarkup PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})

aero_apply_compiler_options(AeroMarkup)

add_library(AeroModuleCatalog ${AERO_LIBRARY_TYPE}
    src/Module.cpp
    src/BuiltinModules.cpp
    src/SchemaBundle.cpp)
add_library(Aero::DetailModuleCatalog ALIAS AeroModuleCatalog)
target_include_directories(AeroModuleCatalog
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroModuleCatalog
    PUBLIC Aero::Markup
    PRIVATE Aero::DetailAppModel)
target_compile_features(AeroModuleCatalog PUBLIC cxx_std_17)
set_target_properties(AeroModuleCatalog PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON)
aero_apply_compiler_options(AeroModuleCatalog)

if(AERO_BUILD_TOOLS)
    add_executable(aero-schema-gen tools/schema-gen/main.cpp)
    add_executable(Aero::schema-gen ALIAS aero-schema-gen)
    target_link_libraries(aero-schema-gen PRIVATE AeroModuleCatalog)
    target_include_directories(aero-schema-gen PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(aero-schema-gen PRIVATE cxx_std_17)
    set_target_properties(aero-schema-gen PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    aero_apply_compiler_options(aero-schema-gen)

    add_executable(aero-xamlc tools/xamlc/main.cpp)
    add_executable(Aero::xamlc ALIAS aero-xamlc)
    target_link_libraries(aero-xamlc PRIVATE
        AeroModuleCatalog
        Aero::Audio)
    target_include_directories(aero-xamlc PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(aero-xamlc PRIVATE cxx_std_17)
    set_target_properties(aero-xamlc PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    aero_apply_compiler_options(aero-xamlc)
endif()

set(_aero_schema_manifest "")
set(_aero_schema_manifest_dependency "")
set(_aero_generated_schema_manifest
    "${CMAKE_CURRENT_BINARY_DIR}/generated/Aero/Aero.aeroschema")
if(NOT AERO_BUILTIN_SCHEMA_MANIFEST STREQUAL "")
    if(NOT EXISTS "${AERO_BUILTIN_SCHEMA_MANIFEST}")
        message(FATAL_ERROR
            "AERO_BUILTIN_SCHEMA_MANIFEST does not exist: "
            "${AERO_BUILTIN_SCHEMA_MANIFEST}")
    endif()
    set(_aero_schema_manifest "${AERO_BUILTIN_SCHEMA_MANIFEST}")
    set(_aero_schema_manifest_dependency "${AERO_BUILTIN_SCHEMA_MANIFEST}")
elseif(NOT AERO_HOST_SCHEMA_GEN_EXECUTABLE STREQUAL "")
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
    if(NOT AERO_HOST_XAMLC_EXECUTABLE STREQUAL "")
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
        message(FATAL_ERROR
            "AERO_PRECOMPILE_BUILTIN_THEMES requires AERO_BUILD_TOOLS=ON "
            "or AERO_HOST_XAMLC_EXECUTABLE")
    endif()
endif()

set(_aero_compiled_theme_directory
    "${CMAKE_CURRENT_BINARY_DIR}/generated/Aero")
set(_aero_light_theme_compiled
    "${_aero_compiled_theme_directory}/Light.axir")
set(_aero_dark_theme_compiled
    "${_aero_compiled_theme_directory}/Dark.axir")
set(_aero_generic_theme_compiled
    "${_aero_compiled_theme_directory}/Generic.axir")
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
            --origin
            "pack://application:,,,/Aero.Themes;component/Light.xaml"
            "${CMAKE_CURRENT_SOURCE_DIR}/themes/Light.xaml"
            "${_aero_light_theme_compiled}"
        COMMAND "${_aero_xamlc_command}"
            ${_aero_xaml_schema_arguments}
            --origin
            "pack://application:,,,/Aero.Themes;component/Dark.xaml"
            "${CMAKE_CURRENT_SOURCE_DIR}/themes/Dark.xaml"
            "${_aero_dark_theme_compiled}"
        COMMAND "${_aero_xamlc_command}"
            ${_aero_xaml_schema_arguments}
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

add_library(AeroRuntime ${AERO_LIBRARY_TYPE}
    src/RuntimeEnvironment.cpp
    src/ViewRuntime.cpp
    src/IntegrationReloadCoordinator.cpp
    src/integration/RenderEndpoint.cpp
    src/RuntimeSafety.cpp
    src/UiDocument.cpp
    src/runtime/RuntimeUiServices.cpp
    src/runtime/ImageRuntime.cpp
    src/runtime/TextRuntime.cpp
    "${_aero_generated_theme_header}")
add_library(Aero::Runtime ALIAS AeroRuntime)
add_dependencies(AeroRuntime AeroCompiledThemes)
target_include_directories(AeroRuntime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb"
        "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(AeroRuntime
    PRIVATE
        AeroModuleCatalog
        Aero::Platform
        Aero::TextHarfBuzz)
target_compile_features(AeroRuntime PUBLIC cxx_std_17)
set_target_properties(AeroRuntime PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroRuntime)

# Product-facing SDK aggregates. Gui is the retained WPF/XAML class library;
# App and Integration layer lifetime and native hosting above it.
add_library(AeroGui INTERFACE)
add_library(Aero::Gui ALIAS AeroGui)
target_link_libraries(AeroGui INTERFACE Aero::Markup)

add_library(AeroModuleSdk INTERFACE)
add_library(Aero::ModuleSdk ALIAS AeroModuleSdk)
target_link_libraries(AeroModuleSdk INTERFACE Aero::Gui)

add_library(AeroIntegrationSdk INTERFACE)
add_library(Aero::Integration ALIAS AeroIntegrationSdk)
add_library(Aero::IntegrationSdk ALIAS AeroIntegrationSdk)
target_link_libraries(AeroIntegrationSdk INTERFACE
    Aero::Runtime AeroIntegration)

# Compile-only consumers keep the product entry points independently
# consumable and catch accidental reverse header dependencies.
add_library(AeroGuiHeaderConsumer OBJECT
    tools/sdk-consumers/GuiConsumer.cpp)
target_link_libraries(AeroGuiHeaderConsumer PRIVATE Aero::Gui)
aero_apply_compiler_options(AeroGuiHeaderConsumer)

add_library(AeroModuleHeaderConsumer OBJECT
    tools/sdk-consumers/ModuleConsumer.cpp)
target_link_libraries(AeroModuleHeaderConsumer PRIVATE Aero::ModuleSdk)
aero_apply_compiler_options(AeroModuleHeaderConsumer)

add_library(AeroIntegrationHeaderConsumer OBJECT
    tools/sdk-consumers/IntegrationConsumer.cpp)
target_link_libraries(
    AeroIntegrationHeaderConsumer PRIVATE Aero::Integration)
aero_apply_compiler_options(AeroIntegrationHeaderConsumer)

add_library(AeroD3D11IntegrationHeaderConsumer OBJECT
    tools/sdk-consumers/D3D11IntegrationConsumer.cpp)
target_link_libraries(
    AeroD3D11IntegrationHeaderConsumer PRIVATE Aero::Integration)
aero_apply_compiler_options(
    AeroD3D11IntegrationHeaderConsumer)

add_library(AeroOpenGL33IntegrationHeaderConsumer OBJECT
    tools/sdk-consumers/OpenGL33IntegrationConsumer.cpp)
target_link_libraries(
    AeroOpenGL33IntegrationHeaderConsumer PRIVATE Aero::Integration)
aero_apply_compiler_options(
    AeroOpenGL33IntegrationHeaderConsumer)

add_library(AeroHostedGraphicsHeaderConsumer OBJECT
    tools/sdk-consumers/HostedGraphicsConsumer.cpp)
target_link_libraries(
    AeroHostedGraphicsHeaderConsumer PRIVATE Aero::Integration)
aero_apply_compiler_options(
    AeroHostedGraphicsHeaderConsumer)

set(AERO_DEFAULT_THEME_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/themes/Generic.xaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/themes/Light.xaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/themes/Dark.xaml")
add_custom_target(AeroDefaultThemes ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:AeroMarkup>/themes"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        ${AERO_DEFAULT_THEME_FILES}
        "$<TARGET_FILE_DIR:AeroMarkup>/themes"
    DEPENDS ${AERO_DEFAULT_THEME_FILES}
    VERBATIM)
add_dependencies(AeroMarkup AeroDefaultThemes)

add_library(AeroRhi ${AERO_LIBRARY_TYPE}
    src/rhi/Graphics.cpp
    src/rhi/Rhi.cpp
    src/rhi/Surface.cpp)

add_library(Aero::DetailRhi ALIAS AeroRhi)

target_include_directories(AeroRhi
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(AeroRhi PUBLIC Aero::Base)
target_compile_features(AeroRhi PUBLIC cxx_std_17)
set_target_properties(AeroRhi PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})

aero_apply_compiler_options(AeroRhi)

add_library(AeroRhiOpenGL33 ${AERO_LIBRARY_TYPE}
    src/rhi/opengl33/OpenGL33Backend.cpp
    src/rhi/opengl33/OpenGL33Context.cpp
    src/rhi/opengl33/OpenGL33StateCache.cpp)
add_library(Aero::DetailRhiOpenGL33 ALIAS AeroRhiOpenGL33)
target_include_directories(AeroRhiOpenGL33
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(
    AeroRhiOpenGL33 PUBLIC Aero::DetailRhi)
target_compile_features(AeroRhiOpenGL33 PUBLIC cxx_std_17)
target_compile_definitions(
    AeroRhiOpenGL33 PUBLIC AERO_HAS_OPENGL33_BACKEND=1)
set_target_properties(AeroRhiOpenGL33 PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroRhiOpenGL33)

if(AERO_ENABLE_WGL_SURFACE)
    if(NOT WIN32)
        message(FATAL_ERROR
            "AERO_ENABLE_WGL_SURFACE is only supported on Windows")
    endif()
    add_library(AeroRhiWGL ${AERO_LIBRARY_TYPE}
        src/rhi/wgl/WglSurface.cpp)
    add_library(Aero::DetailRhiWGL ALIAS AeroRhiWGL)
    target_include_directories(AeroRhiWGL
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(AeroRhiWGL
        PUBLIC Aero::DetailRhiOpenGL33
        PRIVATE gdi32 opengl32 user32)
    target_compile_features(AeroRhiWGL PUBLIC cxx_std_17)
    target_compile_definitions(
        AeroRhiWGL PUBLIC AERO_HAS_WGL_SURFACE=1)
    set_target_properties(AeroRhiWGL PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
        WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
    aero_apply_compiler_options(AeroRhiWGL)
endif()

if(AERO_ENABLE_GLX_SURFACE)
    if(NOT UNIX OR APPLE)
        message(FATAL_ERROR
            "AERO_ENABLE_GLX_SURFACE is only supported on Unix/X11")
    endif()
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    add_library(AeroRhiGLX ${AERO_LIBRARY_TYPE}
        src/rhi/glx/GlxSurface.cpp)
    add_library(Aero::DetailRhiGLX ALIAS AeroRhiGLX)
    target_include_directories(AeroRhiGLX
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(AeroRhiGLX
        PUBLIC Aero::DetailRhiOpenGL33
        PRIVATE X11::X11 OpenGL::GL Threads::Threads)
    target_compile_features(AeroRhiGLX PUBLIC cxx_std_17)
    target_compile_definitions(
        AeroRhiGLX PUBLIC AERO_HAS_GLX_SURFACE=1)
    set_target_properties(AeroRhiGLX PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
        WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
    aero_apply_compiler_options(AeroRhiGLX)
endif()

add_library(AeroRender ${AERO_LIBRARY_TYPE}
    src/render/Renderer.cpp
    src/render/TextRuntimeService.cpp)
add_library(Aero::DetailRender ALIAS AeroRender)
target_include_directories(AeroRender
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(
    AeroRender PUBLIC Aero::Core Aero::DetailRhi)
target_compile_features(AeroRender PUBLIC cxx_std_17)
set_target_properties(AeroRender PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroRender)

add_library(AeroRenderOpenGL33 ${AERO_LIBRARY_TYPE}
    src/render/opengl33/OpenGL33RendererBackend.cpp)
add_library(
    Aero::DetailRenderOpenGL33 ALIAS AeroRenderOpenGL33)
target_include_directories(AeroRenderOpenGL33
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AeroRenderOpenGL33
    PUBLIC
        Aero::DetailRender
        Aero::DetailRhiOpenGL33)
target_compile_features(AeroRenderOpenGL33 PUBLIC cxx_std_17)
set_target_properties(AeroRenderOpenGL33 PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroRenderOpenGL33)

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
        ${_aero_d3d11_backend_fragments})
    add_library(Aero::DetailRhiD3D11 ALIAS AeroRhiD3D11)
    target_include_directories(AeroRhiD3D11
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(AeroRhiD3D11
        PUBLIC Aero::DetailRhi
        PRIVATE d3d11 dxgi d3dcompiler)
    target_compile_features(AeroRhiD3D11 PUBLIC cxx_std_17)
    set_target_properties(AeroRhiD3D11 PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
        WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
    target_compile_definitions(AeroRhiD3D11
        PUBLIC AERO_HAS_D3D11_BACKEND=1)
    aero_apply_compiler_options(AeroRhiD3D11)

    add_library(AeroRenderD3D11 ${AERO_LIBRARY_TYPE}
        src/render/d3d11/D3D11RendererBackend.cpp)
    add_library(
        Aero::DetailRenderD3D11 ALIAS AeroRenderD3D11)
    add_dependencies(AeroRenderD3D11 AeroD3D11RenderPlanShaders)
    target_include_directories(AeroRenderD3D11
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${_aero_d3d11_shader_directory}")
    target_link_libraries(AeroRenderD3D11
        PUBLIC
            Aero::DetailRender
            Aero::DetailRhiD3D11)
    target_compile_features(AeroRenderD3D11 PUBLIC cxx_std_17)
    set_target_properties(AeroRenderD3D11 PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
        WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
    aero_apply_compiler_options(AeroRenderD3D11)

else()
    target_compile_definitions(AeroRhi PUBLIC AERO_HAS_D3D11_BACKEND=0)
endif()

set(_aero_integration_sources
    src/integration/HostedGraphics.cpp
    src/integration/OpenGL33Endpoint.cpp)
if(WIN32)
    list(APPEND _aero_integration_sources
        src/integration/D3D11Endpoint.cpp)
endif()

add_library(AeroIntegration ${AERO_LIBRARY_TYPE}
    ${_aero_integration_sources})
target_include_directories(AeroIntegration
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AeroIntegration
    PUBLIC Aero::Runtime
    PRIVATE
        Aero::DetailRender
        Aero::DetailRhi
        Aero::DetailRenderOpenGL33
        Aero::DetailRhiOpenGL33)
if(WIN32)
    target_link_libraries(AeroIntegration PRIVATE
        Aero::DetailRenderD3D11
        Aero::DetailRhiD3D11)
    if(TARGET AeroRhiWGL)
        target_link_libraries(
            AeroIntegration PRIVATE Aero::DetailRhiWGL)
    endif()
elseif(TARGET AeroRhiGLX)
    target_link_libraries(
        AeroIntegration PRIVATE Aero::DetailRhiGLX)
endif()
target_compile_features(AeroIntegration PUBLIC cxx_std_17)
set_target_properties(AeroIntegration PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED}
    EXPORT_NAME IntegrationDetailFacade)
aero_apply_compiler_options(AeroIntegration)

add_library(AeroApp ${AERO_LIBRARY_TYPE}
    src/app/ApplicationHost.cpp
    src/app/Window.cpp)
add_library(Aero::App ALIAS AeroApp)
target_include_directories(AeroApp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroApp
    PUBLIC
        Aero::Integration
        Aero::Gui
        Aero::DetailAppModel
        Aero::Audio)
target_compile_definitions(AeroApp PRIVATE
    AERO_APP_HAS_D3D11=$<BOOL:${AERO_ENABLE_D3D11_BACKEND}>
    "AERO_APP_HAS_OPENGL_WINDOW=$<OR:$<BOOL:${AERO_ENABLE_WGL_SURFACE}>,$<BOOL:${AERO_ENABLE_GLX_SURFACE}>>")
target_compile_features(AeroApp PUBLIC cxx_std_17)
set_target_properties(AeroApp PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroApp)

add_library(AeroProductHeaderConsumer OBJECT
    tools/sdk-consumers/ProductConsumer.cpp)
target_link_libraries(AeroProductHeaderConsumer PRIVATE Aero::App)
aero_apply_compiler_options(AeroProductHeaderConsumer)

# Keep the public SDK dependency graph pointed in one direction. The runtime
# owns no GPU backend, while ModuleSdk remains a header-only authoring facade.
get_target_property(_aero_runtime_links AeroRuntime LINK_LIBRARIES)
if("${_aero_runtime_links}" MATCHES
        "(^|;)(Aero::DetailRhi|Aero::DetailRender|AeroRhi|AeroRender)")
    message(FATAL_ERROR
        "AeroRuntime must not link render or RHI implementation targets")
endif()
get_target_property(
    _aero_module_sdk_links AeroModuleSdk INTERFACE_LINK_LIBRARIES)
if("${_aero_module_sdk_links}" MATCHES
        "(^|;)(AeroModuleCatalog|Aero::DetailModuleCatalog)(;|$)")
    message(FATAL_ERROR
        "AeroModuleSdk must not link the ModuleCatalog implementation")
endif()
unset(_aero_runtime_links)
unset(_aero_module_sdk_links)

# Installable SDK and CMake package.
set_target_properties(
    AeroBase PROPERTIES EXPORT_NAME Base)
set_target_properties(
    AeroAudio PROPERTIES EXPORT_NAME Audio)
set_target_properties(
    AeroCore PROPERTIES EXPORT_NAME Core)
set_target_properties(
    AeroPlatform PROPERTIES EXPORT_NAME Platform)
set_target_properties(
    AeroText PROPERTIES EXPORT_NAME Text)
set_target_properties(
    freetype PROPERTIES EXPORT_NAME RuntimeDetailFreeType)
set_target_properties(
    AeroTextFreeType PROPERTIES
        EXPORT_NAME RuntimeDetailTextFreeType)
set_target_properties(
    harfbuzz PROPERTIES EXPORT_NAME RuntimeDetailHarfBuzz)
set_target_properties(
    AeroTextHarfBuzz PROPERTIES
        EXPORT_NAME RuntimeDetailTextHarfBuzz)
if(_aero_vendored_expat_target)
    set_target_properties(
        ${_aero_vendored_expat_target} PROPERTIES
            EXPORT_NAME RuntimeDetailExpat)
endif()
set_target_properties(
    AeroCore PROPERTIES EXPORT_NAME Core)
set_target_properties(
    AeroAppModel PROPERTIES EXPORT_NAME AppDetailModel)
set_target_properties(
    AeroControls PROPERTIES EXPORT_NAME Controls)
set_target_properties(
    AeroInspector PROPERTIES EXPORT_NAME Inspector)
set_target_properties(
    AeroMarkupKernel PROPERTIES EXPORT_NAME MarkupKernel)
set_target_properties(
    AeroMarkup PROPERTIES EXPORT_NAME Markup)
set_target_properties(
    AeroModuleCatalog PROPERTIES
        EXPORT_NAME RuntimeDetailModuleCatalog)
set_target_properties(
    AeroRuntime PROPERTIES EXPORT_NAME Runtime)
set_target_properties(
    AeroApp PROPERTIES EXPORT_NAME App)
set_target_properties(
    AeroGui PROPERTIES EXPORT_NAME Gui)
set_target_properties(
    AeroModuleSdk PROPERTIES EXPORT_NAME ModuleSdk)
set_target_properties(
    AeroIntegrationSdk PROPERTIES EXPORT_NAME Integration)
set_target_properties(
    AeroRhi PROPERTIES EXPORT_NAME IntegrationDetailRhi)
set_target_properties(
    AeroRhiOpenGL33 PROPERTIES
        EXPORT_NAME IntegrationDetailRhiOpenGL33)
set_target_properties(
    AeroRender PROPERTIES
        EXPORT_NAME IntegrationDetailRender)
set_target_properties(
    AeroRenderOpenGL33 PROPERTIES
        EXPORT_NAME IntegrationDetailRenderOpenGL33)

set(_aero_sdk_targets
    AeroBase
    AeroAudio
    AeroCore
    AeroPlatform
    AeroText
    freetype
    AeroTextFreeType
    harfbuzz
    AeroTextHarfBuzz
    AeroAppModel
    AeroControls
    AeroInspector
    AeroMarkupKernel
    AeroMarkup
    AeroModuleCatalog
    AeroRuntime
    AeroIntegration
    AeroApp
    AeroGui
    AeroModuleSdk
    AeroIntegrationSdk
    AeroRhi
    AeroRhiOpenGL33
    AeroRender
    AeroRenderOpenGL33)

if(_aero_vendored_expat_target)
    list(APPEND _aero_sdk_targets
        ${_aero_vendored_expat_target})
endif()

foreach(_aero_optional_target IN ITEMS
        AeroRhiWGL
        AeroRhiGLX
        AeroRhiD3D11
        AeroRenderD3D11)
    if(TARGET ${_aero_optional_target})
        list(APPEND _aero_sdk_targets ${_aero_optional_target})
        string(REGEX REPLACE "^Aero" "" _aero_export_name
            "${_aero_optional_target}")
        set_target_properties(${_aero_optional_target}
            PROPERTIES
                EXPORT_NAME
                    "IntegrationDetail${_aero_export_name}")
    endif()
endforeach()

install(TARGETS ${_aero_sdk_targets}
    EXPORT AeroTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(TARGET aero-schema-gen)
    set_target_properties(aero-schema-gen PROPERTIES EXPORT_NAME schema-gen)
    install(TARGETS aero-schema-gen
        EXPORT AeroTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(TARGET aero-xamlc)
    set_target_properties(aero-xamlc PROPERTIES EXPORT_NAME xamlc)
    install(TARGETS aero-xamlc
        EXPORT AeroTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(DIRECTORY include/Aero
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PATTERN "Version.hpp" EXCLUDE)
install(FILES
    "${AERO_GENERATED_INCLUDE_DIR}/Aero/Version.hpp"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Aero)
install(DIRECTORY themes/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/Aero/themes
    FILES_MATCHING PATTERN "*.xaml")
if(NOT _aero_schema_manifest STREQUAL "")
    install(FILES "${_aero_schema_manifest}"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/Aero/schema
        RENAME Aero.aeroschema)
endif()

set(AERO_PACKAGE_WITH_EXPAT OFF)
if(AERO_WITH_EXPAT AND NOT _aero_vendored_expat_target)
    set(AERO_PACKAGE_WITH_EXPAT ON)
endif()
set(AERO_PACKAGE_WITH_GLX ${AERO_ENABLE_GLX_SURFACE})
configure_package_config_file(
    cmake/AeroConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/AeroConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Aero)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AeroConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(EXPORT AeroTargets
    FILE AeroTargets.cmake
    NAMESPACE Aero::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Aero)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/AeroConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/AeroConfigVersion.cmake"
    cmake/AeroAddXaml.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Aero)

if(AERO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(AERO_BUILD_SAMPLES)
    add_subdirectory(samples/ControlGallery)
    add_subdirectory(samples/HelloWorld)
    add_subdirectory(samples/Localization)
    add_subdirectory(samples/Menu3D)
    add_subdirectory(samples/Scoreboard)
endif()
