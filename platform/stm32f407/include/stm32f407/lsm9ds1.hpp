#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "stm32f407/i2c1.hpp"

namespace stm32f407 {

struct Vector3 {
    std::int32_t x;
    std::int32_t y;
    std::int32_t z;
};

class IAccelerationListener {
public:
    // Acceleration in thousandths of g.
    virtual void on_acceleration_changed(const Vector3& acceleration) = 0;

protected:
    ~IAccelerationListener() = default;
};

class IAngularRateListener {
public:
    // Angular rate in thousandths of a degree per second.
    virtual void on_angular_rate_changed(const Vector3& angular_rate) = 0;

protected:
    ~IAngularRateListener() = default;
};

class IMagneticFieldListener {
public:
    // Magnetic field in thousandths of a gauss.
    virtual void on_magnetic_field_changed(const Vector3& magnetic_field) = 0;

protected:
    ~IMagneticFieldListener() = default;
};

// Reads the LSM9DS1 motion sensor on I2C1 ten times a second from its own task,
// at ±2 g, ±245 dps and ±4 gauss. All readings use the accelerometer's axes.
// Listeners run in that task and hear about the first reading, then about every
// reading with an axis at least their reporting threshold away from the last one they heard.
class Lsm9ds1 {
public:
    explicit Lsm9ds1(I2c1& bus) : bus_(bus) {}

    Lsm9ds1(const Lsm9ds1&) = delete;
    Lsm9ds1& operator=(const Lsm9ds1&) = delete;
    Lsm9ds1(Lsm9ds1&&) = delete;
    Lsm9ds1& operator=(Lsm9ds1&&) = delete;

    // Listeners must be added before start(); each reading takes up to two.
    // Returns false when that reading's listeners are full. Thresholds are at
    // least 1, in the listener's units.
    bool add_acceleration_listener(IAccelerationListener& listener, std::int32_t threshold);
    bool add_angular_rate_listener(IAngularRateListener& listener, std::int32_t threshold);
    bool add_magnetic_field_listener(IMagneticFieldListener& listener, std::int32_t threshold);

    // Returns false when the sensor does not answer or cannot be configured.
    bool start();

    // Latest readings, empty until the first one.
    std::optional<Vector3> acceleration() const;
    std::optional<Vector3> angular_rate() const;
    std::optional<Vector3> magnetic_field() const;

private:
    static constexpr std::size_t max_listeners = 2U;

    template <typename Listener>
    struct Subscription {
        Listener* listener;
        std::int32_t threshold;
        std::optional<Vector3> reported;
    };

    template <typename Listener>
    struct Subscriptions {
        std::array<Subscription<Listener>, max_listeners> entries{};
        std::size_t count = 0U;
    };

    template <typename Listener>
    bool add(Subscriptions<Listener>& subscriptions, Listener& listener, std::int32_t threshold);

    static void task_entry(void* context);
    static std::int32_t read_imu(void* context,
                                 std::uint8_t reg,
                                 std::uint8_t* data,
                                 std::uint16_t length);
    static std::int32_t write_imu(void* context,
                                  std::uint8_t reg,
                                  const std::uint8_t* data,
                                  std::uint16_t length);
    static std::int32_t read_mag(void* context,
                                 std::uint8_t reg,
                                 std::uint8_t* data,
                                 std::uint16_t length);
    static std::int32_t write_mag(void* context,
                                  std::uint8_t reg,
                                  const std::uint8_t* data,
                                  std::uint16_t length);

    bool configure();
    bool read(Vector3& acceleration, Vector3& angular_rate, Vector3& magnetic_field);
    void report(const Vector3& acceleration,
                const Vector3& angular_rate,
                const Vector3& magnetic_field);

    I2c1& bus_;

    Subscriptions<IAccelerationListener> acceleration_subscriptions_;
    Subscriptions<IAngularRateListener> angular_rate_subscriptions_;
    Subscriptions<IMagneticFieldListener> magnetic_field_subscriptions_;

    std::optional<Vector3> acceleration_;
    std::optional<Vector3> angular_rate_;
    std::optional<Vector3> magnetic_field_;
    bool started_ = false;
};

} // namespace stm32f407
