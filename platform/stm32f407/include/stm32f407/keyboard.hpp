#pragma once

#include <array>
#include <cstddef>
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

    // Listeners must be added before start(). Returns false when two are already added.
    bool add_listener(IButtonListener& listener);
    void start();

private:
    static void task_entry(void* context);

    void configure_pins();
    void configure_interrupts();
    void capture_initial_state();
    void report_state_changes();
    bool read_button(Button button) const;

    static constexpr std::size_t max_listeners = 2U;

    static Keyboard* instance_;

    std::array<IButtonListener*, max_listeners> listeners_{};
    std::size_t listener_count_ = 0U;
    void* task_handle_ = nullptr;
    bool button_states_[5]{};
    bool started_ = false;
};

} // namespace stm32f407
