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
