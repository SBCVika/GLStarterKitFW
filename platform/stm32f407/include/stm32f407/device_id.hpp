#pragma once

#include <array>
#include <cstdint>

namespace stm32f407 {

// 24 uppercase hex characters, highest-address word first, NUL-terminated.
using DeviceId = std::array<char, 25>;

// The factory-programmed 96-bit unique ID; index 0 is the lowest-address word.
std::array<std::uint32_t, 3> read_unique_id();
DeviceId read_device_id();

} // namespace stm32f407
