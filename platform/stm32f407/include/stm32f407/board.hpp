#pragma once

namespace stm32f407 {

bool initialize_system_clock();
void initialize_board();
void toggle_green_led();
[[noreturn]] void halt();

} // namespace stm32f407
