# View/runtime composition is folded directly into AeroGui. Keeping it as an
# object component preserves source ownership without another SDK binary.
#
# In-tree aero-xamlc/aero-schema-gen link Aero::Gui, so they can never be build
# prerequisites of AeroGui itself. Runtime uses compiled built-in themes only
# when an independent host tool chain is supplied. Otherwise a source-fallback
# header is generated synchronously at configure time; the in-tree tools remain
# ordinary post-Gui tools and AeroCompiledThemes stays an explicit asset target.
set(_aero_runtime_precompiled_themes OFF)
if(AERO_PRECOMPILE_BUILTIN_THEMES AND
   NOT AERO_HOST_XAMLC_EXECUTABLE STREQUAL "")
    if(CMAKE_CROSSCOMPILING OR
       NOT AERO_BUILTIN_SCHEMA_MANIFEST STREQUAL "" OR
       NOT AERO_HOST_SCHEMA_GEN_EXECUTABLE STREQUAL "")
        set(_aero_runtime_precompiled_themes ON)
    endif()
endif()

if(NOT _aero_runtime_precompiled_themes)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DOUTPUT=${_aero_generated_theme_header}"
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

add_library(AeroRuntimeObjects OBJECT
    src/runtime/Gui.cpp
    src/runtime/View.cpp
    src/markup/ReloadCoordinator.cpp
    src/render/RenderDevice.cpp
    src/runtime/Invariants.cpp
    src/runtime/ImageCache.cpp
    src/runtime/StbImageImplementation.cpp
    src/runtime/TextPipeline.cpp
    src/markup/XamlReader.cpp)
if(_aero_runtime_precompiled_themes)
    target_sources(AeroRuntimeObjects PRIVATE
        "${_aero_generated_theme_header}")
    add_dependencies(AeroRuntimeObjects AeroCompiledThemes)
endif()
aero_configure_internal_objects(AeroRuntimeObjects)
target_compile_definitions(AeroRuntimeObjects PRIVATE
    AERO_INTERNAL_RUNTIME=1)
target_include_directories(AeroRuntimeObjects PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(AeroRuntimeObjects PUBLIC
    AeroModuleSetObjects
    AeroTextHarfBuzzObjects)

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

unset(_aero_runtime_precompiled_themes)
unset(_aero_theme_embed_result)
