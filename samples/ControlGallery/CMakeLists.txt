set(_aero_control_gallery_assets
    "${CMAKE_CURRENT_BINARY_DIR}/assets")
set(_aero_control_gallery_stamp
    "${_aero_control_gallery_assets}/.copied")

file(GLOB_RECURSE _aero_control_gallery_data
    CONFIGURE_DEPENDS
    LIST_DIRECTORIES FALSE
    "${CMAKE_CURRENT_SOURCE_DIR}/Data/*")

add_custom_command(
    OUTPUT "${_aero_control_gallery_stamp}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${_aero_control_gallery_assets}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Data"
        "${_aero_control_gallery_assets}"
    COMMAND "${CMAKE_COMMAND}" -E touch
        "${_aero_control_gallery_stamp}"
    DEPENDS ${_aero_control_gallery_data}
    VERBATIM)
add_custom_target(AeroControlGalleryAssets
    DEPENDS "${_aero_control_gallery_stamp}")

add_executable(AeroControlGallery
    main.cpp
    MainWindow.cpp
    MainWindow.hpp)
target_link_libraries(AeroControlGallery
    PRIVATE Aero::App)
target_compile_definitions(AeroControlGallery PRIVATE
    "AERO_CONTROL_GALLERY_APP_FILE=\"${_aero_control_gallery_assets}/App.xaml\""
    "AERO_CONTROL_GALLERY_ASSET_ROOT=\"${_aero_control_gallery_assets}\"")
target_compile_features(AeroControlGallery PRIVATE cxx_std_17)
set_target_properties(AeroControlGallery PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroControlGallery)
if(MSVC)
    target_link_options(AeroControlGallery PRIVATE /DEBUG)
endif()
add_dependencies(AeroControlGallery AeroControlGalleryAssets)

if(AERO_BUILD_TESTS)
    add_test(
        NAME AeroControlGallerySmoke
        COMMAND AeroControlGallery --verify)
    set_tests_properties(AeroControlGallerySmoke PROPERTIES
        LABELS "sample;product"
        TIMEOUT 180)
endif()
