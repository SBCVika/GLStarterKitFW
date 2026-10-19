#pragma once

#include <cstdint>
#include <optional>

namespace stm32f407 {

class IHumidityListener {
public:
    // Relative humidity in whole percent.
    virtual void on_humidity_changed(std::uint8_t humidity) = 0;

protected:
    ~IHumidityListener() = default;
};

class ITemperatureListener {
public:
    // Temperature in tenths of a degree Celsius.
    virtual void on_temperature_changed(std::int16_t temperature) = 0;

protected:
    ~ITemperatureListener() = default;
};

// Reads the DHT11 climate sensor on PD11 every two seconds from its own task.
// Listeners run in that task and hear about the first valid reading, then about
// every reading at least their reporting threshold away from the last one they heard.
class Dht11 {
public:
    Dht11() = default;

    Dht11(const Dht11&) = delete;
    Dht11& operator=(const Dht11&) = delete;
    Dht11(Dht11&&) = delete;
    Dht11& operator=(Dht11&&) = delete;

    // Listeners must be set before start(). Thresholds are at least 1, in
    // whole percent and tenths of a degree Celsius respectively.
    void set_humidity_listener(IHumidityListener* listener, std::uint8_t threshold);
    void set_temperature_listener(ITemperatureListener* listener, std::uint16_t threshold);
    void start();

    // Latest valid readings, empty until the first one.
    std::optional<std::uint8_t> humidity() const;
    std::optional<std::int16_t> temperature() const;

private:
    static void task_entry(void* context);

    void configure_pin();
    bool read_frame(std::uint8_t (&frame)[5]);
    void report(std::uint8_t humidity, std::int16_t temperature);

    IHumidityListener* humidity_listener_ = nullptr;
    std::uint8_t humidity_threshold_ = 1U;
    std::optional<std::uint8_t> reported_humidity_;

    ITemperatureListener* temperature_listener_ = nullptr;
    std::uint16_t temperature_threshold_ = 1U;
    std::optional<std::int16_t> reported_temperature_;

    std::optional<std::uint8_t> humidity_;
    std::optional<std::int16_t> temperature_;
    bool started_ = false;
};

} // namespace stm32f407
