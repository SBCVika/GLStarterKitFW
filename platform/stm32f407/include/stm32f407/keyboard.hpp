#pragma once

#include <cstdint>

namespace stm32f407 {

enum class Button : std::uint8_t {
    SW1,
    SW2,
    SW3,
    SW4,
    SW5,
};

enum class ButtonEventType : std::uint8_t {
    PRESSED,
    RELEASED,
};

struct ButtonEvent {
    Button button;
    ButtonEventType type;
};

class IButtonListener {
public:
    virtual void on_button_event(const ButtonEvent& event) = 0;

protected:
    ~IButtonListener() = default;
};

class Keyboard {
public:
    Keyboard();

    Keyboard(const Keyboard&) = delete;
    Keyboard& operator=(const Keyboard&) = delete;
    Keyboard(Keyboard&&) = delete;
    Keyboard& operator=(Keyboard&&) = delete;

    void set_listener(IButtonListener* listener);
    void start();

private:
    static void task_entry(void* context);

    void configure_pins();
    void configure_interrupts();
    void capture_initial_state();
    void report_state_changes();
    bool read_button(Button button) const;

    static Keyboard* instance_;

    IButtonListener* listener_ = nullptr;
    void* task_handle_ = nullptr;
    bool button_states_[5]{};
    bool started_ = false;
};

} // namespace stm32f407
