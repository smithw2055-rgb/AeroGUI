# Offline tools link product targets; they do not recompile product objects.
if(AERO_BUILD_TOOLS)
    add_executable(aero-schema-gen tools/schema-gen/main.cpp)
    add_executable(Aero::schema-gen ALIAS aero-schema-gen)
    target_link_libraries(aero-schema-gen PRIVATE Aero::App)
    target_include_directories(aero-schema-gen PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(aero-schema-gen PRIVATE
        AERO_GUI_IMPLEMENTATION=1)
    target_compile_features(aero-schema-gen PRIVATE cxx_std_17)
    set_target_properties(aero-schema-gen PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    aero_apply_compiler_options(aero-schema-gen)

    add_executable(aero-xamlc tools/xamlc/main.cpp)
    add_executable(Aero::xamlc ALIAS aero-xamlc)
    target_link_libraries(aero-xamlc PRIVATE
        Aero::App
        Aero::Audio)
    target_include_directories(aero-xamlc PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_compile_definitions(aero-xamlc PRIVATE
        AERO_GUI_IMPLEMENTATION=1)
    target_compile_features(aero-xamlc PRIVATE cxx_std_17)
    set_target_properties(aero-xamlc PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)
    aero_apply_compiler_options(aero-xamlc)
endif()
