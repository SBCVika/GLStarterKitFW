#include "stm32f407/dht11.hpp"

#include <cstddef>
#include <cstdlib>

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
}

#include "stm32f4xx_hal.h"

namespace {

constexpr std::uint16_t dht11_pin = GPIO_PIN_11;
constexpr configSTACK_DEPTH_TYPE dht11_stack_depth = 256U;
constexpr UBaseType_t dht11_task_priority = tskIDLE_PRIORITY + 1U;
constexpr TickType_t read_period = pdMS_TO_TICKS(2000U);
constexpr TickType_t start_signal_duration = pdMS_TO_TICKS(20U);
constexpr std::uint32_t level_timeout_us = 100U;
// A 0 bit is high for 26–28 µs, a 1 bit for 70 µs.
constexpr std::uint32_t one_bit_min_high_us = 50U;

bool line_high()
{
    return (GPIOD->IDR & dht11_pin) != 0U;
}

// Waits until the line leaves the given level and reports how long it stayed there.
bool wait_while(bool high, std::uint32_t timeout_cycles, std::uint32_t& held_cycles)
{
    const std::uint32_t start = DWT->CYCCNT;
    while (line_high() == high) {
        if (DWT->CYCCNT - start > timeout_cycles) {
            return false;
        }
    }

    held_cycles = DWT->CYCCNT - start;
    return true;
}

} // namespace

namespace stm32f407 {

bool Dht11::add_humidity_listener(IHumidityListener& listener, std::uint8_t threshold)
{
    configASSERT(!started_);
    configASSERT(threshold >= 1U);

    if (humidity_subscription_count_ == humidity_subscriptions_.size()) {
        return false;
    }

    humidity_subscriptions_[humidity_subscription_count_] = {&listener, threshold, std::nullopt};
    ++humidity_subscription_count_;
    return true;
}

bool Dht11::add_temperature_listener(ITemperatureListener& listener, std::uint16_t threshold)
{
    configASSERT(!started_);
    configASSERT(threshold >= 1U);

    if (temperature_subscription_count_ == temperature_subscriptions_.size()) {
        return false;
    }

    temperature_subscriptions_[temperature_subscription_count_] = {
        &listener, threshold, std::nullopt};
    ++temperature_subscription_count_;
    return true;
}

void Dht11::start()
{
    configASSERT(!started_);

    configure_pin();

    const BaseType_t task_created = xTaskCreate(task_entry,
                                                 "dht11",
                                                 dht11_stack_depth,
                                                 this,
                                                 dht11_task_priority,
                                                 nullptr);
    configASSERT(task_created == pdPASS);

    started_ = true;
}

std::optional<std::uint8_t> Dht11::humidity() const
{
    taskENTER_CRITICAL();
    const std::optional<std::uint8_t> humidity = humidity_;
    taskEXIT_CRITICAL();
    return humidity;
}

std::optional<std::int16_t> Dht11::temperature() const
{
    taskENTER_CRITICAL();
    const std::optional<std::int16_t> temperature = temperature_;
    taskEXIT_CRITICAL();
    return temperature;
}

void Dht11::task_entry(void* context)
{
    auto* dht11 = static_cast<Dht11*>(context);

    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        // The first wait also covers the sensor's one-second settling time after power-up.
        xTaskDelayUntil(&last_wake, read_period);

        std::uint8_t frame[5];
        if (!dht11->read_frame(frame)) {
            continue;
        }

        // Bit 7 of the temperature's decimal byte marks a negative temperature.
        const auto magnitude = static_cast<std::int16_t>(frame[2] * 10 + (frame[3] & 0x7FU));
        const auto temperature =
            static_cast<std::int16_t>((frame[3] & 0x80U) != 0U ? -magnitude : magnitude);
        dht11->report(frame[0], temperature);
    }
}

void Dht11::configure_pin()
{
    __HAL_RCC_GPIOD_CLK_ENABLE();

    // Open drain against the board's external pull-up; the input stays readable.
    HAL_GPIO_WritePin(GPIOD, dht11_pin, GPIO_PIN_SET);

    GPIO_InitTypeDef gpio{};
    gpio.Pin = dht11_pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &gpio);
}

bool Dht11::read_frame(std::uint8_t (&frame)[5])
{
    HAL_GPIO_WritePin(GPIOD, dht11_pin, GPIO_PIN_RESET);
    vTaskDelay(start_signal_duration);

    const std::uint32_t cycles_per_us = SystemCoreClock / 1000000U;
    const std::uint32_t timeout = level_timeout_us * cycles_per_us;
    std::uint32_t held = 0U;

    // The frame lasts about 5 ms and its bits are told apart by tens of microseconds.
    taskENTER_CRITICAL();
    HAL_GPIO_WritePin(GPIOD, dht11_pin, GPIO_PIN_SET);

    // The sensor answers with about 80 µs low, then 80 µs high.
    bool ok = wait_while(true, timeout, held) && wait_while(false, timeout, held) &&
              wait_while(true, timeout, held);

    // Each bit is about 50 µs low, then high for a length that encodes its value.
    for (std::size_t bit = 0U; ok && bit < 40U; ++bit) {
        ok = wait_while(false, timeout, held) && wait_while(true, timeout, held);
        const bool one = held > one_bit_min_high_us * cycles_per_us;
        frame[bit / 8U] = static_cast<std::uint8_t>((frame[bit / 8U] << 1U) | (one ? 1U : 0U));
    }
    taskEXIT_CRITICAL();

    if (!ok) {
        return false;
    }

    const auto checksum = static_cast<std::uint8_t>(frame[0] + frame[1] + frame[2] + frame[3]);
    return checksum == frame[4];
}

void Dht11::report(std::uint8_t humidity, std::int16_t temperature)
{
    taskENTER_CRITICAL();
    humidity_ = humidity;
    temperature_ = temperature;
    taskEXIT_CRITICAL();

    for (std::size_t index = 0U; index < humidity_subscription_count_; ++index) {
        HumiditySubscription& subscription = humidity_subscriptions_[index];
        if (!subscription.reported ||
            std::abs(humidity - subscription.reported.value_or(0U)) >= subscription.threshold) {
            subscription.reported = humidity;
            subscription.listener->on_humidity_changed(humidity);
        }
    }

    for (std::size_t index = 0U; index < temperature_subscription_count_; ++index) {
        TemperatureSubscription& subscription = temperature_subscriptions_[index];
        if (!subscription.reported ||
            std::abs(temperature - subscription.reported.value_or(0)) >=
                subscription.threshold) {
            subscription.reported = temperature;
            subscription.listener->on_temperature_changed(temperature);
        }
    }
}

} // namespace stm32f407
