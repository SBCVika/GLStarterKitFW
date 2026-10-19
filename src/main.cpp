#include <algorithm>
#include <array>
#include <cstdint>

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
}

#include "stm32f407/board.hpp"
#include "stm32f407/device_id.hpp"
#include "stm32f407/dht11.hpp"
#include "stm32f407/i2c1.hpp"
#include "stm32f407/keyboard.hpp"
#include "stm32f407/lsm9ds1.hpp"

#include "ethernet.hpp"
#include "humidity_publisher.hpp"
#include "motion_publisher.hpp"
#include "mqtt_client.hpp"
#include "tcp_echo_server.hpp"
#include "temperature_publisher.hpp"

namespace {

class KeyboardListener final : public stm32f407::IButtonListener {
public:
    void on_button_event(const stm32f407::ButtonEvent& event) override
    {
        if (event.button == stm32f407::Button::SW1 &&
            event.type == stm32f407::ButtonEventType::PRESSED) {
            stm32f407::toggle_green_led();
        }
    }
};

stm32f407::Keyboard keyboard;
KeyboardListener keyboard_listener;

constexpr network::EthernetConfig ethernet_config{
    {192U, 168U, 137U, 42U},
    {255U, 255U, 255U, 0U},
    {192U, 168U, 137U, 1U},
    {192U, 168U, 137U, 1U},
};

network::Ethernet ethernet{ethernet_config};
network::TcpEchoServer echo_server;

// "sbc-" + the last 16 Device ID characters + NUL, within MQTT 3.1.1's guaranteed 23.
std::array<char, 21> make_mqtt_client_id(const stm32f407::DeviceId& device_id)
{
    std::array<char, 21> client_id{'s', 'b', 'c', '-'};
    std::copy_n(device_id.begin() + 8, 16, client_id.begin() + 4);
    return client_id;
}

const stm32f407::DeviceId device_id = stm32f407::read_device_id();
const std::array<char, 21> mqtt_client_id = make_mqtt_client_id(device_id);
network::MqttClient mqtt_client{{"test.mosquitto.org", 1883U}, mqtt_client_id.data()};
network::TemperaturePublisher temperature_publisher{mqtt_client, device_id};
network::HumidityPublisher humidity_publisher{mqtt_client, device_id};
network::MotionPublisher motion_publisher{mqtt_client, device_id};

stm32f407::Dht11 climate_sensor;
// Whole percent and tenths of a degree Celsius, matching the DHT11's 1 % and 1 °C resolution.
constexpr std::uint8_t humidity_reporting_threshold = 1U;
constexpr std::uint16_t temperature_reporting_threshold = 10U;

stm32f407::I2c1 i2c1;
stm32f407::Lsm9ds1 motion_sensor{i2c1};
// About 3° of tilt, 5 dps and 5–10° of heading in Earth's field.
constexpr std::int32_t acceleration_reporting_threshold = 5;
constexpr std::int32_t angular_rate_reporting_threshold = 500;
constexpr std::int32_t magnetic_field_reporting_threshold = 5;

void led_task(void*)
{
    for (;;) {
        //stm32f407::toggle_green_led();
        vTaskDelay(pdMS_TO_TICKS(1500U));
    }
}

} // namespace

extern "C" {

void vApplicationMallocFailedHook()
{
    stm32f407::halt();
}

void vApplicationStackOverflowHook(TaskHandle_t, char*)
{
    stm32f407::halt();
}

// coreMQTT uses assert(); newlib's default handler would pull in stdio.
void __assert_func(const char*, int, const char*, const char*)
{
    stm32f407::halt();
}

} // extern "C"

int main()
{
    if (!stm32f407::initialize_system_clock()) {
        stm32f407::halt();
    }

    stm32f407::initialize_board();



    if (xTaskCreate(led_task,
                    "blink",
                    configMINIMAL_STACK_SIZE,
                    nullptr,
                    tskIDLE_PRIORITY + 1U,
                    nullptr) != pdPASS) {
        stm32f407::halt();
    }

    keyboard.set_listener(&keyboard_listener);
    keyboard.start();

    if (!ethernet.start() || !echo_server.start() ||
        !ethernet.add_link_state_client(echo_server)) {
        stm32f407::halt();
    }

    if (!mqtt_client.start() || !ethernet.add_link_state_client(mqtt_client) ||
        !temperature_publisher.start() || !humidity_publisher.start() ||
        !motion_publisher.start()) {
        stm32f407::halt();
    }

    climate_sensor.set_humidity_listener(&humidity_publisher, humidity_reporting_threshold);
    climate_sensor.set_temperature_listener(&temperature_publisher,
                                            temperature_reporting_threshold);
    climate_sensor.start();

    if (!i2c1.start()) {
        stm32f407::halt();
    }

    motion_sensor.set_acceleration_listener(&motion_publisher, acceleration_reporting_threshold);
    motion_sensor.set_angular_rate_listener(&motion_publisher, angular_rate_reporting_threshold);
    motion_sensor.set_magnetic_field_listener(&motion_publisher,
                                              magnetic_field_reporting_threshold);
    // A missing motion sensor leaves the rest of the board running.
    (void)motion_sensor.start();

    vTaskStartScheduler();
    stm32f407::halt();
}
