#pragma once

#include <array>

#include "mqtt_client.hpp"
#include "stm32f407/device_id.hpp"

namespace network {

// Publishes the board's temperature to its temperature topic once a second.
class TemperaturePublisher final {
public:
    TemperaturePublisher(MqttClient& client, const stm32f407::DeviceId& device_id);

    TemperaturePublisher(const TemperaturePublisher&) = delete;
    TemperaturePublisher& operator=(const TemperaturePublisher&) = delete;
    TemperaturePublisher(TemperaturePublisher&&) = delete;
    TemperaturePublisher& operator=(TemperaturePublisher&&) = delete;

    bool start();

private:
    static void task_entry(void* context);

    void run();

    MqttClient& client_;
    // "SBC/" + Device ID + "/temperature" + NUL.
    std::array<char, 41> topic_{};
};

} // namespace network
