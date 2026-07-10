/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <string.h>

#include "ble.h"

#define LED_GPIO_PIN 12

static const char *TAG = "MAIN";
static bool s_led_state = false;

// ============================================================================
// Example: Data storage for BLE characteristics
// ============================================================================

// -- Sensor Service data --
static uint8_t temperature_value = 25;  // Temperature in Celsius (read-only)
static uint8_t temp_threshold = 30;     // Temperature alert threshold (write-only)
static uint8_t humidity_value = 60;     // Humidity percentage (read-write)

// -- Control Service data --
static uint8_t led_brightness = 105;     // LED brightness (read-only)
static char device_status[16] = "IDLE";  // Device status string (write-only via command)
static uint8_t fan_speed = 0;            // Fan speed 0-100% (read-write)

// -- Device Info Service data --
static const char firmware_version[] = "1.0.0";  // Firmware version (read-only)
static uint8_t reset_flag = 0;                   // Reset command (write-only)
static char device_name[16] = "AIR-FRYER";       // Device name (read-write)

// ============================================================================
// Sensor Service Handlers
// ============================================================================
static int on_read_temperature(uint8_t *out_buffer, size_t max_len)
{
  ESP_LOGI(TAG, "Reading temperature: %d°C", temperature_value);
  if (max_len < 1)
    return 0;
  out_buffer[0] = temperature_value;
  return 1;
}

static ble_char_error_t on_write_temp_threshold(const uint8_t *data, size_t len)
{
  if (len < 1)
    return BLE_CHAR_ERR_SIZE;
  if (data[0] > 100)
    return BLE_CHAR_ERR_VALUE;

  temp_threshold = data[0];
  ESP_LOGI(TAG, "Temperature threshold set to: %d°C", temp_threshold);
  return BLE_CHAR_OK;
}

static int on_read_humidity(uint8_t *out_buffer, size_t max_len)
{
  ESP_LOGI(TAG, "Reading humidity: %d%%", humidity_value);
  if (max_len < 1)
    return 0;
  out_buffer[0] = humidity_value;
  return 1;
}

static ble_char_error_t on_write_humidity(const uint8_t *data, size_t len)
{
  if (len < 1)
    return BLE_CHAR_ERR_SIZE;
  if (data[0] > 100)
    return BLE_CHAR_ERR_VALUE;

  humidity_value = data[0];
  ESP_LOGI(TAG, "Humidity calibration set to: %d%%", humidity_value);
  return BLE_CHAR_OK;
}

// ============================================================================
// Control Service Handlers
// ============================================================================
static int on_read_brightness(uint8_t *out_buffer, size_t max_len)
{
  ESP_LOGI(TAG, "Reading brightness: %d", led_brightness);
  if (max_len < 1)
    return 0;
  out_buffer[0] = led_brightness;
  return 1;
}

static ble_char_error_t on_write_command(const uint8_t *data, size_t len)
{
  if (len < 1)
    return BLE_CHAR_ERR_SIZE;

  uint8_t command = data[0];
  ESP_LOGI(TAG, "Command received: 0x%02X", command);

  switch (command)
  {
    case 0x01:
      strcpy(device_status, "RUNNING");
      s_led_state = true;
      break;
    case 0x02:
      strcpy(device_status, "STOPPED");
      s_led_state = false;
      break;
    case 0x03:
      strcpy(device_status, "IDLE");
      break;
    default:
      return BLE_CHAR_ERR_VALUE;
  }

  return BLE_CHAR_OK;
}

static int on_read_fan_speed(uint8_t *out_buffer, size_t max_len)
{
  ESP_LOGI(TAG, "Reading fan speed: %d%%", fan_speed);
  if (max_len < 1)
    return 0;
  out_buffer[0] = fan_speed;
  return 1;
}

static ble_char_error_t on_write_fan_speed(const uint8_t *data, size_t len)
{
  if (len < 1)
    return BLE_CHAR_ERR_SIZE;
  if (data[0] > 100)
    return BLE_CHAR_ERR_VALUE;

  fan_speed = data[0];
  ESP_LOGI(TAG, "Fan speed set to: %d%%", fan_speed);
  return BLE_CHAR_OK;
}

// ============================================================================
// Device Info Service Handlers
// ============================================================================
static int on_read_firmware_version(uint8_t *out_buffer, size_t max_len)
{
  size_t len = strlen(firmware_version);
  if (len > max_len)
    len = max_len;
  memcpy(out_buffer, firmware_version, len);
  ESP_LOGI(TAG, "Reading firmware version: %s", firmware_version);
  return len;
}

static ble_char_error_t on_write_reset(const uint8_t *data, size_t len)
{
  if (len < 1)
    return BLE_CHAR_ERR_SIZE;
  if (data[0] != 0xAA)
    return BLE_CHAR_ERR_VALUE;  // Only accept 0xAA as reset confirmation

  reset_flag = data[0];
  ESP_LOGW(TAG, "Reset requested!");
  // Here you would trigger esp_restart()
  esp_restart();
  return BLE_CHAR_OK;
}

static int on_read_device_name(uint8_t *out_buffer, size_t max_len)
{
  size_t len = strlen(device_name);
  if (len > max_len)
    len = max_len;
  memcpy(out_buffer, device_name, len);
  ESP_LOGI(TAG, "Reading device name: %s", device_name);
  return len;
}

static ble_char_error_t on_write_device_name(const uint8_t *data, size_t len)
{
  if (len < 1 || len >= sizeof(device_name))
    return BLE_CHAR_ERR_SIZE;

  memcpy(device_name, data, len);
  device_name[len] = '\0';
  ESP_LOGI(TAG, "Device name changed to: %s", device_name);
  return BLE_CHAR_OK;
}

// ============================================================================
// BLE Characteristic Definitions (grouped by service)
// ============================================================================
static ble_characteristic_t sensor_characteristics[] = {
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
    .name = "Temp Threshold",
    .description = "Temperature alert threshold (0-100°C)",
    .size = 1,
    .read = NULL,  // Write-only
    .write = on_write_temp_threshold,
  },
  {
    .uuid = 0xFF03,
    .name = "Humidity",
    .description = "Humidity percentage (read-write for calibration)",
    .size = 1,
    .read = on_read_humidity,
    .write = on_write_humidity,  // Read-Write
  },
};

static ble_characteristic_t control_characteristics[] = {
  {
    .uuid = 0xFF11,
    .name = "Brightness",
    .description = "LED brightness level (0-255)",
    .size = 1,
    .read = on_read_brightness,
    .write = NULL,  // Read-only
  },
  {
    .uuid = 0xFF12,
    .name = "Command",
    .description = "Control command: 0x01=RUN, 0x02=STOP, 0x03=IDLE",
    .size = 1,
    .read = NULL,  // Write-only
    .write = on_write_command,
  },
  {
    .uuid = 0xFF13,
    .name = "Fan Speed",
    .description = "Fan speed percentage (0-100%)",
    .size = 1,
    .read = on_read_fan_speed,
    .write = on_write_fan_speed,  // Read-Write
  },
};

static ble_characteristic_t device_info_characteristics[] = {
  {
    .uuid = 0xFF21,
    .name = "FW Version",
    .description = "Firmware version string",
    .size = 8,
    .read = on_read_firmware_version,
    .write = NULL,  // Read-only
  },
  {
    .uuid = 0xFF22,
    .name = "Reset",
    .description = "Write 0xAA to reset device",
    .size = 1,
    .read = NULL,  // Write-only
    .write = on_write_reset,
  },
  {
    .uuid = 0xFF23,
    .name = "Device Name",
    .description = "BLE device name (read-write)",
    .size = 16,
    .read = on_read_device_name,
    .write = on_write_device_name,  // Read-Write
  },
};

// ============================================================================
// BLE Service Definitions
// ============================================================================
static ble_service_t my_services[] = {
  {
    .uuid = 0x00FE,
    .name = "Sensor Service",
    .characteristics = sensor_characteristics,
    .characteristic_count = sizeof(sensor_characteristics) / sizeof(sensor_characteristics[0]),
  },
  {
    .uuid = 0x00FD,
    .name = "Control Service",
    .characteristics = control_characteristics,
    .characteristic_count = sizeof(control_characteristics) / sizeof(control_characteristics[0]),
  },
  {
    .uuid = 0x00FC,
    .name = "Device Info Service",
    .characteristics = device_info_characteristics,
    .characteristic_count = sizeof(device_info_characteristics) / sizeof(device_info_characteristics[0]),
  },
};

// ============================================================================
// BLE Configuration
// ============================================================================
static ble_server_config_t ble_config = {
  .device_name = "ESP32-S3 BLE GATT Server",
  .services = my_services,
  .service_count = sizeof(my_services) / sizeof(my_services[0]),
};

void configure_led(void)
{
  /* Configure the GPIO pin for the LED */
  gpio_config_t io_conf = {.pin_bit_mask = (1ULL << LED_GPIO_PIN),
                           .mode = GPIO_MODE_OUTPUT,
                           .pull_up_en = GPIO_PULLUP_DISABLE,
                           .pull_down_en = GPIO_PULLDOWN_DISABLE,
                           .intr_type = GPIO_INTR_DISABLE};
  gpio_config(&io_conf);
}

void blink_led(void)
{
  gpio_set_level(LED_GPIO_PIN, s_led_state ? 1 : 0);
}

void app_main(void)
{
  /* Configure the peripheral according to the LED type */
  configure_led();

  // Initialize BLE with our characteristic configuration
  // The abstraction hides all ESP-IDF complexity!
  int ret = ble_server_init(&ble_config);
  if (ret != 0)
  {
    ESP_LOGE(TAG, "BLE initialization failed!");
    return;
  }

  ESP_LOGI(TAG, "BLE initialized with %d services", ble_config.service_count);
  ESP_LOGI(TAG, "Device name: %s", ble_config.device_name);

  while (1)
  {
    // Update LED based on state (controlled via BLE)
    blink_led();

    // Simulate temperature changes with a simple oscillation pattern (20-35°C)
    static uint8_t temp_direction = 1;  // 1 = rising, 0 = falling
    if (temp_direction)
    {
      temperature_value++;
      if (temperature_value >= 35)
        temp_direction = 0;
    }
    else
    {
      temperature_value--;
      if (temperature_value <= 20)
        temp_direction = 1;
    }

    // Log connection status
    if (ble_server_is_connected())
    {
      ESP_LOGI(TAG, "Client connected | Temp: %d°C | Status: %s", temperature_value, device_status);
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}
