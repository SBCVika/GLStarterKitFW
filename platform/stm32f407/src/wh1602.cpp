#include "stm32f407/wh1602.hpp"

#include <array>

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
}

#include "stm32f4xx_hal.h"

namespace {

constexpr std::uint16_t rs_pin = GPIO_PIN_7;
constexpr std::uint16_t backlight_pin = GPIO_PIN_9;
constexpr std::uint16_t rw_pin = GPIO_PIN_10;
constexpr std::uint16_t enable_pin = GPIO_PIN_11;
// DB4–DB7 are PE12–PE15.
constexpr std::uint32_t data_shift = 12U;
constexpr std::uint16_t data_pins = 0xFU << data_shift;

// ST7066U worst cases: 37 µs per command or character, 1.52 ms to clear.
constexpr std::uint32_t command_time_us = 50U;
constexpr std::uint32_t clear_time_us = 2000U;
constexpr TickType_t power_up_time = pdMS_TO_TICKS(50U);

constexpr std::uint8_t clear_display = 0x01U;
constexpr std::uint8_t entry_mode_increment = 0x06U;
constexpr std::uint8_t display_off = 0x08U;
constexpr std::uint8_t display_on = 0x0CU;
constexpr std::uint8_t function_set_4bit_2line = 0x28U;
constexpr std::uint8_t set_cgram_address = 0x40U;
constexpr std::uint8_t set_ddram_address = 0x80U;
constexpr std::array<std::uint8_t, 2> row_addresses{0x00U, 0x40U};

constexpr std::array<std::uint8_t, 8> degree_glyph{
    0b00110,
    0b01001,
    0b01001,
    0b00110,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
};

void delay_us(std::uint32_t us)
{
    const std::uint32_t start = DWT->CYCCNT;
    const std::uint32_t cycles = us * (SystemCoreClock / 1000000U);
    while (DWT->CYCCNT - start < cycles) {
    }
}

} // namespace

namespace stm32f407 {

void Wh1602::initialize()
{
    configASSERT(!initialized_);

    configure_pins();
    vTaskDelay(power_up_time);

    // The datasheet's reset sequence, which works whatever mode the controller was left in.
    write_nibble(0x3U);
    delay_us(4100U);
    write_nibble(0x3U);
    delay_us(100U);
    write_nibble(0x3U);
    delay_us(command_time_us);
    write_nibble(0x2U);
    delay_us(command_time_us);

    command(function_set_4bit_2line);
    command(display_off);
    clear();
    command(entry_mode_increment);

    command(static_cast<std::uint8_t>(set_cgram_address | (degree * degree_glyph.size())));
    for (const std::uint8_t line : degree_glyph) {
        write_byte(true, line);
    }

    command(display_on);
    initialized_ = true;
}

void Wh1602::clear()
{
    write_byte(false, clear_display);
    delay_us(clear_time_us);
}

void Wh1602::write(std::uint8_t row, std::uint8_t column, std::string_view text)
{
    configASSERT(row < rows && column < columns);

    command(static_cast<std::uint8_t>(set_ddram_address | (row_addresses[row] + column)));
    for (std::uint8_t index = 0U; index < text.size() && column + index < columns; ++index) {
        write_byte(true, static_cast<std::uint8_t>(text[index]));
    }
}

void Wh1602::set_backlight(bool on)
{
    HAL_GPIO_WritePin(GPIOE, backlight_pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Wh1602::configure_pins()
{
    __HAL_RCC_GPIOE_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOE,
                      rs_pin | backlight_pin | rw_pin | enable_pin | data_pins,
                      GPIO_PIN_RESET);

    GPIO_InitTypeDef gpio{};
    gpio.Pin = rs_pin | backlight_pin | rw_pin | enable_pin | data_pins;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &gpio);
}

void Wh1602::write_nibble(std::uint8_t nibble)
{
    const std::uint32_t bits = static_cast<std::uint32_t>(nibble & 0xFU) << data_shift;
    GPIOE->BSRR = bits | ((data_pins & ~bits) << 16U);

    // Enable needs 230 ns high and a 500 ns cycle; 1 µs each side covers both.
    delay_us(1U);
    HAL_GPIO_WritePin(GPIOE, enable_pin, GPIO_PIN_SET);
    delay_us(1U);
    HAL_GPIO_WritePin(GPIOE, enable_pin, GPIO_PIN_RESET);
    delay_us(1U);
}

void Wh1602::write_byte(bool data, std::uint8_t value)
{
    HAL_GPIO_WritePin(GPIOE, rs_pin, data ? GPIO_PIN_SET : GPIO_PIN_RESET);
    write_nibble(static_cast<std::uint8_t>(value >> 4U));
    write_nibble(value);
    delay_us(command_time_us);
}

void Wh1602::command(std::uint8_t value)
{
    write_byte(false, value);
}

} // namespace stm32f407
