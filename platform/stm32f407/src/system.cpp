#include <cstdint>

namespace {

volatile std::uint32_t& reg(const std::uintptr_t address)
{
    return *reinterpret_cast<volatile std::uint32_t*>(address);
}

} // namespace

extern "C" {

std::uint32_t SystemCoreClock = 16'000'000U;
extern const std::uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
extern const std::uint8_t APBPrescTable[8] = {0, 0, 0, 0, 1, 2, 3, 4};

extern std::uint32_t g_pfnVectors;

void SystemInit()
{
    // CMSIS calls this before Reset_Handler initializes .data and .bss.
    // Do not depend on initialized RAM or retain writes there.
    constexpr std::uintptr_t scb_vtor_address = 0xE000ED08U;
    constexpr std::uintptr_t scb_cpacr_address = 0xE000ED88U;

    reg(scb_cpacr_address) |= 0xFU << 20U;
    reg(scb_vtor_address) = reinterpret_cast<std::uintptr_t>(&g_pfnVectors);
    __asm volatile ("dsb");
    __asm volatile ("isb");
}

void _init() __attribute__((weak));
void _init()
{
}

} // extern "C"
