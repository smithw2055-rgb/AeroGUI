cmake_minimum_required(VERSION 3.20)
project(AeroInstalledSdkConsumer LANGUAGES CXX)

find_package(Aero 0.3 CONFIG REQUIRED)

add_executable(AeroSdkConsumer main.cpp)
target_link_libraries(AeroSdkConsumer PRIVATE Aero::EngineHost)
target_compile_features(AeroSdkConsumer PRIVATE cxx_std_17)
set_target_properties(AeroSdkConsumer PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO)

aero_add_xaml(AeroSdkConsumer
    SCHEMA "${Aero_SCHEMA_MANIFEST}"
    ORIGIN_PREFIX "pack://application:,,,/Aero.SdkConsumer;component"
    SOURCES
        Views/Main.xaml
        Themes/Main.xaml)

list(LENGTH AeroSdkConsumer_XAML_OUTPUTS _aero_output_count)
if(NOT _aero_output_count EQUAL 2)
    message(FATAL_ERROR "Expected two compiled XAML outputs")
endif()
list(GET AeroSdkConsumer_XAML_OUTPUTS 0 _aero_first_output)
list(GET AeroSdkConsumer_XAML_OUTPUTS 1 _aero_second_output)
if(_aero_first_output STREQUAL _aero_second_output)
    message(FATAL_ERROR "Equal XAML basenames collided")
endif()
