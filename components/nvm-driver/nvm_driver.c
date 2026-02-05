/**
 * @file nvm_driver.c
 * @author Pedro Luis Dionísio Fraga (pedrodfraga@hotmail.com)
 * @brief
 * @version 0.1
 * @date 2025-05-28
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "nvm_driver.h"

#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <inttypes.h>
#include <nvs.h>
#include <nvs_flash.h>

#define DEBUG 1

// Macros
#define DEBUG_PRINT_ERR(fmt, ...)        \
  do                                     \
  {                                      \
    if (DEBUG)                           \
      ESP_LOGE(TAG, fmt, ##__VA_ARGS__); \
  } while (0)

#define LM75A_CHECK_INSTANCE(evaluated, ret)           \
  if (!evaluated)                                      \
  {                                                    \
    DEBUG_PRINT_ERR("Error: LM75A_ERR_INVALID_PARAM"); \
    return ret;                                        \
  }

#define UNUSED(x) (void)(x)

static const char *TAG = "NVM_DRIVER";

static nvs_handle_t handle;

void nvm_init()
{
  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    DEBUG_PRINT_ERR("NVS partition was truncated or new version found, erasing and reinitializing NVS");
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  ESP_LOGI(TAG, "NVS initialized successfully");
}

void nvm_write(const char *key, uint32_t value)
{
  // Open NVS handle
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
  if (err != ESP_OK)
  {
    DEBUG_PRINT_ERR("Error (%s) opening NVS handle!", esp_err_to_name(err));
    return;
  }

  // Write value
  err = nvs_set_u32(handle, key, value);
  if (err != ESP_OK)
  {
    DEBUG_PRINT_ERR("Error (%s) writing value to NVS!", esp_err_to_name(err));
    return;
  }
  ESP_LOGI(TAG, "Value (%" PRId32 ") written to NVS with key '%s'", value, key);

  // Commit changes
  err = nvs_commit(handle);
  if (err != ESP_OK)
  {
    DEBUG_PRINT_ERR("Error (%s) committing changes to NVS!", esp_err_to_name(err));
    return;
  }
  ESP_LOGI(TAG, "Changes committed successfully to NVS");
  ESP_LOGI(TAG, "Value (%lu) successfully written to NVS with key '%s'", value, key);

  // Close NVS handle
  nvs_close(handle);
}

void nvm_read(const char *key, uint32_t *value)
{
  // Open NVS handle
  esp_err_t err = nvs_open("storage", NVS_READONLY, &handle);
  if (err != ESP_OK)
  {
    DEBUG_PRINT_ERR("Error (%s) opening NVS handle!", esp_err_to_name(err));
    return;
  }

  // Read value
  err = nvs_get_u32(handle, key, value);
  if (err == ESP_ERR_NVS_NOT_FOUND)
  {
    ESP_LOGW(TAG, "Key '%s' not found in NVS", key);
    *value = 0;  // Default value if not found
  }
  else if (err != ESP_OK)
  {
    DEBUG_PRINT_ERR("Error (%s) reading value from NVS!", esp_err_to_name(err));
    return;
  }
  ESP_LOGI(TAG, "Value read from NVS: %" PRId32, *value);

  // Close NVS handle
  nvs_close(handle);
}

void nvm_erase(const char *key)
{
  // Open NVS handle
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
  if (err != ESP_OK)
  {
    DEBUG_PRINT_ERR("Error (%s) opening NVS handle!", esp_err_to_name(err));
    return;
  }

  // Erase key
  err = nvs_erase_key(handle, key);
  if (err != ESP_OK)
  {
    DEBUG_PRINT_ERR("Error (%s) erasing key '%s' from NVS!", esp_err_to_name(err), key);
    return;
  }
  ESP_LOGI(TAG, "Key '%s' erased from NVS", key);

  // Commit changes
  err = nvs_commit(handle);
  if (err != ESP_OK)
  {
    DEBUG_PRINT_ERR("Error (%s) committing changes to NVS!", esp_err_to_name(err));
    return;
  }
  ESP_LOGI(TAG, "Changes committed successfully to NVS");
  ESP_LOGI(TAG, "Key '%s' successfully erased from NVS", key);

  // Close NVS handle
  nvs_close(handle);
}

void nvm_erase_all()
{
  // Open NVS handle
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
  if (err != ESP_OK)
  {
    DEBUG_PRINT_ERR("Error (%s) opening NVS handle!", esp_err_to_name(err));
    return;
  }

  // Erase all keys
  err = nvs_erase_all(handle);
  if (err != ESP_OK)
  {
    DEBUG_PRINT_ERR("Error (%s) erasing all keys from NVS!", esp_err_to_name(err));
    return;
  }
  ESP_LOGI(TAG, "All keys erased from NVS");

  // Commit changes
  err = nvs_commit(handle);
  if (err != ESP_OK)
  {
    DEBUG_PRINT_ERR("Error (%s) committing changes to NVS!", esp_err_to_name(err));
    return;
  }
  ESP_LOGI(TAG, "Changes committed successfully to NVS");
  ESP_LOGI(TAG, "All keys successfully erased from NVS");

  // Close NVS handle
  nvs_close(handle);
}
