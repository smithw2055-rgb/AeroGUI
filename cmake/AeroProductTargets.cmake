# Integration folds View runtime, rendering, text providers and schema
# composition into one product binary. App layers only the default desktop
# lifetime and OS window implementation over that product.
set(_aero_integration_sources
    src/integration/OpenGL33Device.cpp
    src/integration/SourceProvider.cpp
    src/platform/Clipboard.cpp)
if(WIN32)
    list(APPEND _aero_integration_sources
        src/integration/D3D11Device.cpp)
endif()

add_library(AeroIntegration ${AERO_LIBRARY_TYPE}
    ${_aero_integration_sources})
add_library(Aero::Integration ALIAS AeroIntegration)
target_sources(AeroIntegration PRIVATE
    $<TARGET_OBJECTS:AeroAppModelObjects>
    $<TARGET_OBJECTS:AeroModuleSetObjects>
    $<TARGET_OBJECTS:AeroTextFreeTypeObjects>
    $<TARGET_OBJECTS:AeroTextHarfBuzzObjects>
    $<TARGET_OBJECTS:AeroRuntimeObjects>
    $<TARGET_OBJECTS:AeroRenderingObjects>)
target_include_directories(AeroIntegration
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(AeroIntegration
    PUBLIC Aero::Gui
    PRIVATE freetype harfbuzz)
if(AERO_ENABLE_WGL_SURFACE)
    target_link_libraries(AeroIntegration PRIVATE
        gdi32 opengl32 user32)
endif()
if(AERO_ENABLE_GLX_SURFACE)
    target_link_libraries(AeroIntegration PRIVATE
        X11::X11 OpenGL::GL Threads::Threads)
endif()
if(AERO_ENABLE_D3D11_BACKEND)
    target_link_libraries(AeroIntegration PRIVATE
        d3d11 dxgi d3dcompiler)
endif()
target_compile_definitions(AeroIntegration PRIVATE
    AERO_HAS_WGL_SURFACE=$<BOOL:${AERO_ENABLE_WGL_SURFACE}>
    AERO_HAS_GLX_SURFACE=$<BOOL:${AERO_ENABLE_GLX_SURFACE}>)
target_compile_features(AeroIntegration PUBLIC cxx_std_17)
set_target_properties(AeroIntegration PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroIntegration)

add_library(AeroIntegrationHeaderConsumer OBJECT
    tools/sdk-consumers/IntegrationConsumer.cpp)
target_link_libraries(
    AeroIntegrationHeaderConsumer PRIVATE Aero::Integration)
aero_apply_compiler_options(AeroIntegrationHeaderConsumer)

add_library(AeroD3D11IntegrationHeaderConsumer OBJECT
    tools/sdk-consumers/D3D11IntegrationConsumer.cpp)
target_link_libraries(
    AeroD3D11IntegrationHeaderConsumer PRIVATE Aero::Integration)
aero_apply_compiler_options(AeroD3D11IntegrationHeaderConsumer)

add_library(AeroOpenGL33IntegrationHeaderConsumer OBJECT
    tools/sdk-consumers/OpenGL33IntegrationConsumer.cpp)
target_link_libraries(
    AeroOpenGL33IntegrationHeaderConsumer PRIVATE Aero::Integration)
aero_apply_compiler_options(AeroOpenGL33IntegrationHeaderConsumer)

set(_aero_app_sources
    src/app/ApplicationRun.cpp
    src/app/DesktopHost.cpp
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
target_include_directories(AeroApp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AeroApp
    PUBLIC Aero::Integration Aero::Gui)
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

# Product dependency direction is intentionally short and public.
get_target_property(_aero_integration_links
    AeroIntegration LINK_LIBRARIES)
if("${_aero_integration_links}" MATCHES
        "Aero(Runtime|Rendering|ModuleSet|AppModel|Controls|Markup|GuiKernel)")
    message(FATAL_ERROR
        "Integration must fold internal object components, not link support binaries")
endif()
get_target_property(_aero_meta_links
    AeroMeta INTERFACE_LINK_LIBRARIES)
if(NOT "${_aero_meta_links}" STREQUAL "Aero::Gui")
    message(FATAL_ERROR "AeroMeta must remain a Gui-only authoring facade")
endif()
unset(_aero_integration_links)
unset(_aero_meta_links)
