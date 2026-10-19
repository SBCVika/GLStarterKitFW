cmake_minimum_required(VERSION 3.24)

project(embedded_workshop LANGUAGES C CXX ASM)

if(NOT COMMAND configure_firmware_target)
    message(FATAL_ERROR "Configure with the project toolchain file")
endif()

include(cmake/dependencies.cmake)
add_subdirectory(third_party)
add_subdirectory(platform/stm32f407)

target_compile_definitions(freertos_plus_tcp_network_if PRIVATE
    STM32F4
    xPhyConfigure=stm32f407_xPhyConfigure
)
target_link_libraries(freertos_plus_tcp_network_if PUBLIC stm32f4_hal)
# V4.4.1 utility sources include FreeRTOS_IP.h without first including FreeRTOS.h.
target_compile_options(freertos_plus_tcp_utilities PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,GNU>:-include;FreeRTOS.h>
)

add_executable(${PROJECT_NAME}
    src/ethernet.cpp
    src/humidity_publisher.cpp
    src/main.cpp
    src/motion_publisher.cpp
    src/mqtt_client.cpp
    src/mqtt_transport.cpp
    src/tcp_echo_server.cpp
    src/temperature_publisher.cpp
    src/ui.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_link_libraries(${PROJECT_NAME} PRIVATE
    freertos_kernel
    freertos_config
    freertos_plus_tcp
    freertos_plus_tcp_network_if
    core_mqtt_agent
    stm32f407_platform
)

set(LINKER_SCRIPT
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/stm32f407/linker/STM32F407VGTX_FLASH.ld
)
configure_firmware_target(${PROJECT_NAME} ${LINKER_SCRIPT})

add_custom_target(verify_startup ALL
    COMMAND ${CMAKE_COMMAND}
        -DFIRMWARE_ELF=$<TARGET_FILE:${PROJECT_NAME}>
        -DFIRMWARE_MAP=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
        -DREADELF=${CMAKE_READELF}
        -DOBJCOPY=${CMAKE_OBJCOPY}
        -DVECTOR_BINARY=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.isr_vector.bin
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_startup.cmake
    DEPENDS ${PROJECT_NAME}
    COMMENT "Verifying CMSIS startup and FreeRTOS exception vectors"
    VERBATIM
)
