# ESP32 Edge-to-Cloud Environment Monitor

An ESP32-based edge-to-cloud environmental monitoring system. It samples ambient climate (temperature and humidity) and particulate matter, displays these metrics locally in real-time on a 128x64 OLED screen, and transmits the telemetry as structured JSON payloads over secure TLS/MQTT to AWS IoT Core. From there, AWS Lambda ingests the data into DynamoDB, with full tracking and log retention managed via CloudWatch.

---

## 🚀 Key Features

*   **Asynchronous Sensor Reads:** Telemetry sampling and OLED refresh routines run independently of network latency using non-blocking (`millis()`) loops.
*   **Edge Telemetry:** Samples climate metrics via a DHT11 sensor and fine particulate matter (PM2.5/PM10) via a PMS5003 serial dust sensor.
*   **Secure Cloud Communication:** Connects to AWS IoT Core using TLS certificates for secure MQTT transport.
*   **Serverless Ingestion Pipeline:** Automatic storage in Amazon DynamoDB handled by an AWS Lambda function.
*   **Robust Monitoring:** CloudWatch logs and metrics configuration for performance tracking and ingestion verification.

---

## 🛠️ Hardware Bill of Materials (BOM)

Ensure you have the following components wired before deploying the firmware:

*   **Microcontroller:** ESP32 Development Board (e.g., ESP32-WROOM-32 or NodeMCU ESP32)
*   **Climate Sensor:** DHT11 Temperature & Humidity Sensor
*   **Particulate Sensor:** PMS5003 Laser Dust Sensor (UART Serial)
*   **OLED Display:** GME12864 / SSD1306 0.96" OLED (I2C, 128x64 pixels)
*   **Passives & Prototyping:**
    *   1x 10kΩ resistor (pull-up for DHT11 data line if using a bare sensor)
    *   1x 10µF - 100µF electrolytic capacitor (placed across 5V and GND rails for power stability)
    *   Half-size or full-size solderless breadboard
    *   Jumper wires (Male-to-Male and Male-to-Female)
    *   Micro-USB or USB-C data-capable cable

---

## 🔌 Default Pin Configuration

*   **I2C Interface (OLED Display):**
    *   `SDA` ➡️ Pin 21
    *   `SCL` ➡️ Pin 22
*   **UART2 Interface (PMS5003 Sensor):**
    *   `RX2` ➡️ Pin 16
    *   `TX2` ➡️ Pin 17
*   **DHT11 Sensor Data:**
    *   Connect to a free digital input pin (e.g., Pin 4 or similar, configurable in code). Make sure to pull up the data line with a 10kΩ resistor if using a 3-pin bare sensor.

---

## 📁 Repository Structure

```text
├── .agents/                 # AI Assistant/Antigravity configuration
├── firmware/                # PlatformIO C++ Project
│   ├── include/
│   │   └── secrets.h        # WiFi & AWS certificates (Git ignored)
│   ├── src/
│   │   └── main.cpp         # Main C++ source file
│   └── platformio.ini       # PlatformIO configuration & libraries manifest
├── aws/                     # AWS Serverless & Cloud Infrastructure
│   ├── lambda/
│   │   └── lambda_function.py  # Python ingestion handler
│   └── policy_templates/    # IoT Core & IAM policies
├── AGENTS.md                # AI agent instructions and guardrails
└── README.md                # Project documentation
```

---

## ⚡ Setup & Deployment

### 1. Firmware Setup (PlatformIO)
1. Open the `/firmware` directory in VS Code with the PlatformIO extension installed.
2. Under `firmware/include/`, create a `secrets.h` file. **Never commit this file to Git.** Populate it with your Wi-Fi credentials and AWS IoT certificates:
   ```cpp
   #ifndef SECRETS_H
   #define SECRETS_H

   const char* WIFI_SSID = "your_wifi_ssid";
   const char* WIFI_PASS = "your_wifi_password";
   const char* AWS_IOT_ENDPOINT = "your_endpoint.iot.us-east-1.amazonaws.com";

   // AWS IoT Root CA certificate, device certificate, and private key
   const char ROOT_CA[] PROGMEM = R"EOF(...)EOF";
   const char DEVICE_CERT[] PROGMEM = R"EOF(...)EOF";
   const char DEVICE_PRIVATE_KEY[] PROGMEM = R"EOF(...)EOF";

   #endif
   ```
3. Build and upload the firmware using PlatformIO.

### 2. Cloud Setup (AWS)
1. Register a **Thing** in AWS IoT Core and download its private key and certificates (insert these into `secrets.h`).
2. Attach an IoT Policy (templates are in `aws/policy_templates/`).
3. Deploy the AWS Lambda function found in `aws/lambda/` to parse incoming MQTT payloads and write them to DynamoDB.
4. Verify ingestion logs in CloudWatch.

---

## 🔒 Security Guidelines & Rules

*   **Secrets Management:** Never upload `secrets.h` or any credentials to public repositories. Ensure they remain ignored by the root `.gitignore` file.
*   **Non-Blocking Loops:** Do not use `delay()` in your code, as it will block incoming serial bytes from the PMS5003 and cause buffer overflows or data loss. Always use `millis()`-based task scheduling.
*   **Log Expiration:** Always set a logical retention limit on your AWS CloudWatch Log Groups (e.g., 7 or 14 days) to prevent runaway storage costs.
