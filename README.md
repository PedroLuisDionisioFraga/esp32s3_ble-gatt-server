# ESP32-S3 BLE GATT Server Firmware

BLE GATT Server firmware for **ESP32-S3**, built with [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/). Exposes sensor data, device control, and device information as BLE services through a custom abstraction layer that hides ESP-IDF Bluedroid complexity behind a simple, callback-based API.

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [BLE Profile](#ble-profile)
- [Project Structure](#project-structure)
- [Prerequisites](#prerequisites)
- [Build & Flash](#build--flash)
- [Monitoring](#monitoring)
- [Adding a New Service](#adding-a-new-service)
- [Components](#components)
- [Authors](#authors)

## Architecture Overview

The firmware runs as a **BLE Peripheral / GATT Server**. A mobile app or any BLE Central can discover, connect, and interact with the device by reading/writing characteristics organized into services.

```
┌──────────────────────────────────────────────────────────┐
│           Mobile App (BLE Central / GATT Client)         │
└──────────────────────┬───────────────────────────────────┘
                       │  BLE Connection
┌──────────────────────▼───────────────────────────────────┐
│                 ESP32-S3 (BLE Peripheral / GATT Server)  │
│                                                          │
│   main.c ─── Defines services, characteristics           │
│      │        and read/write handlers                    │
│      │                                                   │
│   ble.h ─── Public API (ble_server_init, stop, etc.)     │
│      │                                                   │
│      ├── ble-gap.c ─── Advertising & connection mgmt     │
│      ├── ble-gatts.c ── Service/characteristic registry  │
│      └── ble-gatt.c ── MTU configuration                 │
│                                                          │
│   nvm-driver ── Persistent key-value storage (NVS)       │
│                                                          │
│                 ESP-IDF Bluedroid Stack                  │
└──────────────────────────────────────────────────────────┘
```

## BLE Profile

The device advertises with the name **`AIR-FRYER`** and exposes **3 GATT services** with **9 characteristics** total:

### Sensor Service (`0x00FE`)

| UUID | Name | Permissions | Description |
|------|------|:-----------:|-------------|
| `0xFF01` | Temperature | R | Current temperature in °C |
| `0xFF02` | Temp Threshold | W | Alert threshold (0–100 °C) |
| `0xFF03` | Humidity | R/W | Humidity % (calibration) |

### Control Service (`0x00FD`)

| UUID | Name | Permissions | Description |
|------|------|:-----------:|-------------|
| `0xFF11` | Brightness | R | LED brightness level (0–255) |
| `0xFF12` | Command | W | `0x01` RUN · `0x02` STOP · `0x03` IDLE |
| `0xFF13` | Fan Speed | R/W | Fan speed (0–100 %) |

### Device Info Service (`0x00FC`)

| UUID | Name | Permissions | Description |
|------|------|:-----------:|-------------|
| `0xFF21` | FW Version | R | Firmware version string |
| `0xFF22` | Reset | W | Write `0xAA` to reboot |
| `0xFF23` | Device Name | R/W | BLE device name |

> All characteristics include a **User Description Descriptor** (`0x2901`) automatically. Full UUID map available in [`components/ble/ble_uuids.json`](components/ble/ble_uuids.json).

## Project Structure

```
firmware/
├── CMakeLists.txt              # ESP-IDF project root
├── sdkconfig                   # ESP-IDF configuration
├── main/
│   ├── CMakeLists.txt
│   └── main.c                  # Application entry point & BLE service definitions
└── components/
    ├── ble/                    # BLE abstraction component
    │   ├── include/
    │   │   ├── ble.h           # ★ Public API — only header you need
    │   │   ├── ble-return-code.h
    │   │   ├── ble-gap.h       # (internal)
    │   │   ├── ble-gatt.h      # (internal)
    │   │   └── ble-gatts.h     # (internal)
    │   ├── ble.c               # Server init/stop
    │   ├── ble-gap.c           # Advertising
    │   ├── ble-gatt.c          # MTU
    │   ├── ble-gatts.c         # Service & characteristic registration
    │   ├── ble_uuids.json      # UUID reference map
    │   └── README.md           # Component documentation
    └── nvm-driver/             # NVS key-value storage driver
        ├── include/
        ├── nvm_driver.c
        └── README.md
```

## Prerequisites

| Tool | Version |
|------|---------|
| [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/) | v5.x |
| Python | 3.8+ |
| CMake | 3.16+ |
| Target board | ESP32-S3 |
| USB cable | For flashing & monitoring |

## Adding a New Service

The BLE abstraction makes it straightforward to add services. You only touch `main.c`:

**1. Define data and handlers:**

```c
static uint8_t battery_level = 100;

static int on_read_battery(uint8_t *buf, size_t max_len)
{
  if (max_len < 1) return 0;
  buf[0] = battery_level;
  return 1;
}
```

**2. Define characteristics:**

```c
static ble_characteristic_t battery_chars[] = {
  {
    .uuid = 0xFF31,
    .name = "Battery Level",
    .description = "Battery percentage",
    .size = 1,
    .read = on_read_battery,
    .write = NULL,
  },
};
```

**3. Add the service to the array:**

```c
static ble_service_t my_services[] = {
  // ... existing services ...
  {
    .uuid = 0x00FB,
    .name = "Battery Service",
    .characteristics = battery_chars,
    .characteristic_count = sizeof(battery_chars) / sizeof(battery_chars[0]),
  },
};
```

Build and flash — the new service will appear automatically to BLE clients.

## Components

| Component | Description | Documentation |
|-----------|-------------|:------------:|
| **ble** | BLE GATT Server abstraction over ESP-IDF Bluedroid | [README](components/ble/README.md) |
| **nvm-driver** | Non-volatile memory (NVS) key-value storage | [README](components/nvm-driver/README.md) |

## Authors

- **Pedro Luis Dionísio Fraga** — [pedrodfraga@hotmail.com](mailto:pedrodfraga@hotmail.com)
