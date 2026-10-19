#include "humidity_publisher.hpp"

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

// Formats whole percent as {"humidity":45}.
std::array<char, 24> make_payload(std::uint8_t humidity)
{
    constexpr char prefix[] = R"({"humidity":)";

    std::array<char, 24> payload{};
    char* end = std::copy_n(prefix, sizeof(prefix) - 1U, payload.begin());
    end = std::to_chars(end, payload.end(), humidity).ptr;
    *end = '}';
    return payload;
}

} // namespace

namespace network {

HumidityPublisher::HumidityPublisher(MqttClient& client, const stm32f407::DeviceId& device_id)
    : client_(client)
{
    std::size_t length = 0U;
    for (const char* part : {"SBC/", device_id.data(), "/humidity"}) {
        const std::size_t part_length = std::strlen(part);
        std::memcpy(topic_.data() + length, part, part_length);
        length += part_length;
    }
}

bool HumidityPublisher::start()
{
    changes_ = xQueueCreate(1U, sizeof(std::uint8_t));
    if (changes_ == nullptr) {
        return false;
    }

    return xTaskCreate(task_entry,
                       "humidity",
                       publisher_stack_depth,
                       this,
                       publisher_task_priority,
                       nullptr) == pdPASS;
}

void HumidityPublisher::on_humidity_changed(std::uint8_t humidity)
{
    (void)xQueueOverwrite(changes_, &humidity);
}

void HumidityPublisher::task_entry(void* context)
{
    static_cast<HumidityPublisher*>(context)->run();
}

void HumidityPublisher::run()
{
    std::uint8_t humidity = 0U;
    bool pending = false;
    for (;;) {
        // Times out only while a humidity is still pending, to retry it.
        (void)xQueueReceive(changes_, &humidity, pending ? retry_period : portMAX_DELAY);
        pending = !client_.publish(topic_.data(), make_payload(humidity).data());
    }
}

} // namespace network
