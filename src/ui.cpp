#include "ui.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <initializer_list>
#include <string_view>

namespace {

constexpr configSTACK_DEPTH_TYPE ui_stack_depth = 512U;
constexpr UBaseType_t ui_task_priority = tskIDLE_PRIORITY + 1U;

using Line = std::array<char, stm32f407::Wh1602::columns>;

Line blank_line()
{
    Line line;
    line.fill(' ');
    return line;
}

Line text_line(std::string_view text)
{
    Line line = blank_line();
    std::copy_n(text.begin(), std::min(text.size(), line.size()), line.begin());
    return line;
}

// Writes a sign and the digits, returning the end of what it wrote. Values that
// round to zero are "+", never "-".
char* put_signed(char* begin, char* end, bool negative, std::uint32_t digits)
{
    *begin++ = negative ? '-' : '+';
    return std::to_chars(begin, end, digits).ptr;
}

// Tenths as "-5.0" or "23.0".
Line tenths_line(std::int16_t tenths)
{
    Line line = blank_line();
    char* position = line.data();
    if (tenths < 0) {
        *position++ = '-';
    }
    const auto magnitude = static_cast<std::uint32_t>(std::abs(tenths));
    position = std::to_chars(position, line.end(), magnitude / 10U).ptr;
    *position++ = '.';
    std::to_chars(position, line.end(), magnitude % 10U);
    return line;
}

Line whole_line(std::uint8_t value)
{
    Line line = blank_line();
    std::to_chars(line.data(), line.end(), value);
    return line;
}

// Thousandths as three hundredths with no separator: "+0.01-0.02+0.98".
Line hundredths_line(const stm32f407::Vector3& thousandths)
{
    Line line = blank_line();
    char* position = line.data();
    for (const std::int32_t axis : {thousandths.x, thousandths.y, thousandths.z}) {
        const std::uint32_t hundredths = (static_cast<std::uint32_t>(std::abs(axis)) + 5U) / 10U;
        const bool negative = axis < 0 && hundredths != 0U;
        position = put_signed(position, line.end(), negative, hundredths / 100U);
        *position++ = '.';
        *position++ = static_cast<char>('0' + hundredths / 10U % 10U);
        *position++ = static_cast<char>('0' + hundredths % 10U);
    }
    return line;
}

// Thousandths as three whole numbers, right-aligned in five columns: " +245  -12   +0".
Line whole_fields_line(const stm32f407::Vector3& thousandths)
{
    constexpr std::size_t field_width = 5U;

    Line line = blank_line();
    std::size_t field_end = field_width;
    for (const std::int32_t axis : {thousandths.x, thousandths.y, thousandths.z}) {
        const std::uint32_t whole = (static_cast<std::uint32_t>(std::abs(axis)) + 500U) / 1000U;
        std::array<char, field_width> field{};
        char* end = put_signed(field.data(), field.end(), axis < 0 && whole != 0U, whole);
        const auto length = static_cast<std::size_t>(end - field.data());
        std::copy(field.data(), end, line.data() + field_end - length);
        field_end += field_width;
    }
    return line;
}

template <typename Value, typename Format>
Line value_line(const std::optional<Value>& value, Format format)
{
    return value ? format(*value) : text_line("--");
}

} // namespace

namespace ui {

Ui::Ui(stm32f407::Wh1602& display, const std::array<std::uint8_t, 4>& ip)
    : display_(display)
    , ip_line_(blank_line())
{
    char* position = ip_line_.data();
    for (std::size_t index = 0U; index < ip.size(); ++index) {
        if (index != 0U) {
            *position++ = '.';
        }
        position = std::to_chars(position, ip_line_.end(), ip[index]).ptr;
    }
}

bool Ui::start()
{
    configASSERT(task_ == nullptr);

    return xTaskCreate(task_entry, "ui", ui_stack_depth, this, ui_task_priority, &task_) ==
           pdPASS;
}

void Ui::on_humidity_changed(std::uint8_t humidity)
{
    change([humidity](State& state) { state.humidity = humidity; });
}

void Ui::on_temperature_changed(std::int16_t temperature)
{
    change([temperature](State& state) { state.temperature = temperature; });
}

void Ui::on_acceleration_changed(const stm32f407::Vector3& acceleration)
{
    change([&acceleration](State& state) { state.acceleration = acceleration; });
}

void Ui::on_angular_rate_changed(const stm32f407::Vector3& angular_rate)
{
    change([&angular_rate](State& state) { state.angular_rate = angular_rate; });
}

void Ui::on_magnetic_field_changed(const stm32f407::Vector3& magnetic_field)
{
    change([&magnetic_field](State& state) { state.magnetic_field = magnetic_field; });
}

void Ui::on_button_event(const stm32f407::ButtonEvent& event)
{
    if (event.type != stm32f407::ButtonEventType::PRESSED) {
        return;
    }

    std::size_t step;
    if (event.button == stm32f407::Button::SW1) {
        step = 1U;
    } else if (event.button == stm32f407::Button::SW3) {
        step = screen_count - 1U;
    } else {
        return;
    }

    change([step](State& state) {
        const std::size_t screen = static_cast<std::size_t>(state.screen);
        state.screen = static_cast<Screen>((screen + step) % screen_count);
    });
}

void Ui::on_link_state_changed(network::LinkState state)
{
    const bool link_up = state == network::LinkState::UP;
    change([link_up](State& state) { state.link_up = link_up; });
}

void Ui::on_broker_connection_changed(bool connected)
{
    change([connected](State& state) { state.broker_connected = connected; });
}

template <typename Update>
void Ui::change(Update update)
{
    configASSERT(task_ != nullptr);

    taskENTER_CRITICAL();
    update(state_);
    taskEXIT_CRITICAL();

    xTaskNotifyGive(task_);
}

void Ui::task_entry(void* context)
{
    static_cast<Ui*>(context)->run();
}

void Ui::run()
{
    display_.initialize();
    display_.set_backlight(true);

    for (;;) {
        taskENTER_CRITICAL();
        const State state = state_;
        taskEXIT_CRITICAL();

        draw(state);
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

void Ui::draw(const State& state)
{
    Line title = blank_line();
    Line value = blank_line();

    switch (state.screen) {
    case Screen::NETWORK:
    {
        // "ETH off MQTT off" at its widest.
        constexpr std::size_t mqtt_column = 8U;
        const std::string_view mqtt = state.broker_connected ? "MQTT on" : "MQTT off";
        title = ip_line_;
        value = text_line(state.link_up ? "ETH on" : "ETH off");
        std::copy(mqtt.begin(), mqtt.end(), value.begin() + mqtt_column);
        break;
    }
    case Screen::TEMPERATURE:
        title = text_line("Temperature");
        title[14] = stm32f407::Wh1602::degree;
        title[15] = 'C';
        value = value_line(state.temperature, tenths_line);
        break;
    case Screen::HUMIDITY:
        title = text_line("Humidity       %");
        value = value_line(state.humidity, whole_line);
        break;
    case Screen::ACCELERATION:
        title = text_line("Acceleration   g");
        value = value_line(state.acceleration, hundredths_line);
        break;
    case Screen::ANGULAR_RATE:
        title = text_line("Angular rate dps");
        value = value_line(state.angular_rate, whole_fields_line);
        break;
    case Screen::MAGNETIC_FIELD:
        title = text_line("Magnetic field G");
        value = value_line(state.magnetic_field, hundredths_line);
        break;
    }

    display_.write(0U, 0U, {title.data(), title.size()});
    display_.write(1U, 0U, {value.data(), value.size()});
}

} // namespace ui
