set(_aero_app_sources
    src/app/Application.cpp
    src/app/Metadata.cpp
    src/app/platform/WindowWait.cpp
    src/app/ApplicationRun.cpp
    src/app/DesktopHost.cpp
    src/app/OpenGL33RenderContext.cpp
    src/app/Presentation.cpp
    src/app/RenderContext.cpp
    src/app/Window.cpp)
if(WIN32)
    list(APPEND _aero_app_sources
        src/app/D3D11RenderContext.cpp
        src/app/platform/win32/Clipboard.cpp
        src/app/platform/win32/Ime.cpp
        src/app/platform/win32/OpenGLWindow.cpp
        src/app/platform/win32/Window.cpp)
else()
    list(APPEND _aero_app_sources
        src/app/platform/x11/Window.cpp)
    if(AERO_ENABLE_GLX_SURFACE)
        list(APPEND _aero_app_sources
            src/app/platform/x11/OpenGLWindow.cpp)
    endif()
endif()

add_library(AeroApp ${AERO_LIBRARY_TYPE} ${_aero_app_sources})
add_library(Aero::App ALIAS AeroApp)
target_include_directories(AeroApp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AeroApp PUBLIC Aero::Gui)
if(TARGET Aero::RenderD3D11)
    target_link_libraries(AeroApp PRIVATE Aero::RenderD3D11)
endif()
if(TARGET Aero::RenderOpenGL33 AND
   (AERO_ENABLE_WGL_SURFACE OR AERO_ENABLE_GLX_SURFACE))
    target_link_libraries(AeroApp PRIVATE Aero::RenderOpenGL33)
endif()
if(WIN32)
    target_link_libraries(AeroApp PRIVATE user32 imm32 d3d11 dxgi gdi32 opengl32)
elseif(AERO_ENABLE_GLX_SURFACE)
    if(NOT TARGET X11::X11)
        message(FATAL_ERROR
            "X11::X11 is required by the enabled default X11 App host")
    endif()
    target_link_libraries(AeroApp PRIVATE X11::X11 OpenGL::GL)
endif()
target_compile_definitions(AeroApp PRIVATE
    $<$<BOOL:${AERO_BUILD_SHARED}>:AERO_APP_EXPORTS>
    AERO_APP_HAS_D3D11=$<BOOL:${AERO_ENABLE_D3D11_BACKEND}>
    "AERO_APP_HAS_OPENGL_WINDOW=$<OR:$<BOOL:${AERO_ENABLE_WGL_SURFACE}>,$<BOOL:${AERO_ENABLE_GLX_SURFACE}>>"
    AERO_PLATFORM_HAS_X11_WINDOW=$<BOOL:${AERO_ENABLE_GLX_SURFACE}>)
target_compile_features(AeroApp PUBLIC cxx_std_17)
set_target_properties(AeroApp PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)
aero_apply_compiler_options(AeroApp)
aero_verify_windows_exports(AeroApp "Application@Aero@@")

add_library(AeroProductHeaderConsumer OBJECT
    tools/sdk-consumers/ProductConsumer.cpp)
target_link_libraries(AeroProductHeaderConsumer PRIVATE Aero::App)
aero_apply_compiler_options(AeroProductHeaderConsumer)

# Product dependency direction is intentionally short. AeroGui is backend
# neutral, RenderD3D11/RenderOpenGL33 provide opt-in native factories, and App
# composes the enabled desktop defaults behind Application/Window.
if(TARGET AeroIntegration OR TARGET Aero::Integration)
    message(FATAL_ERROR
        "The retired Integration product target must not be recreated")
endif()
get_target_property(_aero_gui_sources_property AeroGui SOURCES)
if("${_aero_gui_sources_property}" MATCHES "Aero(AppModel|Runtime|Rendering|GuiKernel|Controls|Markup).*Objects")
    message(FATAL_ERROR "Gui product sources must not reintroduce implementation object-library layers")
endif()
if("${_aero_gui_sources_property}" MATCHES "src/render/(d3d11|opengl33)/")
    message(FATAL_ERROR
        "AeroGui must remain backend neutral; native sources belong to RenderD3D11 or RenderOpenGL33")
endif()
if(NOT TARGET AeroRenderOpenGL33 OR NOT TARGET Aero::RenderOpenGL33)
    message(FATAL_ERROR "The OpenGL33 backend product target is missing")
endif()
if(AERO_ENABLE_D3D11_BACKEND AND
   (NOT TARGET AeroRenderD3D11 OR NOT TARGET Aero::RenderD3D11))
    message(FATAL_ERROR "The enabled D3D11 backend product target is missing")
endif()
get_target_property(_aero_app_sources_property AeroApp SOURCES)
if(NOT "${_aero_app_sources_property}" MATCHES "src/app/Application.cpp")
    message(FATAL_ERROR "App must own the Application/Window object model")
endif()
unset(_aero_gui_runtime_sources)
unset(_aero_gui_sources_property)
unset(_aero_gui_component)
unset(_aero_app_sources_property)
