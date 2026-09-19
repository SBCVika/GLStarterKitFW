foreach(required_variable FIRMWARE_ELF FIRMWARE_MAP READELF OBJCOPY VECTOR_BINARY)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

foreach(required_file FIRMWARE_ELF FIRMWARE_MAP READELF OBJCOPY)
    if(NOT EXISTS "${${required_file}}")
        message(FATAL_ERROR "${required_file} does not exist: ${${required_file}}")
    endif()
endforeach()

function(run_tool output_variable)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${ARGV1} failed: ${error}")
    endif()
    set(${output_variable} "${output}" PARENT_SCOPE)
endfunction()

function(require_single_match variable pattern text description)
    string(REGEX MATCHALL "${pattern}" matches "${text}")
    list(LENGTH matches match_count)
    if(NOT match_count EQUAL 1)
        message(FATAL_ERROR "Expected one ${description}, found ${match_count}")
    endif()
    set(${variable} "${matches}" PARENT_SCOPE)
endfunction()

function(read_symbol symbol expected_type expected_binding expected_size value_variable)
    string(REPLACE "\n" ";" all_symbol_lines "${symbols}")
    set(symbol_lines)
    foreach(symbol_line IN LISTS all_symbol_lines)
        if(symbol_line MATCHES "[ \t]${symbol}([ \t\r]*)$")
            list(APPEND symbol_lines "${symbol_line}")
        endif()
    endforeach()
    list(LENGTH symbol_lines symbol_count)
    if(NOT symbol_count EQUAL 1)
        message(FATAL_ERROR "Expected one ${symbol} symbol, found ${symbol_count}")
    endif()

    list(GET symbol_lines 0 symbol_line)
    string(REGEX MATCH
        "^[ \t]*[0-9]+:[ \t]+([0-9A-Fa-f]+)[ \t]+([0-9]+)[ \t]+${expected_type}[ \t]+${expected_binding}[ \t]+DEFAULT[ \t]+[0-9]+[ \t]+${symbol}$"
        symbol_match
        "${symbol_line}"
    )
    if(NOT symbol_match)
        message(FATAL_ERROR "${symbol} does not have expected ${expected_binding} ${expected_type} metadata: ${symbol_line}")
    endif()
    if(NOT expected_size STREQUAL "ANY" AND NOT CMAKE_MATCH_2 EQUAL expected_size)
        message(FATAL_ERROR "${symbol} has size ${CMAKE_MATCH_2}, expected ${expected_size}")
    endif()
    set(${value_variable} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

function(read_vector_word offset value_variable)
    math(EXPR hex_offset "${offset} * 2")
    string(SUBSTRING "${vector_hex}" ${hex_offset} 8 little_endian_word)
    string(LENGTH "${little_endian_word}" word_length)
    if(NOT word_length EQUAL 8 OR NOT little_endian_word MATCHES "^[0-9A-Fa-f]+$")
        message(FATAL_ERROR "Vector table does not contain a complete word at offset ${offset}")
    endif()
    string(SUBSTRING "${little_endian_word}" 0 2 byte_0)
    string(SUBSTRING "${little_endian_word}" 2 2 byte_1)
    string(SUBSTRING "${little_endian_word}" 4 2 byte_2)
    string(SUBSTRING "${little_endian_word}" 6 2 byte_3)
    set(${value_variable} "${byte_3}${byte_2}${byte_1}${byte_0}" PARENT_SCOPE)
endfunction()

function(require_same_address left right description)
    math(EXPR left_value "0x${left}")
    math(EXPR right_value "0x${right}")
    if(NOT left_value EQUAL right_value)
        message(FATAL_ERROR "${description}: 0x${left} does not equal 0x${right}")
    endif()
endfunction()

run_tool(section_headers "${READELF}" -SW "${FIRMWARE_ELF}")
string(REGEX MATCHALL "[^\n]*\\.isr_vector[^\n]*" vector_section_lines "${section_headers}")
list(LENGTH vector_section_lines vector_section_count)
if(NOT vector_section_count EQUAL 1)
    message(FATAL_ERROR "Expected one .isr_vector section, found ${vector_section_count}")
endif()
list(GET vector_section_lines 0 vector_section_line)
if(NOT vector_section_line MATCHES "^[ \t]*\\[[ \t]*[0-9]+\\][ \t]+\\.isr_vector[ \t]+PROGBITS[ \t]+08000000[ \t]+[0-9A-Fa-f]+[ \t]+000188[ \t]+[0-9A-Fa-f]+[ \t]+A")
    message(FATAL_ERROR "Unexpected .isr_vector section: ${vector_section_line}")
endif()

run_tool(elf_header "${READELF}" -h "${FIRMWARE_ELF}")
string(REGEX MATCH "Entry point address:[ \t]+0x([0-9A-Fa-f]+)" entry_match "${elf_header}")
if(NOT entry_match)
    message(FATAL_ERROR "Could not read the ELF entry point")
endif()
set(entry_address "${CMAKE_MATCH_1}")

run_tool(symbols "${READELF}" -sW "${FIRMWARE_ELF}")
read_symbol(g_pfnVectors OBJECT GLOBAL 392 vector_address)
read_symbol(Reset_Handler FUNC WEAK ANY reset_address)
read_symbol(SVC_Handler FUNC GLOBAL ANY svc_address)
read_symbol(PendSV_Handler FUNC GLOBAL ANY pendsv_address)
read_symbol(SysTick_Handler FUNC GLOBAL ANY systick_address)
require_same_address("${vector_address}" "08000000" "g_pfnVectors address")
require_same_address("${entry_address}" "${reset_address}" "ELF entry point")

file(REMOVE "${VECTOR_BINARY}")
run_tool(vector_output "${OBJCOPY}" -O binary --only-section=.isr_vector "${FIRMWARE_ELF}" "${VECTOR_BINARY}")
file(SIZE "${VECTOR_BINARY}" vector_size)
if(NOT vector_size EQUAL 392)
    message(FATAL_ERROR "Expected a 392-byte vector table, found ${vector_size} bytes")
endif()
file(READ "${VECTOR_BINARY}" vector_hex HEX)
read_vector_word(4 vector_reset_address)
read_vector_word(44 vector_svc_address)
read_vector_word(56 vector_pendsv_address)
read_vector_word(60 vector_systick_address)
require_same_address("${vector_reset_address}" "${reset_address}" "Reset vector")
require_same_address("${vector_svc_address}" "${svc_address}" "SVC vector")
require_same_address("${vector_pendsv_address}" "${pendsv_address}" "PendSV vector")
require_same_address("${vector_systick_address}" "${systick_address}" "SysTick vector")

file(READ "${FIRMWARE_MAP}" firmware_map)
set(cmsis_startup "cmsis-device-f4/Source/Templates/gcc/startup_stm32f407xx\\.s\\.obj")
require_single_match(cmsis_vector_match "\\.isr_vector[^\n]*${cmsis_startup}" "${firmware_map}" "CMSIS vector table input")
require_single_match(cmsis_reset_match "\\.text\\.Reset_Handler[^\n]*\n[^\n]*${cmsis_startup}" "${firmware_map}" "CMSIS Reset_Handler input")

foreach(handler SVC_Handler PendSV_Handler SysTick_Handler)
    require_single_match(
        freertos_${handler}_match
        "\\.text\\.${handler}[^\n]*\n[^\n]*_deps/freertos_kernel-build/libfreertos_kernel\\.a\\(port\\.c\\.obj\\)"
        "${firmware_map}"
        "FreeRTOS ${handler} input"
    )
endforeach()

message(STATUS "Verified CMSIS startup and FreeRTOS exception vectors")
