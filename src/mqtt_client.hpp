#pragma once

#include <array>
#include <atomic>
#include <cstddef>
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

class IBrokerConnectionListener {
public:
    virtual void on_broker_connection_changed(bool connected) = 0;

protected:
    ~IBrokerConnectionListener() = default;
};

// Owns the broker connection and runs the coreMQTT agent task that serves it.
class MqttClient final : public ILinkStateClient {
public:
    MqttClient(MqttBroker broker, const char* client_id);

    MqttClient(const MqttClient&) = delete;
    MqttClient& operator=(const MqttClient&) = delete;
    MqttClient(MqttClient&&) = delete;
    MqttClient& operator=(MqttClient&&) = delete;

    // Listeners must be added before start() and run in the agent task.
    // Returns false when two are already added.
    bool add_broker_connection_listener(IBrokerConnectionListener& listener);
    bool start();
    void on_link_state_changed(LinkState state) override;

    // Sends a QoS 0 message and blocks until the agent has handled it.
    // Fails without blocking while disconnected from the broker.
    bool publish(const char* topic, const char* payload);

private:
    static constexpr std::size_t max_listeners = 2U;

    static void task_entry(void* context);

    void run();
    bool connect_to_broker();
    void set_connected(bool connected);

    MqttBroker broker_;
    const char* client_id_;
    NetworkContext network_{};
    MQTTAgentContext_t agent_{};
    std::array<std::uint8_t, 256> network_buffer_{};
    EventGroupHandle_t link_events_ = nullptr;
    std::atomic<bool> link_up_{false};
    std::atomic<bool> connected_{false};
    std::array<IBrokerConnectionListener*, max_listeners> listeners_{};
    std::size_t listener_count_ = 0U;
    bool started_ = false;
};

} // namespace network
