#pragma once

#include <atomic>
#include <cstdint>

extern "C" {
#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "transport_interface.h"
}

// coreMQTT leaves the transport context to the application.
struct NetworkContext {
    Socket_t socket = FREERTOS_INVALID_SOCKET;
    // Receives fail while the link is down, so coreMQTT drops the connection.
    const std::atomic<bool>* link_up = nullptr;
};

namespace network {

TransportInterface_t make_mqtt_transport(NetworkContext& context);

// Resolves host and opens a TCP connection to it.
bool mqtt_transport_connect(NetworkContext& context, const char* host, std::uint16_t port);
void mqtt_transport_disconnect(NetworkContext& context);

} // namespace network
