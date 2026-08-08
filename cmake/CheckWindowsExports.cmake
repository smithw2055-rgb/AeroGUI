if(NOT DEFINED AERO_DUMPBIN OR AERO_DUMPBIN STREQUAL "")
    message(FATAL_ERROR "AERO_DUMPBIN is required")
endif()
if(NOT DEFINED AERO_DLL OR AERO_DLL STREQUAL "")
    message(FATAL_ERROR "AERO_DLL is required")
endif()
if(NOT DEFINED AERO_EXPECTED_EXPORT OR AERO_EXPECTED_EXPORT STREQUAL "")
    message(FATAL_ERROR "AERO_EXPECTED_EXPORT is required")
endif()

execute_process(
    COMMAND "${AERO_DUMPBIN}" /nologo /exports "${AERO_DLL}"
    RESULT_VARIABLE dump_result
    OUTPUT_VARIABLE dump_output
    ERROR_VARIABLE dump_error)
if(NOT dump_result EQUAL 0)
    message(FATAL_ERROR
        "dumpbin failed for ${AERO_DLL}: ${dump_error}")
endif()

string(FIND "${dump_output}" "${AERO_EXPECTED_EXPORT}" expected_position)
if(expected_position EQUAL -1)
    message(FATAL_ERROR
        "Expected public API export '${AERO_EXPECTED_EXPORT}' is missing from ${AERO_DLL}")
endif()

# API macros are permitted only in installed headers and all Windows targets
# disable auto-export. This symbol-table guard closes the other half of that
# contract: no source-only owner or migration vocabulary may escape a DLL.
foreach(forbidden_export IN ITEMS
        "ViewRenderer"
        "ViewState"
        "GuiState"
        "UiFrameEncoder"
        "RenderContext"
        "DesktopHost"
        "ApplicationHostState"
        "D3D11RenderDevice"
        "OpenGL33RenderDevice"
        "D3D11RenderDeviceState"
        "OpenGL33RenderDeviceState"
        "GuiSchemaState"
        "SchemaState"
        "SchemaManifestState"
        "DependencyGraphState"
        "DocumentCacheState"
        "LoaderState"
        "UiObjectModelState"
        "XamlTemplateSchemaFacetState"
        "DrawPhase"
        "SubmissionStep"
        "CommandQueue"
        "rendererToken"
        "ReleaseRenderer"
        "AERO_INTERNAL_")
    string(FIND "${dump_output}" "${forbidden_export}" forbidden_position)
    if(NOT forbidden_position EQUAL -1)
        message(FATAL_ERROR
            "Source-only export '${forbidden_export}' escaped from ${AERO_DLL}")
    endif()
endforeach()

if(DEFINED AERO_EXPORT_LOG AND NOT AERO_EXPORT_LOG STREQUAL "")
    file(WRITE "${AERO_EXPORT_LOG}" "${dump_output}")
endif()

message(STATUS "Verified Windows exports: ${AERO_DLL}")
