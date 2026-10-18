#include "tcp_echo_server.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

extern "C" {
#include "task.h"
}

namespace {

constexpr std::uint16_t echo_port = 5000U;
constexpr configSTACK_DEPTH_TYPE echo_stack_depth = 512U;
constexpr UBaseType_t echo_task_priority = tskIDLE_PRIORITY + 1U;
constexpr TickType_t socket_timeout = pdMS_TO_TICKS(1000U);
constexpr EventBits_t link_up_bit = 1U << 0U;

bool send_all(Socket_t socket, const std::uint8_t* data, BaseType_t length)
{
    BaseType_t sent = 0;
    while (sent < length) {
        const BaseType_t result = FreeRTOS_send(
            socket,
            data + sent,
            static_cast<std::size_t>(length - sent),
            0);
        if (result <= 0) {
            return false;
        }
        sent += result;
    }
    return true;
}

} // namespace

namespace network {

bool TcpEchoServer::start()
{
    configASSERT(!started_);

    link_state_events_ = xEventGroupCreate();
    if (link_state_events_ == nullptr) {
        return false;
    }

    if (xTaskCreate(task_entry,
                    "echo",
                    echo_stack_depth,
                    this,
                    echo_task_priority,
                    nullptr) != pdPASS) {
        vEventGroupDelete(link_state_events_);
        link_state_events_ = nullptr;
        return false;
    }

    started_ = true;
    return true;
}

void TcpEchoServer::on_link_state_changed(LinkState state)
{
    configASSERT(started_);
    configASSERT(link_state_events_ != nullptr);

    if (state == LinkState::UP) {
        xEventGroupSetBits(link_state_events_, link_up_bit);
    } else {
        xEventGroupClearBits(link_state_events_, link_up_bit);
    }
}

void TcpEchoServer::task_entry(void* context)
{
    static_cast<TcpEchoServer*>(context)->run();
}

void TcpEchoServer::run()
{
    for (;;) {
        xEventGroupWaitBits(
            link_state_events_, link_up_bit, pdFALSE, pdTRUE, portMAX_DELAY);

        const Socket_t server = create_server_socket();
        if (server == FREERTOS_INVALID_SOCKET) {
            vTaskDelay(pdMS_TO_TICKS(1000U));
            continue;
        }

        while (link_is_up()) {
            freertos_sockaddr client_address{};
            socklen_t client_address_length = sizeof(client_address);
            const Socket_t client = FreeRTOS_accept(
                server, &client_address, &client_address_length);
            if (client == FREERTOS_INVALID_SOCKET) {
                continue;
            }

            serve_client(client);
            FreeRTOS_closesocket(client);
        }

        FreeRTOS_closesocket(server);
    }
}

bool TcpEchoServer::link_is_up() const
{
    return (xEventGroupGetBits(link_state_events_) & link_up_bit) != 0U;
}

Socket_t TcpEchoServer::create_server_socket() const
{
    const Socket_t server = FreeRTOS_socket(
        FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
    if (server == FREERTOS_INVALID_SOCKET) {
        return FREERTOS_INVALID_SOCKET;
    }

    (void)FreeRTOS_setsockopt(
        server, 0, FREERTOS_SO_RCVTIMEO, &socket_timeout, sizeof(socket_timeout));

    freertos_sockaddr address{};
    address.sin_port = FreeRTOS_htons(echo_port);
    if (FreeRTOS_bind(server, &address, sizeof(address)) != 0 ||
        FreeRTOS_listen(server, 1) != 0) {
        FreeRTOS_closesocket(server);
        return FREERTOS_INVALID_SOCKET;
    }

    return server;
}

void TcpEchoServer::serve_client(Socket_t client) const
{
    std::array<std::uint8_t, 512> buffer{};
    (void)FreeRTOS_setsockopt(
        client, 0, FREERTOS_SO_RCVTIMEO, &socket_timeout, sizeof(socket_timeout));
    (void)FreeRTOS_setsockopt(
        client, 0, FREERTOS_SO_SNDTIMEO, &socket_timeout, sizeof(socket_timeout));

    while (link_is_up()) {
        const BaseType_t received = FreeRTOS_recv(client, buffer.data(), buffer.size(), 0);
        if (received > 0) {
            if (!send_all(client, buffer.data(), received)) {
                break;
            }
        } else if (received != -pdFREERTOS_ERRNO_EWOULDBLOCK) {
            break;
        }
    }
}

} // namespace network
