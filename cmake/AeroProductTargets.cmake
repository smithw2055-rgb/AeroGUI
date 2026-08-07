# AeroGui is the single embeddable product binary. It owns the WPF/XAML object
# model together with View runtime, providers, native rendering and backend
# factories. App adds only the default desktop lifetime and OS window policy.
set(_aero_gui_runtime_sources
    src/render/RenderTarget.cpp
    src/render/opengl33/OpenGL33Device.cpp
    src/render/opengl33/OpenGL33Embedded.cpp
    src/render/opengl33/OpenGL33Factories.cpp
    src/providers/XamlProvider.cpp
    src/platform/Clipboard.cpp)
if(WIN32)
    list(APPEND _aero_gui_runtime_sources
        src/render/d3d11/D3D11Device.cpp
        src/render/d3d11/D3D11Factories.cpp)
endif()

target_sources(AeroGui PRIVATE
    ${_aero_gui_runtime_sources}
    $<TARGET_OBJECTS:AeroModuleSetObjects>
    $<TARGET_OBJECTS:AeroTextFreeTypeObjects>
    $<TARGET_OBJECTS:AeroTextHarfBuzzObjects>
    $<TARGET_OBJECTS:AeroRuntimeObjects>
    $<TARGET_OBJECTS:AeroRenderingObjects>)
target_include_directories(AeroGui PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(AeroGui PRIVATE
    Aero::Audio freetype harfbuzz)
if(AERO_ENABLE_WGL_SURFACE)
    target_link_libraries(AeroGui PRIVATE
        gdi32 opengl32 user32)
endif()
if(AERO_ENABLE_GLX_SURFACE)
    target_link_libraries(AeroGui PRIVATE
        X11::X11 OpenGL::GL Threads::Threads)
endif()
if(AERO_ENABLE_D3D11_BACKEND)
    target_link_libraries(AeroGui PRIVATE
        d3d11 dxgi d3dcompiler)
endif()
target_compile_definitions(AeroGui PRIVATE
    AERO_HAS_WGL_SURFACE=$<BOOL:${AERO_ENABLE_WGL_SURFACE}>
    AERO_HAS_GLX_SURFACE=$<BOOL:${AERO_ENABLE_GLX_SURFACE}>)

add_library(AeroRuntimeHeaderConsumer OBJECT
    tools/sdk-consumers/GuiRuntimeConsumer.cpp)
target_link_libraries(
    AeroRuntimeHeaderConsumer PRIVATE Aero::Gui)
aero_apply_compiler_options(AeroRuntimeHeaderConsumer)

add_library(AeroProvidersHeaderConsumer OBJECT
    tools/sdk-consumers/ProvidersConsumer.cpp)
target_link_libraries(
    AeroProvidersHeaderConsumer PRIVATE Aero::Gui)
aero_apply_compiler_options(AeroProvidersHeaderConsumer)

add_library(AeroD3D11HeaderConsumer OBJECT
    tools/sdk-consumers/D3D11Consumer.cpp)
target_link_libraries(
    AeroD3D11HeaderConsumer PRIVATE Aero::Gui)
aero_apply_compiler_options(AeroD3D11HeaderConsumer)

add_library(AeroOpenGL33HeaderConsumer OBJECT
    tools/sdk-consumers/OpenGL33Consumer.cpp)
target_link_libraries(
    AeroOpenGL33HeaderConsumer PRIVATE Aero::Gui)
aero_apply_compiler_options(AeroOpenGL33HeaderConsumer)

set(_aero_app_sources
    src/platform/WindowWait.cpp
    src/app/ApplicationRun.cpp
    src/app/DesktopHost.cpp
    src/app/RenderContext.cpp
    src/app/Window.cpp)
if(WIN32)
    list(APPEND _aero_app_sources
        src/platform/win32/Clipboard.cpp
        src/platform/win32/Ime.cpp
        src/platform/win32/Window.cpp)
else()
    list(APPEND _aero_app_sources
        src/platform/x11/Window.cpp)
endif()

add_library(AeroApp ${AERO_LIBRARY_TYPE}
    ${_aero_app_sources})
add_library(Aero::App ALIAS AeroApp)
target_sources(AeroApp PRIVATE
    $<TARGET_OBJECTS:AeroAppModelObjects>)
target_include_directories(AeroApp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AeroApp PUBLIC Aero::Gui)
if(WIN32)
    target_link_libraries(AeroApp PRIVATE user32 imm32)
elseif(AERO_ENABLE_GLX_SURFACE)
    if(NOT TARGET X11::X11)
        message(FATAL_ERROR
            "X11::X11 is required by the enabled default X11 App host")
    endif()
    target_link_libraries(AeroApp PRIVATE X11::X11)
endif()
target_compile_definitions(AeroApp PRIVATE
    AERO_APP_HAS_D3D11=$<BOOL:${AERO_ENABLE_D3D11_BACKEND}>
    "AERO_APP_HAS_OPENGL_WINDOW=$<OR:$<BOOL:${AERO_ENABLE_WGL_SURFACE}>,$<BOOL:${AERO_ENABLE_GLX_SURFACE}>>"
    AERO_PLATFORM_HAS_X11_WINDOW=$<BOOL:${AERO_ENABLE_GLX_SURFACE}>)
target_compile_features(AeroApp PUBLIC cxx_std_17)
set_target_properties(AeroApp PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroApp)

add_library(AeroProductHeaderConsumer OBJECT
    tools/sdk-consumers/ProductConsumer.cpp)
target_link_libraries(AeroProductHeaderConsumer PRIVATE Aero::App)
aero_apply_compiler_options(AeroProductHeaderConsumer)

# Product dependency direction is intentionally short and public. AeroGui owns
# the complete embeddable runtime; App owns only Application/Window and desktop
# host policy.
if(TARGET AeroIntegration OR TARGET Aero::Integration)
    message(FATAL_ERROR
        "The retired Integration product target must not be recreated")
endif()
get_target_property(_aero_gui_sources_property AeroGui SOURCES)
foreach(_aero_gui_component IN ITEMS
        AeroModuleSetObjects
        AeroTextFreeTypeObjects
        AeroTextHarfBuzzObjects
        AeroRuntimeObjects
        AeroRenderingObjects)
    if(NOT "${_aero_gui_sources_property}" MATCHES
            "${_aero_gui_component}")
        message(FATAL_ERROR
            "Gui must fold the embeddable component: ${_aero_gui_component}")
    endif()
endforeach()
if("${_aero_gui_sources_property}" MATCHES "AeroAppModelObjects")
    message(FATAL_ERROR
        "Gui must not fold the optional Application/Window object model")
endif()
get_target_property(_aero_app_sources_property AeroApp SOURCES)
if(NOT "${_aero_app_sources_property}" MATCHES
        "AeroAppModelObjects")
    message(FATAL_ERROR
        "App must own the Application/Window object model")
endif()
get_target_property(_aero_meta_links
    AeroMeta INTERFACE_LINK_LIBRARIES)
if(NOT "${_aero_meta_links}" STREQUAL "Aero::Gui")
    message(FATAL_ERROR "AeroMeta must remain a Gui-only authoring facade")
endif()
unset(_aero_gui_runtime_sources)
unset(_aero_gui_sources_property)
unset(_aero_gui_component)
unset(_aero_app_sources_property)
unset(_aero_meta_links)
