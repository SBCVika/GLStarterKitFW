set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(FREERTOS_PORT "GCC_ARM_CM4F" CACHE STRING "" FORCE)

set(ARM_CPU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)
string(JOIN " " ARM_CPU_FLAGS_STRING ${ARM_CPU_FLAGS})

set(CMAKE_C_FLAGS_INIT
    "${ARM_CPU_FLAGS_STRING} -ffunction-sections -fdata-sections -Wall -Wextra -Wpedantic"
)
set(CMAKE_CXX_FLAGS_INIT
    "${CMAKE_C_FLAGS_INIT} -fno-exceptions -fno-rtti -fno-threadsafe-statics"
)
set(CMAKE_ASM_FLAGS_INIT "${ARM_CPU_FLAGS_STRING}")

find_program(ARM_NONE_EABI_GCC arm-none-eabi-gcc REQUIRED)
find_program(ARM_NONE_EABI_GXX arm-none-eabi-g++ REQUIRED)
find_program(ARM_NONE_EABI_AR arm-none-eabi-ar REQUIRED)
find_program(ARM_NONE_EABI_RANLIB arm-none-eabi-ranlib REQUIRED)
find_program(ARM_NONE_EABI_OBJCOPY arm-none-eabi-objcopy REQUIRED)
find_program(ARM_NONE_EABI_SIZE arm-none-eabi-size REQUIRED)
find_program(ARM_NONE_EABI_READELF arm-none-eabi-readelf REQUIRED)

set(CMAKE_C_COMPILER ${ARM_NONE_EABI_GCC})
set(CMAKE_CXX_COMPILER ${ARM_NONE_EABI_GXX})
set(CMAKE_ASM_COMPILER ${ARM_NONE_EABI_GCC})
set(CMAKE_AR ${ARM_NONE_EABI_AR} CACHE FILEPATH "archive tool")
set(CMAKE_RANLIB ${ARM_NONE_EABI_RANLIB} CACHE FILEPATH "archive index tool")
set(CMAKE_OBJCOPY ${ARM_NONE_EABI_OBJCOPY} CACHE FILEPATH "objcopy tool")
set(CMAKE_SIZE ${ARM_NONE_EABI_SIZE} CACHE FILEPATH "size tool")
set(CMAKE_READELF ${ARM_NONE_EABI_READELF} CACHE FILEPATH "ELF inspection tool")

function(configure_firmware_target target linker_script)
    target_link_options(${target} PRIVATE
        ${ARM_CPU_FLAGS}
        -T${linker_script}
        -nostartfiles
        --specs=nano.specs
        --specs=nosys.specs
        -Wl,--gc-sections
        # Keep the task and queue symbols consumed by OpenOCD and RTOS Views.
        -Wl,--undefined=pxCurrentTCB
        -Wl,--undefined=pxReadyTasksLists
        -Wl,--undefined=xDelayedTaskList1
        -Wl,--undefined=xDelayedTaskList2
        -Wl,--undefined=pxDelayedTaskList
        -Wl,--undefined=pxOverflowDelayedTaskList
        -Wl,--undefined=xPendingReadyList
        -Wl,--undefined=uxCurrentNumberOfTasks
        -Wl,--undefined=uxTopUsedPriority
        -Wl,--undefined=xQueueRegistry
        -Wl,-Map=${target}.map
        -Wl,--print-memory-usage
    )
    set_target_properties(${target} PROPERTIES
        SUFFIX ".elf"
        LINK_DEPENDS ${linker_script}
    )

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex
                $<TARGET_FILE:${target}> ${target}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary
                $<TARGET_FILE:${target}> ${target}.bin
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${target}>
        COMMENT "Generating firmware images"
    )
endfunction()
