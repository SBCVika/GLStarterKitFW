#pragma once

#include <array>

#include "mqtt_client.hpp"
#include "stm32f407/device_id.hpp"
#include "stm32f407/lsm9ds1.hpp"

extern "C" {
#include "FreeRTOS.h"
#include "semphr.h"
}

namespace network {

// Publishes each reported acceleration, angular rate and magnetic field to the
// board's topic for it, retrying while the broker is unreachable until a newer
// reading of the same quantity replaces it.
class MotionPublisher final : public stm32f407::IAccelerationListener,
                              public stm32f407::IAngularRateListener,
                              public stm32f407::IMagneticFieldListener {
public:
    MotionPublisher(MqttClient& client, const stm32f407::DeviceId& device_id);

    MotionPublisher(const MotionPublisher&) = delete;
    MotionPublisher& operator=(const MotionPublisher&) = delete;
    MotionPublisher(MotionPublisher&&) = delete;
    MotionPublisher& operator=(MotionPublisher&&) = delete;

    bool start();
    void on_acceleration_changed(const stm32f407::Vector3& acceleration) override;
    void on_angular_rate_changed(const stm32f407::Vector3& angular_rate) override;
    void on_magnetic_field_changed(const stm32f407::Vector3& magnetic_field) override;

private:
    struct Quantity {
        const char* name;
        // "SBC/" + Device ID + "/magnetic_field" + NUL, the longest of the three.
        std::array<char, 44> topic;
        // Written by the sensor's task, taken by the publisher's task.
        stm32f407::Vector3 latest;
        bool fresh;
    };

    static void task_entry(void* context);

    void update(Quantity& quantity, const stm32f407::Vector3& reading);
    void run();

    MqttClient& client_;
    std::array<Quantity, 3> quantities_{};
    // Not a task notification: MqttClient::publish waits on this task's notification.
    SemaphoreHandle_t changed_ = nullptr;
};

} // namespace network
