#pragma once
#include <mqtt_client.h>

namespace remote { namespace ota {
bool begin(const char *prefix);
// Called only from the ESP-MQTT event task.
void receive(esp_mqtt_event_handle_t event);
void disconnected();
// Called only from the cloud worker task.
bool subscribe(esp_mqtt_client_handle_t client);
void tick(esp_mqtt_client_handle_t client, bool connected);
// Arduino loop handshake: flash writes start only after pump GPIO is off.
bool requested();
void pumpStopped();
}} // namespace remote::ota
