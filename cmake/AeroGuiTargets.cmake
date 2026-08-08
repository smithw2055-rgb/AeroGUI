# WPF/XAML class library. Product implementation sources compile directly into
# AeroGui; only source sets reused by offline tools remain object components.
function(aero_configure_internal_objects target)
    target_include_directories(${target}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_features(${target} PUBLIC cxx_std_17)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON)
    aero_apply_compiler_options(${target})
endfunction()

set(_aero_vendored_expat_target "")
set(_aero_expat_target "")
if(AERO_WITH_EXPAT)
    if(AERO_THIRD_PARTY_ROOT STREQUAL "")
        get_filename_component(_aero_sibling_third_party
            "${CMAKE_CURRENT_SOURCE_DIR}/../AeroGUI/third_party" ABSOLUTE)
        if(EXISTS "${_aero_sibling_third_party}/expat/expat/CMakeLists.txt")
            set(AERO_THIRD_PARTY_ROOT "${_aero_sibling_third_party}")
        endif()
    endif()
    set(_aero_expat_source "${AERO_THIRD_PARTY_ROOT}/expat/expat")
    if(EXISTS "${_aero_expat_source}/CMakeLists.txt")
        set(EXPAT_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
        set(EXPAT_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        add_subdirectory("${_aero_expat_source}"
            "${CMAKE_CURRENT_BINARY_DIR}/third_party/expat" EXCLUDE_FROM_ALL)
        set_target_properties(expat PROPERTIES POSITION_INDEPENDENT_CODE ON)
        set(_aero_expat_target expat::expat)
        set(_aero_vendored_expat_target expat)
    else()
        find_package(EXPAT QUIET)
        if(TARGET EXPAT::EXPAT)
            set(_aero_expat_target EXPAT::EXPAT)
        else()
            message(FATAL_ERROR
                "AERO_WITH_EXPAT requires Expat or ${_aero_expat_source}")
        endif()
    endif()
endif()

set(_aero_gui_sources
    src/gui/BindingPath.cpp
    src/gui/PropertySystem.cpp
    src/gui/Freezable.cpp
    src/diagnostics/Diagnostics.cpp
    src/gui/Dispatcher.cpp
    src/gui/RoutedEvents.cpp
    src/gui/Metadata.cpp
    src/gui/EnumMetadata.cpp
    src/gui/BuiltinMetadata.cpp
    src/gui/ObjectFactory.cpp
    src/gui/Value.cpp
    src/media/AnimationEngine.cpp
    src/media/Animation.cpp
    src/render/DrawingContext.cpp
    src/gui/Binding.cpp
    src/gui/BindingObjects.cpp
    src/media/Brushes.cpp
    src/gui/Commands.cpp
    src/media/Effects.cpp
    src/media/Geometry.cpp
    src/gui/Input.cpp
    src/gui/Interactivity.cpp
    src/media/Images.cpp
    src/gui/Layout.cpp
    src/gui/ContentElement.cpp
    src/gui/ElementTree.cpp
    src/gui/Resources.cpp
    src/render/RenderTree.cpp
    src/media/Transforms.cpp
    src/gui/Style.cpp
    src/controls/Bars.cpp
    src/controls/BlendBehaviors.cpp
    src/controls/Buttons.cpp
    src/controls/ContentControls.cpp
    src/controls/ControlBehavior.cpp
    src/controls/Controls.cpp
    src/controls/Documents.cpp
    src/controls/Images.cpp
    src/controls/Items.cpp
    src/controls/ListView.cpp
    src/controls/Menus.cpp
    src/controls/Metadata.cpp
    src/controls/Path.cpp
    src/controls/Scroll.cpp
    src/controls/Selection.cpp
    src/controls/Shapes.cpp
    src/controls/TextBox.cpp
    src/controls/Templates.cpp
    src/controls/Trees.cpp
    src/controls/Virtualization.cpp
    src/controls/VisualStates.cpp
    src/diagnostics/Inspector.cpp
    src/markup/MarkupParser.cpp
    src/markup/MarkupSchema.cpp
    src/markup/MarkupWriter.cpp
    src/markup/MarkupTemplates.cpp
    src/markup/MarkupLoader.cpp
    src/gui/modules/Module.cpp
    src/gui/modules/BuiltinModules.cpp
    src/markup/GuiSchema.cpp)

add_library(AeroGui ${AERO_LIBRARY_TYPE} ${_aero_gui_sources})
add_library(Aero::Gui ALIAS AeroGui)
target_include_directories(AeroGui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AeroGui PUBLIC Aero::Base Threads::Threads)
if(AERO_WITH_EXPAT)
    if(_aero_vendored_expat_target)
        target_link_libraries(AeroGui PRIVATE
            $<BUILD_INTERFACE:${_aero_expat_target}>
            $<INSTALL_INTERFACE:Aero::_PrivateExpat>)
    else()
        target_link_libraries(AeroGui PRIVATE
            $<BUILD_INTERFACE:${_aero_expat_target}>
            $<INSTALL_INTERFACE:EXPAT::EXPAT>)
    endif()
endif()
target_compile_definitions(AeroGui PRIVATE
    AERO_UI_RESOURCE_MODEL=2
    AERO_CONTROLS_TEMPLATE_ABI=10
    AERO_MARKUP_UI_RESOURCES=1
    AERO_WITH_EXPAT=$<BOOL:${AERO_WITH_EXPAT}>)
target_compile_features(AeroGui PUBLIC cxx_std_17)
set_target_properties(AeroGui PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroGui)

# Application/Window descriptors and the module bootstrap are also reused by
# schema tools. They remain source-only object components, never SDK targets.
add_library(AeroAppModelObjects OBJECT
    src/app/Application.cpp
    src/app/Metadata.cpp)
aero_configure_internal_objects(AeroAppModelObjects)
target_link_libraries(AeroAppModelObjects PUBLIC Aero::Gui)

add_library(AeroModuleSetObjects OBJECT
    src/gui/modules/Module.cpp
    src/gui/modules/BuiltinModules.cpp
    src/markup/GuiSchema.cpp)
aero_configure_internal_objects(AeroModuleSetObjects)
target_link_libraries(AeroModuleSetObjects PUBLIC Aero::Gui)

add_library(AeroMeta INTERFACE)
add_library(Aero::Meta ALIAS AeroMeta)
target_link_libraries(AeroMeta INTERFACE Aero::Gui)

add_library(AeroGuiHeaderConsumer OBJECT tools/sdk-consumers/GuiConsumer.cpp)
target_link_libraries(AeroGuiHeaderConsumer PRIVATE Aero::Gui)
aero_apply_compiler_options(AeroGuiHeaderConsumer)
add_library(AeroEventsTriggersHeaderConsumer OBJECT
    tools/sdk-consumers/EventsTriggersConsumer.cpp)
target_link_libraries(AeroEventsTriggersHeaderConsumer PRIVATE Aero::Gui)
aero_apply_compiler_options(AeroEventsTriggersHeaderConsumer)
add_library(AeroMetaHeaderConsumer OBJECT tools/sdk-consumers/MetaConsumer.cpp)
target_link_libraries(AeroMetaHeaderConsumer PRIVATE Aero::Meta)
aero_apply_compiler_options(AeroMetaHeaderConsumer)

unset(_aero_gui_sources)
