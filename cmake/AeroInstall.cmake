# Install only product binaries. Internal Aero domains are object components and
# never appear in AeroTargets.cmake. Static packages additionally carry the
# three vendored archives required to resolve private third-party symbols.
set_target_properties(AeroBase PROPERTIES EXPORT_NAME Base)
set_target_properties(AeroAudio PROPERTIES EXPORT_NAME Audio)
set_target_properties(AeroGui PROPERTIES EXPORT_NAME Gui)
set_target_properties(AeroRender PROPERTIES EXPORT_NAME Render)
set_target_properties(AeroRenderOpenGL33 PROPERTIES EXPORT_NAME RenderOpenGL33)
set_target_properties(AeroApp PROPERTIES EXPORT_NAME App)
if(TARGET AeroRenderD3D11)
    set_target_properties(AeroRenderD3D11 PROPERTIES EXPORT_NAME RenderD3D11)
endif()

set(_aero_sdk_targets
    AeroBase
    AeroAudio
    AeroGui
    AeroRender
    AeroRenderOpenGL33
    AeroApp)
if(TARGET AeroRenderD3D11)
    list(APPEND _aero_sdk_targets AeroRenderD3D11)
endif()

if(NOT AERO_BUILD_SHARED)
    set_target_properties(freetype PROPERTIES
        EXPORT_NAME _PrivateFreeType)
    set_target_properties(harfbuzz PROPERTIES
        EXPORT_NAME _PrivateHarfBuzz)
    list(APPEND _aero_sdk_targets freetype harfbuzz)
    if(_aero_vendored_expat_target)
        set_target_properties(${_aero_vendored_expat_target} PROPERTIES
            EXPORT_NAME _PrivateExpat)
        list(APPEND _aero_sdk_targets ${_aero_vendored_expat_target})
    endif()
endif()

install(TARGETS ${_aero_sdk_targets}
    EXPORT AeroTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

if(TARGET aero-schema-gen)
    set_target_properties(aero-schema-gen PROPERTIES EXPORT_NAME schema-gen)
    install(TARGETS aero-schema-gen
        EXPORT AeroTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(TARGET aero-xamlc)
    set_target_properties(aero-xamlc PROPERTIES EXPORT_NAME xamlc)
    install(TARGETS aero-xamlc
        EXPORT AeroTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

foreach(_aero_public_header IN LISTS AERO_PUBLIC_HEADERS)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_aero_public_header}")
        message(FATAL_ERROR
            "Public SDK header is missing: ${_aero_public_header}")
    endif()
    get_filename_component(
        _aero_public_header_directory
        "${_aero_public_header}" DIRECTORY)
    string(REGEX REPLACE "^include/" ""
        _aero_public_install_directory
        "${_aero_public_header_directory}")
    install(FILES "${_aero_public_header}"
        DESTINATION
            "${CMAKE_INSTALL_INCLUDEDIR}/${_aero_public_install_directory}")
endforeach()
unset(_aero_public_header)
unset(_aero_public_header_directory)
unset(_aero_public_install_directory)

# B4: Meta authoring detail (TypeBuilder session/helpers) — not under include/Aero,
# so it stays off the AeroPublicHeaders whitelist while remaining usable via
# <Aero/Meta.hpp> ("gui/meta/..." through aero-meta-authoring include root).
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/meta/TypeBuilderDetail.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/meta/MetadataRegistrations.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gui/meta/TypeBuilderInternal.inc"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/aero-meta-authoring/gui/meta")

install(FILES
    "${AERO_GENERATED_INCLUDE_DIR}/Aero/Version.hpp"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Aero)
install(DIRECTORY themes/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/Aero/themes
    FILES_MATCHING PATTERN "*.xaml")
if(NOT _aero_schema_manifest STREQUAL "")
    install(FILES "${_aero_schema_manifest}"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/Aero/schema
        RENAME Aero.aeroschema)
endif()

set(AERO_PACKAGE_WITH_EXPAT OFF)
if(AERO_WITH_EXPAT AND NOT _aero_vendored_expat_target)
    set(AERO_PACKAGE_WITH_EXPAT ON)
endif()
set(AERO_PACKAGE_WITH_GLX ${AERO_ENABLE_GLX_SURFACE})
configure_package_config_file(
    cmake/AeroConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/AeroConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Aero)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AeroConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)
install(EXPORT AeroTargets
    FILE AeroTargets.cmake
    NAMESPACE Aero::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Aero)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/AeroConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/AeroConfigVersion.cmake"
    cmake/AeroAddXaml.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Aero)
