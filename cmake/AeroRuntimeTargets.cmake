# Runtime composition and public Gui/Meta aggregate targets.
add_library(AeroRuntime ${AERO_LIBRARY_TYPE}
    src/runtime/View.cpp
    src/runtime/ViewState.cpp
    src/integration/ReloadCoordinator.cpp
    src/integration/RenderEndpoint.cpp
    src/runtime/RuntimeSafety.cpp
    src/runtime/ViewUiServices.cpp
    src/runtime/ImageRuntime.cpp
    src/runtime/StbImageImplementation.cpp
    src/runtime/TextRuntime.cpp
    "${_aero_generated_theme_header}")
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
        Aero::_DetailTextHarfBuzz)
target_compile_features(AeroRuntime PUBLIC cxx_std_17)
set_target_properties(AeroRuntime PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroRuntime)

# Product-facing SDK aggregates. Gui is the retained WPF/XAML class library;
# Meta adds custom-type authoring without exposing runtime composition. App and
# Integration are concrete optional hosting products defined below.
add_library(AeroGui INTERFACE)
add_library(Aero::Gui ALIAS AeroGui)
target_link_libraries(AeroGui INTERFACE Aero::_DetailMarkup)

add_library(AeroMeta INTERFACE)
add_library(Aero::Meta ALIAS AeroMeta)
target_link_libraries(AeroMeta INTERFACE Aero::Gui)

# Compile-only consumers keep the product entry points independently
# consumable and catch accidental reverse header dependencies.
add_library(AeroGuiHeaderConsumer OBJECT
    tools/sdk-consumers/GuiConsumer.cpp)
target_link_libraries(AeroGuiHeaderConsumer PRIVATE Aero::Gui)
aero_apply_compiler_options(AeroGuiHeaderConsumer)

add_library(AeroMetaHeaderConsumer OBJECT
    tools/sdk-consumers/MetaConsumer.cpp)
target_link_libraries(AeroMetaHeaderConsumer PRIVATE Aero::Meta)
aero_apply_compiler_options(AeroMetaHeaderConsumer)

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
