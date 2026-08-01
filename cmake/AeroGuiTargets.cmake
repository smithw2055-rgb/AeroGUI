# WPF/XAML kernel, controls, markup and schema composition targets.
add_library(AeroGuiKernel ${AERO_LIBRARY_TYPE}
    src/gui/BindingPath.cpp
    src/gui/DependencyProperty.cpp
    src/diagnostics/Diagnostics.cpp
    src/gui/Dispatcher.cpp
    src/gui/EffectiveValueEngine.cpp
    src/gui/RoutedEventCatalog.cpp
    src/gui/CoreMetadata.cpp
    src/gui/MetadataAuthoring.cpp
    src/gui/MetadataContext.cpp
    src/gui/MetadataRuntimeData.cpp
    src/gui/MetadataBehaviorRegistrationStore.cpp
    src/gui/MetadataDomain.cpp
    src/gui/MetadataRegistrationValues.cpp
    src/gui/MetadataRuntime.cpp
    src/gui/MetadataValueFacets.cpp
    src/gui/MetadataValueRegistrationStore.cpp
    src/gui/ValueConversion.cpp
    src/gui/ObjectServices.cpp
    src/gui/TypeRegistry.cpp
    src/gui/Value.cpp
    src/media/AnimationRuntime.cpp
    src/media/Animation.cpp
    src/render/BatchPlanner.cpp
    src/render/DrawingContext.cpp
    src/gui/Binding.cpp
    src/gui/BindingObjects.cpp
    src/media/Brushes.cpp
    src/gui/Commands.cpp
    src/media/Effects.cpp
    src/gui/Input.cpp
    src/media/Images.cpp
    src/gui/Layout.cpp
    src/gui/UiMetadata.cpp
    src/gui/ContentElement.cpp
    src/gui/GuiContext.cpp
    src/gui/Resources.cpp
    src/render/RenderTree.cpp
    src/media/Transforms.cpp
    src/gui/Style.cpp)

add_library(Aero::_DetailGuiKernel ALIAS AeroGuiKernel)

target_include_directories(AeroGuiKernel
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(AeroGuiKernel
    PUBLIC
        Aero::Base
        Threads::Threads)

target_compile_definitions(
    AeroGuiKernel PUBLIC AERO_UI_RESOURCE_MODEL=2)
target_compile_features(AeroGuiKernel PUBLIC cxx_std_17)
set_target_properties(AeroGuiKernel PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})

aero_apply_compiler_options(AeroGuiKernel)



# Application object model and metadata are shared by the default App
# product and the built-in schema catalog without pulling in native hosts.
add_library(AeroAppModel ${AERO_LIBRARY_TYPE}
    src/app/Application.cpp
    src/app/Metadata.cpp)
add_library(Aero::_DetailAppModel ALIAS AeroAppModel)
target_include_directories(AeroAppModel
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroAppModel PUBLIC Aero::_DetailGuiKernel)
target_compile_features(AeroAppModel PUBLIC cxx_std_17)
set_target_properties(AeroAppModel PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroAppModel)

add_library(AeroControls ${AERO_LIBRARY_TYPE}
    src/controls/Bars.cpp
    src/controls/Buttons.cpp
    src/controls/ContentControls.cpp
    src/controls/ControlBehaviorService.cpp
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
    src/controls/VisualStates.cpp)

add_library(Aero::_DetailControls ALIAS AeroControls)

target_include_directories(AeroControls
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AeroControls
    PUBLIC
        Aero::_DetailGuiKernel
        Aero::_DetailText)
target_compile_definitions(
    AeroControls PUBLIC AERO_CONTROLS_TEMPLATE_ABI=10)
target_compile_features(AeroControls PUBLIC cxx_std_17)
set_target_properties(AeroControls PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroControls)

# Window derives from ContentControl. Keep the App object model independent of
# native hosting, but express its real shared-library dependency on Controls.
target_link_libraries(AeroAppModel PUBLIC Aero::_DetailControls)
get_target_property(_aero_app_model_links AeroAppModel LINK_LIBRARIES)
if(NOT "${_aero_app_model_links}" MATCHES
        "(^|;)(Aero::_DetailControls|AeroControls)(;|$)")
    message(FATAL_ERROR
        "AeroAppModel must express its ContentControl dependency")
endif()
unset(_aero_app_model_links)

add_library(AeroInspector ${AERO_LIBRARY_TYPE}
    src/diagnostics/Inspector.cpp)
add_library(Aero::_DetailInspector ALIAS
    AeroInspector)
target_include_directories(AeroInspector
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AeroInspector
    PUBLIC Aero::_DetailControls)
target_compile_features(AeroInspector
    PUBLIC cxx_std_17)
set_target_properties(AeroInspector PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroInspector)

add_library(AeroMarkupKernel ${AERO_LIBRARY_TYPE}
    src/markup/CompiledCache.cpp
    src/markup/CompiledDocument.cpp
    src/markup/XmlTokenizer.cpp
    src/markup/NodeReader.cpp)
add_library(Aero::_DetailMarkupKernel ALIAS AeroMarkupKernel)

target_include_directories(AeroMarkupKernel
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(AeroMarkupKernel PUBLIC Aero::_DetailGuiKernel)
target_compile_features(AeroMarkupKernel PUBLIC cxx_std_17)
set_target_properties(AeroMarkupKernel PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})
aero_apply_compiler_options(AeroMarkupKernel)

add_library(AeroMarkup ${AERO_LIBRARY_TYPE}
    src/markup/BindingExtension.cpp
    src/markup/CompiledSchema.cpp
    src/markup/Metadata.cpp
    src/markup/FacetStore.cpp
    src/markup/SchemaManifest.cpp
    src/markup/DynamicResourceExtension.cpp
    src/markup/StyleSupport.cpp
    src/markup/TemplateSupport.cpp
    src/markup/TemplateBindingExtension.cpp
    src/markup/TemplateCompiler.cpp
    src/markup/TypeExtension.cpp
    src/markup/StaticExtension.cpp
    src/markup/Scopes.cpp
    src/markup/LoaderResult.cpp
    src/markup/ObjectWriterState.cpp
    src/markup/DocumentCache.cpp
    src/markup/Loader.cpp
    src/markup/Resources.cpp
    src/markup/Schema.cpp
    src/markup/ObjectWriter.cpp
    src/markup/SchemaServices.cpp
    src/markup/XamlDocument.cpp)

if(AERO_WITH_EXPAT)
    set(_aero_vendored_expat_target "")
    if(AERO_THIRD_PARTY_ROOT STREQUAL "")
        get_filename_component(_aero_sibling_third_party
            "${CMAKE_CURRENT_SOURCE_DIR}/../AeroGUI/third_party"
            ABSOLUTE)
        if(EXISTS
           "${_aero_sibling_third_party}/expat/expat/CMakeLists.txt")
            set(AERO_THIRD_PARTY_ROOT
                "${_aero_sibling_third_party}")
        endif()
    endif()
    set(_aero_expat_source
        "${AERO_THIRD_PARTY_ROOT}/expat/expat")
    if(EXISTS "${_aero_expat_source}/CMakeLists.txt")
        set(EXPAT_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
        set(EXPAT_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
        set(EXPAT_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        add_subdirectory(
            "${_aero_expat_source}"
            "${CMAKE_CURRENT_BINARY_DIR}/third_party/expat"
            EXCLUDE_FROM_ALL)
        set_target_properties(expat PROPERTIES
            POSITION_INDEPENDENT_CODE ON)
        set(_aero_expat_target expat::expat)
        set(_aero_vendored_expat_target expat)
    else()
        find_package(EXPAT QUIET)
        if(TARGET EXPAT::EXPAT)
            set(_aero_expat_target EXPAT::EXPAT)
        else()
            message(FATAL_ERROR
                "AERO_WITH_EXPAT requires Expat or "
                "${_aero_expat_source}")
        endif()
    endif()
    target_sources(AeroMarkupKernel PRIVATE
        src/markup/ExpatXmlTokenizer.cpp)
    if(_aero_vendored_expat_target)
        target_link_libraries(AeroMarkupKernel PRIVATE
            $<BUILD_INTERFACE:${_aero_expat_target}>
            $<INSTALL_INTERFACE:Aero::_DetailExpat>)
    else()
        target_link_libraries(AeroMarkupKernel PRIVATE
            $<BUILD_INTERFACE:${_aero_expat_target}>
            $<INSTALL_INTERFACE:EXPAT::EXPAT>)
    endif()
endif()

add_library(Aero::_DetailMarkup ALIAS AeroMarkup)

target_include_directories(AeroMarkup
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_compile_definitions(
    AeroMarkupKernel
    PUBLIC
        $<$<BOOL:${AERO_WITH_EXPAT}>:AERO_WITH_EXPAT=1>
        $<$<NOT:$<BOOL:${AERO_WITH_EXPAT}>>:AERO_WITH_EXPAT=0>)
target_link_libraries(AeroMarkup
    PUBLIC Aero::_DetailMarkupKernel Aero::_DetailControls)
target_compile_definitions(
    AeroMarkup PRIVATE AERO_MARKUP_UI_RESOURCES=1)
target_compile_features(AeroMarkup PUBLIC cxx_std_17)
set_target_properties(AeroMarkup PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ${AERO_BUILD_SHARED})

aero_apply_compiler_options(AeroMarkup)

add_library(AeroModuleCatalog ${AERO_LIBRARY_TYPE}
    src/runtime/modules/Module.cpp
    src/runtime/modules/BuiltinModules.cpp
    src/markup/SchemaBundle.cpp)
add_library(Aero::_DetailModuleCatalog ALIAS AeroModuleCatalog)
target_include_directories(AeroModuleCatalog
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(AeroModuleCatalog
    PUBLIC Aero::_DetailMarkup
    PRIVATE Aero::_DetailAppModel)
target_compile_features(AeroModuleCatalog PUBLIC cxx_std_17)
set_target_properties(AeroModuleCatalog PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON)
aero_apply_compiler_options(AeroModuleCatalog)
