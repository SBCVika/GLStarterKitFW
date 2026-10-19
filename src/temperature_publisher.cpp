#include "temperature_publisher.hpp"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstring>
#include <initializer_list>

extern "C" {
#include "task.h"
}

namespace {

constexpr configSTACK_DEPTH_TYPE publisher_stack_depth = 512U;
constexpr UBaseType_t publisher_task_priority = tskIDLE_PRIORITY + 1U;
constexpr TickType_t retry_period = pdMS_TO_TICKS(1000U);

// Formats tenths of a degree Celsius as {"temperature":-12.3}.
std::array<char, 32> make_payload(std::int16_t temperature)
{
    constexpr char prefix[] = R"({"temperature":)";

    std::array<char, 32> payload{};
    char* end = std::copy_n(prefix, sizeof(prefix) - 1U, payload.begin());
    int magnitude = temperature;
    if (magnitude < 0) {
        *end++ = '-';
        magnitude = -magnitude;
    }
    end = std::to_chars(end, payload.end(), magnitude / 10).ptr;
    *end++ = '.';
    *end++ = static_cast<char>('0' + magnitude % 10);
    *end = '}';
    return payload;
}

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
    changes_ = xQueueCreate(1U, sizeof(std::int16_t));
    if (changes_ == nullptr) {
        return false;
    }

    return xTaskCreate(task_entry,
                       "temperature",
                       publisher_stack_depth,
                       this,
                       publisher_task_priority,
                       nullptr) == pdPASS;
}

void TemperaturePublisher::on_temperature_changed(std::int16_t temperature)
{
    (void)xQueueOverwrite(changes_, &temperature);
}

void TemperaturePublisher::task_entry(void* context)
{
    static_cast<TemperaturePublisher*>(context)->run();
}

void TemperaturePublisher::run()
{
    std::int16_t temperature = 0;
    bool pending = false;
    for (;;) {
        // Times out only while a temperature is still pending, to retry it.
        (void)xQueueReceive(changes_, &temperature, pending ? retry_period : portMAX_DELAY);
        pending = !client_.publish(topic_.data(), make_payload(temperature).data());
    }
}

} // namespace network
