#pragma once

#include <cstdint>
#include <string_view>

namespace stm32f407 {

// Drives the WH1602W display in 4-bit mode on PE7 and PE10–PE15, with its
// backlight on PE9. R/W stays low, so every command waits its worst-case time.
class Wh1602 {
public:
    static constexpr std::uint8_t rows = 2U;
    static constexpr std::uint8_t columns = 16U;
    // A custom glyph, for writing "°C".
    static constexpr char degree = '\x01';

    Wh1602() = default;

    Wh1602(const Wh1602&) = delete;
    Wh1602& operator=(const Wh1602&) = delete;
    Wh1602(Wh1602&&) = delete;
    Wh1602& operator=(Wh1602&&) = delete;

    // Call once from a task: it waits out the display's power-up time.
    void initialize();

    void clear();
    // Writes from the given position, cutting the text off at the end of the row.
    void write(std::uint8_t row, std::uint8_t column, std::string_view text);
    void set_backlight(bool on);

private:
    void configure_pins();
    void write_nibble(std::uint8_t nibble);
    void write_byte(bool data, std::uint8_t value);
    void command(std::uint8_t value);

    bool initialized_ = false;
};

} // namespace stm32f407
