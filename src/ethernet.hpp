#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

extern "C" {
#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"
#include "semphr.h"
}

namespace network {

enum class LinkState : std::uint8_t {
    DOWN,
    UP,
};

class ILinkStateClient {
public:
    virtual void on_link_state_changed(LinkState state) = 0;

protected:
    ~ILinkStateClient() = default;
};

struct EthernetConfig {
    std::array<std::uint8_t, 4> ip;
    std::array<std::uint8_t, 4> netmask;
    std::array<std::uint8_t, 4> gateway;
    std::array<std::uint8_t, 4> dns;
};

struct EthernetHookAccess;

class Ethernet final {
public:
    explicit Ethernet(EthernetConfig config);

    Ethernet(const Ethernet&) = delete;
    Ethernet& operator=(const Ethernet&) = delete;
    Ethernet(Ethernet&&) = delete;
    Ethernet& operator=(Ethernet&&) = delete;

    bool start();
    bool add_link_state_client(ILinkStateClient& client);

private:
    friend struct EthernetHookAccess;

    static constexpr std::size_t max_link_state_clients = 4U;

    void set_link_state(LinkState state);
    std::uint32_t next_random();

    static Ethernet* instance_;

    EthernetConfig config_;
    NetworkInterface_t interface_{};
    NetworkEndPoint_t endpoint_{};
    std::array<ILinkStateClient*, max_link_state_clients> clients_{};
    std::size_t client_count_ = 0U;
    SemaphoreHandle_t clients_mutex_ = nullptr;
    LinkState link_state_ = LinkState::DOWN;
    std::uint32_t random_state_ = 0U;
    bool started_ = false;
};

} // namespace network
