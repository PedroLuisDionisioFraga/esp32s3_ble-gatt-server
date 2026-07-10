# Basic Example — BLE GATT Server

Full "air fryer" device built with the [`pedroluisdionisiofraga/ble-gatt-server`](../../) component. Exposes sensor data, device control, and device information as BLE services — a mobile app or any BLE Central can discover, connect, and read/write characteristics.

## Architecture Overview

```text
┌──────────────────────────────────────────────────────────┐
│           Mobile App (BLE Central / GATT Client)         │
└──────────────────────┬───────────────────────────────────┘
                       │  BLE Connection
┌──────────────────────▼───────────────────────────────────┐
│                 ESP32 (BLE Peripheral / GATT Server)     │
│                                                          │
│   main.c ─── Defines services, characteristics           │
│      │        and read/write handlers                    │
│      │                                                   │
│   ble-gatt-server component ── Advertising, GATT,        │
│                                connection management     │
│                                                          │
│                 ESP-IDF Bluedroid Stack                  │
└──────────────────────────────────────────────────────────┘
```

The example builds against the component at the repo root via `override_path` in [main/idf_component.yml](main/idf_component.yml); consumers who copy it get the published Registry version instead.

## BLE Profile

The device advertises with the name **`ESP32-S3 BLE GATT Server`** and exposes **3 GATT services** with **9 characteristics** total:

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

> All characteristics include a **User Description Descriptor** (`0x2901`) automatically. Full UUID map available in [`ble_uuids.json`](ble_uuids.json).

## Build & Flash

From this directory:

```bash
idf.py set-target esp32s3   # or esp32, esp32c3, esp32c6, esp32h2
idf.py build flash monitor
```

Tested on **ESP32-S3**. The `0x01` RUN / `0x02` STOP commands toggle an LED on GPIO 12.

## Adding a New Service

You only touch `main/main.c`:

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

## Authors

- **Pedro Luis Dionísio Fraga** — [pedrodfraga@hotmail.com](mailto:pedrodfraga@hotmail.com)
