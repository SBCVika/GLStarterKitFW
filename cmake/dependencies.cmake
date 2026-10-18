include(FetchContent)

add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/config
)
# Export scheduler objects that OpenOCD needs to enumerate FreeRTOS tasks.
target_compile_definitions(freertos_config INTERFACE
    portREMOVE_STATIC_QUALIFIER
    projCOVERAGE_TEST=0
)

set(FREERTOS_HEAP "4" CACHE STRING "" FORCE)

FetchContent_Declare(
    freertos_kernel
    GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Kernel.git
    GIT_TAG V11.3.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(freertos_kernel)

set(FREERTOS_PLUS_TCP_NETWORK_IF "STM32" CACHE STRING "" FORCE)
set(FREERTOS_PLUS_TCP_STM32_IF_DRIVER "None" CACHE STRING "" FORCE)
set(FREERTOS_PLUS_TCP_BUFFER_ALLOCATION "1" CACHE STRING "" FORCE)

FetchContent_Declare(
    freertos_plus_tcp
    GIT_REPOSITORY https://github.com/FreeRTOS/FreeRTOS-Plus-TCP.git
    GIT_TAG c12361095aca68aeed858f45d14395fbffa92c0d
    GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(freertos_plus_tcp)
