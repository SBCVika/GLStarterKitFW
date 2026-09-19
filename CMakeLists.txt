cmake_minimum_required(VERSION 3.24)

project(embedded_workshop LANGUAGES C CXX ASM)

if(NOT COMMAND configure_firmware_target)
    message(FATAL_ERROR "Configure with the project toolchain file")
endif()

include(cmake/dependencies.cmake)
add_subdirectory(third_party)
add_subdirectory(platform/stm32f407)

add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_link_libraries(${PROJECT_NAME} PRIVATE
    freertos_kernel
    freertos_config
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
