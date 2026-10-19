#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "ethernet.hpp"
#include "mqtt_client.hpp"
#include "stm32f407/dht11.hpp"
#include "stm32f407/keyboard.hpp"
#include "stm32f407/lsm9ds1.hpp"
#include "stm32f407/wh1602.hpp"

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
}

namespace ui {

// Shows one Screen at a time on the Display, starting on the Network Screen;
// SW1 moves forward and SW3 back, wrapping at both ends. Listeners only record
// what they hear; the UI's own task does all drawing.
class Ui final : public stm32f407::IHumidityListener,
                 public stm32f407::ITemperatureListener,
                 public stm32f407::IAccelerationListener,
                 public stm32f407::IAngularRateListener,
                 public stm32f407::IMagneticFieldListener,
                 public stm32f407::IButtonListener,
                 public network::ILinkStateClient,
                 public network::IBrokerConnectionListener {
public:
    Ui(stm32f407::Wh1602& display, const std::array<std::uint8_t, 4>& ip);

    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;
    Ui(Ui&&) = delete;
    Ui& operator=(Ui&&) = delete;

    // Must run before any of the UI's sources start.
    bool start();

    void on_humidity_changed(std::uint8_t humidity) override;
    void on_temperature_changed(std::int16_t temperature) override;
    void on_acceleration_changed(const stm32f407::Vector3& acceleration) override;
    void on_angular_rate_changed(const stm32f407::Vector3& angular_rate) override;
    void on_magnetic_field_changed(const stm32f407::Vector3& magnetic_field) override;
    void on_button_event(const stm32f407::ButtonEvent& event) override;
    void on_link_state_changed(network::LinkState state) override;
    void on_broker_connection_changed(bool connected) override;

private:
    enum class Screen : std::uint8_t {
        NETWORK,
        TEMPERATURE,
        HUMIDITY,
        ACCELERATION,
        ANGULAR_RATE,
        MAGNETIC_FIELD,
    };

    static constexpr std::size_t screen_count = 6U;

    struct State {
        Screen screen = Screen::NETWORK;
        bool link_up = false;
        bool broker_connected = false;
        std::optional<std::int16_t> temperature;
        std::optional<std::uint8_t> humidity;
        std::optional<stm32f407::Vector3> acceleration;
        std::optional<stm32f407::Vector3> angular_rate;
        std::optional<stm32f407::Vector3> magnetic_field;
    };

    using Line = std::array<char, stm32f407::Wh1602::columns>;

    static void task_entry(void* context);

    void run();
    void draw(const State& state);
    template <typename Update>
    void change(Update update);

    stm32f407::Wh1602& display_;
    Line ip_line_{};
    TaskHandle_t task_ = nullptr;
    State state_;
};

} // namespace ui
