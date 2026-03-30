#pragma once

#include <cstddef>

/// SEN55 entity count and names, shared with MQTT publishing.
constexpr size_t kSen55EntityCount = 8;
extern const char *const kSen55Entities[kSen55EntityCount];

/// Publish MQTT Discovery config for the local SEN55 sensor (8 entities).
/// Call on every MQTT connect (including reconnects).
void ha_discovery_publish_sen55(const char *device_id);
