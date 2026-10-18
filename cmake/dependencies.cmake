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

FetchContent_Declare(
    core_mqtt
    GIT_REPOSITORY https://github.com/FreeRTOS/coreMQTT.git
    GIT_TAG 04845c6a8e5f9cf2d232f1c6e80baeb81302e690 # v5.0.2
)
FetchContent_Declare(
    core_mqtt_agent
    GIT_REPOSITORY https://github.com/FreeRTOS/coreMQTT-Agent.git
    GIT_TAG 2338d00db41196e4a1fce4bc3ab32f907e77f871 # v2.0.0
    GIT_SUBMODULES ""
    # The repository has no top-level CMakeLists.txt; build it from its file list.
    SOURCE_SUBDIR no-cmake-project
)
FetchContent_MakeAvailable(core_mqtt core_mqtt_agent)

target_link_libraries(core_mqtt PUBLIC freertos_config)

include(${core_mqtt_agent_SOURCE_DIR}/mqttAgentFilePaths.cmake)
add_library(core_mqtt_agent STATIC ${MQTT_AGENT_SOURCES})
target_include_directories(core_mqtt_agent PUBLIC ${MQTT_AGENT_INCLUDE_PUBLIC_DIRS})
target_link_libraries(core_mqtt_agent PUBLIC core_mqtt)
