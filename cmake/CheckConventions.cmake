if(NOT DEFINED AERO_SOURCE_DIR OR AERO_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "AERO_SOURCE_DIR is required")
endif()

# Permanent conventions only. Historical API migration policy is reviewed in
# source history and design notes rather than frozen into every future build.
include("${AERO_SOURCE_DIR}/cmake/AeroPublicHeaders.cmake")

function(aero_collect_duplicate_includes output)
    set(matches)
    foreach(relative IN LISTS ARGN)
        set(path "${AERO_SOURCE_DIR}/${relative}")
        if(NOT EXISTS "${path}")
            list(APPEND matches "${relative}: missing")
            continue()
        endif()
        file(STRINGS "${path}" includes REGEX "^[ \t]*#[ \t]*include[ \t]+")
        set(seen)
        foreach(include_line IN LISTS includes)
            string(STRIP "${include_line}" include_line)
            list(FIND seen "${include_line}" existing)
            if(NOT existing EQUAL -1)
                list(APPEND matches "${relative}: ${include_line}")
            else()
                list(APPEND seen "${include_line}")
            endif()
        endforeach()
    endforeach()
    set(${output} "${matches}" PARENT_SCOPE)
endfunction()

aero_collect_duplicate_includes(aero_duplicate_public_includes
    ${AERO_PUBLIC_HEADERS})
if(aero_duplicate_public_includes)
    message(FATAL_ERROR
        "Installed headers must exist and contain no duplicate direct includes: "
        "${aero_duplicate_public_includes}")
endif()

# DependencyProperty and RoutedEvent descriptors are intentionally concise in
# the C++ authoring surface. Keep each inline static declaration on one physical
# line without constraining private implementation layout.
set(multiline_static_members)
foreach(relative IN LISTS AERO_PUBLIC_HEADERS)
    set(path "${AERO_SOURCE_DIR}/${relative}")
    file(STRINGS "${path}" public_header_lines)
    set(line_number 0)
    foreach(line IN LISTS public_header_lines)
        math(EXPR line_number "${line_number} + 1")
        if(line MATCHES "inline[ \t]+static[ \t]+constexpr.*(Property|RoutedEvent|Event)" AND
           NOT line MATCHES ";[ \t]*$")
            list(APPEND multiline_static_members "${relative}:${line_number}")
        endif()
    endforeach()
endforeach()
if(multiline_static_members)
    message(FATAL_ERROR
        "DependencyProperty/RoutedEvent static declarations must stay on one line: "
        "${multiline_static_members}")
endif()

# Installed WPF-facing headers must not recreate the retired render invalidation
# spelling. Private implementation helpers are free to use domain-specific
# names when they do not escape into the SDK.
set(retired_public_render_invalidation)
foreach(relative IN LISTS AERO_PUBLIC_HEADERS)
    file(READ "${AERO_SOURCE_DIR}/${relative}" content)
    if(content MATCHES "InvalidateRender[ \t]*\\(")
        list(APPEND retired_public_render_invalidation "${relative}")
    endif()
endforeach()
if(retired_public_render_invalidation)
    message(FATAL_ERROR
        "Installed WPF-facing APIs must use InvalidateVisual: "
        "${retired_public_render_invalidation}")
endif()

message(STATUS "Aero convention checks passed")
