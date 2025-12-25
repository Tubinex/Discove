function(generate_font_header INPUT_FILE OUTPUT_FILE VARIABLE_NAME)
    file(READ "${INPUT_FILE}" FILE_CONTENTS HEX)
    file(SIZE "${INPUT_FILE}" FILE_SIZE)

    string(LENGTH "${FILE_CONTENTS}" HEX_LENGTH)
    math(EXPR ARRAY_LENGTH "${HEX_LENGTH} / 2")

    string(APPEND HEADER_CONTENT "#pragma once\n\n")
    string(APPEND HEADER_CONTENT "#include <cstddef>\n\n")
    string(APPEND HEADER_CONTENT "namespace EmbeddedFonts {\n\n")
    string(APPEND HEADER_CONTENT "static const unsigned char ${VARIABLE_NAME}[] = {\n")

    set(BYTE_INDEX 0)
    set(LINE_BYTES "")
    while(BYTE_INDEX LESS HEX_LENGTH)
        string(SUBSTRING "${FILE_CONTENTS}" ${BYTE_INDEX} 2 BYTE_HEX)
        string(APPEND LINE_BYTES "0x${BYTE_HEX}, ")

        math(EXPR BYTE_INDEX "${BYTE_INDEX} + 2")
        math(EXPR BYTES_IN_LINE "(${BYTE_INDEX} / 2) % 16")

        if(BYTES_IN_LINE EQUAL 0)
            string(APPEND HEADER_CONTENT "    ${LINE_BYTES}\n")
            set(LINE_BYTES "")
        endif()
    endwhile()

    if(NOT LINE_BYTES STREQUAL "")
        string(APPEND HEADER_CONTENT "    ${LINE_BYTES}\n")
    endif()

    string(APPEND HEADER_CONTENT "};\n\n")
    string(APPEND HEADER_CONTENT "static const size_t ${VARIABLE_NAME}_size = ${FILE_SIZE};\n\n")
    string(APPEND HEADER_CONTENT "}\n")

    file(WRITE "${OUTPUT_FILE}" "${HEADER_CONTENT}")
endfunction()

if(INPUT_FONT AND OUTPUT_HEADER AND FONT_VAR_NAME)
    generate_font_header("${INPUT_FONT}" "${OUTPUT_HEADER}" "${FONT_VAR_NAME}")
endif()
