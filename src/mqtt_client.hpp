#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include "ethernet.hpp"
#include "mqtt_transport.hpp"

extern "C" {
#include "core_mqtt_agent.h"
#include "event_groups.h"
}

namespace network {

struct MqttBroker {
    const char* host;
    std::uint16_t port;
};

// Owns the broker connection and runs the coreMQTT agent task that serves it.
class MqttClient final : public ILinkStateClient {
public:
    MqttClient(MqttBroker broker, const char* client_id);

    MqttClient(const MqttClient&) = delete;
    MqttClient& operator=(const MqttClient&) = delete;
    MqttClient(MqttClient&&) = delete;
    MqttClient& operator=(MqttClient&&) = delete;

    bool start();
    void on_link_state_changed(LinkState state) override;

    // Sends a QoS 0 message and blocks until the agent has handled it.
    // Fails without blocking while disconnected from the broker.
    bool publish(const char* topic, const char* payload);

private:
    static void task_entry(void* context);

    void run();
    bool connect_to_broker();

    MqttBroker broker_;
    const char* client_id_;
    NetworkContext network_{};
    MQTTAgentContext_t agent_{};
    std::array<std::uint8_t, 256> network_buffer_{};
    EventGroupHandle_t link_events_ = nullptr;
    std::atomic<bool> link_up_{false};
    std::atomic<bool> connected_{false};
    bool started_ = false;
};

} // namespace network
