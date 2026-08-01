# Installed product targets, private static-link support archives and tools.
# Installable SDK and CMake package.
#
# Only these targets are supported product components. Static packages still
# ship private support archives so the product targets can resolve their link
# graph, but those imports use an underscore-prefixed _Detail name and are not
# part of the supported SDK surface.
set_target_properties(AeroBase PROPERTIES EXPORT_NAME Base)
set_target_properties(AeroAudio PROPERTIES EXPORT_NAME Audio)
set_target_properties(AeroGui PROPERTIES EXPORT_NAME Gui)
set_target_properties(AeroMeta PROPERTIES EXPORT_NAME Meta)
set_target_properties(AeroIntegration PROPERTIES EXPORT_NAME Integration)
set_target_properties(AeroApp PROPERTIES EXPORT_NAME App)

set_target_properties(AeroCore PROPERTIES EXPORT_NAME _DetailCore)
set_target_properties(AeroPlatform PROPERTIES EXPORT_NAME _DetailPlatform)
set_target_properties(AeroText PROPERTIES EXPORT_NAME _DetailText)
set_target_properties(freetype PROPERTIES EXPORT_NAME _DetailFreeType)
set_target_properties(AeroTextFreeType PROPERTIES EXPORT_NAME _DetailTextFreeType)
set_target_properties(harfbuzz PROPERTIES EXPORT_NAME _DetailHarfBuzz)
set_target_properties(AeroTextHarfBuzz PROPERTIES EXPORT_NAME _DetailTextHarfBuzz)
set_target_properties(AeroAppModel PROPERTIES EXPORT_NAME _DetailAppModel)
set_target_properties(AeroControls PROPERTIES EXPORT_NAME _DetailControls)
set_target_properties(AeroMarkupKernel PROPERTIES EXPORT_NAME _DetailMarkupKernel)
set_target_properties(AeroMarkup PROPERTIES EXPORT_NAME _DetailMarkup)
set_target_properties(AeroModuleCatalog PROPERTIES EXPORT_NAME _DetailModuleCatalog)
set_target_properties(AeroRuntime PROPERTIES EXPORT_NAME _DetailRuntime)
set_target_properties(AeroGraphics PROPERTIES EXPORT_NAME _DetailGraphics)
set_target_properties(AeroGraphicsOpenGL33 PROPERTIES EXPORT_NAME _DetailGraphicsOpenGL33)
set_target_properties(AeroRender PROPERTIES EXPORT_NAME _DetailRender)
set_target_properties(AeroRenderOpenGL33 PROPERTIES EXPORT_NAME _DetailRenderOpenGL33)

if(_aero_vendored_expat_target)
    set_target_properties(${_aero_vendored_expat_target} PROPERTIES EXPORT_NAME _DetailExpat)
endif()

set(_aero_product_targets
    AeroBase
    AeroAudio
    AeroGui
    AeroMeta
    AeroIntegration
    AeroApp)

set(_aero_static_support_targets
    AeroCore
    AeroPlatform
    AeroText
    freetype
    AeroTextFreeType
    harfbuzz
    AeroTextHarfBuzz
    AeroAppModel
    AeroControls
    AeroMarkupKernel
    AeroMarkup
    AeroModuleCatalog
    AeroRuntime
    AeroGraphics
    AeroGraphicsOpenGL33
    AeroRender
    AeroRenderOpenGL33)

if(_aero_vendored_expat_target)
    list(APPEND _aero_static_support_targets ${_aero_vendored_expat_target})
endif()

foreach(_aero_optional_target IN ITEMS
        AeroPlatformWGL
        AeroPlatformGLX
        AeroGraphicsD3D11
        AeroRenderD3D11)
    if(TARGET ${_aero_optional_target})
        list(APPEND _aero_static_support_targets ${_aero_optional_target})
        string(REGEX REPLACE "^Aero" "" _aero_export_name "${_aero_optional_target}")
        set_target_properties(${_aero_optional_target} PROPERTIES EXPORT_NAME "_Detail${_aero_export_name}")
    endif()
endforeach()

set(_aero_sdk_targets ${_aero_product_targets} ${_aero_static_support_targets})

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
