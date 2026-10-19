#include "mqtt_client.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>

extern "C" {
#include "queue.h"
#include "task.h"
}

// coreMQTT-Agent leaves these two contexts to the application.
struct MQTTAgentMessageContext {
    QueueHandle_t queue;
};

struct MQTTAgentCommandContext {
    TaskHandle_t task;
    MQTTStatus_t status;
};

namespace {

constexpr configSTACK_DEPTH_TYPE agent_stack_depth = 768U;
constexpr UBaseType_t agent_task_priority = ipconfigIP_TASK_PRIORITY - 1U;
constexpr std::size_t command_pool_size = 4U;
constexpr std::uint16_t keep_alive_seconds = 60U;
constexpr std::uint32_t connack_timeout_ms = 5000U;
constexpr TickType_t min_retry_delay = pdMS_TO_TICKS(1000U);
constexpr TickType_t max_retry_delay = pdMS_TO_TICKS(30000U);
constexpr EventBits_t link_up_bit = 1U << 0U;
constexpr EventBits_t link_came_up_bit = 1U << 1U;

std::array<MQTTAgentCommand_t, command_pool_size> command_pool{};
QueueHandle_t free_commands = nullptr;
MQTTAgentMessageContext message_context{};

bool send_command(MQTTAgentMessageContext_t* context,
                  MQTTAgentCommand_t* const* command,
                  std::uint32_t block_time_ms)
{
    return xQueueSendToBack(context->queue, command, pdMS_TO_TICKS(block_time_ms)) == pdPASS;
}

bool receive_command(MQTTAgentMessageContext_t* context,
                     MQTTAgentCommand_t** command,
                     std::uint32_t block_time_ms)
{
    return xQueueReceive(context->queue, command, pdMS_TO_TICKS(block_time_ms)) == pdPASS;
}

MQTTAgentCommand_t* get_command(std::uint32_t block_time_ms)
{
    MQTTAgentCommand_t* command = nullptr;
    (void)xQueueReceive(free_commands, &command, pdMS_TO_TICKS(block_time_ms));
    return command;
}

bool release_command(MQTTAgentCommand_t* command)
{
    return xQueueSendToBack(free_commands, &command, 0U) == pdPASS;
}

std::uint32_t current_time_ms()
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void ignore_incoming_publish(MQTTAgentContext_t*, std::uint16_t, MQTTPublishInfo_t*)
{
}

void notify_command_complete(MQTTAgentCommandContext_t* context,
                             MQTTAgentReturnInfo_t* result)
{
    context->status = result->returnCode;
    xTaskNotifyGive(context->task);
}

} // namespace

namespace network {

MqttClient::MqttClient(MqttBroker broker, const char* client_id)
    : broker_(broker)
    , client_id_(client_id)
{
}

bool MqttClient::add_broker_connection_listener(IBrokerConnectionListener& listener)
{
    configASSERT(!started_);

    if (listener_count_ == listeners_.size()) {
        return false;
    }

    listeners_[listener_count_] = &listener;
    ++listener_count_;
    return true;
}

bool MqttClient::start()
{
    configASSERT(!started_);

    link_events_ = xEventGroupCreate();
    message_context.queue = xQueueCreate(command_pool_size, sizeof(MQTTAgentCommand_t*));
    free_commands = xQueueCreate(command_pool_size, sizeof(MQTTAgentCommand_t*));
    if (link_events_ == nullptr || message_context.queue == nullptr ||
        free_commands == nullptr) {
        return false;
    }
    for (MQTTAgentCommand_t& command : command_pool) {
        (void)release_command(&command);
    }

    MQTTAgentMessageInterface_t messages{};
    messages.pMsgCtx = &message_context;
    messages.send = send_command;
    messages.recv = receive_command;
    messages.getCommand = get_command;
    messages.releaseCommand = release_command;

    network_.link_up = &link_up_;
    const TransportInterface_t transport = make_mqtt_transport(network_);
    const MQTTFixedBuffer_t buffer{network_buffer_.data(), network_buffer_.size()};
    if (MQTTAgent_Init(&agent_,
                       &messages,
                       &buffer,
                       &transport,
                       current_time_ms,
                       ignore_incoming_publish,
                       nullptr,
                       nullptr,
                       0U) != MQTTSuccess) {
        return false;
    }

    if (xTaskCreate(task_entry,
                    "mqtt",
                    agent_stack_depth,
                    this,
                    agent_task_priority,
                    nullptr) != pdPASS) {
        return false;
    }

    started_ = true;
    return true;
}

void MqttClient::on_link_state_changed(LinkState state)
{
    configASSERT(started_);

    link_up_ = state == LinkState::UP;
    if (state == LinkState::UP) {
        xEventGroupSetBits(link_events_, link_up_bit | link_came_up_bit);
    } else {
        xEventGroupClearBits(link_events_, link_up_bit);
    }
}

bool MqttClient::publish(const char* topic, const char* payload)
{
    if (!connected_) {
        return false;
    }

    MQTTPublishInfo_t message{};
    message.qos = MQTTQoS0;
    message.pTopicName = topic;
    message.topicNameLength = std::strlen(topic);
    message.pPayload = payload;
    message.payloadLength = std::strlen(payload);

    MQTTAgentPublishArgs_t arguments{&message, nullptr};
    MQTTAgentCommandContext_t completion{xTaskGetCurrentTaskHandle(), MQTTSuccess};
    const MQTTAgentCommandInfo_t command{notify_command_complete, &completion, 0U};
    if (MQTTAgent_Publish(&agent_, &arguments, &command) != MQTTSuccess) {
        return false;
    }

    // The agent keeps pointers to the locals above until it completes the command.
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    return completion.status == MQTTSuccess;
}

void MqttClient::task_entry(void* context)
{
    static_cast<MqttClient*>(context)->run();
}

void MqttClient::run()
{
    TickType_t retry_delay = min_retry_delay;
    for (;;) {
        xEventGroupWaitBits(link_events_, link_up_bit, pdFALSE, pdTRUE, portMAX_DELAY);
        xEventGroupClearBits(link_events_, link_came_up_bit);

        if (connect_to_broker()) {
            retry_delay = min_retry_delay;
            // Drop publishes queued before this connection existed.
            (void)MQTTAgent_CancelAll(&agent_);
            set_connected(true);
            // Returns once the connection fails.
            (void)MQTTAgent_CommandLoop(&agent_);
            set_connected(false);
            (void)MQTT_Disconnect(&agent_.mqttContext, nullptr, nullptr);
        }
        mqtt_transport_disconnect(network_);
        (void)MQTTAgent_CancelAll(&agent_);

        // A link that comes back up ends the wait early.
        xEventGroupWaitBits(link_events_, link_came_up_bit, pdTRUE, pdTRUE, retry_delay);
        retry_delay = std::min<TickType_t>(retry_delay * 2U, max_retry_delay);
    }
}

void MqttClient::set_connected(bool connected)
{
    connected_ = connected;
    for (std::size_t index = 0U; index < listener_count_; ++index) {
        listeners_[index]->on_broker_connection_changed(connected);
    }
}

bool MqttClient::connect_to_broker()
{
    if (!mqtt_transport_connect(network_, broker_.host, broker_.port)) {
        return false;
    }

    MQTTConnectInfo_t connect_info{};
    connect_info.cleanSession = true;
    connect_info.keepAliveSeconds = keep_alive_seconds;
    connect_info.pClientIdentifier = client_id_;
    connect_info.clientIdentifierLength = std::strlen(client_id_);

    bool session_present = false;
    return MQTT_Connect(&agent_.mqttContext,
                        &connect_info,
                        nullptr,
                        connack_timeout_ms,
                        &session_present,
                        nullptr,
                        nullptr) == MQTTSuccess;
}

} // namespace network
