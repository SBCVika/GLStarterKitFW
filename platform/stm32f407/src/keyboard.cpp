#include "stm32f407/keyboard.hpp"

#include <array>
#include <cstddef>

extern "C" {
#include "FreeRTOS.h"
#include "task.h"
}

#include "stm32f4xx_hal.h"

namespace {

constexpr std::array<stm32f407::Button, 5> buttons{
    stm32f407::Button::SW1,
    stm32f407::Button::SW2,
    stm32f407::Button::SW3,
    stm32f407::Button::SW4,
    stm32f407::Button::SW5,
};

constexpr TickType_t debounce_delay = pdMS_TO_TICKS(20U);
constexpr configSTACK_DEPTH_TYPE keyboard_stack_depth = 256U;
constexpr UBaseType_t keyboard_task_priority = tskIDLE_PRIORITY + 1U;

TaskHandle_t keyboard_task_handle;

void notify_keyboard_task_from_interrupt()
{
    if (keyboard_task_handle == nullptr) {
        return;
    }

    BaseType_t higher_priority_task_woken = pdFALSE;
    xTaskNotifyFromISR(keyboard_task_handle,
                       0U,
                       eIncrement,
                       &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

} // namespace

namespace stm32f407 {

Keyboard* Keyboard::instance_ = nullptr;

Keyboard::Keyboard()
{
    configASSERT(instance_ == nullptr);
    instance_ = this;
}

void Keyboard::set_listener(IButtonListener* listener)
{
    taskENTER_CRITICAL();
    listener_ = listener;
    taskEXIT_CRITICAL();
}

void Keyboard::start()
{
    configASSERT(!started_);

    configure_pins();
    capture_initial_state();

    TaskHandle_t task_handle;
    const BaseType_t task_created = xTaskCreate(task_entry,
                                                 "keyboard",
                                                 keyboard_stack_depth,
                                                 this,
                                                 keyboard_task_priority,
                                                 &task_handle);
    configASSERT(task_created == pdPASS);

    task_handle_ = task_handle;
    keyboard_task_handle = task_handle;
    configure_interrupts();
    started_ = true;
}

void Keyboard::task_entry(void* context)
{
    auto* keyboard = static_cast<Keyboard*>(context);

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        do {
            vTaskDelay(debounce_delay);
        } while (ulTaskNotifyTake(pdTRUE, 0U) != 0U);

        keyboard->report_state_changes();
    }
}

void Keyboard::configure_pins()
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef gpio{};
    gpio.Mode = GPIO_MODE_IT_RISING_FALLING;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = GPIO_PIN_15;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_11;
    HAL_GPIO_Init(GPIOC, &gpio);
}

void Keyboard::configure_interrupts()
{
    constexpr std::uint32_t interrupt_priority =
        configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8U - configPRIO_BITS);

    HAL_NVIC_SetPriority(EXTI9_5_IRQn, interrupt_priority, 0U);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, interrupt_priority, 0U);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void Keyboard::capture_initial_state()
{
    for (std::size_t index = 0U; index < buttons.size(); ++index) {
        button_states_[index] = read_button(buttons[index]);
    }
}

void Keyboard::report_state_changes()
{
    for (std::size_t index = 0U; index < buttons.size(); ++index) {
        const bool pressed = read_button(buttons[index]);
        if (pressed == button_states_[index]) {
            continue;
        }

        button_states_[index] = pressed;

        taskENTER_CRITICAL();
        IButtonListener* listener = listener_;
        taskEXIT_CRITICAL();

        if (listener != nullptr) {
            listener->on_button_event(ButtonEvent{
                buttons[index],
                pressed ? ButtonEventType::PRESSED : ButtonEventType::RELEASED,
            });
        }
    }
}

bool Keyboard::read_button(Button button) const
{
    GPIO_TypeDef* port;
    std::uint16_t pin;

    switch (button) {
    case Button::SW1:
        port = GPIOC;
        pin = GPIO_PIN_11;
        break;
    case Button::SW2:
        port = GPIOA;
        pin = GPIO_PIN_15;
        break;
    case Button::SW3:
        port = GPIOC;
        pin = GPIO_PIN_9;
        break;
    case Button::SW4:
        port = GPIOC;
        pin = GPIO_PIN_6;
        break;
    case Button::SW5:
        port = GPIOC;
        pin = GPIO_PIN_8;
        break;
    }

    return HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET;
}

} // namespace stm32f407

extern "C" void HAL_GPIO_EXTI_Callback(std::uint16_t)
{
    notify_keyboard_task_from_interrupt();
}

extern "C" void EXTI9_5_IRQHandler()
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_6);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_8);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_9);
}

extern "C" void EXTI15_10_IRQHandler()
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_11);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_15);
}
