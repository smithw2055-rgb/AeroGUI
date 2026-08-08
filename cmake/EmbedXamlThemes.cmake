if(NOT DEFINED OUTPUT OR
   NOT DEFINED LIGHT_SOURCE OR
   NOT DEFINED DARK_SOURCE OR
   NOT DEFINED GENERIC_SOURCE)
    message(FATAL_ERROR
        "EmbedXamlThemes.cmake requires output and source paths")
endif()

# Build the initializer in memory and append it in one write. The former
# byte-by-byte file(APPEND) loop made large compiled dictionaries take minutes
# to embed on CI and scaled quadratically with filesystem overhead.
function(aero_append_byte_array symbol input)
    set(content "")
    if(NOT "${input}" STREQUAL "")
        file(READ "${input}" content HEX)
    endif()
    string(LENGTH "${content}" content_length)
    set(generated
        "inline constexpr std::uint8_t ${symbol}[] = {\n")
    set(offset 0)
    set(chunk_hex_chars 512)
    while(offset LESS content_length)
        math(EXPR remaining "${content_length} - ${offset}")
        if(remaining GREATER chunk_hex_chars)
            set(length ${chunk_hex_chars})
        else()
            set(length ${remaining})
        endif()
        string(SUBSTRING "${content}" ${offset} ${length} chunk)
        string(REGEX REPLACE
            "([0-9A-Fa-f][0-9A-Fa-f])"
            "0x\\1U,"
            chunk "${chunk}")
        string(APPEND generated "${chunk}\n")
        math(EXPR offset "${offset} + ${length}")
    endwhile()
    if(content_length EQUAL 0)
        string(APPEND generated "0x00U,\n")
    endif()
    string(APPEND generated "};\n")
    math(EXPR byte_count "${content_length} / 2")
    string(APPEND generated
        "inline constexpr std::uint32_t ${symbol}Size = ${byte_count}U;\n")
    file(APPEND "${OUTPUT}" "${generated}")
endfunction()

file(WRITE "${OUTPUT}"
    "#pragma once\n\n#include <cstdint>\n\nnamespace Aero {\n")
aero_append_byte_array(AeroThemeLightCompiled "${LIGHT_COMPILED}")
aero_append_byte_array(AeroThemeDarkCompiled "${DARK_COMPILED}")
aero_append_byte_array(AeroThemeGenericCompiled "${GENERIC_COMPILED}")
aero_append_byte_array(AeroThemeLightSource "${LIGHT_SOURCE}")
aero_append_byte_array(AeroThemeDarkSource "${DARK_SOURCE}")
aero_append_byte_array(AeroThemeGenericSource "${GENERIC_SOURCE}")
file(APPEND "${OUTPUT}" "} // namespace Aero\n")
