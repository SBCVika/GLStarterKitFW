add_library(stm32f407_platform OBJECT
    src/board.cpp
    src/device_id.cpp
    src/dht11.cpp
    src/ethernet.c
    src/i2c1.cpp
    src/keyboard.cpp
    src/lsm9ds1.cpp
    src/system.cpp
    src/wh1602.cpp
    ${PROJECT_SOURCE_DIR}/third_party/cmsis-device-f4/Source/Templates/gcc/startup_stm32f407xx.s
)

target_compile_definitions(stm32f407_platform PUBLIC -DHSE_VALUE=8000000U)
target_compile_definitions(stm32f407_platform PUBLIC -DUSE_HAL_DRIVER)
target_compile_definitions(stm32f407_platform PUBLIC -DDEBUG)
target_compile_definitions(stm32f407_platform PUBLIC -DSTM32F407xx)

target_compile_features(stm32f407_platform PUBLIC cxx_std_17)
target_include_directories(stm32f407_platform PUBLIC include)
target_link_libraries(stm32f407_platform PUBLIC
    freertos_kernel
    freertos_plus_tcp_network_if_common
    lsm9ds1_pid
    stm32f4_hal
)
