add_library(AeroControlGalleryModule STATIC
    StatusBadge.cpp)
target_include_directories(AeroControlGalleryModule
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(AeroControlGalleryModule
    PUBLIC Aero::Markup)
target_compile_features(AeroControlGalleryModule
    PUBLIC cxx_std_17)
set_target_properties(AeroControlGalleryModule PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
aero_apply_compiler_options(
    AeroControlGalleryModule)

add_executable(aero-controlgallery-xamlc
    XamlCompiler.cpp)
target_link_libraries(aero-controlgallery-xamlc
    PRIVATE AeroControlGalleryModule)
target_compile_features(
    aero-controlgallery-xamlc PRIVATE cxx_std_17)
set_target_properties(
    aero-controlgallery-xamlc PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
aero_apply_compiler_options(
    aero-controlgallery-xamlc)

set(_aero_control_gallery_assets
    "${CMAKE_CURRENT_BINARY_DIR}/assets")
set(_aero_control_gallery_compiled
    "${_aero_control_gallery_assets}/ControlGallery.axir")
set(_aero_control_gallery_source_copy
    "${_aero_control_gallery_assets}/ControlGallery.xaml")
set(_aero_control_gallery_theme_dir
    "${_aero_control_gallery_assets}/themes")
set(_aero_control_gallery_theme_outputs
    "${_aero_control_gallery_theme_dir}/Generic.xaml"
    "${_aero_control_gallery_theme_dir}/Light.xaml"
    "${_aero_control_gallery_theme_dir}/Dark.xaml")

add_custom_command(
    OUTPUT
        "${_aero_control_gallery_compiled}"
        "${_aero_control_gallery_source_copy}"
        ${_aero_control_gallery_theme_outputs}
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${_aero_control_gallery_assets}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${_aero_control_gallery_theme_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/ControlGallery.xaml"
        "${_aero_control_gallery_source_copy}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_SOURCE_DIR}/themes/Generic.xaml"
        "${_aero_control_gallery_theme_dir}/Generic.xaml"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_SOURCE_DIR}/themes/Light.xaml"
        "${_aero_control_gallery_theme_dir}/Light.xaml"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_SOURCE_DIR}/themes/Dark.xaml"
        "${_aero_control_gallery_theme_dir}/Dark.xaml"
    COMMAND "$<TARGET_FILE:aero-controlgallery-xamlc>"
        "${CMAKE_CURRENT_SOURCE_DIR}/ControlGallery.xaml"
        "${_aero_control_gallery_compiled}"
    DEPENDS
        aero-controlgallery-xamlc
        "${CMAKE_CURRENT_SOURCE_DIR}/ControlGallery.xaml"
        "${CMAKE_SOURCE_DIR}/themes/Generic.xaml"
        "${CMAKE_SOURCE_DIR}/themes/Light.xaml"
        "${CMAKE_SOURCE_DIR}/themes/Dark.xaml"
    VERBATIM)
add_custom_target(AeroControlGalleryAssets ALL
    DEPENDS
        "${_aero_control_gallery_compiled}"
        "${_aero_control_gallery_source_copy}"
        ${_aero_control_gallery_theme_outputs})

add_executable(AeroControlGallery
    GalleryRuntime.cpp
    NativeGallery.cpp
    main.cpp)
target_link_libraries(AeroControlGallery
    PRIVATE AeroControlGalleryModule)
target_compile_definitions(AeroControlGallery PRIVATE
    "AERO_CONTROL_GALLERY_ASSET_DIR=\"${_aero_control_gallery_assets}\"")
target_compile_features(AeroControlGallery
    PRIVATE cxx_std_17)
set_target_properties(AeroControlGallery PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroControlGallery)
add_dependencies(
    AeroControlGallery
    AeroControlGalleryAssets)

if(WIN32 AND AERO_ENABLE_D3D11_BACKEND
        AND AERO_ENABLE_WGL_SURFACE)
    target_sources(AeroControlGallery PRIVATE
        NativeGalleryWindows.cpp)
    target_link_libraries(AeroControlGallery
        PRIVATE
            Aero::RenderD3D11
            Aero::RenderOpenGL33
            Aero::RhiWGL
            user32)
    target_compile_definitions(
        AeroControlGallery PRIVATE
        AERO_CONTROL_GALLERY_WINDOWS_NATIVE=1)
elseif(AERO_ENABLE_GLX_SURFACE)
    target_sources(AeroControlGallery PRIVATE
        NativeGalleryGlx.cpp)
    target_link_libraries(AeroControlGallery
        PRIVATE
            Aero::RenderOpenGL33
            Aero::RhiGLX
            X11::X11)
    target_compile_definitions(
        AeroControlGallery PRIVATE
        AERO_CONTROL_GALLERY_GLX_NATIVE=1)
endif()

if(AERO_BUILD_TESTS)
    add_test(
        NAME AeroControlGallerySmoke
        COMMAND AeroControlGallery
            --backend=null
            --xaml=both
            --theme=both
            --simulate-context-loss)
    set_tests_properties(
        AeroControlGallerySmoke PROPERTIES
        TIMEOUT 60)
    if(WIN32 AND AERO_ENABLE_D3D11_BACKEND
            AND AERO_ENABLE_WGL_SURFACE)
        add_test(
            NAME AeroControlGalleryD3D11Smoke
            COMMAND AeroControlGallery
                --backend=d3d11
                --xaml=compiled
                --theme=light
                --simulate-context-loss)
        add_test(
            NAME AeroControlGalleryWglSmoke
            COMMAND AeroControlGallery
                --backend=opengl
                --xaml=compiled
                --theme=dark
                --simulate-context-loss)
        set_tests_properties(
            AeroControlGalleryD3D11Smoke
            AeroControlGalleryWglSmoke
            PROPERTIES TIMEOUT 60)
    elseif(AERO_ENABLE_GLX_SURFACE)
        add_test(
            NAME AeroControlGalleryGlxSmoke
            COMMAND AeroControlGallery
                --backend=opengl
                --xaml=compiled
                --theme=dark
                --simulate-context-loss)
        set_tests_properties(
            AeroControlGalleryGlxSmoke
            PROPERTIES TIMEOUT 60
            SKIP_RETURN_CODE 77)
    endif()
endif()
