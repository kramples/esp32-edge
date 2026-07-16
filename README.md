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
    *   `DATA` ➡️ Pin 4 (`PIN_DHT` in `firmware/src/main.cpp` — change there if rewired). Make sure to pull up the data line with a 10kΩ resistor if using a 3-pin bare sensor.

---

## 📁 Repository Structure

```text
├── firmware/                # PlatformIO C++ Project
│   ├── include/
│   │   ├── secrets.h.example   # Credentials template (copy to secrets.h)
│   │   └── secrets.h        # WiFi & AWS certificates (Git ignored)
│   ├── src/
│   │   └── main.cpp         # Main C++ source file
│   └── platformio.ini       # PlatformIO configuration & libraries manifest
├── aws/                     # AWS Serverless & Cloud Infrastructure
│   ├── README.md            # Step-by-step AWS deployment guide
│   ├── lambda/
│   │   ├── lambda_function.py  # Python ingestion handler
│   │   └── tests/           # pytest local validation suite
│   └── policy_templates/    # Least-privilege IoT Core & IAM policies
├── AGENTS.md                # AI agent instructions and guardrails
├── CHANGELOG.md             # Release history (Keep a Changelog format)
└── README.md                # Project documentation
```

---

## ⚡ Setup & Deployment

### 1. Firmware Setup (PlatformIO)
1. Open the `/firmware` directory in VS Code with the PlatformIO extension installed.
2. Copy `firmware/include/secrets.h.example` to `firmware/include/secrets.h` and fill in your Wi-Fi credentials, AWS IoT endpoint, Thing name, and certificates. **Never commit this file to Git** (it is already covered by `.gitignore`).
3. Build and upload the firmware using PlatformIO (`pio run -t upload`), then watch the serial monitor at 115200 baud (`pio device monitor`).

### 2. Cloud Setup (AWS)
Follow the step-by-step guide in [aws/README.md](aws/README.md):
1. Create the `esp32-edge-telemetry` DynamoDB table.
2. Register a **Thing** in AWS IoT Core, download its private key and certificates (insert these into `secrets.h`), and attach the least-privilege policy from `aws/policy_templates/iot_device_policy.json`.
3. Deploy the AWS Lambda function in `aws/lambda/` and create the IoT rule on `esp32-edge/+/telemetry`.
4. Set CloudWatch log retention (14 days) and verify ingestion logs.

Telemetry payload published to `esp32-edge/<THING_NAME>/telemetry`:
```json
{
  "deviceId": "esp32-edge-01",
  "timestamp": 1752537600,
  "temperature_c": 24.5,
  "humidity_pct": 41.0,
  "pm1_0": 3,
  "pm2_5": 7,
  "pm10": 9
}
```

---

## 🔒 Security Guidelines & Rules

*   **Secrets Management:** Never upload `secrets.h` or any credentials to public repositories. Ensure they remain ignored by the root `.gitignore` file.
*   **Non-Blocking Loops:** Do not use `delay()` in your code, as it will block incoming serial bytes from the PMS5003 and cause buffer overflows or data loss. Always use `millis()`-based task scheduling.
*   **Log Expiration:** Always set a logical retention limit on your AWS CloudWatch Log Groups (e.g., 7 or 14 days) to prevent runaway storage costs.

---

## 📚 Additional Resources

*   **AWS IoT + ESP32:**
    *   [Building an AWS IoT Core device using AWS Serverless and an ESP32](https://aws.amazon.com/blogs/compute/building-an-aws-iot-core-device-using-aws-serverless-and-an-esp32/) — official AWS reference architecture this project follows
    *   [AWS IoT Core policies](https://docs.aws.amazon.com/iot/latest/developerguide/iot-policies.html) and [Rules for AWS IoT](https://docs.aws.amazon.com/iot/latest/developerguide/iot-rules.html)
    *   [Troubleshooting ESP32 TLS handshake errors with AWS IoT Core](https://repost.aws/questions/QU4Iuork20TSqIR7BtslETLA/esp32-fails-to-connect-to-aws-iot-core-via-mqtt-tls-handshake-error) — most failures are missing NTP time sync or undersized buffers
*   **Sensors:**
    *   [PMS5003 with ESP32 guide](https://dronebotworkshop.com/air-quality/) — wiring and protocol background; use hardware UART2, not software serial
    *   [PMserial](https://github.com/avaldebe/PMserial) and [PMSx003](https://github.com/topics/pms5003) — alternative Plantower libraries if you outgrow the built-in frame parser
*   **Libraries:** [PubSubClient](https://github.com/knolleary/pubsubclient) · [ArduinoJson](https://arduinojson.org/) · [Adafruit SSD1306](https://github.com/adafruit/Adafruit_SSD1306) · [DHT sensor library](https://github.com/adafruit/DHT-sensor-library)
*   **Conventions:** [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) · [Semantic Versioning](https://semver.org/) · [Conventional Commits](https://www.conventionalcommits.org/)

See [CHANGELOG.md](CHANGELOG.md) for release history.

---

## 📄 License

Released under the [MIT License](LICENSE).
