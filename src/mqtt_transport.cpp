#include "mqtt_transport.hpp"

extern "C" {
#include "FreeRTOS_DNS.h"
}

namespace {

constexpr TickType_t connect_timeout = pdMS_TO_TICKS(5000U);
constexpr TickType_t send_timeout = pdMS_TO_TICKS(1000U);
// MQTT packets here are tiny; the 4-MSS default would cost ~11 KB of heap.
constexpr std::uint32_t stream_buffer_size = ipconfigTCP_MSS;

std::int32_t transport_recv(NetworkContext* context, void* buffer, std::size_t length)
{
    if (!context->link_up->load()) {
        return -1;
    }
    // coreMQTT polls; a return of 0 means no data yet.
    return FreeRTOS_recv(context->socket, buffer, length, FREERTOS_MSG_DONTWAIT);
}

std::int32_t transport_send(NetworkContext* context, const void* buffer, std::size_t length)
{
    return FreeRTOS_send(context->socket, buffer, length, 0);
}

} // namespace

namespace network {

TransportInterface_t make_mqtt_transport(NetworkContext& context)
{
    TransportInterface_t transport{};
    transport.recv = transport_recv;
    transport.send = transport_send;
    transport.pNetworkContext = &context;
    return transport;
}

bool mqtt_transport_connect(NetworkContext& context, const char* host, std::uint16_t port)
{
    const std::uint32_t address = FreeRTOS_gethostbyname(host);
    if (address == 0U) {
        return false;
    }

    context.socket = FreeRTOS_socket(
        FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
    if (context.socket == FREERTOS_INVALID_SOCKET) {
        return false;
    }

    // FreeRTOS_connect waits for the receive timeout.
    (void)FreeRTOS_setsockopt(
        context.socket, 0, FREERTOS_SO_RCVTIMEO, &connect_timeout, sizeof(connect_timeout));
    (void)FreeRTOS_setsockopt(
        context.socket, 0, FREERTOS_SO_SNDTIMEO, &send_timeout, sizeof(send_timeout));
    (void)FreeRTOS_setsockopt(
        context.socket, 0, FREERTOS_SO_RCVBUF, &stream_buffer_size, sizeof(stream_buffer_size));
    (void)FreeRTOS_setsockopt(
        context.socket, 0, FREERTOS_SO_SNDBUF, &stream_buffer_size, sizeof(stream_buffer_size));

    freertos_sockaddr broker{};
    broker.sin_family = FREERTOS_AF_INET;
    broker.sin_port = FreeRTOS_htons(port);
    broker.sin_address.ulIP_IPv4 = address;
    if (FreeRTOS_connect(context.socket, &broker, sizeof(broker)) != 0) {
        mqtt_transport_disconnect(context);
        return false;
    }
    return true;
}

void mqtt_transport_disconnect(NetworkContext& context)
{
    if (context.socket == FREERTOS_INVALID_SOCKET) {
        return;
    }
    (void)FreeRTOS_shutdown(context.socket, FREERTOS_SHUT_RDWR);
    FreeRTOS_closesocket(context.socket);
    context.socket = FREERTOS_INVALID_SOCKET;
}

} // namespace network
