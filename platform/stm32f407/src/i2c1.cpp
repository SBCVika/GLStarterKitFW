#include "stm32f407/i2c1.hpp"

namespace {

constexpr std::uint32_t clock_speed_hz = 100000U;
constexpr std::uint32_t transfer_timeout_ms = 10U;

} // namespace

namespace stm32f407 {

bool I2c1::start()
{
    configASSERT(mutex_ == nullptr);

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    // Open drain against the board's external pull-ups.
    GPIO_InitTypeDef gpio{};
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_AF_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &gpio);

    handle_.Instance = I2C1;
    handle_.Init.ClockSpeed = clock_speed_hz;
    handle_.Init.DutyCycle = I2C_DUTYCYCLE_2;
    handle_.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    handle_.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    handle_.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    handle_.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&handle_) != HAL_OK) {
        return false;
    }

    mutex_ = xSemaphoreCreateMutex();
    return mutex_ != nullptr;
}

bool I2c1::read(std::uint8_t address, std::uint8_t reg, std::uint8_t* data, std::size_t length)
{
    (void)xSemaphoreTake(mutex_, portMAX_DELAY);
    const HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&handle_,
                                                      static_cast<std::uint16_t>(address << 1U),
                                                      reg,
                                                      I2C_MEMADD_SIZE_8BIT,
                                                      data,
                                                      static_cast<std::uint16_t>(length),
                                                      transfer_timeout_ms);
    (void)xSemaphoreGive(mutex_);
    return status == HAL_OK;
}

bool I2c1::write(std::uint8_t address,
                 std::uint8_t reg,
                 const std::uint8_t* data,
                 std::size_t length)
{
    (void)xSemaphoreTake(mutex_, portMAX_DELAY);
    // The HAL takes a non-const buffer but only reads from it when transmitting.
    const HAL_StatusTypeDef status = HAL_I2C_Mem_Write(&handle_,
                                                       static_cast<std::uint16_t>(address << 1U),
                                                       reg,
                                                       I2C_MEMADD_SIZE_8BIT,
                                                       const_cast<std::uint8_t*>(data),
                                                       static_cast<std::uint16_t>(length),
                                                       transfer_timeout_ms);
    (void)xSemaphoreGive(mutex_);
    return status == HAL_OK;
}

} // namespace stm32f407
