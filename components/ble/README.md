# 📡 BLE Component

Bluetooth Low Energy (BLE) component for ESP32 project. This component provides a **simple abstraction layer** over the ESP-IDF Bluedroid stack, allowing developers to create BLE GATT servers with minimal boilerplate code.

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
- [Complete Usage Example](#complete-usage-example)
- [Error Codes](#error-codes)
- [Configuration](#configuration)
- [Author](#author)

## 🎯 Overview

This component simplifies BLE development by providing a clean, callback-based API. Instead of dealing with complex ESP-IDF BLE internals, you simply:

1. **Define characteristics** with read/write handlers
2. **Call `ble_server_init()`** with your configuration
3. **Done!** The library handles everything else

## ✨ Features

- **Simple API**: Only include `ble.h` - no need to understand ESP-IDF BLE internals
- **Callback-based**: Define read/write handlers for each characteristic
- **Automatic advertising**: Starts advertising automatically after initialization
- **Auto-reconnect**: Restarts advertising when client disconnects
- **Flexible characteristics**: Support for read-only, write-only, and read/write characteristics
- **Error handling**: Comprehensive error codes for validation
- **Up to 16 characteristics**: Per GATT service

## 🏗️ Architecture

The component is divided into layers that abstract away ESP-IDF complexity:

```
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
|-----------------|--------------------------------------------------------------|
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

### Step 3: Define characteristics

```c
static ble_characteristic_t my_characteristics[] = {
    {
        .uuid = 0xFF01,
        .name = "Temperature",
        .size = 1,
        .read = on_read_temperature,
        .write = NULL,  // Read-only
    },
    {
        .uuid = 0xFF02,
        .name = "Brightness",
        .size = 1,
        .read = NULL,
        .write = on_write_brightness,  // Write-only
    },
};
```

### Step 4: Configure and initialize

```c
static ble_server_config_t config = {
    .device_name = "MY-DEVICE",
    .service_uuid = 0x00FF,
    .characteristics = my_characteristics,
    .characteristic_count = 2,
};

void app_main(void)
{
    int ret = ble_server_init(&config);
    if (ret != 0) {
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
- `BLE_INVALID_CONFIG`: NULL config or device name
- `BLE_INVALID_CHARS`: No characteristics defined
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
    const char *device_name;                // BLE device name (shown during scan)
    uint16_t service_uuid;                  // Primary service UUID (e.g., 0x00FF)
    ble_characteristic_t *characteristics;  // Array of characteristic definitions
    size_t characteristic_count;            // Number of characteristics
} ble_server_config_t;
```

#### `ble_characteristic_t`

```c
typedef struct {
    uint16_t uuid;           // 16-bit UUID (e.g., 0xFF01)
    const char *name;        // Human-readable name for debugging
    uint8_t size;            // Maximum data size in bytes
    ble_char_read_t read;    // Read handler (NULL = write-only)
    ble_char_write_t write;  // Write handler (NULL = read-only)
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

## 🎯 Complete Usage Example

This example shows a complete Air Fryer BLE server implementation (based on `main.c`):

```c
#include <esp_log.h>
#include <string.h>
#include "ble.h"

static const char *TAG = "MAIN";

// ============================================================================
// Data Storage - Your application data
// ============================================================================
static uint8_t temperature_value = 25;  // Temperature in Celsius
static uint8_t led_brightness = 100;    // LED brightness (0-255)
static char device_status[16] = "IDLE";

// ============================================================================
// READ HANDLERS - Called when BLE client reads a characteristic
// ============================================================================

static int on_read_temperature(uint8_t *out_buffer, size_t max_len)
{
    ESP_LOGI(TAG, "Reading temperature: %d°C", temperature_value);
    if (max_len < 1) return 0;
    out_buffer[0] = temperature_value;
    return 1;  // Return number of bytes written
}

static int on_read_brightness(uint8_t *out_buffer, size_t max_len)
{
    ESP_LOGI(TAG, "Reading brightness: %d", led_brightness);
    if (max_len < 1) return 0;
    out_buffer[0] = led_brightness;
    return 1;
}

static int on_read_status(uint8_t *out_buffer, size_t max_len)
{
    size_t len = strlen(device_status);
    if (len > max_len) len = max_len;
    memcpy(out_buffer, device_status, len);
    ESP_LOGI(TAG, "Reading status: %s", device_status);
    return len;
}

// ============================================================================
// WRITE HANDLERS - Called when BLE client writes to a characteristic
// ============================================================================

static ble_char_error_t on_write_brightness(const uint8_t *data, size_t len)
{
    if (len < 1) return BLE_CHAR_ERR_SIZE;

    led_brightness = data[0];
    ESP_LOGI(TAG, "Brightness set to: %d", led_brightness);

    // Apply to hardware here...
    return BLE_CHAR_OK;
}

static ble_char_error_t on_write_command(const uint8_t *data, size_t len)
{
    if (len < 1) return BLE_CHAR_ERR_SIZE;

    uint8_t command = data[0];
    ESP_LOGI(TAG, "Command received: 0x%02X", command);

    switch (command)
    {
        case 0x01: strcpy(device_status, "RUNNING"); break;
        case 0x02: strcpy(device_status, "STOPPED"); break;
        case 0x03: strcpy(device_status, "IDLE");    break;
        default:   return BLE_CHAR_ERR_VALUE;  // Invalid command
    }

    return BLE_CHAR_OK;
}

// ============================================================================
// CHARACTERISTIC DEFINITIONS
// ============================================================================

static ble_characteristic_t my_characteristics[] = {
    {
        .uuid = 0xFF01,
        .name = "Temperature",
        .size = 1,
        .read = on_read_temperature,
        .write = NULL,  // Read-only (no write handler)
    },
    {
        .uuid = 0xFF02,
        .name = "Brightness",
        .size = 1,
        .read = on_read_brightness,
        .write = on_write_brightness,  // Read/Write
    },
    {
        .uuid = 0xFF03,
        .name = "Status",
        .size = 16,
        .read = on_read_status,
        .write = NULL,  // Read-only
    },
    {
        .uuid = 0xFF04,
        .name = "Command",
        .size = 1,
        .read = NULL,  // Write-only (no read handler)
        .write = on_write_command,
    },
};

// ============================================================================
// SERVER CONFIGURATION
// ============================================================================

static ble_server_config_t ble_config = {
    .device_name = "AIR-FRYER",
    .service_uuid = 0x00FF,
    .characteristics = my_characteristics,
    .characteristic_count = sizeof(my_characteristics) / sizeof(my_characteristics[0]),
};

// ============================================================================
// MAIN APPLICATION
// ============================================================================

void app_main(void)
{
    // Initialize BLE server
    int ret = ble_server_init(&ble_config);
    if (ret != 0)
    {
        ESP_LOGE(TAG, "BLE initialization failed with code: %d", ret);
        return;
    }

    ESP_LOGI(TAG, "BLE initialized with %d characteristics", ble_config.characteristic_count);
    ESP_LOGI(TAG, "Device name: %s, Service UUID: 0x%04X",
             ble_config.device_name, ble_config.service_uuid);

    // Main loop
    while (1)
    {
        // Check connection status
        if (ble_server_is_connected())
        {
            ESP_LOGI(TAG, "Client connected | Temp: %d°C | Status: %s",
                     temperature_value, device_status);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

### Characteristic Types Summary

| Type | Read Handler | Write Handler | Use Case |
|------|-------------|---------------|----------|
| **Read-only** | ✅ Defined | ❌ NULL | Sensors, status |
| **Write-only** | ❌ NULL | ✅ Defined | Commands, settings |
| **Read/Write** | ✅ Defined | ✅ Defined | Configurable values |

## ❌ Error Codes

### Server Return Codes (`ble_return_code_t`)

| Code | Value | Description |
|------|-------|-------------|
| `BLE_SUCCESS` | 0 | Operation successful |
| `BLE_GENERIC_ERROR` | 1 | Generic error |
| `BLE_ALREADY_INITIALIZED` | 2 | Server already running |
| `BLE_NOT_INITIALIZED` | 3 | Server not started |
| `BLE_INVALID_CONFIG` | 4 | Invalid configuration |
| `BLE_INVALID_CHARS` | 5 | No characteristics defined |

### Characteristic Handler Codes (`ble_char_error_t`)

| Code | Value | Description | When to Use |
|------|-------|-------------|-------------|
| `BLE_CHAR_OK` | 0 | Success | Data accepted |
| `BLE_CHAR_ERR_SIZE` | 1 | Invalid size | Wrong data length |
| `BLE_CHAR_ERR_VALUE` | 2 | Invalid value | Value out of range |
| `BLE_CHAR_ERR_READONLY` | 3 | Read-only | Write not allowed |
| `BLE_CHAR_ERR_BUSY` | 4 | Device busy | Try again later |

## ⚙️ Configuration

### Default Advertising Parameters

| Parameter | Value | Description |
|-----------|-------|-------------|
| Interval Min | 0x20 (20ms) | Minimum advertising interval |
| Interval Max | 0x40 (40ms) | Maximum advertising interval |
| Type | ADV_TYPE_IND | Connectable undirected |
| Address Type | PUBLIC | Public Bluetooth address |
| Channels | ALL | All advertising channels (37, 38, 39) |

### GATT Configuration

| Parameter | Value | Description |
|-----------|-------|-------------|
| Max MTU | 500 bytes | Maximum Transmission Unit |
| Max Characteristics | 16 | Per service limit |
| App ID | 0 | GATTS application ID |

## 📦 Dependencies

This component requires:

- **ESP-IDF**: v5.0+ (Bluedroid stack)
- **bt**: ESP-IDF Bluetooth component
- **nvm-driver**: My own [open-source](https://github.com/PedroLuisDionisioFraga/esp32-nvm-driver) Non-Volatile Memory driver

### CMakeLists.txt

```cmake
idf_component_register(
    SRCS "ble-gatts.c" "ble-gatt.c" "ble-gap.c" "ble.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES nvm-driver bt
)
```

## 🔑 Service UUIDs

| UUID | Name | Description |
|------|------|-------------|
| `0x00FF` | Air Fryer Service | Main GATT service |
| `0xED58` | Advertising UUID | In advertising data |
| `0xAFBD` | Scan Response UUID | In scan response |
| `0xFF01-0xFF04` | Characteristics | User-defined characteristics |

## 👤 Author

**Pedro Luis Dionísio Fraga**
- Email: pedrodfraga@hotmail.com

## 📄 License

Copyright (c) 2025 - All rights reserved.

---

> **Note**: This component was developed for ESP32 using ESP-IDF v5.x. Make sure you have the development environment properly configured before compiling.

## 📝 References

- [ESP-IDF BLE GATT Server Documentation](https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/bluedroid/ble/gatt_server/tutorial/Gatt_Server_Example_Walkthrough.md)
- [Bluetooth Core Specification](https://www.bluetooth.com/specifications/specs/core-specification-5-3/)
