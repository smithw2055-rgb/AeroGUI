# Public Integration/App products and SDK dependency checks.
set(_aero_integration_sources
    src/integration/HostedGraphics.cpp
    src/integration/OpenGL33Endpoint.cpp)
if(WIN32)
    list(APPEND _aero_integration_sources
        src/integration/D3D11Endpoint.cpp)
endif()

add_library(AeroIntegration ${AERO_LIBRARY_TYPE}
    ${_aero_integration_sources})
add_library(Aero::Integration ALIAS AeroIntegration)
target_include_directories(AeroIntegration
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AeroIntegration
    PUBLIC AeroRuntime
    PRIVATE
        Aero::_DetailRender
        Aero::_DetailGraphics
        Aero::_DetailRenderOpenGL33
        Aero::_DetailGraphicsOpenGL33)
if(WIN32)
    target_link_libraries(AeroIntegration PRIVATE
        Aero::_DetailRenderD3D11
        Aero::_DetailGraphicsD3D11)
    if(TARGET AeroPlatformWGL)
        target_link_libraries(
            AeroIntegration PRIVATE Aero::_DetailPlatformWGL)
    endif()
elseif(TARGET AeroPlatformGLX)
    target_link_libraries(
        AeroIntegration PRIVATE Aero::_DetailPlatformGLX)
endif()
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
aero_apply_compiler_options(
    AeroD3D11IntegrationHeaderConsumer)

add_library(AeroOpenGL33IntegrationHeaderConsumer OBJECT
    tools/sdk-consumers/OpenGL33IntegrationConsumer.cpp)
target_link_libraries(
    AeroOpenGL33IntegrationHeaderConsumer PRIVATE Aero::Integration)
aero_apply_compiler_options(
    AeroOpenGL33IntegrationHeaderConsumer)

add_library(AeroHostedGraphicsHeaderConsumer OBJECT
    tools/sdk-consumers/HostedGraphicsConsumer.cpp)
target_link_libraries(
    AeroHostedGraphicsHeaderConsumer PRIVATE Aero::Integration)
aero_apply_compiler_options(
    AeroHostedGraphicsHeaderConsumer)

add_library(AeroApp ${AERO_LIBRARY_TYPE}
    src/app/Launcher.cpp
    src/app/Window.cpp)
add_library(Aero::App ALIAS AeroApp)
target_include_directories(AeroApp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AeroApp
    PUBLIC
        Aero::Integration
        Aero::Gui
        Aero::_DetailAppModel)
target_compile_definitions(AeroApp PRIVATE
    AERO_APP_HAS_D3D11=$<BOOL:${AERO_ENABLE_D3D11_BACKEND}>
    "AERO_APP_HAS_OPENGL_WINDOW=$<OR:$<BOOL:${AERO_ENABLE_WGL_SURFACE}>,$<BOOL:${AERO_ENABLE_GLX_SURFACE}>>")
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

# Keep the public SDK dependency graph pointed in one direction. The runtime
# owns no GPU backend, while Meta remains a header-only authoring facade.
get_target_property(_aero_runtime_links AeroRuntime LINK_LIBRARIES)
if("${_aero_runtime_links}" MATCHES
        "(^|;)(Aero::_DetailGraphics|Aero::_DetailRender|AeroGraphics|AeroRender)")
    message(FATAL_ERROR
        "AeroRuntime must not link render or RHI implementation targets")
endif()
get_target_property(
    _aero_meta_links AeroMeta INTERFACE_LINK_LIBRARIES)
if("${_aero_meta_links}" MATCHES
        "(^|;)(AeroModuleCatalog|Aero::_DetailModuleCatalog)(;|$)")
    message(FATAL_ERROR
        "AeroMeta must not link the ModuleCatalog implementation")
endif()
unset(_aero_runtime_links)
unset(_aero_meta_links)
