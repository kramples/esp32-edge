# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- GitHub Actions CI (`.github/workflows/ci.yml`): PlatformIO firmware build
  (with placeholder `secrets.h`) and Lambda pytest suite on every push and
  pull request, with PlatformIO package caching.
- `aws/lambda/requirements-dev.txt` for local/CI test dependencies.
- `.gitattributes` normalizing line endings (LF in repo, native on checkout).
- Firmware: sensor readings now expire after 60 s without a fresh sample —
  stale values are dropped from the OLED and the MQTT payload instead of
  being re-published indefinitely after a sensor failure.

## [0.1.0] - 2026-07-15

### Added

- ESP32 firmware (`firmware/src/main.cpp`): non-blocking `millis()`-scheduled
  tasks for DHT11 climate reads, PMS5003 particulate parsing (hardware UART2,
  checksum-validated frames), SSD1306 OLED refresh, and JSON telemetry
  publishing over TLS/MQTT to AWS IoT Core with NTP clock sync and automatic
  WiFi/MQTT reconnect.
- Pinned PlatformIO library dependencies (`firmware/platformio.ini`):
  PubSubClient, ArduinoJson 7, Adafruit SSD1306/GFX/Unified Sensor, DHT
  sensor library.
- Credentials template `firmware/include/secrets.h.example` (real `secrets.h`
  stays git-ignored).
- AWS ingestion pipeline (`aws/`): Python 3.12 Lambda handler writing telemetry
  to DynamoDB with structured CloudWatch JSON logging, pytest suite, and
  least-privilege IoT device / Lambda execution role policy templates.
- AWS deployment guide (`aws/README.md`) covering DynamoDB, IoT Thing/rule
  setup, and mandatory CloudWatch log retention.
- This changelog.

### Changed

- Replaced the PlatformIO hello-world stub with the full firmware
  implementation.

## [0.0.1] - 2026-07-14

### Added

- Initial workspace scaffold: PlatformIO project skeleton, README, AGENTS.md.

[Unreleased]: https://github.com/kramples/esp32-edge/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/kramples/esp32-edge/compare/v0.0.1...v0.1.0
[0.0.1]: https://github.com/kramples/esp32-edge/releases/tag/v0.0.1
