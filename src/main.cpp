#include <algorithm>
#include <array>

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
}

#include "stm32f407/board.hpp"
#include "stm32f407/device_id.hpp"
#include "stm32f407/keyboard.hpp"

#include "ethernet.hpp"
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
        !temperature_publisher.start()) {
        stm32f407::halt();
    }

    vTaskStartScheduler();
    stm32f407::halt();
}
