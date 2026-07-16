/**
 * ESP32 Edge-to-Cloud Environment Monitor
 *
 * Samples ambient climate (DHT11) and particulate matter (PMS5003, UART2),
 * renders live values on a 128x64 SSD1306 OLED, and publishes structured
 * JSON telemetry over TLS/MQTT to AWS IoT Core.
 *
 * Scheduling is fully non-blocking (millis()-based). delay() is never used
 * in loop() so incoming PMS5003 UART bytes are never dropped.
 *
 * Wiring (see README.md):
 *   OLED    SDA=21 SCL=22 (I2C, addr 0x3C)
 *   PMS5003 TX -> GPIO16 (RX2), RX -> GPIO17 (TX2)
 *   DHT11   DATA -> GPIO4 (10k pull-up if bare sensor)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <time.h>

#include "secrets.h"  // WiFi credentials + AWS endpoint/certs (git-ignored)

// ---------------------------------------------------------------------------
// Pin & peripheral configuration
// ---------------------------------------------------------------------------
constexpr uint8_t PIN_DHT      = 4;
constexpr uint8_t PIN_I2C_SDA  = 21;
constexpr uint8_t PIN_I2C_SCL  = 22;
constexpr int8_t  PIN_PMS_RX   = 16;  // ESP32 RX2  <- PMS5003 TX
constexpr int8_t  PIN_PMS_TX   = 17;  // ESP32 TX2  -> PMS5003 RX

constexpr uint8_t  OLED_WIDTH  = 128;
constexpr uint8_t  OLED_HEIGHT = 64;
constexpr uint8_t  OLED_ADDR   = 0x3C;

constexpr uint16_t MQTT_PORT   = 8883;

// ---------------------------------------------------------------------------
// Task intervals (ms)
// ---------------------------------------------------------------------------
constexpr uint32_t INTERVAL_DHT_READ   = 5000;   // DHT11 needs >= 2 s between reads
constexpr uint32_t INTERVAL_OLED       = 1000;
constexpr uint32_t INTERVAL_PUBLISH    = 30000;
constexpr uint32_t INTERVAL_MQTT_RETRY = 5000;
constexpr uint32_t INTERVAL_WIFI_RETRY = 10000;

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
WiFiClientSecure tlsClient;
PubSubClient     mqtt(tlsClient);
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
DHT              dht(PIN_DHT, DHT11);

String telemetryTopic;  // esp32-edge/<THING_NAME>/telemetry

struct Telemetry {
  float    temperatureC = NAN;
  float    humidityPct  = NAN;
  uint16_t pm1_0        = 0;
  uint16_t pm2_5        = 0;
  uint16_t pm10         = 0;
  bool     climateValid = false;
  bool     pmValid      = false;
} telemetry;

uint32_t lastDhtRead   = 0;
uint32_t lastOledDraw  = 0;
uint32_t lastPublish   = 0;
uint32_t lastMqttRetry = 0;
uint32_t lastWifiRetry = 0;
bool     oledPresent   = false;

// ---------------------------------------------------------------------------
// PMS5003 non-blocking frame parser
//
// 32-byte frame: 0x42 0x4D | frame length | 13x uint16 data | uint16 checksum.
// Bytes are consumed as they arrive so the loop never blocks on the UART.
// ---------------------------------------------------------------------------
namespace pms {

constexpr size_t FRAME_LEN = 32;
uint8_t  buf[FRAME_LEN];
size_t   pos = 0;

uint16_t word(size_t i) { return (uint16_t)(buf[i] << 8 | buf[i + 1]); }

bool frameValid() {
  if (word(2) != 28) return false;  // payload length field
  uint16_t sum = 0;
  for (size_t i = 0; i < FRAME_LEN - 2; i++) sum += buf[i];
  return sum == word(FRAME_LEN - 2);
}

// Pump all pending UART bytes; returns true when a valid frame completes.
bool poll() {
  bool gotFrame = false;
  while (Serial2.available() > 0) {
    uint8_t b = (uint8_t)Serial2.read();
    if (pos == 0 && b != 0x42) continue;
    if (pos == 1 && b != 0x4D) { pos = 0; continue; }
    buf[pos++] = b;
    if (pos == FRAME_LEN) {
      pos = 0;
      if (frameValid()) {
        // Words 10..12 = PM1.0 / PM2.5 / PM10 under atmospheric environment
        telemetry.pm1_0   = word(10);
        telemetry.pm2_5   = word(12);
        telemetry.pm10    = word(14);
        telemetry.pmValid = true;
        gotFrame = true;
      }
    }
  }
  return gotFrame;
}

}  // namespace pms

// ---------------------------------------------------------------------------
// Connectivity
// ---------------------------------------------------------------------------
void connectWiFi() {
  Serial.printf("[wifi] connecting to %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

// TLS certificate validation requires a correct wall clock.
void syncClock() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("[ntp] syncing time");
  time_t now = time(nullptr);
  uint32_t start = millis();
  while (now < 8 * 3600 * 2 && millis() - start < 15000) {  // bounded wait, setup only
    delay(250);
    Serial.print('.');
    now = time(nullptr);
  }
  Serial.printf("\n[ntp] epoch=%ld\n", (long)now);
}

bool connectMqtt() {
  Serial.printf("[mqtt] connecting to %s:%u as %s\n", AWS_IOT_ENDPOINT, MQTT_PORT, THING_NAME);
  if (mqtt.connect(THING_NAME)) {
    Serial.println("[mqtt] connected");
    return true;
  }
  Serial.printf("[mqtt] failed, rc=%d\n", mqtt.state());
  return false;
}

// ---------------------------------------------------------------------------
// Tasks
// ---------------------------------------------------------------------------
void taskReadDht() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("[dht] read failed, keeping last good values");
    return;
  }
  telemetry.humidityPct  = h;
  telemetry.temperatureC = t;
  telemetry.climateValid = true;
}

void taskDrawOled() {
  if (!oledPresent) return;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("ESP32 Edge Monitor");

  display.setCursor(0, 14);
  if (telemetry.climateValid) {
    display.printf("Temp: %.1f C", telemetry.temperatureC);
    display.setCursor(0, 26);
    display.printf("Hum : %.1f %%", telemetry.humidityPct);
  } else {
    display.print("Climate: waiting...");
  }

  display.setCursor(0, 40);
  if (telemetry.pmValid) {
    display.printf("PM2.5: %u ug/m3", telemetry.pm2_5);
    display.setCursor(0, 50);
    display.printf("PM10 : %u ug/m3", telemetry.pm10);
  } else {
    display.print("PM: waiting...");
  }

  // Connectivity status, top-right
  display.setCursor(104, 0);
  display.print(WiFi.status() == WL_CONNECTED ? (mqtt.connected() ? "OK" : "W-") : "--");

  display.display();
}

void taskPublish() {
  if (!mqtt.connected()) return;
  if (!telemetry.climateValid && !telemetry.pmValid) return;  // nothing to report yet

  JsonDocument doc;
  doc["deviceId"]  = THING_NAME;
  doc["timestamp"] = (uint32_t)time(nullptr);
  if (telemetry.climateValid) {
    doc["temperature_c"] = serialized(String(telemetry.temperatureC, 1));
    doc["humidity_pct"]  = serialized(String(telemetry.humidityPct, 1));
  }
  if (telemetry.pmValid) {
    doc["pm1_0"] = telemetry.pm1_0;
    doc["pm2_5"] = telemetry.pm2_5;
    doc["pm10"]  = telemetry.pm10;
  }

  char payload[512];
  size_t n = serializeJson(doc, payload, sizeof(payload));
  if (mqtt.publish(telemetryTopic.c_str(), payload, false)) {
    Serial.printf("[mqtt] published %u bytes: %s\n", (unsigned)n, payload);
  } else {
    Serial.println("[mqtt] publish failed");
  }
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, PIN_PMS_RX, PIN_PMS_TX);  // PMS5003
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

  dht.begin();

  oledPresent = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledPresent) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Booting...");
    display.display();
  } else {
    Serial.println("[oled] SSD1306 not found at 0x3C, continuing headless");
  }

  connectWiFi();
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);  // bounded wait, setup only
  }
  Serial.printf("[wifi] %s\n", WiFi.status() == WL_CONNECTED
                                   ? WiFi.localIP().toString().c_str()
                                   : "not connected, will retry in loop");

  if (WiFi.status() == WL_CONNECTED) syncClock();

  tlsClient.setCACert(ROOT_CA);
  tlsClient.setCertificate(DEVICE_CERT);
  tlsClient.setPrivateKey(DEVICE_PRIVATE_KEY);

  telemetryTopic = String("esp32-edge/") + THING_NAME + "/telemetry";
  mqtt.setServer(AWS_IOT_ENDPOINT, MQTT_PORT);
  mqtt.setBufferSize(1024);
}

void loop() {
  const uint32_t now = millis();

  // Always drain PMS5003 UART first so its 64-byte HW buffer never overflows.
  pms::poll();

  // WiFi reconnect (non-blocking)
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWifiRetry >= INTERVAL_WIFI_RETRY) {
      lastWifiRetry = now;
      Serial.println("[wifi] reconnecting...");
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
  } else if (!mqtt.connected()) {
    // MQTT reconnect (non-blocking retry window)
    if (now - lastMqttRetry >= INTERVAL_MQTT_RETRY) {
      lastMqttRetry = now;
      connectMqtt();
    }
  } else {
    mqtt.loop();
  }

  if (now - lastDhtRead >= INTERVAL_DHT_READ) {
    lastDhtRead = now;
    taskReadDht();
  }

  if (now - lastOledDraw >= INTERVAL_OLED) {
    lastOledDraw = now;
    taskDrawOled();
  }

  if (now - lastPublish >= INTERVAL_PUBLISH) {
    lastPublish = now;
    taskPublish();
  }
}
