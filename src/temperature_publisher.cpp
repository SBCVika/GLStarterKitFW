#include "temperature_publisher.hpp"

#include <cstddef>
#include <cstring>
#include <initializer_list>

extern "C" {
#include "task.h"
}

namespace {

constexpr configSTACK_DEPTH_TYPE publisher_stack_depth = 512U;
constexpr UBaseType_t publisher_task_priority = tskIDLE_PRIORITY + 1U;
constexpr TickType_t publish_period = pdMS_TO_TICKS(1000U);
constexpr char payload[] = R"({"temperature":36.6})";

} // namespace

namespace network {

TemperaturePublisher::TemperaturePublisher(MqttClient& client,
                                           const stm32f407::DeviceId& device_id)
    : client_(client)
{
    std::size_t length = 0U;
    for (const char* part : {"SBC/", device_id.data(), "/temperature"}) {
        const std::size_t part_length = std::strlen(part);
        std::memcpy(topic_.data() + length, part, part_length);
        length += part_length;
    }
}

bool TemperaturePublisher::start()
{
    return xTaskCreate(task_entry,
                       "temperature",
                       publisher_stack_depth,
                       this,
                       publisher_task_priority,
                       nullptr) == pdPASS;
}

void TemperaturePublisher::task_entry(void* context)
{
    static_cast<TemperaturePublisher*>(context)->run();
}

void TemperaturePublisher::run()
{
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        xTaskDelayUntil(&last_wake, publish_period);
        // Samples taken while disconnected are dropped.
        client_.publish(topic_.data(), payload);
    }
}

} // namespace network
