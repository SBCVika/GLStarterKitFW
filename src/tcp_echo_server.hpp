#pragma once

#include "ethernet.hpp"

extern "C" {
#include "FreeRTOS_Sockets.h"
#include "event_groups.h"
}

namespace network {

class TcpEchoServer final : public ILinkStateClient {
public:
    TcpEchoServer() = default;

    TcpEchoServer(const TcpEchoServer&) = delete;
    TcpEchoServer& operator=(const TcpEchoServer&) = delete;
    TcpEchoServer(TcpEchoServer&&) = delete;
    TcpEchoServer& operator=(TcpEchoServer&&) = delete;

    bool start();
    void on_link_state_changed(LinkState state) override;

private:
    static void task_entry(void* context);

    void run();
    bool link_is_up() const;
    Socket_t create_server_socket() const;
    void serve_client(Socket_t client) const;

    EventGroupHandle_t link_state_events_ = nullptr;
    bool started_ = false;
};

} // namespace network
