#include "motion_publisher.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <initializer_list>

extern "C" {
#include "task.h"
}

namespace {

constexpr configSTACK_DEPTH_TYPE publisher_stack_depth = 512U;
constexpr UBaseType_t publisher_task_priority = tskIDLE_PRIORITY + 1U;
constexpr TickType_t retry_period = pdMS_TO_TICKS(1000U);

constexpr std::size_t acceleration = 0U;
constexpr std::size_t angular_rate = 1U;
constexpr std::size_t magnetic_field = 2U;

// Writes thousandths as a decimal with three places, such as -0.003.
char* write_thousandths(char* out, char* end, std::int32_t thousandths)
{
    if (thousandths < 0) {
        *out++ = '-';
        thousandths = -thousandths;
    }
    out = std::to_chars(out, end, thousandths / 1000).ptr;
    *out++ = '.';
    const std::int32_t fraction = thousandths % 1000;
    *out++ = static_cast<char>('0' + fraction / 100);
    *out++ = static_cast<char>('0' + fraction / 10 % 10);
    *out++ = static_cast<char>('0' + fraction % 10);
    return out;
}

// Formats thousandths of a unit as {"<name>":{"x":0.012,"y":-0.003,"z":0.998}}.
std::array<char, 80> make_payload(const char* name, const stm32f407::Vector3& reading)
{
    std::array<char, 80> payload{};
    char* const end = payload.end() - 1;
    char* out = payload.begin();
    const auto append = [&out](const char* text) {
        const std::size_t length = std::strlen(text);
        out = std::copy_n(text, length, out);
    };

    append(R"({")");
    append(name);
    append(R"(":{"x":)");
    out = write_thousandths(out, end, reading.x);
    append(R"(,"y":)");
    out = write_thousandths(out, end, reading.y);
    append(R"(,"z":)");
    out = write_thousandths(out, end, reading.z);
    append("}}");
    return payload;
}

} // namespace

namespace network {

MotionPublisher::MotionPublisher(MqttClient& client, const stm32f407::DeviceId& device_id)
    : client_(client)
{
    quantities_[acceleration].name = "acceleration";
    quantities_[angular_rate].name = "angular_rate";
    quantities_[magnetic_field].name = "magnetic_field";

    for (Quantity& quantity : quantities_) {
        std::size_t length = 0U;
        for (const char* part : {"SBC/", device_id.data(), "/", quantity.name}) {
            const std::size_t part_length = std::strlen(part);
            std::memcpy(quantity.topic.data() + length, part, part_length);
            length += part_length;
        }
    }
}

bool MotionPublisher::start()
{
    changed_ = xSemaphoreCreateBinary();
    if (changed_ == nullptr) {
        return false;
    }

    return xTaskCreate(task_entry,
                       "motion",
                       publisher_stack_depth,
                       this,
                       publisher_task_priority,
                       nullptr) == pdPASS;
}

void MotionPublisher::on_acceleration_changed(const stm32f407::Vector3& reading)
{
    update(quantities_[acceleration], reading);
}

void MotionPublisher::on_angular_rate_changed(const stm32f407::Vector3& reading)
{
    update(quantities_[angular_rate], reading);
}

void MotionPublisher::on_magnetic_field_changed(const stm32f407::Vector3& reading)
{
    update(quantities_[magnetic_field], reading);
}

void MotionPublisher::task_entry(void* context)
{
    static_cast<MotionPublisher*>(context)->run();
}

void MotionPublisher::update(Quantity& quantity, const stm32f407::Vector3& reading)
{
    taskENTER_CRITICAL();
    quantity.latest = reading;
    quantity.fresh = true;
    taskEXIT_CRITICAL();
    (void)xSemaphoreGive(changed_);
}

void MotionPublisher::run()
{
    std::array<stm32f407::Vector3, 3> readings{};
    std::array<bool, 3> pending{};
    for (;;) {
        // Times out only while a reading is still pending, to retry it.
        const bool retrying = std::any_of(pending.begin(), pending.end(), [](bool p) { return p; });
        (void)xSemaphoreTake(changed_, retrying ? retry_period : portMAX_DELAY);

        taskENTER_CRITICAL();
        for (std::size_t i = 0U; i < quantities_.size(); ++i) {
            if (quantities_[i].fresh) {
                readings[i] = quantities_[i].latest;
                pending[i] = true;
                quantities_[i].fresh = false;
            }
        }
        taskEXIT_CRITICAL();

        for (std::size_t i = 0U; i < quantities_.size(); ++i) {
            if (pending[i]) {
                const auto payload = make_payload(quantities_[i].name, readings[i]);
                pending[i] = !client_.publish(quantities_[i].topic.data(), payload.data());
            }
        }
    }
}

} // namespace network
