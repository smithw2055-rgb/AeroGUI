include_guard(GLOBAL)

function(aero_configure_platform_window_sources)
    if(NOT TARGET AeroPlatform)
        message(FATAL_ERROR
            "AeroPlatform must exist before native window sources are configured")
    endif()

    set(_aero_platform_source_root
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/..")
    target_sources(AeroPlatform PRIVATE
        "${_aero_platform_source_root}/src/platform/Win32Window.cpp"
        "${_aero_platform_source_root}/src/platform/X11Window.cpp")

    if(WIN32)
        target_link_libraries(AeroPlatform PRIVATE user32)
    endif()

    if(AERO_ENABLE_GLX_SURFACE)
        if(NOT TARGET X11::X11)
            message(FATAL_ERROR
                "X11::X11 is required by the enabled X11 window carrier")
        endif()
        target_compile_definitions(AeroPlatform PRIVATE
            AERO_PLATFORM_HAS_X11_WINDOW=1)
        target_link_libraries(AeroPlatform PRIVATE X11::X11)
    else()
        target_compile_definitions(AeroPlatform PRIVATE
            AERO_PLATFORM_HAS_X11_WINDOW=0)
    endif()
endfunction()

cmake_language(DEFER CALL aero_configure_platform_window_sources)
