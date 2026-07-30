add_library(AeroLocalizationModel STATIC LocalizationModel.cpp)
target_include_directories(AeroLocalizationModel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(AeroLocalizationModel PUBLIC Aero::ModuleSdk Aero::Controls)
target_compile_features(AeroLocalizationModel PUBLIC cxx_std_17)
set_target_properties(AeroLocalizationModel PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroLocalizationModel)

aero_add_schema_manifest(AeroLocalizationSchema
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/Localization.aeroschema"
    MODULE_HEADER "LocalizationModel.hpp"
    MODULE_FUNCTION "Aero::Samples::Localization::MakeLocalizationModuleManifest"
    LIBRARIES AeroLocalizationModel)

add_custom_target(AeroLocalizationXaml ALL)
set(_aero_localization_xaml_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/App.xaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.xaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources.xaml")
set(_aero_localization_language_resources
    "${CMAKE_CURRENT_SOURCE_DIR}/Language-en.xaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/Language-fr.xaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/Language-jp.xaml"
    "${CMAKE_CURRENT_SOURCE_DIR}/Language-ar.xaml")
aero_add_xaml(AeroLocalizationXaml
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/assets"
    SCHEMA "${AeroLocalizationSchema_OUTPUT}"
    ORIGIN_PREFIX "pack://application:,,,/Aero.Localization;component"
    SOURCES ${_aero_localization_xaml_sources})
add_dependencies(AeroLocalizationXaml AeroLocalizationSchema)

set(_aero_localization_assets "${CMAKE_CURRENT_BINARY_DIR}/assets")
set(_aero_localization_asset_outputs)
set(_aero_localization_asset_sources
    ${_aero_localization_xaml_sources}
    ${_aero_localization_language_resources})
foreach(_aero_localization_xaml IN LISTS _aero_localization_asset_sources)
    get_filename_component(_aero_localization_name "${_aero_localization_xaml}" NAME)
    list(APPEND _aero_localization_asset_outputs "${_aero_localization_assets}/${_aero_localization_name}")
    add_custom_command(
        OUTPUT "${_aero_localization_assets}/${_aero_localization_name}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_aero_localization_assets}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_aero_localization_xaml}" "${_aero_localization_assets}/${_aero_localization_name}"
        DEPENDS "${_aero_localization_xaml}"
        VERBATIM)
endforeach()
add_custom_target(AeroLocalizationAssets DEPENDS ${_aero_localization_asset_outputs})

add_executable(AeroLocalization main.cpp)
target_link_libraries(AeroLocalization PRIVATE Aero::App AeroLocalizationModel)
target_compile_definitions(AeroLocalization PRIVATE "AERO_LOCALIZATION_APP_FILE=\"${_aero_localization_assets}/App.xaml\"")
target_compile_features(AeroLocalization PRIVATE cxx_std_17)
set_target_properties(AeroLocalization PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroLocalization)
add_dependencies(AeroLocalization
    AeroLocalizationAssets
    AeroLocalizationSchema
    AeroLocalizationXaml)

if(AERO_BUILD_TESTS)
    add_test(NAME AeroLocalizationSmoke COMMAND AeroLocalization --verify)
    set_tests_properties(AeroLocalizationSmoke PROPERTIES LABELS "conformance;product" TIMEOUT 30)
endif()
