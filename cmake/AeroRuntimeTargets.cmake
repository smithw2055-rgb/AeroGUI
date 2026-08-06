# View/runtime composition is folded directly into AeroGui. Keeping it as an
# object component preserves source ownership without another SDK binary.
add_library(AeroRuntimeObjects OBJECT
    src/runtime/Gui.cpp
    src/runtime/View.cpp
    src/markup/ReloadCoordinator.cpp
    src/render/RenderDevice.cpp
    src/runtime/Invariants.cpp
    src/runtime/ImageCache.cpp
    src/runtime/StbImageImplementation.cpp
    src/runtime/TextPipeline.cpp
    src/markup/XamlReader.cpp
    "${_aero_generated_theme_header}")
add_dependencies(AeroRuntimeObjects AeroCompiledThemes)
aero_configure_internal_objects(AeroRuntimeObjects)
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
add_custom_target(AeroDefaultThemes ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:AeroGui>/themes"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        ${AERO_DEFAULT_THEME_FILES}
        "$<TARGET_FILE_DIR:AeroGui>/themes"
    DEPENDS ${AERO_DEFAULT_THEME_FILES}
    VERBATIM)
add_dependencies(AeroGui AeroDefaultThemes)
