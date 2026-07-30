set(_aero_control_gallery_assets
    "${CMAKE_CURRENT_BINARY_DIR}/assets")
set(_aero_control_gallery_reference_data_dir
    "${CMAKE_CURRENT_SOURCE_DIR}/Data")
set(_aero_control_gallery_reference_data_output
    "${_aero_control_gallery_assets}/Data/.copy-stamp")
file(GLOB_RECURSE _aero_control_gallery_reference_data_files
    CONFIGURE_DEPENDS
    "${_aero_control_gallery_reference_data_dir}/*")
set(_aero_reference_gallery_theme_dir
    "${CMAKE_SOURCE_DIR}/../AeroGUI/Packages/App/Theme/Data/Theme")
set(_aero_control_gallery_reference_theme_output
    "${_aero_control_gallery_assets}/Theme/.copy-stamp")
file(GLOB_RECURSE _aero_control_gallery_reference_theme_files
    CONFIGURE_DEPENDS
    "${_aero_reference_gallery_theme_dir}/*")
set(_aero_control_gallery_theme_dir
    "${_aero_control_gallery_assets}/themes")
set(_aero_control_gallery_theme_outputs
    "${_aero_control_gallery_theme_dir}/Generic.xaml"
    "${_aero_control_gallery_theme_dir}/Light.xaml"
    "${_aero_control_gallery_theme_dir}/Dark.xaml")
set(_aero_control_gallery_font_dir
    "${_aero_control_gallery_assets}/fonts")
set(_aero_control_gallery_icon_font
    "${_aero_control_gallery_font_dir}/WidgetIcons.ttf")
set(_aero_control_gallery_image_dir
    "${_aero_control_gallery_assets}/Images")
set(_aero_control_gallery_image_outputs
    "${_aero_control_gallery_image_dir}/Nature.png"
    "${_aero_control_gallery_image_dir}/floppy-disk.png"
    "${_aero_control_gallery_image_dir}/MainWindowGradient.png")
set(_aero_reference_gallery_icon_font
    "${CMAKE_SOURCE_DIR}/../AeroGUI/Packages/Gallery/Data/Samples/Fonts/WidgetIcons.ttf")
if(NOT EXISTS "${_aero_reference_gallery_icon_font}")
    message(FATAL_ERROR
        "ControlGallery visual comparison requires the reference WidgetIcons font: "
        "${_aero_reference_gallery_icon_font}")
endif()
if(NOT EXISTS "${_aero_reference_gallery_theme_dir}")
    message(FATAL_ERROR
        "ControlGallery XAML requires the reference Aero.GUI.Extensions theme: "
        "${_aero_reference_gallery_theme_dir}")
endif()

add_executable(AeroControlGallery
    GalleryRuntime.cpp
    main.cpp)
target_link_libraries(AeroControlGallery
    PRIVATE Aero::IntegrationSdk)
target_include_directories(AeroControlGallery PRIVATE
    "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(AeroControlGallery PRIVATE
    "AERO_CONTROL_GALLERY_ASSET_DIR=\"${_aero_control_gallery_assets}\"")
target_compile_features(AeroControlGallery
    PRIVATE cxx_std_17)
set_target_properties(AeroControlGallery PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroControlGallery)

add_custom_command(
    OUTPUT
        "${_aero_control_gallery_icon_font}"
        ${_aero_control_gallery_image_outputs}
        ${_aero_control_gallery_theme_outputs}
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${_aero_control_gallery_assets}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${_aero_control_gallery_theme_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${_aero_control_gallery_font_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${_aero_control_gallery_image_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_SOURCE_DIR}/themes/Generic.xaml"
        "${_aero_control_gallery_theme_dir}/Generic.xaml"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_SOURCE_DIR}/themes/Light.xaml"
        "${_aero_control_gallery_theme_dir}/Light.xaml"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_SOURCE_DIR}/themes/Dark.xaml"
        "${_aero_control_gallery_theme_dir}/Dark.xaml"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_aero_reference_gallery_icon_font}"
        "${_aero_control_gallery_icon_font}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Images/Nature.png"
        "${_aero_control_gallery_image_dir}/Nature.png"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Images/floppy-disk.png"
        "${_aero_control_gallery_image_dir}/floppy-disk.png"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/Images/MainWindowGradient.png"
        "${_aero_control_gallery_image_dir}/MainWindowGradient.png"
    DEPENDS
        "${CMAKE_SOURCE_DIR}/themes/Generic.xaml"
        "${CMAKE_SOURCE_DIR}/themes/Light.xaml"
        "${CMAKE_SOURCE_DIR}/themes/Dark.xaml"
        "${_aero_reference_gallery_icon_font}"
        "${CMAKE_CURRENT_SOURCE_DIR}/Images/Nature.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/Images/floppy-disk.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/Images/MainWindowGradient.png"
    VERBATIM)
add_custom_command(
    OUTPUT "${_aero_control_gallery_reference_data_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${_aero_control_gallery_assets}/Data"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${_aero_control_gallery_reference_data_dir}"
        "${_aero_control_gallery_assets}/Data"
    COMMAND "${CMAKE_COMMAND}" -E touch
        "${_aero_control_gallery_reference_data_output}"
    DEPENDS ${_aero_control_gallery_reference_data_files}
    VERBATIM)
add_custom_command(
    OUTPUT "${_aero_control_gallery_reference_theme_output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${_aero_control_gallery_assets}/Theme"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${_aero_reference_gallery_theme_dir}"
        "${_aero_control_gallery_assets}/Theme"
    COMMAND "${CMAKE_COMMAND}" -E touch
        "${_aero_control_gallery_reference_theme_output}"
    DEPENDS ${_aero_control_gallery_reference_theme_files}
    VERBATIM)
add_custom_target(AeroControlGalleryAssets ALL
    DEPENDS
        "${_aero_control_gallery_reference_data_output}"
        "${_aero_control_gallery_reference_theme_output}"
        "${_aero_control_gallery_icon_font}"
        ${_aero_control_gallery_image_outputs}
        ${_aero_control_gallery_theme_outputs})
add_dependencies(AeroControlGallery AeroControlGalleryAssets)

if(AERO_BUILD_TESTS)
    add_test(
        NAME AeroControlGallerySmoke
        COMMAND AeroControlGallery
            --xaml=both
            --theme=both)
    set_tests_properties(
        AeroControlGallerySmoke PROPERTIES
        LABELS "conformance;product"
        TIMEOUT 60)
    add_test(
        NAME AeroControlGalleryInteraction
        COMMAND AeroControlGallery
            --xaml=both
            --theme=light
            --page=buttons
            --scenario=interaction
            --frames=8)
    add_test(
        NAME AeroControlGalleryVirtualization
        COMMAND AeroControlGallery
            --xaml=compiled
            --theme=dark
            --page=items
            --scenario=scroll
            --frames=32)
    add_test(
        NAME AeroControlGalleryBatchingDisabled
        COMMAND AeroControlGallery
            --xaml=compiled
            --theme=light
            --page=layout
            --scenario=batch
            --frames=8
            --batching=off)
    add_test(
        NAME AeroControlGalleryLifecycle
        COMMAND AeroControlGallery
            --xaml=compiled
            --theme=light
            --page=buttons
            --scenario=interaction
            --frames=2
            --cycles=10)
    set_tests_properties(
        AeroControlGalleryInteraction
        AeroControlGalleryVirtualization
        AeroControlGalleryBatchingDisabled
        PROPERTIES
            LABELS "conformance;product"
            TIMEOUT 60)
    set_tests_properties(
        AeroControlGalleryLifecycle
        PROPERTIES
            LABELS "conformance;lifecycle;product"
            TIMEOUT 120)
endif()
