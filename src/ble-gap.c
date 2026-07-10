/**
 * @file ble-gap.c
 * @author Pedro Luis Dionísio Fraga (pedrodfraga@hotmail.com)
 * @brief
 * @version 0.1
 * @date 2025-06-19
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "ble-gap.h"

#include <esp_gap_ble_api.h>  // Implements GATT Server configuration such as creating services and characteristics.
#include <esp_log.h>
#include <string.h>

#define RAW_ADV_DATA_SERVICE_UUID      0xED58  // Example service UUID
#define RAW_ADV_DATA_SIZE              31
#define RAW_SCAN_RSP_DATA_SERVICE_UUID 0xAFBD  // Example scan response service UUID
#define RAW_SCAN_RSP_DATA_SIZE         31
#define ADV_CONFIG_FLAG                (1 << 0)
#define SCAN_RSP_CONFIG_FLAG           (1 << 1)

static const char *TAG_GAP = "BLE_GAP";

static uint8_t adv_config_done = 0;

static uint8_t *s_raw_adv_data = NULL;
static esp_ble_adv_params_t *s_adv_params = NULL;
static uint8_t *s_raw_scan_rsp_data = NULL;

static bool s_is_adv_data_set = false;

static uint16_t ble_gap_config_adv(uint16_t service_uuid, const char *local_name, size_t size)
{
  //* Advertise data
  if (s_raw_adv_data != NULL)
    return 0;

  // Buffer temporário para montar os dados
  uint8_t adv_data[RAW_ADV_DATA_SIZE];
  int idx = 0;

  //* Flags
  adv_data[idx++] = 0x02;
  adv_data[idx++] = ESP_BLE_AD_TYPE_FLAG;
  adv_data[idx++] = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

  //* TX Power Level
  adv_data[idx++] = 0x02;
  adv_data[idx++] = ESP_BLE_AD_TYPE_TX_PWR;
  adv_data[idx++] = 0xFA;

  //* Service UUID (16 bit)
  adv_data[idx++] = 0x03;
  adv_data[idx++] = ESP_BLE_AD_TYPE_16SRV_CMPL;
  adv_data[idx++] = (uint8_t)(service_uuid & 0xFF);         // LSB
  adv_data[idx++] = (uint8_t)((service_uuid >> 8) & 0xFF);  // MSB

  //* Appearance
  adv_data[idx++] = 0x02;
  adv_data[idx++] = ESP_BLE_AD_TYPE_APPEARANCE;
  adv_data[idx++] = ESP_BLE_APPEARANCE_GENERIC_COMPUTER;

  //* Complete Local Name
  uint8_t max_name_len = RAW_ADV_DATA_SIZE - idx - 2;  // 2 bytes for length and type
  uint8_t used_name_len = (size > max_name_len) ? max_name_len : size;
  adv_data[idx++] = used_name_len + 1;  // Size of the name + 1 byte for type
  adv_data[idx++] = ESP_BLE_AD_TYPE_NAME_CMPL;
  memcpy(&adv_data[idx], local_name, used_name_len);
  idx += used_name_len;

  // Allocate and copy the advertising data
  s_raw_adv_data = (uint8_t *)calloc(idx, sizeof(uint8_t));
  memcpy(s_raw_adv_data, adv_data, idx);

  //* Advertise parameters
  if (s_adv_params != NULL)
    return 0;

  s_adv_params = (esp_ble_adv_params_t *)calloc(1, sizeof(esp_ble_adv_params_t));
  if (s_adv_params == NULL)
  {
    ESP_LOGE(TAG_GAP, "Failed to allocate memory for advertising parameters");
    return 0;
  }

  s_adv_params->adv_int_min = 0x20;                                     // Minimum advertising interval (20 ms)
  s_adv_params->adv_int_max = 0x40;                                     // Maximum advertising interval (40 ms)
  s_adv_params->adv_type = ADV_TYPE_IND;                                // Connectable undirected advertising
  s_adv_params->own_addr_type = BLE_ADDR_TYPE_PUBLIC;                   // Public address type
  s_adv_params->channel_map = ADV_CHNL_ALL;                             // All channels
  s_adv_params->adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;  // Allow scan and connect from any device

  return idx;
}

static uint16_t ble_gap_init_scan_rsp_data(uint16_t service_uuid, const char *local_name, size_t name_len)
{
  if (s_raw_scan_rsp_data != NULL)
    return 0;

  uint8_t scan_rsp_data[RAW_SCAN_RSP_DATA_SIZE];
  int idx = 0;

  //* Flags
  scan_rsp_data[idx++] = 0x02;
  scan_rsp_data[idx++] = ESP_BLE_AD_TYPE_FLAG;
  scan_rsp_data[idx++] = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

  //* TX Power Level
  scan_rsp_data[idx++] = 0x02;
  scan_rsp_data[idx++] = ESP_BLE_AD_TYPE_TX_PWR;
  scan_rsp_data[idx++] = 0xAA;

  //* Service UUID (16 bit)
  scan_rsp_data[idx++] = 0x03;
  scan_rsp_data[idx++] = ESP_BLE_AD_TYPE_16SRV_CMPL;
  scan_rsp_data[idx++] = (uint8_t)(service_uuid & 0xFF);         // LSB
  scan_rsp_data[idx++] = (uint8_t)((service_uuid >> 8) & 0xFF);  // MSB

  //* Complete Local Name
  uint8_t max_name_len = RAW_SCAN_RSP_DATA_SIZE - idx - 2;
  uint8_t used_name_len = (name_len > max_name_len) ? max_name_len : name_len;
  scan_rsp_data[idx++] = 1 + used_name_len;
  scan_rsp_data[idx++] = ESP_BLE_AD_TYPE_NAME_CMPL;
  memcpy(&scan_rsp_data[idx], local_name, used_name_len);
  idx += used_name_len;

  // Allocate and copy the scan response data
  s_raw_scan_rsp_data = (uint8_t *)calloc(idx, sizeof(uint8_t));
  memcpy(s_raw_scan_rsp_data, scan_rsp_data, idx);

  return idx;
}

static void ble_free_scan_rsp_data(void)
{
  if (s_raw_scan_rsp_data == NULL)
    return;

  free(s_raw_scan_rsp_data);
  s_raw_scan_rsp_data = NULL;
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
  // Handle GAP events here
  switch (event)
  {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
    {
      ESP_LOGI(TAG_GAP, "Advertising data set successfully");

      adv_config_done &= ~ADV_CONFIG_FLAG;  // Clear the flag
      if (0 == adv_config_done)
        ble_gap_start_adv();

      break;
    }
    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
    {
      ESP_LOGI(TAG_GAP, "Scan response data set successfully");

      adv_config_done &= ~SCAN_RSP_CONFIG_FLAG;  // Clear the flag
      if (0 == adv_config_done)
        ble_gap_start_adv();

      break;
    }
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
    {
      if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        ESP_LOGI(TAG_GAP, "Advertising start failed");
      else
        ESP_LOGI(TAG_GAP, "Advertising start successfully");

      break;
    }
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
    {
      if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        ESP_LOGI(TAG_GAP, "Advertising stop failed");
      else
        ESP_LOGI(TAG_GAP, "Stop adv successfully");

      break;
    }
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
    {
      ESP_LOGI(TAG_GAP,
               "update connection params status = %d, conn_int = %d, latency = %d, "
               "timeout = %d",
               param->update_conn_params.status,
               param->update_conn_params.conn_int,
               param->update_conn_params.latency,
               param->update_conn_params.timeout);
      break;
    }
    case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
    {
      if (param->pkt_data_length_cmpl.status != ESP_BT_STATUS_SUCCESS)
        ESP_LOGI(TAG_GAP, "Set packet length failed");
      else
        ESP_LOGI(TAG_GAP,
                 "Set packet length successfully, tx_len = %d, rx_len = %d",
                 param->pkt_data_length_cmpl.params.tx_len,
                 param->pkt_data_length_cmpl.params.rx_len);

      break;
    }
    default:
    {
      ESP_LOGI(TAG_GAP, "Unhandled GAP event: %d", event);
      break;
    }
  }
}

esp_err_t ble_gap_init(const char *device_name)
{
  esp_err_t ret;

  ret = esp_ble_gap_register_callback(gap_event_handler);
  if (ret)
  {
    ESP_LOGI(TAG_GAP, "GAP callback registration failed: %s", esp_err_to_name(ret));
    return ret;
  }

  // Set device name
  ret = esp_ble_gap_set_device_name(device_name);
  if (ret)
  {
    ESP_LOGI(TAG_GAP, "Setting device name failed: %s", esp_err_to_name(ret));
    return ret;
  }

  uint16_t raw_adv_data_size = ble_gap_config_adv(RAW_ADV_DATA_SERVICE_UUID, device_name, strlen(device_name));
  ESP_LOGI(TAG_GAP, "Advertising data size: %d", raw_adv_data_size);
  ret = esp_ble_gap_config_adv_data_raw(s_raw_adv_data, raw_adv_data_size);
  if (ret)
  {
    ESP_LOGI(TAG_GAP, "Configuring advertising data failed: %s", esp_err_to_name(ret));
    return ret;
  }

  uint16_t scab_rsp_data_size =
    ble_gap_init_scan_rsp_data(RAW_SCAN_RSP_DATA_SERVICE_UUID,
                               device_name,
                               strlen(device_name));  // Example service UUID and device name
  ESP_LOGI(TAG_GAP, "Scan response data size: %d", scab_rsp_data_size);
  ret = esp_ble_gap_config_scan_rsp_data_raw(s_raw_scan_rsp_data, scab_rsp_data_size);
  if (ret)
  {
    ESP_LOGI(TAG_GAP, "Configuring scan response data failed: %s", esp_err_to_name(ret));
    return ret;
  }

  adv_config_done |= ADV_CONFIG_FLAG | SCAN_RSP_CONFIG_FLAG;

  ESP_LOGI(TAG_GAP, "GAP initialized successfully with device name: %s", device_name);
  return ESP_OK;
}

static void ble_gap_free_adv_data()
{
  // Free advertising data
  if (s_raw_adv_data == NULL)
    return;

  free(s_raw_adv_data);
  s_raw_adv_data = NULL;

  // Free advertising parameters
  if (s_adv_params == NULL)
    return;

  free(s_adv_params);
  s_adv_params = NULL;
}

esp_err_t ble_gap_start_adv()
{
  if (s_raw_adv_data == NULL || s_adv_params == NULL)
  {
    ESP_LOGE(TAG_GAP, "Advertising data or parameters not set");
    return ESP_ERR_INVALID_STATE;
  }

  esp_err_t ret = esp_ble_gap_start_advertising(s_adv_params);
  if (ret)
  {
    ESP_LOGE(TAG_GAP, "Starting advertising failed: %s", esp_err_to_name(ret));
    return ret;
  }

  return ESP_OK;
}

esp_err_t ble_gap_update_connection_params(uint8_t *bda, uint16_t min_interval, uint16_t max_interval, uint16_t latency,
                                           uint16_t timeout)
{
  esp_err_t ret;
  esp_ble_conn_update_params_t conn_params = {
    .min_int = min_interval,
    .max_int = max_interval,
    .latency = latency,
    .timeout = timeout,
  };
  memcpy(conn_params.bda, bda, ESP_BD_ADDR_LEN);

  ret = esp_ble_gap_update_conn_params(&conn_params);
  if (ret)
  {
    ESP_LOGE(TAG_GAP, "Updating connection parameters failed: %s", esp_err_to_name(ret));
    return ret;
  }

  return ESP_OK;
}

esp_err_t ble_gap_stop_adv(void)
{
  esp_err_t ret = esp_ble_gap_stop_advertising();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG_GAP, "Stop advertising failed: %s", esp_err_to_name(ret));
    return ret;
  }

  // Free allocated resources
  ble_gap_free_adv_data();
  ble_free_scan_rsp_data();

  return ESP_OK;
}
