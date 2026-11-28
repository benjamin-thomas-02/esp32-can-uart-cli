# ESP32 CAN/UART Command Interface

[![CI](https://github.com/benjamin-thomas-02/esp32-can-uart-cli/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/benjamin-thomas-02/esp32-can-uart-cli/actions/workflows/ci.yml)

🚧 Work in progress

ESP-IDF (v6.1) firmware for ESP32 with a UART command-line interface, built with a
layered, testable architecture.

## Features
- UART command-line interface (`help`, `version`, `uptime`, `echo`)
- FreeRTOS tasks with queue-based message passing
- Layered architecture: App / Service / Port / Platform
- Host-based unit tests with Unity (28 tests, no hardware needed)
- GitHub Actions CI: unit tests + firmware build on every push
- Planned: CAN (TWAI) self-test, pytest + pyserial hardware-in-the-loop tests

## Project structure

## Build & flash
```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Run unit tests (no hardware)
```bash
cmake -S test/host -B build-host
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```
