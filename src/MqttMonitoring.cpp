#include "MqttMonitoring.h"
#include "MqttPacket.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <cJSON.h>
#include <mqtt_client.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <time.h>

namespace remote {
namespace {
QueueHandle_t commands = nullptr, snapshots = nullptr, incoming = nullptr;
EventGroupHandle_t connection = nullptr;
constexpr EventBits_t CONNECTED = BIT0, NEW_SESSION = BIT1;
struct RawCommand { char payload[CommandPacket::capacity + 1]; };
Config settings{};
String commandTopic, telemetryTopic, ackTopic, statusTopic;
Snapshot current{};
bool haveSnapshot = false;
CommandPacket packet;

void mqttEvent(void *, esp_event_base_t, int32_t eventId, void *eventData) {
  const auto event = static_cast<esp_mqtt_event_handle_t>(eventData);
  if (eventId == MQTT_EVENT_CONNECTED) {
    xEventGroupSetBits(connection, CONNECTED | NEW_SESSION);
    Serial.println("MQTT cloud connected");
  } else if (eventId == MQTT_EVENT_DISCONNECTED) {
    xEventGroupClearBits(connection, CONNECTED | NEW_SESSION);
    packet.reset();
    Serial.println("MQTT cloud disconnected; reconnecting automatically");
  } else if (eventId == MQTT_EVENT_DATA) {
    const bool topicMatches = event->topic && event->topic_len == int(commandTopic.length()) &&
        !memcmp(event->topic, commandTopic.c_str(), commandTopic.length());
    if (event->current_data_offset < 0 || event->total_data_len < 0 || event->data_len < 0) {
      packet.reset();
      return;
    }
    if (packet.append(event->current_data_offset, event->total_data_len,
                      event->data, event->data_len, topicMatches && !event->retain)) {
      RawCommand raw{};
      memcpy(raw.payload, packet.data(), event->total_data_len + 1);
      if (xQueueSend(incoming, &raw, 0) != pdTRUE)
        Serial.println("MQTT command queue full; command dropped");
      packet.reset();
    }
  } else if (eventId == MQTT_EVENT_ERROR) {
    Serial.println("MQTT connection error; check broker, credentials, TLS and ACL");
  }
}

String number(float value) {
  return std::isfinite(value) ? String(value, 2) : String("null");
}
String json() {
  String body;
  body.reserve(768);
  body = "{\"device_id\":\"" + String(settings.deviceId) + "\",\"uptime_ms\":" + String(current.uptimeMs);
  body += ",\"sample_age_ms\":" + String(current.sampleAgeMs);
  body += ",\"temperature_c\":" + number(current.temperature);
  body += ",\"humidity_pct\":" + number(current.humidity);
  body += ",\"light_lux\":" + number(current.lux);
  body += ",\"soil_pct\":" + number(current.soil);
  body += ",\"soil_raw\":" + String(current.soilRaw);
  body += ",\"sensors_valid\":" + String(current.valid ? "true" : "false");
  body += ",\"soil_calibrated\":" + String(current.calibrated ? "true" : "false");
  body += ",\"automatic_enabled\":" + String(current.automatic ? "true" : "false");
  body += ",\"samples\":" + String(current.samples);
  body += ",\"mood\":" + String(current.mood);
  body += ",\"pump_on\":" + String(current.pumping ? "true" : "false");
  body += ",\"watering_state\":\"" + String(current.wateringState) + "\"";
  body += ",\"pulses\":" + String(current.pulses);
  body += ",\"last_command\":{\"id\":" + String(current.commandId);
  body += ",\"result\":\"" + String(current.commandResult) + "\"}}";
  return body;
}
void networkTask(void *) {
  // Receipt is persisted BEFORE the pump command is queued.
  Preferences storage;
  const bool storageReady = storage.begin("namika-cloud", false);
  uint32_t seenId = storageReady ? storage.getUInt("seen-id", 0) : 0;
  if (!storageReady) Serial.println("MQTT commands disabled: NVS unavailable");
  esp_mqtt_client_config_t config{};
  config.host = settings.host;
  config.port = settings.port;
  config.transport = MQTT_TRANSPORT_OVER_SSL;
  config.cert_pem = settings.rootCa;
  config.client_id = settings.deviceId;
  config.username = settings.username;
  config.password = settings.password;
  config.lwt_topic = statusTopic.c_str();
  config.lwt_msg = "offline";
  config.lwt_qos = 1;
  config.lwt_retain = 1;
  config.keepalive = 30;
  config.reconnect_timeout_ms = 5000;
  config.network_timeout_ms = 5000;
  config.buffer_size = 1024;
  config.out_buffer_size = 1024;
  // Clean session: do not replay a broker queue of commands after going offline.
  config.disable_clean_session = false;
  config.protocol_ver = MQTT_PROTOCOL_V_3_1_1;
  esp_mqtt_client_handle_t client = nullptr;
  uint32_t pendingId = 0, lastPush = 0;
  const char *pendingResult = nullptr;
  bool lastPumping = false;
  uint32_t lastCommand = 0;
  const char *lastState = "";
  for (;;) {
    if (xQueueReceive(snapshots, &current, 0) == pdTRUE) haveSnapshot = true;
    if (pendingId && !pendingResult && haveSnapshot && current.commandId == pendingId)
      pendingResult = current.commandResult;
    if (!client && WiFi.status() == WL_CONNECTED && time(nullptr) >= 1704067200) {
      client = esp_mqtt_client_init(&config);
      if (!client || esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqttEvent, nullptr) != ESP_OK ||
          esp_mqtt_client_start(client) != ESP_OK) {
        if (client) esp_mqtt_client_destroy(client);
        client = nullptr;
        Serial.println("MQTT startup failed");
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }
    const EventBits_t bits = xEventGroupGetBits(connection);
    if (client && (bits & CONNECTED)) {
      if ((bits & NEW_SESSION) && esp_mqtt_client_get_outbox_size(client) < 4096) {
        if (esp_mqtt_client_subscribe(client, commandTopic.c_str(), 1) >= 0 &&
            esp_mqtt_client_enqueue(client, statusTopic.c_str(), "online", 0, 1, 1, true) >= 0) {
          xEventGroupClearBits(connection, NEW_SESSION);
          lastPush = millis() - 5000;
        }
      }
      // Bound the outbox under slow connections; prioritize command decisions.
      const bool outboxAvailable = esp_mqtt_client_get_outbox_size(client) < 4096;
      if (pendingId && pendingResult && outboxAvailable) {
        const String body = "{\"id\":" + String(pendingId) + ",\"result\":\"" +
                            String(pendingResult) + "\"}";
        if (esp_mqtt_client_enqueue(client, ackTopic.c_str(), body.c_str(), 0, 1, 1, true) >= 0) {
          pendingId = 0;
          pendingResult = nullptr;
        }
      }
      if (!pendingId && outboxAvailable) {
        RawCommand raw{};
        if (xQueueReceive(incoming, &raw, 0) == pdTRUE) {
          cJSON *root = cJSON_ParseWithOpts(raw.payload, nullptr, true);
          const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
          const cJSON *action = cJSON_GetObjectItemCaseSensitive(root, "action");
          const cJSON *expires = cJSON_GetObjectItemCaseSensitive(root, "expires_at");
          Command command{};
          const char *validation = "invalid_command";
          if (cJSON_IsObject(root) && cJSON_IsNumber(id) && cJSON_IsString(action) &&
              cJSON_IsNumber(expires))
            validation = validateCommand(id->valuedouble, action->valuestring,
                                         expires->valuedouble, uint32_t(time(nullptr)), command);
          cJSON_Delete(root);
          if (command.id) {
            pendingId = command.id;
            if (!storageReady) pendingResult = "storage_error";
            else if (command.id <= seenId) {
              pendingResult = haveSnapshot && current.commandId == command.id ?
                              current.commandResult : "duplicate_or_interrupted";
            } else if (storage.putUInt("seen-id", command.id) != sizeof(uint32_t)) {
              pendingResult = "storage_error";
            } else {
              seenId = command.id;
              if (strcmp(validation, "valid")) pendingResult = validation;
              else if (xQueueSend(commands, &command, 0) != pdTRUE)
                pendingResult = "queue_full";
            }
          } else Serial.println("MQTT: malformed command ignored");
        }
      }
      const bool changed = haveSnapshot && (current.pumping != lastPumping ||
          current.commandId != lastCommand || strcmp(current.wateringState, lastState));
      if (haveSnapshot && outboxAvailable &&
          (changed || uint32_t(millis() - lastPush) >= 5000)) {
        const String body = json();
        if (esp_mqtt_client_enqueue(client, telemetryTopic.c_str(), body.c_str(), 0, 1, 1, true) >= 0) {
          lastPush = millis();
          lastPumping = current.pumping;
          lastCommand = current.commandId;
          lastState = current.wateringState;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
}  // namespace
bool begin(const Config &config) {
  settings = config;
  if (!settings.host[0]) {
    Serial.println("MQTT disabled: broker not configured");
    return true;
  }
  // Device IDs also appear in JSON, client IDs and topic path components.
  const size_t idLength = strlen(settings.deviceId);
  if (!idLength || idLength > 48 || !settings.port || !settings.username[0] ||
      !settings.password[0] || !settings.rootCa[0]) return false;
  for (size_t i = 0; i < idLength; ++i) {
    const char c = settings.deviceId[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
  }
  const String prefix = "namika/" + String(settings.deviceId);
  commandTopic = prefix + "/commands";
  telemetryTopic = prefix + "/monitoring";
  ackTopic = prefix + "/ack";
  statusTopic = prefix + "/status";
  commands = xQueueCreate(1, sizeof(Command));
  snapshots = xQueueCreate(1, sizeof(Snapshot));
  incoming = xQueueCreate(4, sizeof(RawCommand));
  connection = xEventGroupCreate();
  if (!commands || !snapshots || !incoming || !connection ||
      xTaskCreate(networkTask, "namika-cloud", 8192, nullptr, 1, nullptr) != pdPASS) {
    if (commands) vQueueDelete(commands);
    if (snapshots) vQueueDelete(snapshots);
    if (incoming) vQueueDelete(incoming);
    if (connection) vEventGroupDelete(connection);
    commands = snapshots = incoming = nullptr;
    connection = nullptr;
    return false;
  }
  return true;
}
bool takeCommand(Command &command) {
  return commands && xQueueReceive(commands, &command, 0) == pdTRUE;
}
void publish(const Snapshot &snapshot) {
  if (snapshots) xQueueOverwrite(snapshots, &snapshot);
}
}  // namespace remote
