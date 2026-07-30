add_library(AeroMenu3DModel STATIC Menu3DModel.cpp)
target_include_directories(AeroMenu3DModel PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# The command model directly controls the hosted view, so the schema generator
# must link the application/runtime surface as well as the metadata SDK.
target_link_libraries(AeroMenu3DModel PUBLIC Aero::ModuleSdk Aero::App)
target_compile_features(AeroMenu3DModel PUBLIC cxx_std_17)
set_target_properties(AeroMenu3DModel PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroMenu3DModel)

aero_add_schema_manifest(AeroMenu3DSchema
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/Menu3D.aeroschema"
    MODULE_HEADER "Menu3DModel.hpp"
    MODULE_FUNCTION "Aero::Samples::Menu3D::MakeMenu3DModuleManifest"
    LIBRARIES AeroMenu3DModel)

set(_aero_menu3d_assets "${CMAKE_CURRENT_BINARY_DIR}/assets")
set(_aero_menu3d_images "${_aero_menu3d_assets}/Images")
set(_aero_menu3d_sounds "${_aero_menu3d_assets}/Sounds")
set(_aero_menu3d_reference "${CMAKE_SOURCE_DIR}/../AeroGUI/Packages/Samples/Menu3D/Data")
set(_aero_menu3d_image_names BackClouds.png FrontClouds.png FrontTrees.png LakeAndMountains.png SkyAndSun.png)
set(_aero_menu3d_sound_names SoundTrack.mp3 WaterDropBig.mp3 WaterDropSmall.mp3)

add_custom_target(AeroMenu3DXaml ALL)
aero_add_xaml(AeroMenu3DXaml
    OUTPUT_DIRECTORY "${_aero_menu3d_assets}"
    SCHEMA "${AeroMenu3DSchema_OUTPUT}"
    ORIGIN_PREFIX "pack://application:,,,/Aero.Menu3D;component"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/App.xaml" "${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.xaml")
add_dependencies(AeroMenu3DXaml AeroMenu3DSchema)

set(_aero_menu3d_outputs "${_aero_menu3d_assets}/App.xaml" "${_aero_menu3d_assets}/MainWindow.xaml")
foreach(_image IN LISTS _aero_menu3d_image_names)
    list(APPEND _aero_menu3d_outputs "${_aero_menu3d_images}/${_image}")
endforeach()
foreach(_sound IN LISTS _aero_menu3d_sound_names)
    list(APPEND _aero_menu3d_outputs "${_aero_menu3d_sounds}/${_sound}")
endforeach()
add_custom_command(
    OUTPUT ${_aero_menu3d_outputs}
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_aero_menu3d_images}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_aero_menu3d_sounds}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/App.xaml" "${_aero_menu3d_assets}/App.xaml"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.xaml" "${_aero_menu3d_assets}/MainWindow.xaml"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_aero_menu3d_reference}/Images/BackClouds.png" "${_aero_menu3d_images}/BackClouds.png"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_aero_menu3d_reference}/Images/FrontClouds.png" "${_aero_menu3d_images}/FrontClouds.png"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_aero_menu3d_reference}/Images/FrontTrees.png" "${_aero_menu3d_images}/FrontTrees.png"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_aero_menu3d_reference}/Images/LakeAndMountains.png" "${_aero_menu3d_images}/LakeAndMountains.png"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/Images/SkyAndSun.png" "${_aero_menu3d_images}/SkyAndSun.png"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_aero_menu3d_reference}/Sounds/SoundTrack.mp3" "${_aero_menu3d_sounds}/SoundTrack.mp3"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_aero_menu3d_reference}/Sounds/WaterDropBig.mp3" "${_aero_menu3d_sounds}/WaterDropBig.mp3"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_aero_menu3d_reference}/Sounds/WaterDropSmall.mp3" "${_aero_menu3d_sounds}/WaterDropSmall.mp3"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/App.xaml" "${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.xaml"
        "${_aero_menu3d_reference}/Images/BackClouds.png" "${_aero_menu3d_reference}/Images/FrontClouds.png" "${_aero_menu3d_reference}/Images/FrontTrees.png" "${_aero_menu3d_reference}/Images/LakeAndMountains.png" "${CMAKE_CURRENT_SOURCE_DIR}/Images/SkyAndSun.png"
        "${_aero_menu3d_reference}/Sounds/SoundTrack.mp3" "${_aero_menu3d_reference}/Sounds/WaterDropBig.mp3" "${_aero_menu3d_reference}/Sounds/WaterDropSmall.mp3"
    VERBATIM)
add_custom_target(AeroMenu3DAssets ALL DEPENDS ${_aero_menu3d_outputs})

add_executable(AeroMenu3D main.cpp)
target_link_libraries(AeroMenu3D PRIVATE Aero::App AeroMenu3DModel)
target_compile_definitions(AeroMenu3D PRIVATE
    "AERO_MENU3D_APP_FILE=\"${_aero_menu3d_assets}/App.xaml\""
    "AERO_MENU3D_ASSET_DIR=\"${_aero_menu3d_assets}\"")
target_compile_features(AeroMenu3D PRIVATE cxx_std_17)
set_target_properties(AeroMenu3D PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
aero_apply_compiler_options(AeroMenu3D)
add_dependencies(AeroMenu3D AeroMenu3DAssets AeroMenu3DSchema)

if(AERO_BUILD_TESTS)
    add_test(NAME AeroMenu3DSmoke COMMAND AeroMenu3D --verify)
    set_tests_properties(AeroMenu3DSmoke PROPERTIES LABELS "conformance;product" TIMEOUT 30)
endif()
