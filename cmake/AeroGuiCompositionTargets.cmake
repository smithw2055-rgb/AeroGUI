# View/Gui composition is folded directly into AeroGui. Keeping it as an
# object component preserves source ownership without another SDK binary.
#
# In-tree aero-xamlc/aero-schema-gen link Aero::Gui, so they can never be build
# prerequisites of AeroGui itself. Runtime uses compiled built-in themes only
# when an independent host tool chain is supplied. Otherwise a source-fallback
# header is generated synchronously at configure time; the in-tree tools remain
# ordinary post-Gui tools and AeroCompiledThemes stays an explicit asset target.
set(_aero_gui_precompiled_themes OFF)
if(AERO_PRECOMPILE_BUILTIN_THEMES AND
   NOT "${AERO_HOST_XAMLC_EXECUTABLE}" STREQUAL "")
    if(CMAKE_CROSSCOMPILING OR
       NOT "${AERO_BUILTIN_SCHEMA_MANIFEST}" STREQUAL "" OR
       NOT "${AERO_HOST_SCHEMA_GEN_EXECUTABLE}" STREQUAL "")
        set(_aero_gui_precompiled_themes ON)
    endif()
endif()

set(_aero_gui_theme_include_dir
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
if(NOT _aero_gui_precompiled_themes)
    # Gui fallback must not share an output with AeroCompiledThemes. Ninja
    # otherwise binds View.cpp's generated-header dependency to aero-xamlc and
    # recreates the AeroGui -> aero-xamlc -> AeroGui bootstrap cycle.
    set(_aero_gui_theme_include_dir
        "${CMAKE_CURRENT_BINARY_DIR}/runtime-generated")
    set(_aero_gui_theme_header
        "${_aero_gui_theme_include_dir}/Aero/BuiltinThemes.generated.hpp")
    file(MAKE_DIRECTORY
        "${_aero_gui_theme_include_dir}/Aero")
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DOUTPUT=${_aero_gui_theme_header}"
            "-DLIGHT_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/Light.xaml"
            "-DDARK_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/Dark.xaml"
            "-DGENERIC_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/themes/Generic.xaml"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedXamlThemes.cmake"
        RESULT_VARIABLE _aero_theme_embed_result)
    if(NOT _aero_theme_embed_result EQUAL 0)
        message(FATAL_ERROR
            "Unable to generate the built-in theme source fallback header")
    endif()
endif()

set(_aero_gui_composition_sources
    src/gui/Gui.cpp
    src/gui/View.cpp
    src/markup/ReloadCoordinator.cpp
    src/render/RenderDevice.cpp
    src/gui/Invariants.cpp
    src/media/ImageCache.cpp
    src/media/StbImageImplementation.cpp
    src/text/TextPipeline.cpp
    src/markup/XamlReader.cpp)
if(_aero_gui_precompiled_themes)
    list(APPEND _aero_gui_composition_sources "${_aero_generated_theme_header}")
    add_dependencies(AeroGui AeroCompiledThemes)
endif()
target_sources(AeroGui PRIVATE ${_aero_gui_composition_sources})
target_compile_definitions(AeroGui PRIVATE AERO_INTERNAL_GUI_COMPOSITION=1)
target_include_directories(AeroGui PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb"
    "${_aero_gui_theme_include_dir}"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
unset(_aero_gui_composition_sources)

set(AERO_DEFAULT_THEME_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/themes/Generic.xaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/themes/Light.xaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/themes/Dark.xaml")
# TARGET_FILE_DIR creates the required AeroDefaultThemes -> AeroGui ordering.
# Do not add the reverse AeroGui -> AeroDefaultThemes dependency.
add_custom_target(AeroDefaultThemes ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:AeroGui>/themes"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        ${AERO_DEFAULT_THEME_FILES}
        "$<TARGET_FILE_DIR:AeroGui>/themes"
    DEPENDS ${AERO_DEFAULT_THEME_FILES}
    VERBATIM)

unset(_aero_gui_precompiled_themes)
unset(_aero_gui_theme_include_dir)
unset(_aero_gui_theme_header)
unset(_aero_theme_embed_result)
