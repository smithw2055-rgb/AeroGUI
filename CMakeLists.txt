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
    "Build the optional FreeType text provider adapter" OFF)
option(AERO_WITH_HARFBUZZ
    "Build the optional HarfBuzz text shaping adapter" OFF)
option(AERO_WITH_EXPAT
    "Build the recommended Expat XML tokenizer adapter" ON)
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

if(AERO_WITH_HARFBUZZ AND NOT AERO_WITH_FREETYPE)
    message(FATAL_ERROR
        "AERO_WITH_HARFBUZZ currently requires AERO_WITH_FREETYPE")
endif()

if(AERO_WITH_FREETYPE)
    if(AERO_THIRD_PARTY_ROOT STREQUAL "")
        get_filename_component(_aero_sibling_third_party
            "${CMAKE_CURRENT_SOURCE_DIR}/../AeroGUI/third_party"
            ABSOLUTE)
        if(EXISTS "${_aero_sibling_third_party}/freetype/freetype.c")
            set(AERO_THIRD_PARTY_ROOT "${_aero_sibling_third_party}")
        endif()
    endif()
    if(AERO_TEXT_TEST_FONT STREQUAL "" AND
       EXISTS "${AERO_THIRD_PARTY_ROOT}/harfbuzz/perf/fonts/Roboto-Regular.ttf")
        set(AERO_TEXT_TEST_FONT
            "${AERO_THIRD_PARTY_ROOT}/harfbuzz/perf/fonts/Roboto-Regular.ttf")
    endif()

    set(_aero_freetype_source
        "${AERO_THIRD_PARTY_ROOT}/freetype/freetype.c")
    set(_aero_freetype_include
        "${AERO_THIRD_PARTY_ROOT}/freetype/include")
    foreach(_aero_required_path IN ITEMS
            "${_aero_freetype_source}"
            "${_aero_freetype_include}/ft2build.h")
        if(NOT EXISTS "${_aero_required_path}")
            message(FATAL_ERROR
                "AERO_WITH_FREETYPE requires: "
                "${_aero_required_path}")
        endif()
    endforeach()

    add_library(freetype STATIC "${_aero_freetype_source}")
    target_include_directories(freetype PUBLIC "${_aero_freetype_include}")
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
    src/core/events/RoutedEventCatalog.cpp
    src/core/metadata/CoreMetadata.cpp
    src/core/metadata/MetadataDescriptors.cpp
    src/core/metadata/MetadataBehaviorRegistrationStore.cpp
    src/core/metadata/MetadataDomain.cpp
    src/core/metadata/MetadataValueFacets.cpp
    src/core/metadata/MetadataValueRegistrationStore.cpp
    src/core/ObjectServices.cpp
    src/core/metadata/TypeRegistry.cpp
    src/core/metadata/Value.cpp)

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

add_library(AeroPresentation ${AERO_LIBRARY_TYPE}
    src/presentation/Binding.cpp
    src/presentation/Commands.cpp
    src/presentation/Input.cpp
    src/presentation/Layout.cpp
    src/presentation/Metadata.cpp
    src/presentation/MountService.cpp
    src/presentation/ObjectTree.cpp
    src/presentation/QueuedRenderBackend.cpp
    src/presentation/Resources.cpp
    src/presentation/Rendering.cpp
    src/presentation/VisualTreeMount.cpp
    src/presentation/Style.cpp)

add_library(Aero::Presentation ALIAS AeroPresentation)

target_include_directories(AeroPresentation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroPresentation PUBLIC Aero::Core)
target_compile_definitions(
    AeroPresentation PUBLIC AERO_PRESENTATION_RESOURCE_MODEL=2)
target_compile_features(AeroPresentation PUBLIC cxx_std_17)
set_target_properties(AeroPresentation PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroPresentation)

add_library(AeroControls ${AERO_LIBRARY_TYPE}
    src/controls/Buttons.cpp
    src/controls/Controls.cpp
    src/controls/Items.cpp
    src/controls/Metadata.cpp
    src/controls/Scroll.cpp
    src/controls/Selection.cpp
    src/controls/TextBox.cpp
    src/controls/TextBlockLayoutService.cpp
    src/controls/Templates.cpp
    src/controls/Virtualization.cpp
    src/controls/VisualStates.cpp)

add_library(Aero::Controls ALIAS AeroControls)

target_include_directories(AeroControls
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroControls
    PUBLIC
        Aero::Presentation
        Aero::Platform
        Aero::Text)
target_compile_definitions(
    AeroControls PUBLIC AERO_CONTROLS_TEMPLATE_ABI=2)
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
        $<INSTALL_INTERFACE:include>)
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
    src/markup/compiled/XamlCompiledCache.cpp
    src/markup/compiled/XamlCompiledDocument.cpp
    src/markup/parsing/XmlTokenizer.cpp
    src/markup/parsing/XamlNodeReader.cpp)
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
    src/markup/extensions/XamlBinding.cpp
    src/markup/compiled/XamlCompiledSchema.cpp
    src/markup/schema/Metadata.cpp
    src/markup/schema/XamlFacetStore.cpp
    src/markup/schema/XamlSchemaManifest.cpp
    src/markup/extensions/XamlDynamicResource.cpp
    src/markup/resources/XamlStyle.cpp
    src/markup/resources/XamlTemplate.cpp
    src/markup/resources/XamlTemplateCompiler.cpp
    src/markup/extensions/XamlTypeExtension.cpp
    src/markup/runtime/XamlActivation.cpp
    src/markup/resources/XamlScopes.cpp
    src/markup/runtime/XamlLoadResult.cpp
    src/markup/runtime/XamlLoadSession.cpp
    src/markup/runtime/XamlDocumentCache.cpp
    src/markup/runtime/XamlLoader.cpp
    src/markup/resources/XamlResources.cpp
    src/markup/schema/XamlRuntimeSchema.cpp
    src/markup/runtime/XamlObjectWriter.cpp
    src/markup/schema/XamlSchemaContext.cpp
    src/markup/runtime/XamlContentWriter.cpp)

if(AERO_WITH_EXPAT)
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
        src/markup/parsing/ExpatXmlTokenizer.cpp)
    target_link_libraries(AeroMarkupKernel PRIVATE
        $<BUILD_INTERFACE:${_aero_expat_target}>
        $<INSTALL_INTERFACE:EXPAT::EXPAT>)
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
    AeroMarkup PRIVATE AERO_MARKUP_PRESENTATION_RESOURCES=1)
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
add_library(Aero::ModuleCatalog ALIAS AeroModuleCatalog)
target_include_directories(AeroModuleCatalog
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroModuleCatalog PUBLIC Aero::Markup)
target_compile_features(AeroModuleCatalog PUBLIC cxx_std_17)
set_target_properties(AeroModuleCatalog PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON)
aero_apply_compiler_options(AeroModuleCatalog)

if(AERO_BUILD_TOOLS)
    add_executable(aero-schema-gen tools/schema-gen/main.cpp)
    target_link_libraries(aero-schema-gen PRIVATE AeroModuleCatalog)
    target_compile_features(aero-schema-gen PRIVATE cxx_std_17)
    set_target_properties(aero-schema-gen PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    aero_apply_compiler_options(aero-schema-gen)

    add_executable(aero-xamlc tools/xamlc/main.cpp)
    target_link_libraries(aero-xamlc PRIVATE AeroModuleCatalog)
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
    src/RuntimeHost.cpp
    src/XamlReloadCoordinator.cpp
    src/RuntimeSafety.cpp
    src/UiDocument.cpp
    src/runtime/RuntimePresentationServices.cpp
    "${_aero_generated_theme_header}")
add_library(Aero::Runtime ALIAS AeroRuntime)
add_dependencies(AeroRuntime AeroCompiledThemes)
target_include_directories(AeroRuntime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(AeroRuntime
    PUBLIC
        AeroModuleCatalog
        Aero::Platform)
target_compile_features(AeroRuntime PUBLIC cxx_std_17)
set_target_properties(AeroRuntime PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroRuntime)

# Product-facing SDK aggregates. Lower-level targets remain available for
# advanced embedders, while normal applications can depend on one stable entry.
add_library(AeroModuleSdk INTERFACE)
add_library(Aero::ModuleSdk ALIAS AeroModuleSdk)
target_link_libraries(AeroModuleSdk INTERFACE Aero::ModuleCatalog)

add_library(AeroEngineHost INTERFACE)
add_library(Aero::EngineHost ALIAS AeroEngineHost)
target_link_libraries(AeroEngineHost INTERFACE
    Aero::Runtime Aero::Render Aero::Rhi)

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

add_library(AeroRhiOpenGL33 ${AERO_LIBRARY_TYPE}
    src/rhi/opengl33/OpenGL33Backend.cpp
    src/rhi/opengl33/OpenGL33Context.cpp
    src/rhi/opengl33/OpenGL33StateCache.cpp)
add_library(Aero::RhiOpenGL33 ALIAS AeroRhiOpenGL33)
target_include_directories(AeroRhiOpenGL33
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroRhiOpenGL33 PUBLIC Aero::Rhi)
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
    add_library(Aero::RhiWGL ALIAS AeroRhiWGL)
    target_include_directories(AeroRhiWGL
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)
    target_link_libraries(AeroRhiWGL
        PUBLIC Aero::RhiOpenGL33
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
    add_library(Aero::RhiGLX ALIAS AeroRhiGLX)
    target_include_directories(AeroRhiGLX
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)
    target_link_libraries(AeroRhiGLX
        PUBLIC Aero::RhiOpenGL33
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
    src/render/Renderer.cpp)
add_library(Aero::Render ALIAS AeroRender)
target_include_directories(AeroRender
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroRender PUBLIC Aero::Presentation Aero::Rhi)
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
add_library(Aero::RenderOpenGL33 ALIAS AeroRenderOpenGL33)
target_include_directories(AeroRenderOpenGL33
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroRenderOpenGL33
    PUBLIC Aero::Render Aero::RhiOpenGL33)
target_compile_features(AeroRenderOpenGL33 PUBLIC cxx_std_17)
set_target_properties(AeroRenderOpenGL33 PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroRenderOpenGL33)

add_library(AeroTextRender ${AERO_LIBRARY_TYPE}
    src/render/TextBlockRenderService.cpp)
add_library(Aero::TextRender ALIAS AeroTextRender)
target_include_directories(AeroTextRender
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroTextRender
    PUBLIC Aero::Controls Aero::Text Aero::Rhi)
target_compile_features(AeroTextRender PUBLIC cxx_std_17)
set_target_properties(AeroTextRender PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroTextRender)

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
    add_library(Aero::RhiD3D11 ALIAS AeroRhiD3D11)
    target_include_directories(AeroRhiD3D11
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)
    target_link_libraries(AeroRhiD3D11
        PUBLIC Aero::Rhi
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
    add_library(Aero::RenderD3D11 ALIAS AeroRenderD3D11)
    add_dependencies(AeroRenderD3D11 AeroD3D11RenderPlanShaders)
    target_include_directories(AeroRenderD3D11
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE "${_aero_d3d11_shader_directory}")
    target_link_libraries(AeroRenderD3D11
        PUBLIC Aero::Render Aero::RhiD3D11)
    target_compile_features(AeroRenderD3D11 PUBLIC cxx_std_17)
    set_target_properties(AeroRenderD3D11 PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
        WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
    aero_apply_compiler_options(AeroRenderD3D11)

    add_library(AeroTextRenderD3D11 ${AERO_LIBRARY_TYPE}
        src/render/d3d11/D3D11TextBlockRenderService.cpp)
    add_library(Aero::TextRenderD3D11 ALIAS AeroTextRenderD3D11)
    target_include_directories(AeroTextRenderD3D11
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)
    target_link_libraries(AeroTextRenderD3D11
        PUBLIC Aero::TextRender Aero::RenderD3D11)
    target_compile_features(AeroTextRenderD3D11 PUBLIC cxx_std_17)
    set_target_properties(AeroTextRenderD3D11 PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
        WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
    aero_apply_compiler_options(AeroTextRenderD3D11)
else()
    target_compile_definitions(AeroRhi PUBLIC AERO_HAS_D3D11_BACKEND=0)
endif()

# Installable SDK and CMake package.
set_target_properties(
    AeroBase PROPERTIES EXPORT_NAME Base)
set_target_properties(
    AeroCore PROPERTIES EXPORT_NAME Core)
set_target_properties(
    AeroPlatform PROPERTIES EXPORT_NAME Platform)
set_target_properties(
    AeroText PROPERTIES EXPORT_NAME Text)
set_target_properties(
    AeroPresentation PROPERTIES EXPORT_NAME Presentation)
set_target_properties(
    AeroControls PROPERTIES EXPORT_NAME Controls)
set_target_properties(
    AeroInspector PROPERTIES EXPORT_NAME Inspector)
set_target_properties(
    AeroMarkupKernel PROPERTIES EXPORT_NAME MarkupKernel)
set_target_properties(
    AeroMarkup PROPERTIES EXPORT_NAME Markup)
set_target_properties(
    AeroModuleCatalog PROPERTIES EXPORT_NAME ModuleCatalog)
set_target_properties(
    AeroRuntime PROPERTIES EXPORT_NAME Runtime)
set_target_properties(
    AeroModuleSdk PROPERTIES EXPORT_NAME ModuleSdk)
set_target_properties(
    AeroEngineHost PROPERTIES EXPORT_NAME EngineHost)
set_target_properties(
    AeroRhi PROPERTIES EXPORT_NAME Rhi)
set_target_properties(
    AeroRhiOpenGL33 PROPERTIES EXPORT_NAME RhiOpenGL33)
set_target_properties(
    AeroRender PROPERTIES EXPORT_NAME Render)
set_target_properties(
    AeroRenderOpenGL33 PROPERTIES EXPORT_NAME RenderOpenGL33)
set_target_properties(
    AeroTextRender PROPERTIES EXPORT_NAME TextRender)

set(_aero_sdk_targets
    AeroBase
    AeroCore
    AeroPlatform
    AeroText
    AeroPresentation
    AeroControls
    AeroInspector
    AeroMarkupKernel
    AeroMarkup
    AeroModuleCatalog
    AeroRuntime
    AeroModuleSdk
    AeroEngineHost
    AeroRhi
    AeroRhiOpenGL33
    AeroRender
    AeroRenderOpenGL33
    AeroTextRender)

foreach(_aero_optional_target IN ITEMS
        AeroRhiWGL
        AeroRhiGLX
        AeroRhiD3D11
        AeroRenderD3D11
        AeroTextRenderD3D11)
    if(TARGET ${_aero_optional_target})
        list(APPEND _aero_sdk_targets ${_aero_optional_target})
        string(REGEX REPLACE "^Aero" "" _aero_export_name
            "${_aero_optional_target}")
        set_target_properties(${_aero_optional_target}
            PROPERTIES EXPORT_NAME "${_aero_export_name}")
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

set(AERO_PACKAGE_WITH_EXPAT ${AERO_WITH_EXPAT})
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
endif()
