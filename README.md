# Embedded workshop

Minimal CMake/FreeRTOS firmware for the STM32F407 Discovery board
(STM32F407VGT6). It uses the official STM32F4 HAL and CMSIS components without
requiring a generated STM32CubeMX project.

The application starts FreeRTOS, retains the button/keyboard input support, and
runs FreeRTOS-Plus-TCP over the board's KSZ8081RNDCA Ethernet PHY. The PHY is
polled for link-state changes, so its PE8 interrupt pin is not used.

## Prerequisites

- CMake 3.24 or newer
- A CMake build backend such as Ninja
- Arm GNU Toolchain (`arm-none-eabi-gcc`, `arm-none-eabi-g++`,
  `arm-none-eabi-ar`, `arm-none-eabi-ranlib`, `arm-none-eabi-objcopy`, and
  `arm-none-eabi-size` on PATH)
- Git and network access during the first configure, to fetch the pinned
  FreeRTOS kernel and FreeRTOS-Plus-TCP releases

## Build

Clone the repository with its STM32 dependencies:

```sh
git clone --recurse-submodules <repository-url>
```

For an existing clone, initialize them with:

```sh
git submodule update --init --recursive
```

```sh
cmake --preset debug
cmake --build --preset debug
```

The build creates these files in `build/debug/`:

- `embedded_workshop.elf`
- `embedded_workshop.hex`
- `embedded_workshop.bin`
- `embedded_workshop.map`

## Ethernet

The firmware uses these fixed IPv4 settings:

- Address: `192.168.1.42`
- Netmask: `255.255.255.0`
- Gateway and DNS: `192.168.1.1`
- TCP echo service: port `5000`

The locally administered MAC address is derived from the MCU's 96-bit unique
identifier. Connect the board and a host to the same `192.168.1.0/24` network,
then test ICMP and the echo service with:

```sh
ping 192.168.1.42
printf 'hello\n' | nc 192.168.1.42 5000
```

The second command should print `hello` back to the terminal.

## Project layout

- `src/` contains application code.
- `config/` contains the FreeRTOS application configuration.
- `platform/stm32f407/` contains board support and the linker script.
- `cmake/toolchains/` contains compiler-specific configuration.
- `third_party/` contains upstream HAL and CMSIS Git submodules, including the
  STM32F4 startup code.
- `THIRD_PARTY_NOTICES.md` records third-party licensing boundaries.

## Cloned STM32 dependencies

The following official STMicroelectronics repositories are Git submodules under
`third_party/`:

- [STM32F4 HAL driver](https://github.com/STMicroelectronics/stm32f4xx-hal-driver.git),
  commit `1f6451c3e07728b4c830744de380e56bf5bc0026`
- [STM32F4 CMSIS Device](https://github.com/STMicroelectronics/cmsis-device-f4.git),
  commit `a833f4af71410f25b01468f976560d7ff63a2fc9`
- [CMSIS Core](https://github.com/STMicroelectronics/cmsis-core.git), commit
  `afc5ca6af0a49232fde7eb4548dd0962d119ce14`

FreeRTOS-Plus-TCP V4.4.1 is fetched at the exact commit recorded in
`THIRD_PARTY_NOTICES.md`. The submodules pin the STM32 commits shown above.
