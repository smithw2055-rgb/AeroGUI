add_library(AeroScoreboardModel STATIC
    ScoreboardModel.cpp)
target_include_directories(AeroScoreboardModel
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(AeroScoreboardModel
    PUBLIC
        Aero::ModuleSdk
        Aero::Controls)
target_compile_features(AeroScoreboardModel
    PUBLIC cxx_std_17)
set_target_properties(AeroScoreboardModel PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroScoreboardModel)

aero_add_schema_manifest(AeroScoreboardSchema
    OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/generated/Scoreboard.aeroschema"
    MODULE_HEADER "ScoreboardModel.hpp"
    MODULE_FUNCTION
        "Aero::Samples::Scoreboard::MakeScoreboardModuleManifest"
    LIBRARIES AeroScoreboardModel)

add_custom_target(AeroScoreboardXaml ALL)
aero_add_xaml(AeroScoreboardXaml
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/assets"
    SCHEMA "${AeroScoreboardSchema_OUTPUT}"
    ORIGIN_PREFIX "pack://application:,,,/Aero.Scoreboard;component"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/App.xaml"
        "${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.xaml")
add_dependencies(AeroScoreboardXaml AeroScoreboardSchema)

set(_aero_scoreboard_assets
    "${CMAKE_CURRENT_BINARY_DIR}/assets")
set(_aero_scoreboard_fonts
    "${_aero_scoreboard_assets}/Fonts")
set(_aero_scoreboard_reference_data
    "${CMAKE_SOURCE_DIR}/../AeroGUI/Packages/Samples/Scoreboard/Data")
add_custom_command(
    OUTPUT
        "${_aero_scoreboard_assets}/App.xaml"
        "${_aero_scoreboard_assets}/MainWindow.xaml"
        "${_aero_scoreboard_fonts}/Cheboyga.ttf"
        "${_aero_scoreboard_fonts}/PERRYGOT.TTF"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${_aero_scoreboard_fonts}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/App.xaml"
        "${_aero_scoreboard_assets}/App.xaml"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.xaml"
        "${_aero_scoreboard_assets}/MainWindow.xaml"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_aero_scoreboard_reference_data}/Fonts/Cheboyga.ttf"
        "${_aero_scoreboard_fonts}/Cheboyga.ttf"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${_aero_scoreboard_reference_data}/Fonts/PERRYGOT.TTF"
        "${_aero_scoreboard_fonts}/PERRYGOT.TTF"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/App.xaml"
        "${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.xaml"
        "${_aero_scoreboard_reference_data}/Fonts/Cheboyga.ttf"
        "${_aero_scoreboard_reference_data}/Fonts/PERRYGOT.TTF"
    VERBATIM)
add_custom_target(AeroScoreboardAssets
    DEPENDS
        "${_aero_scoreboard_assets}/App.xaml"
        "${_aero_scoreboard_assets}/MainWindow.xaml"
        "${_aero_scoreboard_fonts}/Cheboyga.ttf"
        "${_aero_scoreboard_fonts}/PERRYGOT.TTF")

add_executable(AeroScoreboard main.cpp)
target_link_libraries(AeroScoreboard
    PRIVATE
        Aero::App
        AeroScoreboardModel)
target_compile_definitions(AeroScoreboard PRIVATE
    "AERO_SCOREBOARD_APP_FILE=\"${_aero_scoreboard_assets}/App.xaml\"")
target_compile_features(AeroScoreboard PRIVATE cxx_std_17)
set_target_properties(AeroScoreboard PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroScoreboard)
add_dependencies(AeroScoreboard
    AeroScoreboardAssets
    AeroScoreboardSchema)
