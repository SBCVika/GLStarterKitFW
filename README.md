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
  FreeRTOS kernel, FreeRTOS-Plus-TCP, coreMQTT, and coreMQTT-Agent releases

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

- Address: `192.168.137.42`
- Netmask: `255.255.255.0`
- Gateway and DNS: `192.168.137.1`
- TCP echo service: port `5000`

These match Windows Internet Connection Sharing: connect the board to a
Windows PC's Ethernet port and share the PC's internet connection to that
adapter, which then takes `192.168.137.1` and serves routing and DNS.

The locally administered MAC address is derived from the MCU's 96-bit unique
identifier. From a host on the `192.168.137.0/24` network, test ICMP and the
echo service with:

```sh
ping 192.168.137.42
printf 'hello\n' | nc 192.168.137.42 5000
```

The second command should print `hello` back to the terminal.

## MQTT

Once the link is up, the firmware connects to `test.mosquitto.org` on port
`1883` (plain TCP, MQTT 5) and publishes `{"temperature":36.6}` once a second,
with QoS 0 and without retain, to:

```text
SBC/<Device ID>/temperature
```

The Device ID is the MCU's 96-bit unique identifier as 24 uppercase hex
characters, highest-address word first. The MQTT client identifier is `sbc-`
followed by the last 16 of those characters. The value is a fixed placeholder,
not a sensor reading.

The board's network must route to the internet on TCP port 1883. Watch the
messages from a host with:

```sh
mosquitto_sub -h test.mosquitto.org -t 'SBC/+/temperature' -v
```

While the link or broker is unavailable, samples are dropped and the client
reconnects with a backoff of 1 s doubling to 30 s; a link that comes back up
triggers an immediate reconnect. The test broker is public, so anyone can
read or publish to these topics.

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

FreeRTOS-Plus-TCP V4.4.1, coreMQTT v5.0.2, and coreMQTT-Agent v2.0.0 are
fetched at the exact commits recorded in `THIRD_PARTY_NOTICES.md`. The submodules pin the STM32 commits shown above.
