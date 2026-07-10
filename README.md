# 📡 BLE GATT SERVER

[![Component Registry](https://components.espressif.com/components/pedroluisdionisiofraga/ble-gatt-server/badge.svg)](https://components.espressif.com/components/pedroluisdionisiofraga/ble-gatt-server)
[![Build Examples](https://img.shields.io/github/actions/workflow/status/PedroLuisDionisioFraga/esp32s3_ble-gatt-server/build.yml?branch=main&label=builds)](https://github.com/PedroLuisDionisioFraga/esp32s3_ble-gatt-server/actions/workflows/build.yml)
![GitHub repo size](https://img.shields.io/github/repo-size/PedroLuisDionisioFraga/esp32s3_ble-gatt-server)
[![License](https://img.shields.io/github/license/PedroLuisDionisioFraga/esp32s3_ble-gatt-server)](LICENSE)
![Targets](https://img.shields.io/badge/targets-ESP32%20%7C%20S2%20%7C%20S3%20%7C%20C3%20%7C%20C5%20%7C%20C6%20%7C%20H2%20%7C%20P4-blue)
![ESP-IDF](https://img.shields.io/badge/ESP--IDF-%E2%89%A56.0-orange)
<!-- ![Last commit](https://img.shields.io/github/last-commit/PedroLuisDionisioFraga/esp32s3_ble-gatt-server) -->

Callback-based BLE GATT server abstraction over the ESP-IDF Bluedroid stack. Define services and characteristics with simple read/write handlers — no Bluedroid boilerplate.

## 📋 Table of Contents

- [Overview](#-overview)
- [Features](#-features)
- [Installation](#-installation)
- [Architecture](#%EF%B8%8F-architecture)
- [Quick Start](#-quick-start)
- [API Reference](#-api-reference)
- [Examples](#-examples)
- [Error Codes](#-error-codes)
- [Configuration](#%EF%B8%8F-configuration)
- [Author](#-author)

## 🎯 Overview

This component simplifies BLE development by providing a clean, callback-based API. Instead of dealing with complex ESP-IDF BLE internals, you simply:

1. **Define characteristics** with read/write handlers
2. **Group them into services**
3. **Call `ble_server_init()`** with your configuration
4. **Done!** The library handles everything else

## ✨ Features

- **Simple API**: Only include `ble.h` - no need to understand ESP-IDF BLE internals
- **Callback-based**: Define read/write handlers for each characteristic
- **Multiple services**: Up to 8 services with up to 16 characteristics each
- **Automatic advertising**: Starts advertising automatically after initialization
- **Auto-reconnect**: Restarts advertising when client disconnects
- **Flexible characteristics**: Support for read-only, write-only, and read/write characteristics
- **User descriptions**: Each characteristic gets a User Description Descriptor (`0x2901`) shown to BLE clients
- **Error handling**: Comprehensive error codes for validation

## 📥 Installation

Add the component to your project from the [ESP Component Registry](https://components.espressif.com/components/pedroluisdionisiofraga/ble-gatt-server):

```bash
idf.py add-dependency "pedroluisdionisiofraga/ble-gatt-server^0.1.0"
```

Or add it manually to your `main/idf_component.yml`:

```yaml
dependencies:
  pedroluisdionisiofraga/ble-gatt-server: "^0.1.0"
```

Bluetooth must be enabled in your project's `sdkconfig.defaults`:

```ini
CONFIG_BT_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED=y
CONFIG_BT_CONTROLLER_ENABLED=y
CONFIG_BT_BLE_ENABLED=y
CONFIG_BT_GATTS_ENABLE=y
CONFIG_BT_BLE_42_FEATURES_SUPPORTED=y
```

> `CONFIG_BT_BLE_42_FEATURES_SUPPORTED` is required on BLE5-capable chips (e.g. ESP32-S3): the component uses the legacy advertising API, which Bluedroid only compiles when this option is set.

## 🏗️ Architecture

The component is divided into layers that abstract away ESP-IDF complexity:

```text
┌─────────────────────────────────────────────────────────────┐
│                    YOUR APPLICATION                         │
│           (Only uses ble.h - simple API)                    │
├─────────────────────────────────────────────────────────────┤
│                      ble.c (Main)                           │
│              Server initialization & control                │
├─────────────────────────────────────────────────────────────┤
│     ble-gap.c     │     ble-gatt.c     │    ble-gatts.c     │
│   (Advertising)   │      (MTU)         │  (Characteristics) │
├─────────────────────────────────────────────────────────────┤
│                 ESP-IDF Bluedroid Stack                     │
└─────────────────────────────────────────────────────────────┘
```

### Internal Modules (You don't need to use these directly)

| Module          | Description                                                  |
| --------------- | ------------------------------------------------------------ |
| **ble.c**       | Main entry point, initializes BT controller and Bluedroid    |
| **ble-gap.c**   | Generic Access Profile - Manages advertising and connections |
| **ble-gatt.c**  | Generic Attribute Profile - MTU configuration                |
| **ble-gatts.c** | GATT Server - Services, characteristics and event handlers   |

## 🚀 Quick Start

### Step 1: Include the header

```c
#include "ble.h"
```

### Step 2: Define read/write handlers

```c
// Read handler - called when client reads the characteristic
static int on_read_temperature(uint8_t *out_buffer, size_t max_len)
{
    if (max_len < 1) return 0;
    out_buffer[0] = get_temperature();  // Your data
    return 1;  // Return bytes written
}

// Write handler - called when client writes to the characteristic
static ble_char_error_t on_write_brightness(const uint8_t *data, size_t len)
{
    if (len < 1) return BLE_CHAR_ERR_SIZE;
    if (data[0] > 100) return BLE_CHAR_ERR_VALUE;
    set_brightness(data[0]);  // Apply the value
    return BLE_CHAR_OK;
}
```

### Step 3: Define characteristics and group them into a service

```c
static ble_characteristic_t my_characteristics[] = {
    {
        .uuid = 0xFF01,
        .name = "Temperature",
        .description = "Current temperature in Celsius",
        .size = 1,
        .read = on_read_temperature,
        .write = NULL,  // Read-only
    },
    {
        .uuid = 0xFF02,
        .name = "Brightness",
        .description = "LED brightness (0-100)",
        .size = 1,
        .read = NULL,
        .write = on_write_brightness,  // Write-only
    },
};

static ble_service_t my_services[] = {
    {
        .uuid = 0x00FF,
        .name = "My Service",
        .characteristics = my_characteristics,
        .characteristic_count = sizeof(my_characteristics) / sizeof(my_characteristics[0]),
    },
};
```

### Step 4: Configure and initialize

```c
static ble_server_config_t config = {
    .device_name = "MY-DEVICE",
    .services = my_services,
    .service_count = sizeof(my_services) / sizeof(my_services[0]),
};

void app_main(void)
{
    ble_return_code_t ret = ble_server_init(&config);
    if (ret != BLE_SUCCESS) {
        ESP_LOGE(TAG, "BLE init failed!");
        return;
    }
    // BLE is now running and advertising!
}
```

## 📚 API Reference

### Main Functions

#### `ble_server_init()`

Initialize and start the BLE GATT server.

```c
ble_return_code_t ble_server_init(const ble_server_config_t *config);
```

**Parameters:**

- `config`: Pointer to server configuration (must remain valid during operation)

**Returns:**

- `BLE_SUCCESS (0)`: Success
- `BLE_ALREADY_INITIALIZED`: Server already running
- `BLE_INVALID_CONFIG`: NULL config, device name, or no services defined
- `BLE_GENERIC_ERROR`: ESP-IDF error

---

#### `ble_server_stop()`

Stop the BLE server and release resources.

```c
ble_return_code_t ble_server_stop();
```

**Returns:**

- `BLE_SUCCESS (0)`: Success
- `BLE_NOT_INITIALIZED`: Server not running

---

#### `ble_server_is_connected()`

Check if a BLE client is currently connected.

```c
bool ble_server_is_connected();
```

**Returns:**

- `true`: Client connected
- `false`: No client connected

---

### Configuration Structures

#### `ble_server_config_t`

```c
typedef struct {
    const char *device_name;  // BLE device name (shown during scan)
    ble_service_t *services;  // Array of service definitions
    size_t service_count;     // Number of services
} ble_server_config_t;
```

#### `ble_service_t`

```c
typedef struct {
    uint16_t uuid;                          // 16-bit service UUID (e.g., 0x00FE)
    const char *name;                       // Human-readable name for debugging
    ble_characteristic_t *characteristics;  // Array of characteristic definitions
    size_t characteristic_count;            // Number of characteristics
} ble_service_t;
```

#### `ble_characteristic_t`

```c
typedef struct {
    uint16_t uuid;            // 16-bit UUID (e.g., 0xFF01)
    const char *name;         // Human-readable name for debugging
    const char *description;  // User description shown to BLE clients (0x2901)
    uint8_t size;             // Maximum data size in bytes
    ble_char_read_t read;     // Read handler (NULL = write-only)
    ble_char_write_t write;   // Write handler (NULL = read-only)
} ble_characteristic_t;
```

---

### Handler Function Types

#### Read Handler

```c
typedef int (*ble_char_read_t)(uint8_t *out_buffer, size_t max_len);
```

**Parameters:**

- `out_buffer`: Buffer to fill with data
- `max_len`: Maximum buffer size

**Returns:**

- Number of bytes written (≥ 0), or negative on error

#### Write Handler

```c
typedef ble_char_error_t (*ble_char_write_t)(const uint8_t *in_data, size_t len);
```

**Parameters:**

- `in_data`: Pointer to received data
- `len`: Length of received data

**Returns:**

- `BLE_CHAR_OK` on success, or error code

### Characteristic Types Summary

| Type | Read Handler | Write Handler | Use Case |
| ------ | ------------- | --------------- | ---------- |
| **Read-only** | ✅ Defined | ❌ NULL | Sensors, status |
| **Write-only** | ❌ NULL | ✅ Defined | Commands, settings |
| **Read/Write** | ✅ Defined | ✅ Defined | Configurable values |

## 🧪 Examples

| Example | Description |
| --------- | ------------- |
| [basic](examples/basic) | Full "air fryer" device: 3 services, 9 characteristics (sensors, control, device info). |

Create a project from the example:

```bash
idf.py create-project-from-example "pedroluisdionisiofraga/ble-gatt-server:basic"
```

## ❌ Error Codes

### Server Return Codes (`ble_return_code_t`)

| Code | Value | Description |
| ------ | ------- | ------------- |
| `BLE_SUCCESS` | 0 | Operation successful |
| `BLE_GENERIC_ERROR` | 1 | Generic error |
| `BLE_ALREADY_INITIALIZED` | 2 | Server already running |
| `BLE_NOT_INITIALIZED` | 3 | Server not started |
| `BLE_INVALID_CONFIG` | 4 | Invalid configuration |
| `BLE_INVALID_CHARS` | 5 | No characteristics defined |

### Characteristic Handler Codes (`ble_char_error_t`)

| Code | Value | Description | When to Use |
| ------ | ------- | ------------- | ------------- |
| `BLE_CHAR_OK` | 0 | Success | Data accepted |
| `BLE_CHAR_ERR_SIZE` | 1 | Invalid size | Wrong data length |
| `BLE_CHAR_ERR_VALUE` | 2 | Invalid value | Value out of range |
| `BLE_CHAR_ERR_READONLY` | 3 | Read-only | Write not allowed |
| `BLE_CHAR_ERR_BUSY` | 4 | Device busy | Try again later |

## ⚙️ Configuration

### Default Advertising Parameters

| Parameter | Value | Description |
| ----------- | ------- | ------------- |
| Interval Min | 0x20 (20ms) | Minimum advertising interval |
| Interval Max | 0x40 (40ms) | Maximum advertising interval |
| Type | ADV_TYPE_IND | Connectable undirected |
| Address Type | PUBLIC | Public Bluetooth address |
| Channels | ALL | All advertising channels (37, 38, 39) |

> The 16-bit UUIDs embedded in the advertising data (`0xED58`) and scan response (`0xAFBD`) are currently fixed in `src/ble-gap.c`.

### GATT Configuration

| Parameter | Value | Description |
| ----------- | ------- | ------------- |
| Max MTU | 500 bytes | Maximum Transmission Unit |
| Max Services | 8 | Per server limit |
| Max Characteristics | 16 | Per service limit |

## 📦 Dependencies

Only ESP-IDF built-in components (no external dependencies):

- **ESP-IDF**: v6.0+ (Bluedroid stack)
- **bt**: ESP-IDF Bluetooth component
- **nvs_flash**: NVS storage, required by the BT controller

## 👤 Author

- [Pedro Luis Dionísio Fraga](https://pedro-fraga.vercel.app/)
- Email: <pedrodfraga@hotmail.com>

## 📄 License

[MIT](LICENSE)

## 📝 References

- [ESP-IDF BLE GATT Server Documentation](https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/bluedroid/ble/gatt_server/tutorial/Gatt_Server_Example_Walkthrough.md)
- [Bluetooth Core Specification](https://www.bluetooth.com/specifications/specs/core-specification-5-3/)
