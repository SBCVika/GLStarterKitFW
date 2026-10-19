#include "stm32f407/lsm9ds1.hpp"

#include <cstdlib>

extern "C" {
#include "FreeRTOS.h"
#include "task.h"

#include "lsm9ds1_reg.h"
}

namespace {

// 7-bit addresses with SDO_A/G and SDO_M pulled high on the board.
constexpr std::uint8_t imu_address = 0x6BU;
constexpr std::uint8_t mag_address = 0x1EU;
// The magnetometer only advances through registers when the address has bit 7 set.
constexpr std::uint8_t mag_auto_increment = 0x80U;

constexpr configSTACK_DEPTH_TYPE lsm9ds1_stack_depth = 256U;
constexpr UBaseType_t lsm9ds1_task_priority = tskIDLE_PRIORITY + 1U;
constexpr TickType_t read_period = pdMS_TO_TICKS(100U);

// Datasheet sensitivities: 0.061 mg, 8.75 mdps and 0.14 mgauss per LSB.
constexpr std::int32_t to_milli_g(std::int16_t lsb)
{
    return lsb * 61 / 1000;
}

constexpr std::int32_t to_milli_dps(std::int16_t lsb)
{
    return lsb * 875 / 100;
}

constexpr std::int32_t to_milli_gauss(std::int16_t lsb)
{
    return lsb * 14 / 100;
}

bool crossed(const std::optional<stm32f407::Vector3>& reported,
             const stm32f407::Vector3& reading,
             std::int32_t threshold)
{
    if (!reported) {
        return true;
    }

    return std::abs(reading.x - reported->x) >= threshold ||
           std::abs(reading.y - reported->y) >= threshold ||
           std::abs(reading.z - reported->z) >= threshold;
}

} // namespace

namespace stm32f407 {

template <typename Listener>
bool Lsm9ds1::add(Subscriptions<Listener>& subscriptions,
                  Listener& listener,
                  std::int32_t threshold)
{
    configASSERT(!started_);
    configASSERT(threshold >= 1);

    if (subscriptions.count == subscriptions.entries.size()) {
        return false;
    }

    subscriptions.entries[subscriptions.count] = {&listener, threshold, std::nullopt};
    ++subscriptions.count;
    return true;
}

bool Lsm9ds1::add_acceleration_listener(IAccelerationListener& listener, std::int32_t threshold)
{
    return add(acceleration_subscriptions_, listener, threshold);
}

bool Lsm9ds1::add_angular_rate_listener(IAngularRateListener& listener, std::int32_t threshold)
{
    return add(angular_rate_subscriptions_, listener, threshold);
}

bool Lsm9ds1::add_magnetic_field_listener(IMagneticFieldListener& listener,
                                          std::int32_t threshold)
{
    return add(magnetic_field_subscriptions_, listener, threshold);
}

bool Lsm9ds1::start()
{
    configASSERT(!started_);

    if (!configure()) {
        return false;
    }

    const BaseType_t task_created = xTaskCreate(task_entry,
                                                 "lsm9ds1",
                                                 lsm9ds1_stack_depth,
                                                 this,
                                                 lsm9ds1_task_priority,
                                                 nullptr);
    configASSERT(task_created == pdPASS);

    started_ = true;
    return true;
}

std::optional<Vector3> Lsm9ds1::acceleration() const
{
    taskENTER_CRITICAL();
    const std::optional<Vector3> acceleration = acceleration_;
    taskEXIT_CRITICAL();
    return acceleration;
}

std::optional<Vector3> Lsm9ds1::angular_rate() const
{
    taskENTER_CRITICAL();
    const std::optional<Vector3> angular_rate = angular_rate_;
    taskEXIT_CRITICAL();
    return angular_rate;
}

std::optional<Vector3> Lsm9ds1::magnetic_field() const
{
    taskENTER_CRITICAL();
    const std::optional<Vector3> magnetic_field = magnetic_field_;
    taskEXIT_CRITICAL();
    return magnetic_field;
}

void Lsm9ds1::task_entry(void* context)
{
    auto* lsm9ds1 = static_cast<Lsm9ds1*>(context);

    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        xTaskDelayUntil(&last_wake, read_period);

        Vector3 acceleration{};
        Vector3 angular_rate{};
        Vector3 magnetic_field{};
        if (lsm9ds1->read(acceleration, angular_rate, magnetic_field)) {
            lsm9ds1->report(acceleration, angular_rate, magnetic_field);
        }
    }
}

std::int32_t Lsm9ds1::read_imu(void* context,
                               std::uint8_t reg,
                               std::uint8_t* data,
                               std::uint16_t length)
{
    return static_cast<Lsm9ds1*>(context)->bus_.read(imu_address, reg, data, length) ? 0 : -1;
}

std::int32_t Lsm9ds1::write_imu(void* context,
                                std::uint8_t reg,
                                const std::uint8_t* data,
                                std::uint16_t length)
{
    return static_cast<Lsm9ds1*>(context)->bus_.write(imu_address, reg, data, length) ? 0 : -1;
}

std::int32_t Lsm9ds1::read_mag(void* context,
                               std::uint8_t reg,
                               std::uint8_t* data,
                               std::uint16_t length)
{
    const auto address = static_cast<std::uint8_t>(reg | mag_auto_increment);
    return static_cast<Lsm9ds1*>(context)->bus_.read(mag_address, address, data, length) ? 0 : -1;
}

std::int32_t Lsm9ds1::write_mag(void* context,
                                std::uint8_t reg,
                                const std::uint8_t* data,
                                std::uint16_t length)
{
    const auto address = static_cast<std::uint8_t>(reg | mag_auto_increment);
    return static_cast<Lsm9ds1*>(context)->bus_.write(mag_address, address, data, length) ? 0
                                                                                           : -1;
}

bool Lsm9ds1::configure()
{
    stmdev_ctx_t imu{write_imu, read_imu, nullptr, this, nullptr};
    stmdev_ctx_t mag{write_mag, read_mag, nullptr, this, nullptr};

    lsm9ds1_id_t id{};
    if (lsm9ds1_dev_id_get(&mag, &imu, &id) != 0 || id.imu != LSM9DS1_IMU_ID ||
        id.mag != LSM9DS1_MAG_ID) {
        return false;
    }

    // Block data update keeps each axis's low and high bytes from different samples.
    return lsm9ds1_block_data_update_set(&mag, &imu, PROPERTY_ENABLE) == 0 &&
           lsm9ds1_xl_full_scale_set(&imu, LSM9DS1_2g) == 0 &&
           lsm9ds1_gy_full_scale_set(&imu, LSM9DS1_245dps) == 0 &&
           lsm9ds1_mag_full_scale_set(&mag, LSM9DS1_4Ga) == 0 &&
           lsm9ds1_imu_data_rate_set(&imu, LSM9DS1_IMU_59Hz5) == 0 &&
           lsm9ds1_mag_data_rate_set(&mag, LSM9DS1_MAG_UHP_10Hz) == 0;
}

bool Lsm9ds1::read(Vector3& acceleration, Vector3& angular_rate, Vector3& magnetic_field)
{
    stmdev_ctx_t imu{write_imu, read_imu, nullptr, this, nullptr};
    stmdev_ctx_t mag{write_mag, read_mag, nullptr, this, nullptr};

    std::int16_t raw_acceleration[3];
    std::int16_t raw_angular_rate[3];
    std::int16_t raw_magnetic_field[3];
    if (lsm9ds1_acceleration_raw_get(&imu, raw_acceleration) != 0 ||
        lsm9ds1_angular_rate_raw_get(&imu, raw_angular_rate) != 0 ||
        lsm9ds1_magnetic_raw_get(&mag, raw_magnetic_field) != 0) {
        return false;
    }

    acceleration = {to_milli_g(raw_acceleration[0]),
                    to_milli_g(raw_acceleration[1]),
                    to_milli_g(raw_acceleration[2])};
    angular_rate = {to_milli_dps(raw_angular_rate[0]),
                    to_milli_dps(raw_angular_rate[1]),
                    to_milli_dps(raw_angular_rate[2])};
    // Datasheet figure 1: the magnetometer's X and Y point along the
    // accelerometer's -Y and -X; both Z axes point up.
    magnetic_field = {-to_milli_gauss(raw_magnetic_field[1]),
                      -to_milli_gauss(raw_magnetic_field[0]),
                      to_milli_gauss(raw_magnetic_field[2])};
    return true;
}

void Lsm9ds1::report(const Vector3& acceleration,
                     const Vector3& angular_rate,
                     const Vector3& magnetic_field)
{
    taskENTER_CRITICAL();
    acceleration_ = acceleration;
    angular_rate_ = angular_rate;
    magnetic_field_ = magnetic_field;
    taskEXIT_CRITICAL();

    for (std::size_t index = 0U; index < acceleration_subscriptions_.count; ++index) {
        auto& subscription = acceleration_subscriptions_.entries[index];
        if (crossed(subscription.reported, acceleration, subscription.threshold)) {
            subscription.reported = acceleration;
            subscription.listener->on_acceleration_changed(acceleration);
        }
    }

    for (std::size_t index = 0U; index < angular_rate_subscriptions_.count; ++index) {
        auto& subscription = angular_rate_subscriptions_.entries[index];
        if (crossed(subscription.reported, angular_rate, subscription.threshold)) {
            subscription.reported = angular_rate;
            subscription.listener->on_angular_rate_changed(angular_rate);
        }
    }

    for (std::size_t index = 0U; index < magnetic_field_subscriptions_.count; ++index) {
        auto& subscription = magnetic_field_subscriptions_.entries[index];
        if (crossed(subscription.reported, magnetic_field, subscription.threshold)) {
            subscription.reported = magnetic_field;
            subscription.listener->on_magnetic_field_changed(magnetic_field);
        }
    }
}

} // namespace stm32f407
