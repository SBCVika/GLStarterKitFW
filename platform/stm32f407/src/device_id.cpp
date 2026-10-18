#include "stm32f407/device_id.hpp"

#include "stm32f4xx_hal.h"

namespace stm32f407 {

std::array<std::uint32_t, 3> read_unique_id()
{
    const auto* uid = reinterpret_cast<volatile const std::uint32_t*>(UID_BASE);
    return {uid[0], uid[1], uid[2]};
}

DeviceId read_device_id()
{
    constexpr char hex_digits[] = "0123456789ABCDEF";
    const auto words = read_unique_id();

    DeviceId id{};
    char* out = id.data();
    for (auto word = words.rbegin(); word != words.rend(); ++word) {
        for (std::uint32_t shift = 32U; shift > 0U; shift -= 4U) {
            *out++ = hex_digits[(*word >> (shift - 4U)) & 0xFU];
        }
    }
    return id;
}

} // namespace stm32f407
