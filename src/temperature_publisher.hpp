#pragma once

#include <array>
#include <cstdint>

#include "mqtt_client.hpp"
#include "stm32f407/device_id.hpp"
#include "stm32f407/dht11.hpp"

extern "C" {
#include "FreeRTOS.h"
#include "queue.h"
}

namespace network {

// Publishes each reported temperature to the board's temperature topic,
// retrying while the broker is unreachable until a newer temperature replaces it.
class TemperaturePublisher final : public stm32f407::ITemperatureListener {
public:
    TemperaturePublisher(MqttClient& client, const stm32f407::DeviceId& device_id);

    TemperaturePublisher(const TemperaturePublisher&) = delete;
    TemperaturePublisher& operator=(const TemperaturePublisher&) = delete;
    TemperaturePublisher(TemperaturePublisher&&) = delete;
    TemperaturePublisher& operator=(TemperaturePublisher&&) = delete;

    bool start();
    void on_temperature_changed(std::int16_t temperature) override;

private:
    static void task_entry(void* context);

    void run();

    MqttClient& client_;
    // "SBC/" + Device ID + "/temperature" + NUL.
    std::array<char, 41> topic_{};
    // Not a task notification: MqttClient::publish waits on this task's notification.
    QueueHandle_t changes_ = nullptr;
};

} // namespace network
