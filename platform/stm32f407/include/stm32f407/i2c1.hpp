#pragma once

#include <cstddef>
#include <cstdint>

extern "C" {
#include "FreeRTOS.h"
#include "semphr.h"
}

#include "stm32f4xx_hal.h"

namespace stm32f407 {

// The I2C1 bus on PB6 (SCL) and PB9 (SDA) at 100 kHz, shared by the board's
// I2C devices. Transfers from different tasks take turns.
class I2c1 {
public:
    I2c1() = default;

    I2c1(const I2c1&) = delete;
    I2c1& operator=(const I2c1&) = delete;
    I2c1(I2c1&&) = delete;
    I2c1& operator=(I2c1&&) = delete;

    bool start();

    // Addresses are 7-bit; reg is the device's register address.
    bool read(std::uint8_t address, std::uint8_t reg, std::uint8_t* data, std::size_t length);
    bool write(std::uint8_t address,
               std::uint8_t reg,
               const std::uint8_t* data,
               std::size_t length);

private:
    I2C_HandleTypeDef handle_{};
    SemaphoreHandle_t mutex_ = nullptr;
};

} // namespace stm32f407
