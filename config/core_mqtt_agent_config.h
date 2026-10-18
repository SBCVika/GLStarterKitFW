#pragma once

/* Only QoS 0 publishes are sent, so no acknowledgments are tracked. */
#define MQTT_AGENT_USE_QOS_1_2_PUBLISH            0
#define MQTT_AGENT_MAX_OUTSTANDING_ACKS           1U
