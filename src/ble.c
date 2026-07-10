/**
 * @file ble.c
 * @author Pedro Luis Dionísio Fraga (pedrodfraga@hotmail.com)
 * @brief BLE Server Implementation - Abstraction Layer over ESP-IDF BLE stack
 * @version 0.2
 * @date 2025-07-20
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "ble.h"

#include <esp_bt.h>  // Implements BT controller and VHCI configuration procedures from the host side.
#include <esp_bt_device.h>
#include <esp_bt_main.h>      // Implements initialization and enabling of the Bluedroid stack.
#include <esp_gap_ble_api.h>  // Implements GAP configuration such as advertising and connection parameters.
#include <esp_gatt_common_api.h>
#include <esp_gatt_defs.h>  // Implements GATT server API definitions.
#include <esp_log.h>
#include <nvs_flash.h>  // NVS is required by the BT controller for PHY calibration data.
#include <string.h>

#include "ble-gap.h"
#include "ble-gatt.h"
#include "ble-gatts.h"
#include "ble-return-code.h"

static const char *TAG = "BLE";

// Server state
static bool s_initialized = false;
static const ble_server_config_t *s_config = NULL;

/**
 * @brief Initialize and start the BLE GATT server
 */
ble_return_code_t ble_server_init(const ble_server_config_t *config)
{
  if (s_initialized)
  {
    ESP_LOGW(TAG, "BLE server already initialized");
    return BLE_ALREADY_INITIALIZED;
  }

  if (config == NULL || config->device_name == NULL)
  {
    ESP_LOGE(TAG, "Invalid configuration");
    return BLE_INVALID_CONFIG;
  }

  if (config->services == NULL || config->service_count == 0)
  {
    ESP_LOGE(TAG, "No services defined");
    return BLE_INVALID_CONFIG;
  }

  s_config = config;
  esp_err_t ret;

  ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ret = esp_bt_controller_init(&bt_cfg);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "BT controller init failed: %s", esp_err_to_name(ret));
    return BLE_GENERIC_ERROR;
  }

  ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "BT controller enable failed: %s", esp_err_to_name(ret));
    return BLE_GENERIC_ERROR;
  }

  ret = esp_bluedroid_init();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Bluedroid init failed: %s", esp_err_to_name(ret));
    return BLE_GENERIC_ERROR;
  }

  ret = esp_bluedroid_enable();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Bluedroid enable failed: %s", esp_err_to_name(ret));
    return BLE_GENERIC_ERROR;
  }

  ret = ble_gatts_init(config->services, config->service_count);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "GATTS init failed: %s", esp_err_to_name(ret));
    return BLE_GENERIC_ERROR;
  }

  ret = ble_gap_init(config->device_name);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "GAP init failed: %s", esp_err_to_name(ret));
    return BLE_GENERIC_ERROR;
  }

  s_initialized = true;
  ESP_LOGI(TAG, "BLE server initialized with %d services", config->service_count);

  return BLE_SUCCESS;
}

/**
 * @brief Stop the BLE server and release resources
 */
ble_return_code_t ble_server_stop()
{
  if (!s_initialized)
  {
    ESP_LOGW(TAG, "BLE server not initialized");
    return BLE_NOT_INITIALIZED;
  }

  esp_err_t ret;

  ret = ble_gap_stop_adv();
  if (ret != ESP_OK)
  {
    ESP_LOGW(TAG, "Failed to stop advertising: %s", esp_err_to_name(ret));
  }

  ret = esp_bluedroid_disable();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Bluedroid disable failed: %s", esp_err_to_name(ret));
    return BLE_GENERIC_ERROR;
  }

  ret = esp_bluedroid_deinit();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "Bluedroid deinit failed: %s", esp_err_to_name(ret));
    return BLE_GENERIC_ERROR;
  }

  ret = esp_bt_controller_disable();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "BT controller disable failed: %s", esp_err_to_name(ret));
    return BLE_GENERIC_ERROR;
  }

  ret = esp_bt_controller_deinit();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "BT controller deinit failed: %s", esp_err_to_name(ret));
    return BLE_GENERIC_ERROR;
  }

  s_initialized = false;
  s_config = NULL;

  ESP_LOGI(TAG, "BLE server stopped");
  return BLE_SUCCESS;
}

/**
 * @brief Check if a BLE client is currently connected
 */
bool ble_server_is_connected()
{
  return ble_gatts_is_connected();
}
