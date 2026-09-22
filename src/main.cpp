extern "C" {
#include "FreeRTOS.h"
#include "task.h"
}

#include "stm32f407/board.hpp"
#include "stm32f407/keyboard.hpp"


namespace {

class KeyboardListener final : public stm32f407::IButtonListener {
public:
    void on_button_event(const stm32f407::ButtonEvent& event) override
    {
        if (event.button == stm32f407::Button::SW1 &&
            event.type == stm32f407::ButtonEventType::PRESSED) {
            stm32f407::toggle_green_led();
        }
    }
};

stm32f407::Keyboard keyboard;
KeyboardListener keyboard_listener;

void led_task(void*)
{
    for (;;) {
        //stm32f407::toggle_green_led();
        vTaskDelay(pdMS_TO_TICKS(1500U));
    }
}

} // namespace

extern "C" {

void vApplicationMallocFailedHook()
{
    stm32f407::halt();
}

void vApplicationStackOverflowHook(TaskHandle_t, char*)
{
    stm32f407::halt();
}

} // extern "C"

int main()
{
    if (!stm32f407::initialize_system_clock()) {
        stm32f407::halt();
    }

    stm32f407::initialize_board();



    if (xTaskCreate(led_task,
                    "blink",
                    configMINIMAL_STACK_SIZE,
                    nullptr,
                    tskIDLE_PRIORITY + 1U,
                    nullptr) != pdPASS) {
        stm32f407::halt();
    }

    keyboard.set_listener(&keyboard_listener);
    keyboard.start();

    vTaskStartScheduler();
    stm32f407::halt();
}
