#include "FreeRTOS.h"
#include "FreeRTOSIPConfig.h"
#include "task.h"

#include "phyHandling.h"
#include "stm32f4xx_hal.h"

#define KSZ8081_PHY_ADDRESS              3U
#define KSZ8081_PHY_ID                   0x00221560UL
#define KSZ8081_OPERATION_MODE_OVERRIDE  0x16U
#define KSZ8081_CONTROL_2                0x1FU
#define KSZ8081_BROADCAST_DISABLE        (1UL << 9U)
#define KSZ8081_RND_25MHZ_MODE           (1UL << 7U)

static void configure_rmii_pins(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF11_ETH;

    gpio.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOC, &gpio);
}

static void reset_phy(void)
{
    GPIO_InitTypeDef gpio = {0};

    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, GPIO_PIN_RESET);
    gpio.Pin = GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &gpio);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
    gpio.Pin = GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &gpio);

    HAL_Delay(10U);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_10, GPIO_PIN_SET);
    HAL_Delay(1U);
}

static BaseType_t configure_ksz8081(ETH_HandleTypeDef *eth)
{
    uint32_t identifier_1;
    uint32_t identifier_2;
    uint32_t value;

    if (HAL_ETH_ReadPHYRegister(eth,
                                KSZ8081_PHY_ADDRESS,
                                2U,
                                &identifier_1) != HAL_OK ||
        HAL_ETH_ReadPHYRegister(eth,
                                KSZ8081_PHY_ADDRESS,
                                3U,
                                &identifier_2) != HAL_OK) {
        return pdFAIL;
    }

    const uint32_t identifier = (identifier_1 << 16U) | (identifier_2 & 0xFFF0U);
    if (identifier != KSZ8081_PHY_ID) {
        return pdFAIL;
    }

    if (HAL_ETH_ReadPHYRegister(eth,
                                KSZ8081_PHY_ADDRESS,
                                KSZ8081_OPERATION_MODE_OVERRIDE,
                                &value) != HAL_OK ||
        HAL_ETH_WritePHYRegister(eth,
                                 KSZ8081_PHY_ADDRESS,
                                 KSZ8081_OPERATION_MODE_OVERRIDE,
                                 value | KSZ8081_BROADCAST_DISABLE) != HAL_OK ||
        HAL_ETH_ReadPHYRegister(eth,
                                KSZ8081_PHY_ADDRESS,
                                KSZ8081_CONTROL_2,
                                &value) != HAL_OK ||
        HAL_ETH_WritePHYRegister(eth,
                                 KSZ8081_PHY_ADDRESS,
                                 KSZ8081_CONTROL_2,
                                 value | KSZ8081_RND_25MHZ_MODE) != HAL_OK ||
        HAL_ETH_ReadPHYRegister(eth,
                                KSZ8081_PHY_ADDRESS,
                                KSZ8081_CONTROL_2,
                                &value) != HAL_OK ||
        (value & KSZ8081_RND_25MHZ_MODE) == 0U) {
        return pdFAIL;
    }

    return pdPASS;
}

void HAL_ETH_MspInit(ETH_HandleTypeDef *eth)
{
    if (eth->Instance != ETH) {
        return;
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_ETH_CLK_ENABLE();

    reset_phy();
    configure_rmii_pins();
    HAL_ETH_SetMDIOClockRange(eth);
    configASSERT(configure_ksz8081(eth) == pdPASS);

    HAL_NVIC_SetPriority(
        ETH_IRQn,
        configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8U - configPRIO_BITS),
        0U);
    HAL_NVIC_EnableIRQ(ETH_IRQn);
}

BaseType_t stm32f407_xPhyConfigure(EthernetPhy_t *phy,
                                    const PhyProperties_t *properties)
{
    const BaseType_t result = xPhyConfigure(phy, properties);
    if (result != 0 || phy->xPortCount != 1 ||
        phy->ucPhyIndexes[0] != KSZ8081_PHY_ADDRESS ||
        phy->ulPhyIDs[0] != KSZ8081_PHY_ID) {
        return -1;
    }

    uint32_t value;
    if (phy->fnPhyRead(KSZ8081_PHY_ADDRESS,
                       KSZ8081_OPERATION_MODE_OVERRIDE,
                       &value) != 0 ||
        phy->fnPhyWrite(KSZ8081_PHY_ADDRESS,
                        KSZ8081_OPERATION_MODE_OVERRIDE,
                        value | KSZ8081_BROADCAST_DISABLE) != 0 ||
        phy->fnPhyRead(KSZ8081_PHY_ADDRESS, KSZ8081_CONTROL_2, &value) != 0 ||
        phy->fnPhyWrite(KSZ8081_PHY_ADDRESS,
                        KSZ8081_CONTROL_2,
                        value | KSZ8081_RND_25MHZ_MODE) != 0 ||
        phy->fnPhyRead(KSZ8081_PHY_ADDRESS, KSZ8081_CONTROL_2, &value) != 0 ||
        (value & KSZ8081_RND_25MHZ_MODE) == 0U) {
        return -1;
    }

    vTaskDelay(pdMS_TO_TICKS(1U));
    return 0;
}
