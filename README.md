# Embedded workshop

Minimal CMake/FreeRTOS firmware skeleton for the STM32F407 Discovery board
(STM32F407VGT6). It uses the official STM32F4 HAL and CMSIS components without
requiring a generated STM32CubeMX project.

The initial application starts FreeRTOS and toggles the green LED on PD12 every
500 ms. The MCU remains on its reset clock, the 16 MHz HSI oscillator.

## Prerequisites

- CMake 3.24 or newer
- A CMake build backend such as Ninja
- Arm GNU Toolchain (`arm-none-eabi-gcc`, `arm-none-eabi-g++`,
  `arm-none-eabi-ar`, `arm-none-eabi-ranlib`, `arm-none-eabi-objcopy`, and
  `arm-none-eabi-size` on PATH)
- Git and network access during the first configure, to fetch the pinned
  FreeRTOS kernel release

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

The CMake target currently compiles the HAL GPIO module used by the board LED.
The submodules pin the commits shown above.
