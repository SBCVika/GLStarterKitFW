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

// Publishes each reported humidity to the board's humidity topic,
// retrying while the broker is unreachable until a newer humidity replaces it.
class HumidityPublisher final : public stm32f407::IHumidityListener {
public:
    HumidityPublisher(MqttClient& client, const stm32f407::DeviceId& device_id);

    HumidityPublisher(const HumidityPublisher&) = delete;
    HumidityPublisher& operator=(const HumidityPublisher&) = delete;
    HumidityPublisher(HumidityPublisher&&) = delete;
    HumidityPublisher& operator=(HumidityPublisher&&) = delete;

    bool start();
    void on_humidity_changed(std::uint8_t humidity) override;

private:
    static void task_entry(void* context);

    void run();

    MqttClient& client_;
    // "SBC/" + Device ID + "/humidity" + NUL.
    std::array<char, 38> topic_{};
    // Not a task notification: MqttClient::publish waits on this task's notification.
    QueueHandle_t changes_ = nullptr;
};

} // namespace network
