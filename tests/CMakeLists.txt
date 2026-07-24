include_directories("${CMAKE_CURRENT_SOURCE_DIR}/support")

add_executable(AeroBaseTests
    base/BaseTests.cpp)
target_link_libraries(AeroBaseTests PRIVATE Aero::Base)
target_compile_features(AeroBaseTests PRIVATE cxx_std_17)
set_target_properties(AeroBaseTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroBaseTests)
add_test(NAME AeroBaseTests COMMAND AeroBaseTests)

add_executable(AeroContainerTests base/ContainerTests.cpp)
target_link_libraries(AeroContainerTests PRIVATE Aero::Base)
target_compile_features(AeroContainerTests PRIVATE cxx_std_17)
set_target_properties(AeroContainerTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroContainerTests)
add_test(NAME AeroContainerTests COMMAND AeroContainerTests)

add_executable(AeroTextProviderTests text/TextProviderTests.cpp)
target_link_libraries(AeroTextProviderTests PRIVATE Aero::Text)
target_compile_features(AeroTextProviderTests PRIVATE cxx_std_17)
set_target_properties(AeroTextProviderTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroTextProviderTests)
add_test(NAME AeroTextProviderTests COMMAND AeroTextProviderTests)

add_executable(AeroCoreTests core/TypeRegistryTests.cpp)
target_link_libraries(AeroCoreTests PRIVATE Aero::Presentation)
target_compile_features(AeroCoreTests PRIVATE cxx_std_17)
set_target_properties(AeroCoreTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroCoreTests)
add_test(NAME AeroCoreTests COMMAND AeroCoreTests)

add_executable(AeroMetadataModuleTests core/MetadataModuleTests.cpp)
target_link_libraries(AeroMetadataModuleTests PRIVATE Aero::Controls)
target_compile_features(AeroMetadataModuleTests PRIVATE cxx_std_17)
set_target_properties(AeroMetadataModuleTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroMetadataModuleTests)
add_test(NAME AeroMetadataModuleTests COMMAND AeroMetadataModuleTests)

add_test(
    NAME AeroArchitectureChecks
    COMMAND "${CMAKE_COMMAND}"
        "-DAERO_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
        -P "${CMAKE_SOURCE_DIR}/cmake/CheckArchitecture.cmake")

add_executable(AeroDiagnosticsTests core/DiagnosticsTests.cpp)
target_link_libraries(AeroDiagnosticsTests PRIVATE Aero::Core)
target_compile_features(AeroDiagnosticsTests PRIVATE cxx_std_17)
set_target_properties(AeroDiagnosticsTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroDiagnosticsTests)
add_test(NAME AeroDiagnosticsTests COMMAND AeroDiagnosticsTests)

add_executable(AeroDispatcherTests core/DispatcherTests.cpp)
target_link_libraries(AeroDispatcherTests PRIVATE Aero::Core)
target_compile_features(AeroDispatcherTests PRIVATE cxx_std_17)
set_target_properties(AeroDispatcherTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroDispatcherTests)
add_test(NAME AeroDispatcherTests COMMAND AeroDispatcherTests)

add_executable(AeroDependencyPropertyTests core/DependencyPropertyTests.cpp)
target_link_libraries(AeroDependencyPropertyTests PRIVATE Aero::Core)
target_compile_features(AeroDependencyPropertyTests PRIVATE cxx_std_17)
set_target_properties(AeroDependencyPropertyTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroDependencyPropertyTests)
add_test(NAME AeroDependencyPropertyTests COMMAND AeroDependencyPropertyTests)

add_executable(AeroBindingPathTests core/BindingPathTests.cpp)
target_link_libraries(AeroBindingPathTests PRIVATE Aero::Core)
target_compile_features(AeroBindingPathTests PRIVATE cxx_std_17)
set_target_properties(AeroBindingPathTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroBindingPathTests)
add_test(NAME AeroBindingPathTests COMMAND AeroBindingPathTests)

add_executable(AeroBindingTests presentation/BindingTests.cpp)
target_link_libraries(AeroBindingTests PRIVATE Aero::Presentation)
target_compile_features(AeroBindingTests PRIVATE cxx_std_17)
set_target_properties(AeroBindingTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroBindingTests)
add_test(NAME AeroBindingTests COMMAND AeroBindingTests)

add_executable(AeroEffectiveValueTests core/EffectiveValueEngineTests.cpp)
target_link_libraries(AeroEffectiveValueTests PRIVATE Aero::Core)
target_compile_features(AeroEffectiveValueTests PRIVATE cxx_std_17)
set_target_properties(AeroEffectiveValueTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroEffectiveValueTests)
add_test(NAME AeroEffectiveValueTests COMMAND AeroEffectiveValueTests)

add_executable(AeroStyleTests presentation/StyleTests.cpp)
target_link_libraries(AeroStyleTests PRIVATE Aero::Presentation)
target_compile_features(AeroStyleTests PRIVATE cxx_std_17)
set_target_properties(AeroStyleTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroStyleTests)
add_test(NAME AeroStyleTests COMMAND AeroStyleTests)

add_executable(AeroObjectTreeTests presentation/ObjectTreeTests.cpp)
target_link_libraries(AeroObjectTreeTests PRIVATE Aero::Presentation)
target_compile_features(AeroObjectTreeTests PRIVATE cxx_std_17)
set_target_properties(AeroObjectTreeTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroObjectTreeTests)
add_test(NAME AeroObjectTreeTests COMMAND AeroObjectTreeTests)

add_executable(AeroDataContextTests presentation/DataContextTests.cpp)
target_link_libraries(AeroDataContextTests PRIVATE Aero::Presentation)
target_compile_features(AeroDataContextTests PRIVATE cxx_std_17)
set_target_properties(AeroDataContextTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroDataContextTests)
add_test(NAME AeroDataContextTests COMMAND AeroDataContextTests)

add_executable(AeroLayoutTests controls/LayoutTests.cpp)
target_link_libraries(AeroLayoutTests PRIVATE Aero::Controls)
target_compile_features(AeroLayoutTests PRIVATE cxx_std_17)
set_target_properties(AeroLayoutTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroLayoutTests)
add_test(NAME AeroLayoutTests COMMAND AeroLayoutTests)

add_executable(AeroTemplateTests controls/TemplatesTests.cpp)
target_link_libraries(AeroTemplateTests PRIVATE Aero::Controls)
target_compile_features(AeroTemplateTests PRIVATE cxx_std_17)
set_target_properties(AeroTemplateTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroTemplateTests)
add_test(NAME AeroTemplateTests COMMAND AeroTemplateTests)

add_executable(AeroInputTests presentation/InputTests.cpp)
target_link_libraries(AeroInputTests PRIVATE Aero::Controls)
target_compile_features(AeroInputTests PRIVATE cxx_std_17)
set_target_properties(AeroInputTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroInputTests)
add_test(NAME AeroInputTests COMMAND AeroInputTests)

add_executable(AeroRenderingTests presentation/RenderingTests.cpp)
target_link_libraries(AeroRenderingTests PRIVATE Aero::Controls)
target_compile_features(AeroRenderingTests PRIVATE cxx_std_17)
set_target_properties(AeroRenderingTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroRenderingTests)
add_test(NAME AeroRenderingTests COMMAND AeroRenderingTests)

add_executable(AeroMarkupTests markup/MarkupTests.cpp)
target_link_libraries(AeroMarkupTests PRIVATE Aero::Markup)
target_compile_features(AeroMarkupTests PRIVATE cxx_std_17)
set_target_properties(AeroMarkupTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroMarkupTests)
add_test(NAME AeroMarkupTests COMMAND AeroMarkupTests)

add_executable(AeroXamlObjectWriterTests markup/XamlObjectWriterTests.cpp)
target_link_libraries(AeroXamlObjectWriterTests PRIVATE Aero::Markup)
target_compile_features(AeroXamlObjectWriterTests PRIVATE cxx_std_17)
set_target_properties(AeroXamlObjectWriterTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroXamlObjectWriterTests)
add_test(NAME AeroXamlObjectWriterTests COMMAND AeroXamlObjectWriterTests)

add_executable(AeroXamlDirectivesResourcesTests
    markup/XamlDirectivesResourcesTests.cpp)
target_link_libraries(AeroXamlDirectivesResourcesTests PRIVATE Aero::Markup)
target_compile_features(AeroXamlDirectivesResourcesTests PRIVATE cxx_std_17)
set_target_properties(AeroXamlDirectivesResourcesTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroXamlDirectivesResourcesTests)
add_test(NAME AeroXamlDirectivesResourcesTests COMMAND AeroXamlDirectivesResourcesTests)

add_executable(AeroXamlDependencyPropertyTests
    markup/XamlDependencyPropertyTests.cpp)
target_link_libraries(AeroXamlDependencyPropertyTests PRIVATE Aero::Markup)
target_compile_features(AeroXamlDependencyPropertyTests PRIVATE cxx_std_17)
set_target_properties(AeroXamlDependencyPropertyTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroXamlDependencyPropertyTests)
add_test(NAME AeroXamlDependencyPropertyTests COMMAND AeroXamlDependencyPropertyTests)

add_executable(AeroXamlBindingTests markup/XamlBindingTests.cpp)
target_link_libraries(AeroXamlBindingTests PRIVATE Aero::Markup)
target_compile_features(AeroXamlBindingTests PRIVATE cxx_std_17)
set_target_properties(AeroXamlBindingTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroXamlBindingTests)
add_test(NAME AeroXamlBindingTests COMMAND AeroXamlBindingTests)

add_executable(AeroXamlModuleSdkTests
    markup/XamlModuleSdkTests.cpp)
target_link_libraries(AeroXamlModuleSdkTests PRIVATE Aero::Markup)
target_compile_features(AeroXamlModuleSdkTests PRIVATE cxx_std_17)
set_target_properties(AeroXamlModuleSdkTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroXamlModuleSdkTests)
add_test(NAME AeroXamlModuleSdkTests COMMAND AeroXamlModuleSdkTests)
if(TARGET aero-xamlc)
    add_test(
        NAME AeroXamlCompilerSmoke
        COMMAND aero-xamlc --check
            "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/xamlc_smoke.xaml")
endif()

add_executable(AeroXamlStyleTests markup/XamlStyleTests.cpp)
target_link_libraries(AeroXamlStyleTests PRIVATE Aero::Markup)
target_compile_features(AeroXamlStyleTests PRIVATE cxx_std_17)
set_target_properties(AeroXamlStyleTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroXamlStyleTests)
add_test(NAME AeroXamlStyleTests COMMAND AeroXamlStyleTests)

add_executable(AeroXamlLayoutTests markup/XamlLayoutTests.cpp)
target_link_libraries(AeroXamlLayoutTests PRIVATE Aero::Markup)
target_compile_features(AeroXamlLayoutTests PRIVATE cxx_std_17)
set_target_properties(AeroXamlLayoutTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroXamlLayoutTests)
add_test(NAME AeroXamlLayoutTests COMMAND AeroXamlLayoutTests)

add_executable(AeroXamlTextBlockTests markup/XamlTextBlockTests.cpp)
target_link_libraries(AeroXamlTextBlockTests PRIVATE Aero::Markup)
target_compile_features(AeroXamlTextBlockTests PRIVATE cxx_std_17)
set_target_properties(AeroXamlTextBlockTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroXamlTextBlockTests)
add_test(NAME AeroXamlTextBlockTests COMMAND AeroXamlTextBlockTests)

add_executable(AeroXamlBorderTests markup/XamlBorderTests.cpp)
target_link_libraries(AeroXamlBorderTests PRIVATE Aero::Markup)
target_compile_features(AeroXamlBorderTests PRIVATE cxx_std_17)
set_target_properties(AeroXamlBorderTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroXamlBorderTests)
add_test(NAME AeroXamlBorderTests COMMAND AeroXamlBorderTests)

add_executable(AeroXamlVisualTreeTests markup/XamlVisualTreeTests.cpp)
target_link_libraries(AeroXamlVisualTreeTests PRIVATE Aero::Markup)
target_compile_features(AeroXamlVisualTreeTests PRIVATE cxx_std_17)
set_target_properties(AeroXamlVisualTreeTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroXamlVisualTreeTests)
add_test(NAME AeroXamlVisualTreeTests COMMAND AeroXamlVisualTreeTests)

add_executable(AeroXamlCustomControlTests markup/XamlCustomControlTests.cpp)
target_link_libraries(AeroXamlCustomControlTests PRIVATE Aero::Markup)
target_compile_features(AeroXamlCustomControlTests PRIVATE cxx_std_17)
set_target_properties(AeroXamlCustomControlTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroXamlCustomControlTests)
add_test(NAME AeroXamlCustomControlTests COMMAND AeroXamlCustomControlTests)

add_executable(AeroRhiTests rhi/RhiTests.cpp)
target_link_libraries(AeroRhiTests PRIVATE Aero::Render)
target_compile_features(AeroRhiTests PRIVATE cxx_std_17)
set_target_properties(AeroRhiTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroRhiTests)
add_test(NAME AeroRhiTests COMMAND AeroRhiTests)

add_executable(AeroGraphicsRhiTests rhi/GraphicsRhiTests.cpp)
target_link_libraries(AeroGraphicsRhiTests PRIVATE Aero::Rhi)
target_compile_features(AeroGraphicsRhiTests PRIVATE cxx_std_17)
set_target_properties(AeroGraphicsRhiTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroGraphicsRhiTests)
add_test(NAME AeroGraphicsRhiTests COMMAND AeroGraphicsRhiTests)

add_executable(AeroSurfaceBackendTests rhi/SurfaceBackendTests.cpp)
target_link_libraries(AeroSurfaceBackendTests PRIVATE Aero::Rhi)
target_compile_features(AeroSurfaceBackendTests PRIVATE cxx_std_17)
set_target_properties(AeroSurfaceBackendTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroSurfaceBackendTests)
add_test(NAME AeroSurfaceBackendTests COMMAND AeroSurfaceBackendTests)

if(WIN32 AND TARGET AeroRhiD3D11)
    set(_aero_d3d11_fxc_hints
        "$ENV{WindowsSdkDir}bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64")
    file(GLOB _aero_d3d11_fxc_sdk_directories
        LIST_DIRECTORIES true
        "C:/Program Files (x86)/Windows Kits/10/bin/*/x64")
    list(APPEND _aero_d3d11_fxc_hints ${_aero_d3d11_fxc_sdk_directories})
    find_program(AERO_D3D11_FXC_EXECUTABLE
        NAMES fxc.exe
        HINTS ${_aero_d3d11_fxc_hints}
        DOC "Windows SDK fxc executable used to compile Aero D3D11 test shaders")
    if(NOT AERO_D3D11_FXC_EXECUTABLE)
        message(FATAL_ERROR
            "AeroD3D11BackendTests requires the Windows SDK x64 fxc.exe")
    endif()

    set(_aero_d3d11_shader_source
        "${CMAKE_CURRENT_SOURCE_DIR}/unit/shaders/D3D11Rectangle.hlsl")
    set(_aero_d3d11_texture_array_shader_source
        "${CMAKE_CURRENT_SOURCE_DIR}/unit/shaders/D3D11TextureArray.hlsl")
    set(_aero_d3d11_shader_directory
        "${CMAKE_CURRENT_BINARY_DIR}/generated/d3d11-shaders")
    set(_aero_d3d11_vertex_shader_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RectangleVertexShader.hpp")
    set(_aero_d3d11_pixel_shader_header
        "${_aero_d3d11_shader_directory}/AeroD3D11RectanglePixelShader.hpp")
    set(_aero_d3d11_texture_array_vertex_shader_header
        "${_aero_d3d11_shader_directory}/AeroD3D11TextureArrayVertexShader.hpp")
    set(_aero_d3d11_texture_array_pixel_shader_header
        "${_aero_d3d11_shader_directory}/AeroD3D11TextureArrayPixelShader.hpp")

    add_custom_command(
        OUTPUT "${_aero_d3d11_vertex_shader_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T vs_4_0 /E vs_main
            /Vn AeroD3D11RectangleVertexShader
            /Fh "${_aero_d3d11_vertex_shader_header}"
            "${_aero_d3d11_shader_source}"
        DEPENDS "${_aero_d3d11_shader_source}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_aero_d3d11_pixel_shader_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T ps_4_0 /E ps_main
            /Vn AeroD3D11RectanglePixelShader
            /Fh "${_aero_d3d11_pixel_shader_header}"
            "${_aero_d3d11_shader_source}"
        DEPENDS "${_aero_d3d11_shader_source}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_aero_d3d11_texture_array_vertex_shader_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T vs_4_0 /E vs_main
            /Vn AeroD3D11TextureArrayVertexShader
            /Fh "${_aero_d3d11_texture_array_vertex_shader_header}"
            "${_aero_d3d11_texture_array_shader_source}"
        DEPENDS "${_aero_d3d11_texture_array_shader_source}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_aero_d3d11_texture_array_pixel_shader_header}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${_aero_d3d11_shader_directory}"
        COMMAND "${AERO_D3D11_FXC_EXECUTABLE}" /nologo /Ges /WX
            /T ps_4_0 /E ps_main
            /Vn AeroD3D11TextureArrayPixelShader
            /Fh "${_aero_d3d11_texture_array_pixel_shader_header}"
            "${_aero_d3d11_texture_array_shader_source}"
        DEPENDS "${_aero_d3d11_texture_array_shader_source}"
        VERBATIM)
    add_custom_target(AeroD3D11TestShaders
        DEPENDS
            "${_aero_d3d11_vertex_shader_header}"
            "${_aero_d3d11_pixel_shader_header}"
            "${_aero_d3d11_texture_array_vertex_shader_header}"
            "${_aero_d3d11_texture_array_pixel_shader_header}")

    add_executable(AeroD3D11BackendTests rhi/D3D11BackendTests.cpp)
    add_dependencies(AeroD3D11BackendTests AeroD3D11TestShaders)
    target_include_directories(AeroD3D11BackendTests PRIVATE
        "${_aero_d3d11_shader_directory}")
    target_link_libraries(
        AeroD3D11BackendTests PRIVATE Aero::RhiD3D11 d3d11)
    target_compile_features(AeroD3D11BackendTests PRIVATE cxx_std_17)
    set_target_properties(AeroD3D11BackendTests PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    aero_apply_compiler_options(AeroD3D11BackendTests)
    add_test(NAME AeroD3D11BackendTests COMMAND AeroD3D11BackendTests)
    set_tests_properties(AeroD3D11BackendTests PROPERTIES TIMEOUT 60)

    add_executable(AeroD3D11SurfaceTests rhi/D3D11SurfaceTests.cpp)
    target_link_libraries(
        AeroD3D11SurfaceTests PRIVATE Aero::Markup Aero::RhiD3D11 d3d11 user32)
    target_compile_features(AeroD3D11SurfaceTests PRIVATE cxx_std_17)
    set_target_properties(AeroD3D11SurfaceTests PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    aero_apply_compiler_options(AeroD3D11SurfaceTests)
    add_test(NAME AeroD3D11SurfaceTests COMMAND AeroD3D11SurfaceTests)
    set_tests_properties(AeroD3D11SurfaceTests PROPERTIES TIMEOUT 60)
endif()

add_executable(AeroCAbiSmoke abi/CAbiSmoke.c)
target_link_libraries(AeroCAbiSmoke PRIVATE Aero::Base)
set_target_properties(AeroCAbiSmoke PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO LINKER_LANGUAGE CXX)
if(MSVC)
    target_compile_options(AeroCAbiSmoke PRIVATE /W4 /utf-8 $<$<BOOL:${AERO_WARNINGS_AS_ERRORS}>:/WX>)
else()
    target_compile_options(AeroCAbiSmoke PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion $<$<BOOL:${AERO_WARNINGS_AS_ERRORS}>:-Werror>)
endif()
add_test(NAME AeroCAbiSmoke COMMAND AeroCAbiSmoke)
