# Private retained renderer, render device, native backends and shader catalogs.
# Renderer is the single semantic command/submission owner.
target_sources(AeroGui PRIVATE
    src/render/RenderCommands.cpp
    src/render/RenderDeviceResources.cpp
    src/render/WindowRenderContext.cpp
    src/render/FrameEncoder.cpp
    src/render/Renderer.cpp
    src/render/TextRenderer.cpp
    src/render/opengl33/OpenGL33Backend.cpp
    src/render/opengl33/OpenGL33Context.cpp
    src/render/opengl33/OpenGL33StateCache.cpp
    src/render/opengl33/OpenGL33Shaders.cpp)
target_compile_definitions(AeroGui PRIVATE AERO_HAS_OPENGL33_BACKEND=1)

if(AERO_ENABLE_WGL_SURFACE)
    if(NOT WIN32)
        message(FATAL_ERROR
            "AERO_ENABLE_WGL_SURFACE is only supported on Windows")
    endif()
    target_sources(AeroGui PRIVATE
        src/render/platform/win32/OpenGLRenderContext.cpp)
    target_link_libraries(AeroGui PRIVATE
        gdi32 opengl32 user32)
    target_compile_definitions(AeroGui PRIVATE AERO_HAS_WGL_SURFACE=1)
else()
    target_compile_definitions(AeroGui PRIVATE AERO_HAS_WGL_SURFACE=0)
endif()

if(AERO_ENABLE_GLX_SURFACE)
    if(NOT UNIX OR APPLE)
        message(FATAL_ERROR
            "AERO_ENABLE_GLX_SURFACE is only supported on Unix/X11")
    endif()
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    target_sources(AeroGui PRIVATE
        src/render/platform/x11/OpenGLRenderContext.cpp)
    target_link_libraries(AeroGui PRIVATE
        X11::X11 OpenGL::GL Threads::Threads)
    target_compile_definitions(AeroGui PRIVATE AERO_HAS_GLX_SURFACE=1)
else()
    target_compile_definitions(AeroGui PRIVATE AERO_HAS_GLX_SURFACE=0)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC diagnoses a reference obtained through a temporary Span view even
    # though the view points into the longer-lived immutable RenderFrame.
    target_compile_options(AeroGui PRIVATE -Wno-dangling-reference)
endif()

if(AERO_ENABLE_D3D11_BACKEND)
    if(NOT WIN32)
        message(FATAL_ERROR
            "AERO_ENABLE_D3D11_BACKEND is only supported on Windows")
    endif()

    set(_aero_d3d11_shader_directory
        "${CMAKE_CURRENT_BINARY_DIR}/generated/d3d11-shaders")
    set(_aero_d3d11_render_frame_max_rectangle_instances 64)
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

    # Keep shader declarations data-driven. Each pair follows the same naming
    # convention consumed by D3D11Shaders.cpp; only the source and instance-limit
    # requirement vary.
    set_property(GLOBAL PROPERTY AERO_D3D11_SHADER_OUTPUTS "")
    function(aero_compile_d3d11_shader_pair stem source use_instance_limit)
        foreach(stage IN ITEMS Vertex Pixel)
            if(stage STREQUAL "Vertex")
                set(profile vs_4_0)
                set(entry vs_main)
            else()
                set(profile ps_4_0)
                set(entry ps_main)
            endif()
            set(output
                "${_aero_d3d11_shader_directory}/AeroD3D11${stem}${stage}Shader.hpp")
            set(symbol "AeroD3D11${stem}${stage}Shader")
            set(defines)
            if(use_instance_limit)
                list(APPEND defines
                    /D "AERO_D3D11_RENDER_PLAN_MAX_RECTANGLE_INSTANCES=${_aero_d3d11_render_frame_max_rectangle_instances}")
            endif()
            add_custom_command(
                OUTPUT "${output}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "${_aero_d3d11_shader_directory}"
                COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
                    /T ${profile} /E ${entry}
                    ${defines}
                    /Vn ${symbol}
                    /Fh "${output}"
                    "${source}"
                DEPENDS "${source}"
                VERBATIM)
            set_property(GLOBAL APPEND PROPERTY
                AERO_D3D11_SHADER_OUTPUTS "${output}")
        endforeach()
    endfunction()

    set(_aero_d3d11_shader_root
        "${CMAKE_CURRENT_SOURCE_DIR}/src/render/d3d11/shaders")
    aero_compile_d3d11_shader_pair(
        RenderFrame "${_aero_d3d11_shader_root}/RenderFrameRect.hlsl" TRUE)
    aero_compile_d3d11_shader_pair(
        RenderFrameImage "${_aero_d3d11_shader_root}/RenderFrameImage.hlsl" TRUE)
    aero_compile_d3d11_shader_pair(
        RenderFrameMask "${_aero_d3d11_shader_root}/RenderFrameMask.hlsl" FALSE)
    aero_compile_d3d11_shader_pair(
        RenderFrameEffect "${_aero_d3d11_shader_root}/RenderFrameEffect.hlsl" FALSE)
    aero_compile_d3d11_shader_pair(
        RenderFrameMesh "${_aero_d3d11_shader_root}/RenderFrameMesh.hlsl" TRUE)
    aero_compile_d3d11_shader_pair(
        RenderFrameGlyph "${_aero_d3d11_shader_root}/RenderFrameGlyph.hlsl" TRUE)
    get_property(_aero_d3d11_shader_outputs GLOBAL PROPERTY
        AERO_D3D11_SHADER_OUTPUTS)
    add_custom_target(AeroD3D11RenderFrameShaders
        DEPENDS ${_aero_d3d11_shader_outputs})

    set(_aero_d3d11_backend_fragments
        "${CMAKE_CURRENT_SOURCE_DIR}/src/render/d3d11/D3D11BackendPrivate.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/render/d3d11/D3D11BackendDevice.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/render/d3d11/D3D11BackendResources.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/render/d3d11/D3D11BackendCommands1.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/render/d3d11/D3D11BackendCommands2.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/render/d3d11/D3D11BackendCommands3.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/render/d3d11/D3D11BackendReadback.inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/render/d3d11/D3D11RenderContext.inc")
    set_source_files_properties(${_aero_d3d11_backend_fragments}
        PROPERTIES HEADER_FILE_ONLY TRUE)
    set_property(SOURCE src/render/d3d11/D3D11Backend.cpp APPEND
        PROPERTY OBJECT_DEPENDS "${_aero_d3d11_backend_fragments}")

    target_sources(AeroGui PRIVATE
        src/render/d3d11/D3D11Backend.cpp
        src/render/d3d11/D3D11Shaders.cpp
        ${_aero_d3d11_backend_fragments})
    add_dependencies(AeroGui AeroD3D11RenderFrameShaders)
    target_include_directories(AeroGui PRIVATE "${_aero_d3d11_shader_directory}")
    target_link_libraries(AeroGui PRIVATE d3d11 dxgi d3dcompiler)
    target_compile_definitions(AeroGui PRIVATE AERO_HAS_D3D11_BACKEND=1)

    unset(_aero_d3d11_shader_outputs)
    unset(_aero_d3d11_shader_root)
    unset(_aero_d3d11_backend_fragments)
else()
    target_compile_definitions(AeroGui PRIVATE AERO_HAS_D3D11_BACKEND=0)
endif()
