#include "MqttOta.h"
#include "MqttOtaProtocol.h"
#include "CloudCommand.h"
#include <Arduino.h>
#include <atomic>
#include <cJSON.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <time.h>

namespace remote { namespace ota {
namespace {
struct Message { bool control; size_t length; uint8_t bytes[packetSize + 1]; };
QueueHandle_t queue = nullptr;
String controlTopic, dataTopic, ackTopic;
Packet packet;
bool receivingControl = false;
std::atomic<bool> hold{false}, safe{false}, lost{false};
enum class State { Idle, Waiting, Receiving, Complete };
State state = State::Idle;
char session[33]{}, expectedHash[65]{};
uint32_t imageSize = 0, received = 0, touched = 0, started = 0, rebootAt = 0;
const esp_partition_t *partition = nullptr;
esp_ota_handle_t handle = 0;
mbedtls_sha256_context sha;
bool hashActive = false;

bool topic(esp_mqtt_event_handle_t e, const String &s) {
  return e->topic && e->topic_len == int(s.length()) && !memcmp(e->topic, s.c_str(), s.length());
}
void ack(esp_mqtt_client_handle_t client, const char *id, const char *result, uint32_t next) {
  if (!client || esp_mqtt_client_get_outbox_size(client) >= 4096) return;
  char body[160];
  snprintf(body, sizeof(body), "{\"session\":\"%s\",\"result\":\"%s\",\"next\":%lu}",
           id, result, static_cast<unsigned long>(next));
  esp_mqtt_client_enqueue(client, ackTopic.c_str(), body, 0, 1, 0, true);
}
void release() {
  if (handle) esp_ota_abort(handle);
  handle = 0;
  if (hashActive) mbedtls_sha256_free(&sha);
  hashActive = false;
  state = State::Idle;
  // Clear safe before releasing hold, so a new transfer needs a fresh handshake.
  safe.store(false);
  hold.store(false);
}
void fail(esp_mqtt_client_handle_t client, const char *reason) {
  ack(client, session, reason, received);
  Serial.printf("MQTT OTA: %s (%lu/%lu)\n", reason,
                static_cast<unsigned long>(received), static_cast<unsigned long>(imageSize));
  release();
}
void control(esp_mqtt_client_handle_t client, Message &m) {
  if (m.length > 512 || memchr(m.bytes, 0, m.length)) return;
  m.bytes[m.length] = 0;
  cJSON *root = cJSON_ParseWithOpts(reinterpret_cast<char *>(m.bytes), nullptr, true);
  const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "session");
  const cJSON *action = cJSON_GetObjectItemCaseSensitive(root, "action");
  if (!cJSON_IsObject(root) || !cJSON_IsString(id) || !hex(id->valuestring, 32) || !cJSON_IsString(action)) {
    cJSON_Delete(root);
    return;
  }
  const char *sid = id->valuestring;
  const char *act = action->valuestring;
  const bool same = !strcmp(sid, session);
  if (!strcmp(act, "start")) {
    const cJSON *size = cJSON_GetObjectItemCaseSensitive(root, "size");
    const cJSON *hash = cJSON_GetObjectItemCaseSensitive(root, "sha256");
    const cJSON *expiry = cJSON_GetObjectItemCaseSensitive(root, "expires_at");
    const uint32_t now = uint32_t(time(nullptr));
    const bool valid = cJSON_IsNumber(size) && positiveUint32(size->valuedouble) &&
        cJSON_IsString(hash) && hex(hash->valuestring, 64) && cJSON_IsNumber(expiry) &&
        positiveUint32(expiry->valuedouble) && now >= 1704067200 &&
        expiry->valuedouble > now && expiry->valuedouble - now <= 120;
    if (!valid) ack(client, sid, "invalid_start", 0);
    else if (state != State::Idle) {
      if (!same) ack(client, sid, "busy", 0);
      else if (imageSize != uint32_t(size->valuedouble) || strcmp(expectedHash, hash->valuestring))
        ack(client, sid, "session_conflict", received);
      else if (state != State::Waiting)
        ack(client, sid, state == State::Complete ? "complete" : "ready", received);
    } else {
      partition = esp_ota_get_next_update_partition(nullptr);
      if (!partition || size->valuedouble > partition->size) ack(client, sid, "image_too_large", 0);
      else {
        strcpy(session, sid);
        strcpy(expectedHash, hash->valuestring);
        imageSize = uint32_t(size->valuedouble);
        received = 0;
        touched = started = millis();
        safe.store(false);
        hold.store(true);
        state = State::Waiting;
      }
    }
  } else if (state == State::Idle || !same) {
    ack(client, sid, "no_session", 0);
  } else if (!strcmp(act, "abort")) {
    if (state == State::Complete) ack(client, sid, "complete", received);
    else { ack(client, sid, "aborted", received); release(); }
  } else if (!strcmp(act, "finish")) {
    if (state == State::Complete) ack(client, sid, "complete", received);
    else if (state != State::Receiving || received != imageSize) ack(client, sid, "incomplete", received);
    else {
      uint8_t digest[32];
      char actual[65];
      if (mbedtls_sha256_finish_ret(&sha, digest) != 0) fail(client, "hash_error");
      else {
        for (size_t i = 0; i < 32; ++i) snprintf(actual + 2 * i, 3, "%02x", digest[i]);
        if (strcmp(actual, expectedHash)) fail(client, "hash_mismatch");
        else {
          const esp_err_t result = esp_ota_end(handle);
          handle = 0; // esp_ota_end consumes the handle even on error.
          if (result != ESP_OK) fail(client, "invalid_image");
          else if (esp_ota_set_boot_partition(partition) != ESP_OK) fail(client, "boot_partition_error");
          else {
            mbedtls_sha256_free(&sha);
            hashActive = false;
            state = State::Complete;
            rebootAt = millis();
            ack(client, sid, "complete", received);
            Serial.println("MQTT OTA verified; rebooting");
          }
        }
      }
    }
  } else ack(client, sid, "invalid_action", received);
  cJSON_Delete(root);
}
void data(esp_mqtt_client_handle_t client, const Message &m) {
  if (state != State::Receiving || m.length <= headerSize || memcmp(m.bytes, session, 32)) return;
  const uint32_t at = offset(m.bytes);
  const size_t length = m.length - headerSize;
  const Position p = position(received, imageSize, at, length);
  if (p == Position::Invalid) { ack(client, session, "offset_error", received); return; }
  if (p == Position::Next) {
    if (esp_ota_write(handle, m.bytes + headerSize, length) != ESP_OK) { fail(client, "write_error"); return; }
    if (mbedtls_sha256_update_ret(&sha, m.bytes + headerSize, length) != 0) { fail(client, "hash_error"); return; }
    received += length;
    touched = millis();
  }
  ack(client, session, "chunk", received);
}
} // namespace
bool begin(const char *prefix) {
  controlTopic = String(prefix) + "/ota/control";
  dataTopic = String(prefix) + "/ota/data";
  ackTopic = String(prefix) + "/ota/ack";
  queue = xQueueCreate(2, sizeof(Message));
  return queue != nullptr;
}
bool subscribe(esp_mqtt_client_handle_t client) {
  return esp_mqtt_client_subscribe(client, controlTopic.c_str(), 1) >= 0 &&
         esp_mqtt_client_subscribe(client, dataTopic.c_str(), 1) >= 0;
}
void receive(esp_mqtt_event_handle_t e) {
  if (!queue) return;
  if (e->current_data_offset < 0 || e->total_data_len < 0 || e->data_len < 0) { packet.reset(); return; }
  const bool isControl = topic(e, controlTopic);
  const bool accept = !e->retain && (isControl || topic(e, dataTopic));
  if (!e->current_data_offset) receivingControl = isControl;
  if (packet.append(e->current_data_offset, e->total_data_len, e->data, e->data_len, accept)) {
    Message m{};
    m.control = receivingControl;
    m.length = e->total_data_len;
    memcpy(m.bytes, packet.bytes, m.length);
    // A dropped packet is retried by the sender after its application ACK timeout.
    xQueueSend(queue, &m, 0);
    packet.reset();
  }
}
void disconnected() { packet.reset(); lost.store(true); }
bool requested() { return hold.load(); }
void pumpStopped() { safe.store(true); }
void tick(esp_mqtt_client_handle_t client, bool connected) {
  if (state == State::Complete) {
    Message m;
    if (connected && xQueueReceive(queue, &m, 0) == pdTRUE && m.control) control(client, m);
    if (uint32_t(millis() - rebootAt) >= 5000) ESP.restart();
    return;
  }
  if (lost.exchange(false)) {
    Message discard;
    while (xQueueReceive(queue, &discard, 0) == pdTRUE) {}
    if (state != State::Idle) fail(connected ? client : nullptr, "disconnected");
  }
  if (state != State::Idle && (uint32_t(millis() - touched) >= 60000 ||
                              uint32_t(millis() - started) >= 1800000)) fail(connected ? client : nullptr, "timeout");
  if (!connected) return;
  if (state == State::Waiting && safe.load()) {
    if (esp_ota_begin(partition, imageSize, &handle) != ESP_OK) { handle = 0; fail(client, "begin_error"); }
    else {
      mbedtls_sha256_init(&sha);
      hashActive = true;
      if (mbedtls_sha256_starts_ret(&sha, 0) != 0) fail(client, "hash_error");
      else { state = State::Receiving; touched = millis(); ack(client, session, "ready", received); }
    }
  }
  Message m;
  if (xQueueReceive(queue, &m, 0) == pdTRUE) {
    if (m.control) control(client, m);
    else data(client, m);
  }
}
}} // namespace remote::ota
