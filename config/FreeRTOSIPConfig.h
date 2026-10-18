#pragma once

#define FREERTOS_IP_CONFIG_H

#include "FreeRTOS.h"

#define ipconfigBYTE_ORDER                         pdFREERTOS_LITTLE_ENDIAN

#define ipconfigUSE_IPv4                          1
#define ipconfigUSE_IPv6                          0
#define ipconfigIPv4_BACKWARD_COMPATIBLE          0

#define ipconfigUSE_TCP                           1
#define ipconfigUSE_TCP_WIN                       0
#define ipconfigUSE_UDP                           1
#define ipconfigUSE_DHCP                          0
#define ipconfigUSE_DNS                           1
#define ipconfigUSE_MDNS                          0
#define ipconfigUSE_LLMNR                         0
#define ipconfigUSE_NBNS                          0

#define ipconfigNETWORK_MTU                       1500U
#define ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS    16
#define ipconfigEVENT_QUEUE_LENGTH                21
#define ipconfigIP_TASK_PRIORITY                  (configMAX_PRIORITIES - 2U)
#define ipconfigIP_TASK_STACK_SIZE_WORDS          1024U

#define ipconfigZERO_COPY_RX_DRIVER               1
#define ipconfigZERO_COPY_TX_DRIVER               1
#define ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM    1
#define ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM    1
#define ipconfigETHERNET_DRIVER_FILTERS_FRAME_TYPES 1
#define ipconfigUSE_LINKED_RX_MESSAGES            1

#define ipconfigPHY_MAX_PORTS                     1
#define ipconfigPHY_LS_HIGH_CHECK_TIME_MS         1000
#define ipconfigPHY_LS_LOW_CHECK_TIME_MS          1000

#define ipconfigUSE_NETWORK_EVENT_HOOK            1
#define ipconfigREPLY_TO_INCOMING_PINGS            1
#define ipconfigSUPPORT_OUTGOING_PINGS             0

#define ipconfigARP_CACHE_ENTRIES                 6
#define ipconfigMAX_ARP_RETRANSMISSIONS           5
#define ipconfigMAX_ARP_AGE                       150

#define ipconfigSOCK_DEFAULT_RECEIVE_BLOCK_TIME   pdMS_TO_TICKS(5000U)
#define ipconfigSOCK_DEFAULT_SEND_BLOCK_TIME      pdMS_TO_TICKS(5000U)
#define ipconfigUDP_MAX_SEND_BLOCK_TIME_TICKS     pdMS_TO_TICKS(1000U)

#define ipconfigHAS_PRINTF                        0
#define ipconfigHAS_DEBUG_PRINTF                  0
