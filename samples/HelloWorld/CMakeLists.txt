set(_aero_hello_world_assets
    "${CMAKE_CURRENT_BINARY_DIR}/assets")
set(_aero_hello_world_font
    "${_aero_hello_world_assets}/Aero Matics Regular.ttf")
set(_aero_hello_world_reference_font
    "${CMAKE_SOURCE_DIR}/../AeroGUI/Packages/Samples/HelloWorld/Aero Matics Regular.ttf")

if(NOT EXISTS "${_aero_hello_world_reference_font}")
    message(FATAL_ERROR
        "HelloWorld requires the reference Aero Matics font: "
        "${_aero_hello_world_reference_font}")
endif()

add_executable(AeroHelloWorld
    main.cpp)
target_link_libraries(AeroHelloWorld
    PRIVATE Aero::App)
target_compile_definitions(AeroHelloWorld PRIVATE
    "AERO_HELLO_WORLD_APP_FILE=\"${_aero_hello_world_assets}/App.xaml\"")
target_compile_features(AeroHelloWorld PRIVATE cxx_std_17)
set_target_properties(AeroHelloWorld PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroHelloWorld)

aero_add_xaml(AeroHelloWorld
    OUTPUT_DIRECTORY "${_aero_hello_world_assets}"
    ORIGIN_PREFIX "pack://application:,,,/Aero.HelloWorld;component"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/App.xaml"
        "${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.xaml")

add_custom_command(
    OUTPUT
        "${_aero_hello_world_assets}/App.xaml"
        "${_aero_hello_world_assets}/MainWindow.xaml"
        "${_aero_hello_world_font}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${_aero_hello_world_assets}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/App.xaml"
        "${_aero_hello_world_assets}/App.xaml"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.xaml"
        "${_aero_hello_world_assets}/MainWindow.xaml"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_aero_hello_world_reference_font}"
        "${_aero_hello_world_font}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/App.xaml"
        "${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.xaml"
        "${_aero_hello_world_reference_font}"
    VERBATIM)
add_custom_target(AeroHelloWorldAssets
    DEPENDS
        "${_aero_hello_world_assets}/App.xaml"
        "${_aero_hello_world_assets}/MainWindow.xaml"
        "${_aero_hello_world_font}")
add_dependencies(AeroHelloWorld AeroHelloWorldAssets)

if(AERO_BUILD_TESTS)
    add_test(
        NAME AeroHelloWorldSmoke
        COMMAND AeroHelloWorld --verify)
    set_tests_properties(AeroHelloWorldSmoke PROPERTIES
        LABELS "conformance;product"
        TIMEOUT 30)
endif()
