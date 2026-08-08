cmake_minimum_required(VERSION 3.24)
project(AeroAppTemplate LANGUAGES CXX)

find_package(Aero CONFIG REQUIRED)

add_executable(AeroAppTemplate
    main.cpp)
target_link_libraries(AeroAppTemplate PRIVATE Aero::App)
target_compile_features(AeroAppTemplate PRIVATE cxx_std_17)

foreach(_xaml App.xaml MainWindow.xaml)
    add_custom_command(TARGET AeroAppTemplate POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/${_xaml}"
            "$<TARGET_FILE_DIR:AeroAppTemplate>/${_xaml}"
        VERBATIM)
endforeach()
