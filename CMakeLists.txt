cmake_minimum_required(VERSION 3.20)

project(AeroGUI
    VERSION 0.1.0
    DESCRIPTION "C++17 retained-mode native GPU XAML UI runtime"
    LANGUAGES C CXX)

find_package(Threads REQUIRED)

option(AERO_BUILD_TESTS "Build AeroGUI unit tests" ON)
option(AERO_BUILD_SHARED "Build AeroGUI as shared libraries" OFF)
option(AERO_BUILD_TOOLS "Build AeroGUI offline tools" ON)
option(AERO_ENABLE_EXCEPTIONS "Enable C++ exceptions in AeroGUI targets" OFF)
option(AERO_ENABLE_RTTI "Enable C++ RTTI in AeroGUI targets" OFF)
option(AERO_WARNINGS_AS_ERRORS "Treat AeroGUI warnings as errors" OFF)
option(AERO_WITH_FREETYPE
    "Build the optional FreeType text provider adapter" OFF)
option(AERO_WITH_HARFBUZZ
    "Build the optional HarfBuzz text shaping adapter" OFF)
option(AERO_ENABLE_SOKOL_BACKEND
    "Compile a host-supplied sokol_gfx bridge into AeroRhi" OFF)
option(AERO_ENABLE_D3D11_BACKEND
    "Build the native Windows D3D11 graphics backend" ${WIN32})
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

add_library(AeroText ${AERO_LIBRARY_TYPE}
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
    src/presentation/ObjectTree.cpp
    src/presentation/Rendering.cpp
    src/presentation/Style.cpp)

add_library(Aero::Presentation ALIAS AeroPresentation)

target_include_directories(AeroPresentation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroPresentation PUBLIC Aero::Core)
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
    src/controls/TextBlockLayoutService.cpp
    src/controls/Templates.cpp
    src/controls/Virtualization.cpp
    src/controls/VisualStates.cpp)

add_library(Aero::Controls ALIAS AeroControls)

target_include_directories(AeroControls
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroControls PUBLIC Aero::Presentation)
target_compile_features(AeroControls PUBLIC cxx_std_17)
set_target_properties(AeroControls PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroControls)

add_library(AeroMarkup ${AERO_LIBRARY_TYPE}
    src/markup/XamlBinding.cpp
    src/markup/XamlCompiledCache.cpp
    src/markup/XamlCompiledDocument.cpp
    src/markup/XamlModuleSdk.cpp
    src/markup/XamlDynamicResource.cpp
    src/markup/XamlStyle.cpp
    src/markup/XamlTheme.cpp
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

target_link_libraries(AeroMarkup PUBLIC Aero::Controls)
target_compile_features(AeroMarkup PUBLIC cxx_std_17)
set_target_properties(AeroMarkup PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})

aero_apply_compiler_options(AeroMarkup)

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

if(AERO_BUILD_TOOLS)
    add_executable(aero-xamlc tools/xamlc/main.cpp)
    target_link_libraries(aero-xamlc PRIVATE Aero::Markup)
    target_compile_features(aero-xamlc PRIVATE cxx_std_17)
    set_target_properties(aero-xamlc PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    aero_apply_compiler_options(aero-xamlc)
endif()

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
target_link_libraries(AeroRender PUBLIC Aero::Presentation Aero::Rhi)
target_compile_features(AeroRender PUBLIC cxx_std_17)
set_target_properties(AeroRender PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroRender)

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
        PUBLIC Aero::Rhi Aero::Presentation
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

    add_library(AeroTextRenderD3D11 ${AERO_LIBRARY_TYPE}
        src/render/d3d11/D3D11TextBlockRenderService.cpp)
    add_library(Aero::TextRenderD3D11 ALIAS AeroTextRenderD3D11)
    target_include_directories(AeroTextRenderD3D11
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)
    target_link_libraries(AeroTextRenderD3D11
        PUBLIC Aero::TextRender Aero::RhiD3D11)
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

if(AERO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
