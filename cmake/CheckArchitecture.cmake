if(NOT DEFINED AERO_SOURCE_DIR OR AERO_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "AERO_SOURCE_DIR is required")
endif()

# Final architecture gates only. Historical migration stages belong in Git
# history and design notes, not in the permanent build contract.
function(aero_require_file relative_path)
    if(NOT EXISTS "${AERO_SOURCE_DIR}/${relative_path}")
        message(FATAL_ERROR "Required architecture file is missing: ${relative_path}")
    endif()
endfunction()

function(aero_forbid_file relative_path)
    if(EXISTS "${AERO_SOURCE_DIR}/${relative_path}")
        message(FATAL_ERROR "Retired architecture file was recreated: ${relative_path}")
    endif()
endfunction()

function(aero_require_text relative_path needle description)
    file(READ "${AERO_SOURCE_DIR}/${relative_path}" content)
    string(FIND "${content}" "${needle}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "${description}: ${relative_path}")
    endif()
endfunction()

function(aero_forbid_text relative_path needle description)
    file(READ "${AERO_SOURCE_DIR}/${relative_path}" content)
    string(FIND "${content}" "${needle}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "${description}: ${relative_path}")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Installed SDK surface
# ---------------------------------------------------------------------------
foreach(required_public_entry IN ITEMS
        "include/Aero/Gui.hpp"
        "include/Aero/View.hpp"
        "include/Aero/IRenderer.hpp"
        "include/Aero/RenderDevice.hpp"
        "include/Aero/RenderTarget.hpp"
        "include/Aero/Render/D3D11.hpp"
        "include/Aero/Render/OpenGL33.hpp"
        "include/Aero/Markup.hpp"
        "include/Aero/Meta.hpp"
        "include/Aero/App.hpp"
        "include/Aero/Application.hpp"
        "include/Aero/Window.hpp")
    aero_require_file("${required_public_entry}")
endforeach()

foreach(retired_public_entry IN ITEMS
        "include/Aero/Integration.hpp"
        "include/Aero/Integration"
        "include/Aero/RenderSurface.hpp"
        "include/Aero/Runtime.hpp"
        "include/Aero/RuntimeEnvironment.hpp"
        "include/Aero/RuntimeTypes.hpp")
    aero_forbid_file("${retired_public_entry}")
endforeach()

aero_require_text(
    "include/Aero/IRenderer.hpp"
    "RenderTarget& target"
    "IRenderer must render to the canonical RenderTarget")
aero_forbid_text(
    "include/Aero/IRenderer.hpp"
    "RenderSurface"
    "IRenderer must not expose the retired RenderSurface spelling")
aero_require_text(
    "include/Aero/Gui.hpp"
    "#include <Aero/RenderTarget.hpp>"
    "Gui umbrella must expose RenderTarget")
aero_forbid_text(
    "include/Aero/Gui.hpp"
    "RenderSurface"
    "Gui umbrella must not expose RenderSurface")
aero_require_text(
    "include/Aero/RenderTarget.hpp"
    "class AERO_API RenderTarget final"
    "RenderTarget must be the installed target object")

aero_require_text(
    "include/Aero/Render/D3D11.hpp"
    "CreateD3D11RenderTarget"
    "D3D11 embedded integration must use RenderTarget vocabulary")
aero_require_text(
    "include/Aero/Render/OpenGL33.hpp"
    "CreateOpenGL33RenderTarget"
    "OpenGL embedded integration must use RenderTarget vocabulary")
aero_forbid_text(
    "include/Aero/Render/D3D11.hpp"
    "D3D11EmbeddedSurfaceOptions"
    "D3D11 installed API must not recreate EmbeddedSurface")
aero_forbid_text(
    "include/Aero/Render/OpenGL33.hpp"
    "OpenGL33EmbeddedSurfaceOptions"
    "OpenGL installed API must not recreate EmbeddedSurface")

# Every installed header in the explicit manifest must exist and must not point
# back into source-only implementation trees or the retired Integration API.
include("${AERO_SOURCE_DIR}/cmake/AeroPublicHeaders.cmake")
foreach(public_header IN LISTS AERO_PUBLIC_HEADERS)
    aero_require_file("${public_header}")
    file(READ "${AERO_SOURCE_DIR}/${public_header}" public_content)
    if(public_content MATCHES "#[ \t]*include[ \t]*[<\"](\.\./)*src/" OR
       public_content MATCHES "Aero/Integration(/|[.]hpp)" OR
       public_content MATCHES "Aero::Integration")
        message(FATAL_ERROR
            "Installed header leaks a source/private or retired Integration contract: ${public_header}")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# Product and source ownership
# ---------------------------------------------------------------------------
aero_forbid_file("src/integration")
aero_forbid_file("src/render/RenderSurface.cpp")
aero_require_file("src/render/RenderTarget.cpp")
aero_require_file("src/render/private/RenderTarget.hpp")
aero_require_file("src/app/RenderContext.hpp")
aero_require_file("src/app/RenderContext.cpp")

file(GLOB aero_root_source_files
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp")
if(aero_root_source_files)
    message(FATAL_ERROR
        "Source files must belong to a real domain directory under src/: ${aero_root_source_files}")
endif()

file(GLOB_RECURSE architecture_cpp_sources
    "${AERO_SOURCE_DIR}/include/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/tools/*.hpp"
    "${AERO_SOURCE_DIR}/tools/*.cpp")
foreach(source_file IN LISTS architecture_cpp_sources)
    file(READ "${source_file}" source_content)
    if(source_content MATCHES "Aero/Integration(/|[.]hpp)" OR
       source_content MATCHES "namespace[ \t]+Aero::Integration" OR
       source_content MATCHES "::Aero::Integration")
        file(RELATIVE_PATH relative "${AERO_SOURCE_DIR}" "${source_file}")
        message(FATAL_ERROR "Retired Integration ownership remains in ${relative}")
    endif()
endforeach()

# Public products remain deliberately small. Internal object components are a
# build technique only and may be reorganized without changing this gate.
file(GLOB aero_target_modules
    "${AERO_SOURCE_DIR}/CMakeLists.txt"
    "${AERO_SOURCE_DIR}/cmake/*.cmake")
set(product_text "")
foreach(target_module IN LISTS aero_target_modules)
    file(READ "${target_module}" module_content)
    string(APPEND product_text "\n${module_content}")
endforeach()
foreach(required_product IN ITEMS
        "add_library(Aero::Base ALIAS AeroBase)"
        "add_library(Aero::Gui ALIAS AeroGui)"
        "add_library(Aero::App ALIAS AeroApp)")
    string(FIND "${product_text}" "${required_product}" product_position)
    if(product_position EQUAL -1)
        message(FATAL_ERROR "Required product target is missing: ${required_product}")
    endif()
endforeach()
if(product_text MATCHES "add_library[ \t\r\n]*[(][ \t\r\n]*AeroIntegration" OR
   product_text MATCHES "add_library[ \t\r\n]*[(][ \t\r\n]*Aero::Integration")
    message(FATAL_ERROR "The retired Integration product must not be recreated")
endif()

aero_require_text(
    "cmake/AeroProductTargets.cmake"
    "src/render/RenderTarget.cpp"
    "AeroGui must compile the canonical RenderTarget implementation")
aero_require_text(
    "cmake/AeroProductTargets.cmake"
    "src/app/RenderContext.cpp"
    "AeroApp must own the desktop RenderContext")
aero_forbid_text(
    "cmake/AeroProductTargets.cmake"
    "src/render/RenderSurface.cpp"
    "CMake must not compile the retired RenderSurface implementation")
aero_forbid_text(
    "cmake/AeroProductTargets.cmake"
    "src/integration/"
    "CMake must not reference the retired integration source tree")

# ---------------------------------------------------------------------------
# Runtime / render ownership invariants
# ---------------------------------------------------------------------------
aero_require_text(
    "src/app/DesktopHost.cpp"
    "RenderContext renderContext"
    "DesktopHost must delegate desktop presentation to RenderContext")
aero_forbid_text(
    "src/app/DesktopHost.cpp"
    "CreateRenderSurface"
    "DesktopHost must not own backend surface construction")
aero_forbid_text(
    "src/app/DesktopHost.cpp"
    "renderSurface"
    "DesktopHost must not retain a raw render-surface lifecycle")
aero_require_text(
    "src/app/RenderContext.cpp"
    "renderer.Render(*target_)"
    "RenderContext must own the final target handoff")

aero_forbid_text(
    "src/render/RenderTree.hpp"
    "RenderDevice"
    "RenderTree must only build immutable frames")
aero_forbid_text(
    "src/render/RenderTree.hpp"
    "Submit("
    "RenderTree must not own GPU submission")

# The source-private graphics layer may use SurfaceSession to manage native
# acquire/present semantics, but the installed SDK may not expose it.
foreach(public_header IN LISTS AERO_PUBLIC_HEADERS)
    file(READ "${AERO_SOURCE_DIR}/${public_header}" public_content)
    if(public_content MATCHES "SurfaceSession|ISurfaceBackend|NativeSurfaceDescriptor")
        message(FATAL_ERROR
            "Installed header exposes private graphics surface machinery: ${public_header}")
    endif()
endforeach()

# View's public surface is host-driven and must not grow XAML loader or frame
# scheduler services again. Private implementation helpers are intentionally not
# frozen by string-based gates.
file(READ "${AERO_SOURCE_DIR}/include/Aero/View.hpp" view_header)
string(FIND "${view_header}" "class AERO_API View" view_begin)
if(view_begin EQUAL -1)
    message(FATAL_ERROR "Unable to inspect View public API")
endif()
string(SUBSTRING "${view_header}" ${view_begin} -1 view_class_tail)
string(FIND "${view_class_tail}" "\nprivate:" view_private)
if(view_private EQUAL -1)
    message(FATAL_ERROR "Unable to inspect View public API")
endif()
string(SUBSTRING "${view_class_tail}" 0 ${view_private} view_public)
if(view_public MATCHES "LoadDocument|ParseDocument|LoadCompiledDocument|RunFrame|FindNamedObject")
    message(FATAL_ERROR "View public API recreated loader, scheduler or namescope services")
endif()

message(STATUS "Aero final architecture dependency checks passed")
