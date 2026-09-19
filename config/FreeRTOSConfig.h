#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
extern uint32_t SystemCoreClock;
#ifdef __cplusplus
}
#endif

#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICK_HOOK                     0
#define configUSE_IDLE_HOOK                     0
#define configCPU_CLOCK_HZ                      SystemCoreClock
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                128
#define configTOTAL_HEAP_SIZE                   (16 * 1024)
#define configMAX_TASK_NAME_LEN                 16
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define INCLUDE_xTaskGetSchedulerState           1
/* Expose the queue registry required by the RTOS Views debugger extension. */
#define configQUEUE_REGISTRY_SIZE               10
#define configUSE_TIMERS                        0
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         1
/* Provide task IDs and stack bounds to debugger RTOS views. */
#define configUSE_TRACE_FACILITY                1
#define configRECORD_STACK_HIGH_ADDRESS         1
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0

#define configPRIO_BITS                         4
#define configKERNEL_INTERRUPT_PRIORITY         (15 << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (5 << (8 - configPRIO_BITS))

#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskSuspend                    1

#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

#define configASSERT(condition)                 \
    do {                                        \
        if ((condition) == 0) {                 \
            __asm volatile ("cpsid i");         \
            for (;;) {                          \
            }                                   \
        }                                       \
    } while (0)
