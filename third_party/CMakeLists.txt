add_library(stm32f4_cmsis INTERFACE)
target_include_directories(stm32f4_cmsis SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/cmsis-core/CMSIS/Core/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/cmsis-device-f4/Include
)
target_compile_definitions(stm32f4_cmsis INTERFACE
    STM32F407xx
    USE_HAL_DRIVER
)

add_library(stm32f4_hal STATIC
    stm32f4xx-hal-driver/Src/stm32f4xx_hal.c
    stm32f4xx-hal-driver/Src/stm32f4xx_hal_cortex.c
    stm32f4xx-hal-driver/Src/stm32f4xx_hal_dma.c
    stm32f4xx-hal-driver/Src/stm32f4xx_hal_eth.c
    stm32f4xx-hal-driver/Src/stm32f4xx_hal_gpio.c
    stm32f4xx-hal-driver/Src/stm32f4xx_hal_i2c.c
    stm32f4xx-hal-driver/Src/stm32f4xx_hal_pwr.c
    stm32f4xx-hal-driver/Src/stm32f4xx_hal_pwr_ex.c
    stm32f4xx-hal-driver/Src/stm32f4xx_hal_rcc.c
)
target_include_directories(stm32f4_hal PUBLIC ${PROJECT_SOURCE_DIR}/config)
target_include_directories(stm32f4_hal SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/stm32f4xx-hal-driver/Inc
)
target_link_libraries(stm32f4_hal PUBLIC stm32f4_cmsis)

add_library(lsm9ds1_pid STATIC
    lsm9ds1-pid/lsm9ds1_reg.c
)
target_include_directories(lsm9ds1_pid SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lsm9ds1-pid
)
