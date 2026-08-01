# Private renderer, graphics device, backend and native-surface targets.
add_library(AeroGraphics ${AERO_LIBRARY_TYPE}
    src/graphics/Graphics.cpp
    src/graphics/GraphicsDevice.cpp
    src/platform/Surface.cpp)

add_library(Aero::_DetailGraphics ALIAS AeroGraphics)

target_include_directories(AeroGraphics
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(AeroGraphics PUBLIC Aero::Base)
target_compile_features(AeroGraphics PUBLIC cxx_std_17)
set_target_properties(AeroGraphics PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})

aero_apply_compiler_options(AeroGraphics)

add_library(AeroGraphicsOpenGL33 ${AERO_LIBRARY_TYPE}
    src/graphics/opengl33/OpenGL33Backend.cpp
    src/graphics/opengl33/OpenGL33Context.cpp
    src/graphics/opengl33/OpenGL33StateCache.cpp)
add_library(Aero::_DetailGraphicsOpenGL33 ALIAS AeroGraphicsOpenGL33)
target_include_directories(AeroGraphicsOpenGL33
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(
    AeroGraphicsOpenGL33 PUBLIC Aero::_DetailGraphics)
target_compile_features(AeroGraphicsOpenGL33 PUBLIC cxx_std_17)
target_compile_definitions(
    AeroGraphicsOpenGL33 PUBLIC AERO_HAS_OPENGL33_BACKEND=1)
set_target_properties(AeroGraphicsOpenGL33 PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroGraphicsOpenGL33)

if(AERO_ENABLE_WGL_SURFACE)
    if(NOT WIN32)
        message(FATAL_ERROR
            "AERO_ENABLE_WGL_SURFACE is only supported on Windows")
    endif()
    add_library(AeroPlatformWGL ${AERO_LIBRARY_TYPE}
        src/platform/win32/OpenGLSurface.cpp)
    add_library(Aero::_DetailPlatformWGL ALIAS AeroPlatformWGL)
    target_include_directories(AeroPlatformWGL
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(AeroPlatformWGL
        PUBLIC Aero::_DetailGraphicsOpenGL33
        PRIVATE gdi32 opengl32 user32)
    target_compile_features(AeroPlatformWGL PUBLIC cxx_std_17)
    target_compile_definitions(
        AeroPlatformWGL PUBLIC AERO_HAS_WGL_SURFACE=1)
    set_target_properties(AeroPlatformWGL PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
        WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
    aero_apply_compiler_options(AeroPlatformWGL)
endif()

if(AERO_ENABLE_GLX_SURFACE)
    if(NOT UNIX OR APPLE)
        message(FATAL_ERROR
            "AERO_ENABLE_GLX_SURFACE is only supported on Unix/X11")
    endif()
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    add_library(AeroPlatformGLX ${AERO_LIBRARY_TYPE}
        src/platform/x11/OpenGLSurface.cpp)
    add_library(Aero::_DetailPlatformGLX ALIAS AeroPlatformGLX)
    target_include_directories(AeroPlatformGLX
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(AeroPlatformGLX
        PUBLIC Aero::_DetailGraphicsOpenGL33
        PRIVATE X11::X11 OpenGL::GL Threads::Threads)
    target_compile_features(AeroPlatformGLX PUBLIC cxx_std_17)
    target_compile_definitions(
        AeroPlatformGLX PUBLIC AERO_HAS_GLX_SURFACE=1)
    set_target_properties(AeroPlatformGLX PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
        WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
    aero_apply_compiler_options(AeroPlatformGLX)
endif()

add_library(AeroRender ${AERO_LIBRARY_TYPE}
    src/render/Renderer.cpp
    src/render/TextRuntimeService.cpp)
add_library(Aero::_DetailRender ALIAS AeroRender)
target_include_directories(AeroRender
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(
    AeroRender PUBLIC Aero::_DetailCore Aero::_DetailGraphics)
target_compile_features(AeroRender PUBLIC cxx_std_17)
set_target_properties(AeroRender PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroRender)

add_library(AeroRenderOpenGL33 ${AERO_LIBRARY_TYPE}
    src/render/opengl33/OpenGL33Renderer.cpp)
add_library(
    Aero::_DetailRenderOpenGL33 ALIAS AeroRenderOpenGL33)
target_include_directories(AeroRenderOpenGL33
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AeroRenderOpenGL33
    PUBLIC
        Aero::_DetailRender
        Aero::_DetailGraphicsOpenGL33)
target_compile_features(AeroRenderOpenGL33 PUBLIC cxx_std_17)
set_target_properties(AeroRenderOpenGL33 PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroRenderOpenGL33)

if(AERO_ENABLE_SOKOL_BACKEND)
    if(AERO_SOKOL_BRIDGE_SOURCE STREQUAL "")
        message(FATAL_ERROR
            "AERO_ENABLE_SOKOL_BACKEND requires AERO_SOKOL_BRIDGE_SOURCE")
    endif()
    if(NOT EXISTS "${AERO_SOKOL_BRIDGE_SOURCE}")
        message(FATAL_ERROR
            "AERO_SOKOL_BRIDGE_SOURCE does not exist: ${AERO_SOKOL_BRIDGE_SOURCE}")
    endif()

    target_sources(AeroGraphics PRIVATE "${AERO_SOKOL_BRIDGE_SOURCE}")
    if(NOT AERO_SOKOL_INCLUDE_DIR STREQUAL "")
        target_include_directories(
            AeroGraphics PRIVATE "${AERO_SOKOL_INCLUDE_DIR}")
    endif()
    target_compile_definitions(AeroGraphics PUBLIC AERO_HAS_SOKOL_BACKEND=1)
else()
    target_compile_definitions(AeroGraphics PUBLIC AERO_HAS_SOKOL_BACKEND=0)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC diagnoses a reference obtained through a temporary Span view even
    # though the view points into the longer-lived immutable RenderFrame.
    target_compile_options(AeroGraphics PRIVATE -Wno-dangling-reference)
endif()

if(AERO_ENABLE_D3D11_BACKEND)
    if(NOT WIN32)
        message(FATAL_ERROR
            "AERO_ENABLE_D3D11_BACKEND is only supported on Windows")
    endif()

    set(_aero_d3d11_render_frame_shader_source
        "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/d3d11/shaders/RenderFrameRect.hlsl")
    set(_aero_d3d11_render_frame_image_shader_source
        "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/d3d11/shaders/RenderFrameImage.hlsl")
    set(_aero_d3d11_render_frame_mesh_shader_source
        "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/d3d11/shaders/RenderFrameMesh.hlsl")
    set(_aero_d3d11_render_frame_glyph_shader_source
        "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/d3d11/shaders/RenderFrameGlyph.hlsl")
    set(_aero_d3d11_render_frame_max_rectangle_instances 64)
    set(_aero_d3d11_shader_directory
        "${CMAKE_CURRENT_BINARY_DIR}/generated/d3d11-shaders")
    set(_aero_d3d11_render_frame_vs_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RenderFrameVertexShader.hpp")
    set(_aero_d3d11_render_frame_ps_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RenderFramePixelShader.hpp")
    set(_aero_d3d11_render_frame_image_vs_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RenderFrameImageVertexShader.hpp")
    set(_aero_d3d11_render_frame_image_ps_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RenderFrameImagePixelShader.hpp")
    set(_aero_d3d11_render_frame_mesh_vs_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RenderFrameMeshVertexShader.hpp")
    set(_aero_d3d11_render_frame_mesh_ps_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RenderFrameMeshPixelShader.hpp")
    set(_aero_d3d11_render_frame_glyph_vs_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RenderFrameGlyphVertexShader.hpp")
    set(_aero_d3d11_render_frame_glyph_ps_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RenderFrameGlyphPixelShader.hpp")
    set(_aero_d3d11_fxc_hints
        "$ENV{WindowsSdkDir}bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64")
    file(GLOB _aero_d3d11_fxc_sdk_directories
        LIST_DIRECTORIES true
        "C:/Program Files (x86)/Windows Kits/10/bin/*/x64")
    list(APPEND _aero_d3d11_fxc_hints ${_aero_d3d11_fxc_sdk_directories})
    find_program(AERO_D3D11_FXC_EXECUTABLE
        NAMES fxc.exe
        HINTS ${_aero_d3d11_fxc_hints}
        DOC "Windows SDK fxc executable used to compile Aero D3D11 shaders")
    if(NOT AERO_D3D11_FXC_EXECUTABLE)
        message(FATAL_ERROR
            "AERO_ENABLE_D3D11_BACKEND requires the Windows SDK x64 fxc.exe")
    endif()
    add_custom_command(
        OUTPUT "${_aero_d3d11_render_frame_vs_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T vs_4_0 /E vs_main
            /D AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_frame_max_rectangle_instances}
            /Vn AeroD3D11RenderFrameVertexShader
            /Fh "${_aero_d3d11_render_frame_vs_header}"
            "${_aero_d3d11_render_frame_shader_source}"
        DEPENDS "${_aero_d3d11_render_frame_shader_source}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_aero_d3d11_render_frame_ps_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T ps_4_0 /E ps_main
            /D AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_frame_max_rectangle_instances}
            /Vn AeroD3D11RenderFramePixelShader
            /Fh "${_aero_d3d11_render_frame_ps_header}"
            "${_aero_d3d11_render_frame_shader_source}"
        DEPENDS "${_aero_d3d11_render_frame_shader_source}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_aero_d3d11_render_frame_image_vs_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T vs_4_0 /E vs_main
            /D AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_frame_max_rectangle_instances}
            /Vn AeroD3D11RenderFrameImageVertexShader
            /Fh "${_aero_d3d11_render_frame_image_vs_header}"
            "${_aero_d3d11_render_frame_image_shader_source}"
        DEPENDS "${_aero_d3d11_render_frame_image_shader_source}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_aero_d3d11_render_frame_image_ps_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T ps_4_0 /E ps_main
            /D AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_frame_max_rectangle_instances}
            /Vn AeroD3D11RenderFrameImagePixelShader
            /Fh "${_aero_d3d11_render_frame_image_ps_header}"
            "${_aero_d3d11_render_frame_image_shader_source}"
        DEPENDS "${_aero_d3d11_render_frame_image_shader_source}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_aero_d3d11_render_frame_mesh_vs_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T vs_4_0 /E vs_main
            /D AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_frame_max_rectangle_instances}
            /Vn AeroD3D11RenderFrameMeshVertexShader
            /Fh "${_aero_d3d11_render_frame_mesh_vs_header}"
            "${_aero_d3d11_render_frame_mesh_shader_source}"
        DEPENDS "${_aero_d3d11_render_frame_mesh_shader_source}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_aero_d3d11_render_frame_mesh_ps_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T ps_4_0 /E ps_main
            /D AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_frame_max_rectangle_instances}
            /Vn AeroD3D11RenderFrameMeshPixelShader
            /Fh "${_aero_d3d11_render_frame_mesh_ps_header}"
            "${_aero_d3d11_render_frame_mesh_shader_source}"
        DEPENDS "${_aero_d3d11_render_frame_mesh_shader_source}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_aero_d3d11_render_frame_glyph_vs_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T vs_4_0 /E vs_main
            /D AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_frame_max_rectangle_instances}
            /Vn AeroD3D11RenderFrameGlyphVertexShader
            /Fh "${_aero_d3d11_render_frame_glyph_vs_header}"
            "${_aero_d3d11_render_frame_glyph_shader_source}"
        DEPENDS "${_aero_d3d11_render_frame_glyph_shader_source}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_aero_d3d11_render_frame_glyph_ps_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T ps_4_0 /E ps_main
            /D AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_frame_max_rectangle_instances}
            /Vn AeroD3D11RenderFrameGlyphPixelShader
            /Fh "${_aero_d3d11_render_frame_glyph_ps_header}"
            "${_aero_d3d11_render_frame_glyph_shader_source}"
        DEPENDS "${_aero_d3d11_render_frame_glyph_shader_source}"
        VERBATIM)
    add_custom_target(AeroD3D11RenderFrameShaders
        DEPENDS
            "${_aero_d3d11_render_frame_vs_header}"
            "${_aero_d3d11_render_frame_ps_header}"
            "${_aero_d3d11_render_frame_image_vs_header}"
            "${_aero_d3d11_render_frame_image_ps_header}"
            "${_aero_d3d11_render_frame_mesh_vs_header}"
            "${_aero_d3d11_render_frame_mesh_ps_header}"
            "${_aero_d3d11_render_frame_glyph_vs_header}"
            "${_aero_d3d11_render_frame_glyph_ps_header}")

    set(_aero_d3d11_backend_fragments
        "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/d3d11/D3D11BackendPrivate.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/d3d11/D3D11BackendDevice.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/d3d11/D3D11BackendResources.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/d3d11/D3D11BackendCommands1.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/d3d11/D3D11BackendCommands2.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/d3d11/D3D11BackendCommands3.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/d3d11/D3D11BackendReadback.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/d3d11/D3D11BackendSurface.inc")
    set_source_files_properties(${_aero_d3d11_backend_fragments}
        PROPERTIES HEADER_FILE_ONLY TRUE)
    set_property(SOURCE src/graphics/d3d11/D3D11Backend.cpp APPEND
        PROPERTY OBJECT_DEPENDS "${_aero_d3d11_backend_fragments}")

    add_library(AeroGraphicsD3D11 ${AERO_LIBRARY_TYPE}
        src/graphics/d3d11/D3D11Backend.cpp
        ${_aero_d3d11_backend_fragments})
    add_library(Aero::_DetailGraphicsD3D11 ALIAS AeroGraphicsD3D11)
    target_include_directories(AeroGraphicsD3D11
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(AeroGraphicsD3D11
        PUBLIC Aero::_DetailGraphics
        PRIVATE d3d11 dxgi d3dcompiler)
    target_compile_features(AeroGraphicsD3D11 PUBLIC cxx_std_17)
    set_target_properties(AeroGraphicsD3D11 PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
        WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
    target_compile_definitions(AeroGraphicsD3D11
        PUBLIC AERO_HAS_D3D11_BACKEND=1)
    aero_apply_compiler_options(AeroGraphicsD3D11)

    add_library(AeroRenderD3D11 ${AERO_LIBRARY_TYPE}
        src/render/d3d11/D3D11Renderer.cpp)
    add_library(
        Aero::_DetailRenderD3D11 ALIAS AeroRenderD3D11)
    add_dependencies(AeroRenderD3D11 AeroD3D11RenderFrameShaders)
    target_include_directories(AeroRenderD3D11
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${_aero_d3d11_shader_directory}")
    target_link_libraries(AeroRenderD3D11
        PUBLIC
            Aero::_DetailRender
            Aero::_DetailGraphicsD3D11)
    target_compile_features(AeroRenderD3D11 PUBLIC cxx_std_17)
    set_target_properties(AeroRenderD3D11 PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
        WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
    aero_apply_compiler_options(AeroRenderD3D11)

else()
    target_compile_definitions(AeroGraphics PUBLIC AERO_HAS_D3D11_BACKEND=0)
endif()
