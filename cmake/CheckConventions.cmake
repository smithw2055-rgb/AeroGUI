if(NOT DEFINED AERO_SOURCE_DIR)
    message(FATAL_ERROR "AERO_SOURCE_DIR is required")
endif()

include("${AERO_SOURCE_DIR}/cmake/AeroPublicHeaders.cmake")

function(aero_collect_convention_matches output pattern)
    set(matches)
    foreach(path IN LISTS ARGN)
        file(READ "${path}" content)
        if(content MATCHES "${pattern}")
            file(RELATIVE_PATH relative "${AERO_SOURCE_DIR}" "${path}")
            list(APPEND matches "${relative}")
        endif()
    endforeach()
    set(${output} "${matches}" PARENT_SCOPE)
endfunction()

set(aero_public_headers)
foreach(relative IN LISTS AERO_PUBLIC_HEADERS)
    list(APPEND aero_public_headers "${AERO_SOURCE_DIR}/${relative}")
endforeach()

# Try is reserved for parsing, conversion, conditional ownership and cache
# lookup. Allocation, registration, subscription and ordinary mutation APIs
# use their direct WPF-style verbs instead of a generic Try prefix.
set(aero_try_allowlist
    TryParse
    TryFromString
    TryFromCustom
    TryConvertText
    TryEncodeValue
    TryCreateValue
    TryFromBorrowed
    TryGetCachedReloadRevision)
set(public_try_violations)
foreach(path IN LISTS aero_public_headers)
    file(READ "${path}" content)
    string(REGEX MATCHALL "Try[A-Z][A-Za-z0-9_]*" try_names "${content}")
    foreach(try_name IN LISTS try_names)
        list(FIND aero_try_allowlist "${try_name}" try_index)
        if(try_index EQUAL -1)
            file(RELATIVE_PATH relative "${AERO_SOURCE_DIR}" "${path}")
            list(APPEND public_try_violations "${relative}: ${try_name}")
        endif()
    endforeach()
endforeach()
if(public_try_violations)
    list(REMOVE_DUPLICATES public_try_violations)
    message(FATAL_ERROR
        "Public headers contain a non-canonical Try API: "
        "${public_try_violations}")
endif()

# Checked is the explicit failure-aware companion to retained WPF-shaped
# property mutation APIs.
set(public_result_property_mutators)
foreach(path IN LISTS aero_public_headers)
    file(READ "${path}" content)
    file(RELATIVE_PATH relative "${AERO_SOURCE_DIR}" "${path}")
    string(REGEX REPLACE
        "Base::Result<void>[ \t\r\n]+([A-Za-z_][A-Za-z0-9_:]*::)?(Set|Clear|Reset|Notify)[A-Za-z0-9_]*Checked[ \t\r\n]*[(]"
        "" content_without_checked_mutators "${content}")
    # Style and Setter Set() are authoring/build operations: type conversion,
    # sealing and allocation can fail, so R4 keeps their direct Result API.
    if(relative STREQUAL "include/Aero/Style.hpp" OR
       relative STREQUAL "include/Aero/Triggers/TriggerBase.hpp")
        string(REGEX REPLACE
            "Base::Result<void>[ \t\r\n]+Set[ \t\r\n]*[(]"
            "" content_without_checked_mutators
            "${content_without_checked_mutators}")
    endif()
    if(relative STREQUAL "include/Aero/Controls/Items.hpp")
        string(REGEX REPLACE
            "Base::Result<void>[ \t\r\n]+(Reset|SetHeader)[ \t\r\n]*[(]"
            "" content_without_checked_mutators
            "${content_without_checked_mutators}")
    endif()
    if(relative STREQUAL "include/Aero/Styling.hpp")
        string(REGEX REPLACE
            "Base::Result<void>[ \t\r\n]+Set[A-Za-z0-9_]*[ \t\r\n]*[(]"
            "" content_without_checked_mutators
            "${content_without_checked_mutators}")
    endif()
    if(relative STREQUAL "include/Aero/FrameworkElement.hpp" OR
       relative STREQUAL "include/Aero/Documents.hpp" OR
       relative STREQUAL "include/Aero/Controls/Panels.hpp" OR
       relative STREQUAL "include/Aero/Controls/Text.hpp")
        string(REGEX REPLACE
            "Base::Result<void>[ \t\r\n]+SetFontFamily[ \t\r\n]*[(]"
            "" content_without_checked_mutators
            "${content_without_checked_mutators}")
    endif()
    if(relative STREQUAL "include/Aero/Controls/Common.hpp")
        string(REGEX REPLACE
            "Base::Result<void>[ \t\r\n]+SetHeader[ \t\r\n]*[(]"
            "" content_without_checked_mutators
            "${content_without_checked_mutators}")
    endif()
    string(REGEX MATCH
        "Base::Result<void>[ \t\r\n]+([A-Za-z_][A-Za-z0-9_:]*::)?(Set|Clear|Reset|Notify)[A-Za-z0-9_]*[ \t\r\n]*[(]"
        forbidden_result_mutator "${content_without_checked_mutators}")
    if(forbidden_result_mutator)
        list(APPEND public_result_property_mutators "${relative}")
    endif()
endforeach()
if(public_result_property_mutators)
    message(FATAL_ERROR
        "Public property mutators must not return Base::Result<void>; "
        "use void or a Checked companion: ${public_result_property_mutators}")
endif()

function(aero_collect_duplicate_includes output)
    set(matches)
    foreach(relative IN LISTS ARGN)
        set(path "${AERO_SOURCE_DIR}/${relative}")
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
        "Public headers contain duplicate direct includes: "
        "${aero_duplicate_public_includes}")
endif()

set(wpf_single_track_headers
    "${AERO_SOURCE_DIR}/include/Aero/UIElement.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/FrameworkElement.hpp")
aero_collect_convention_matches(wpf_legacy_accessors
    "[ \t](DesiredSize|RenderSize|LayoutSlot|LayoutClip|IsMeasureValid|IsArrangeValid|ClipToBounds|IsHitTestVisible|IsVisible|IsEnabled|AllowDrop|IsMouseOver|IsPressed|IsKeyboardFocused|IsKeyboardFocusWithin|Focusable|IsTabStop|TabIndex|IsFocusScope|RenderTransform|RenderTransformOrigin|UseLayoutRounding|Width|Height|MinSize|MaxSize|Margin|LayoutTransform|LocalVisualTransform|RenderParent|RenderChildren)[ \t]*[(]"
    ${wpf_single_track_headers})
if(wpf_legacy_accessors)
    message(FATAL_ERROR
        "WPF-facing element headers expose legacy non-Get accessors: "
        "${wpf_legacy_accessors}")
endif()

aero_collect_convention_matches(wpf_legacy_setters
    "[ \t](SetEnabled|SetHitTestVisible|SetTabStop|SetFocusScope|SetLayoutRounding)[ \t]*[(]"
    ${wpf_single_track_headers})
if(wpf_legacy_setters)
    message(FATAL_ERROR
        "WPF-facing element headers expose non-canonical property setters: "
        "${wpf_legacy_setters}")
endif()

set(wpf_shape_headers
    "${AERO_SOURCE_DIR}/include/Aero/Shapes.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Panels.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Controls/Primitives.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/Media/Brushes.hpp")
aero_collect_convention_matches(wpf_shape_legacy_getters
    "[ \t](Fill|FillBrush|Stroke|StrokeBrush|StrokeThickness|RadiusX|RadiusY|LastChildFill)[ \t]*[(][)]"
    ${wpf_shape_headers})
if(wpf_shape_legacy_getters)
    message(FATAL_ERROR
        "WPF-facing property getters must use Get<Property>(): "
        "${wpf_shape_legacy_getters}")
endif()

file(GLOB_RECURSE default_sdk_headers
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp")
set(multiline_static_members)
foreach(path IN LISTS default_sdk_headers)
    file(STRINGS "${path}" public_header_lines)
    set(public_header_line_number 0)
    foreach(line IN LISTS public_header_lines)
        math(EXPR public_header_line_number "${public_header_line_number} + 1")
        if(line MATCHES "inline[ \t]+static[ \t]+constexpr.*(Property|Event)" AND
           NOT line MATCHES ";[ \t]*$")
            file(RELATIVE_PATH relative "${AERO_SOURCE_DIR}" "${path}")
            list(APPEND multiline_static_members
                "${relative}:${public_header_line_number}")
        endif()
    endforeach()
endforeach()
if(multiline_static_members)
    message(FATAL_ERROR
        "DependencyProperty and routed-event static definitions must stay on one line: "
        "${multiline_static_members}")
endif()

file(GLOB_RECURSE framework_element_sources
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp")
aero_collect_convention_matches(retired_render_invalidation_name
    "InvalidateRender[ \t]*\\(" ${framework_element_sources})
if(retired_render_invalidation_name)
    message(FATAL_ERROR
        "Use WPF-style InvalidateVisual instead of InvalidateRender: "
        "${retired_render_invalidation_name}")
endif()

# Most public authoring types stay extensible. Strict PImpl service/host objects
# are intentionally final boundaries.
set(final_class_allowlist View Renderer RenderDevice RenderTarget)
set(class_level_final_violations)
file(GLOB_RECURSE class_level_final_sources
    "${AERO_SOURCE_DIR}/include/Aero/*.hpp"
    "${AERO_SOURCE_DIR}/include/Aero/*.h"
    "${AERO_SOURCE_DIR}/src/*.cpp"
    "${AERO_SOURCE_DIR}/src/*.hpp"
    "${AERO_SOURCE_DIR}/src/*.h"
    "${AERO_SOURCE_DIR}/src/*.inc")
foreach(path IN LISTS class_level_final_sources)
    file(STRINGS "${path}" final_lines
        REGEX "^[ \t]*(class|struct)[^;{}]*[ \t]final[ \t]*([:{]|$)")
    foreach(line IN LISTS final_lines)
        string(REGEX MATCH
            "(class|struct)[^;{}]*[ \t]([A-Za-z_][A-Za-z0-9_]*)[ \t]+final"
            final_declaration "${line}")
        set(final_name "${CMAKE_MATCH_2}")
        list(FIND final_class_allowlist "${final_name}" final_allow_index)
        if(final_allow_index EQUAL -1)
            file(RELATIVE_PATH relative "${AERO_SOURCE_DIR}" "${path}")
            list(APPEND class_level_final_violations
                "${relative}: ${final_name}")
        endif()
    endforeach()
endforeach()
if(class_level_final_violations)
    message(FATAL_ERROR
        "Class-level final is restricted to strict PImpl boundaries: "
        "${class_level_final_violations}")
endif()

message(STATUS "Aero convention checks passed")
