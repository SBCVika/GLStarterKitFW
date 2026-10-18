#include "ethernet.hpp"

#include <utility>

#include "stm32f407/device_id.hpp"

extern "C" {
#include "FreeRTOS_Routing.h"
#include "task.h"

#include "stm32f4xx_hal.h"

NetworkInterface_t* pxSTM32_FillInterfaceDescriptor(BaseType_t index,
                                                     NetworkInterface_t* interface);
}

namespace {

std::array<std::uint8_t, 6> make_mac_address()
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (const std::uint32_t word : stm32f407::read_unique_id()) {
        for (std::size_t byte_index = 0U; byte_index < sizeof(word); ++byte_index) {
            hash ^= static_cast<std::uint8_t>(word >> (byte_index * 8U));
            hash *= 1099511628211ULL;
        }
    }

    std::array<std::uint8_t, 6> mac{};
    mac[0] = 0x02U;
    for (std::size_t index = 1U; index < mac.size(); ++index) {
        mac[index] = static_cast<std::uint8_t>(hash >> ((index - 1U) * 8U));
    }
    return mac;
}

} // namespace

namespace network {

Ethernet* Ethernet::instance_ = nullptr;

struct EthernetHookAccess {
    static Ethernet* instance()
    {
        return Ethernet::instance_;
    }

    static void set_link_state(Ethernet& ethernet, LinkState state)
    {
        ethernet.set_link_state(state);
    }

    static std::uint32_t next_random(Ethernet& ethernet)
    {
        return ethernet.next_random();
    }
};

Ethernet::Ethernet(EthernetConfig config)
    : config_(std::move(config))
{
    configASSERT(instance_ == nullptr);
    instance_ = this;
}

bool Ethernet::start()
{
    configASSERT(!started_);

    clients_mutex_ = xSemaphoreCreateMutex();
    if (clients_mutex_ == nullptr) {
        return false;
    }

    if (pxSTM32_FillInterfaceDescriptor(0, &interface_) == nullptr) {
        vSemaphoreDelete(clients_mutex_);
        clients_mutex_ = nullptr;
        return false;
    }

    const auto mac = make_mac_address();
    FreeRTOS_FillEndPoint(&interface_,
                          &endpoint_,
                          config_.ip.data(),
                          config_.netmask.data(),
                          config_.gateway.data(),
                          config_.dns.data(),
                          mac.data());

    if (FreeRTOS_IPInit_Multi() != pdPASS) {
        vSemaphoreDelete(clients_mutex_);
        clients_mutex_ = nullptr;
        return false;
    }

    started_ = true;
    return true;
}

bool Ethernet::add_link_state_client(ILinkStateClient& client)
{
    if (started_) {
        if (xSemaphoreTake(clients_mutex_, portMAX_DELAY) != pdTRUE) {
            return false;
        }
    }

    for (std::size_t index = 0U; index < client_count_; ++index) {
        if (clients_[index] == &client) {
            if (started_) {
                xSemaphoreGive(clients_mutex_);
            }
            return false;
        }
    }

    if (client_count_ == clients_.size()) {
        if (started_) {
            xSemaphoreGive(clients_mutex_);
        }
        return false;
    }

    clients_[client_count_] = &client;
    ++client_count_;
    client.on_link_state_changed(link_state_);

    if (started_) {
        xSemaphoreGive(clients_mutex_);
    }
    return true;
}

void Ethernet::set_link_state(LinkState state)
{
    configASSERT(started_);
    configASSERT(clients_mutex_ != nullptr);

    if (xSemaphoreTake(clients_mutex_, portMAX_DELAY) != pdTRUE) {
        return;
    }

    if (state != link_state_) {
        link_state_ = state;
        for (std::size_t index = 0U; index < client_count_; ++index) {
            clients_[index]->on_link_state_changed(state);
        }
    }

    xSemaphoreGive(clients_mutex_);
}

std::uint32_t Ethernet::next_random()
{
    taskENTER_CRITICAL();
    if (random_state_ == 0U) {
        const auto uid = stm32f407::read_unique_id();
        random_state_ = uid[0] ^ uid[1] ^ uid[2] ^
                        static_cast<std::uint32_t>(xTaskGetTickCount());
        if (random_state_ == 0U) {
            random_state_ = 0x6D2B79F5U;
        }
    }

    random_state_ ^= random_state_ << 13U;
    random_state_ ^= random_state_ >> 17U;
    random_state_ ^= random_state_ << 5U;
    const std::uint32_t result = random_state_;
    taskEXIT_CRITICAL();
    return result;
}

} // namespace network

extern "C" {

void vApplicationIPNetworkEventHook_Multi(eIPCallbackEvent_t event,
                                           NetworkEndPoint_t*)
{
    network::Ethernet* ethernet = network::EthernetHookAccess::instance();
    configASSERT(ethernet != nullptr);

    if (event == eNetworkUp) {
        /* The upstream STM32 interface enables the F4 broadcast filter (BFD).
         * ARP requests are broadcast and must reach the IP task. */
        CLEAR_BIT(ETH->MACFFR, ETH_MACFFR_BFD);
    }

    network::EthernetHookAccess::set_link_state(
        *ethernet,
        event == eNetworkUp ? network::LinkState::UP : network::LinkState::DOWN);
}

BaseType_t xApplicationGetRandomNumber(std::uint32_t* value)
{
    if (value == nullptr) {
        return pdFALSE;
    }

    network::Ethernet* ethernet = network::EthernetHookAccess::instance();
    configASSERT(ethernet != nullptr);
    *value = network::EthernetHookAccess::next_random(*ethernet);
    return pdTRUE;
}

std::uint32_t ulApplicationGetNextSequenceNumber(std::uint32_t source_address,
                                                  std::uint16_t source_port,
                                                  std::uint32_t destination_address,
                                                  std::uint16_t destination_port)
{
    network::Ethernet* ethernet = network::EthernetHookAccess::instance();
    configASSERT(ethernet != nullptr);
    return network::EthernetHookAccess::next_random(*ethernet) ^ source_address ^
           destination_address ^ (static_cast<std::uint32_t>(source_port) << 16U) ^
           destination_port;
}

} // extern "C"
