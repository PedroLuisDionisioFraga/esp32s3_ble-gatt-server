/**
 * @file ble-gatt.c
 * @author Pedro Luis Dionísio Fraga (pedrodfraga@hotmail.com)
 * @brief
 * @version 0.1
 * @date 2025-06-19
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "ble-gatt.h"

#include <esp_bt_main.h>
#include <esp_err.h>
#include <esp_gatt_common_api.h>
#include <esp_gatt_defs.h>
#include <esp_gatts_api.h>
#include <esp_log.h>
#include <string.h>

#include "ble-gap.h"

#define MAX_MTU_SIZE 500

static const char *GATT_TAG = "BLE_GATT";

esp_err_t ble_gatt_init()
{
  // Set local MTU size
  esp_err_t ret = esp_ble_gatt_set_local_mtu(MAX_MTU_SIZE);
  if (ret)
  {
    ESP_LOGI(GATT_TAG, "Set local MTU failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(GATT_TAG, "GATT initialized successfully");
  return ESP_OK;
}
