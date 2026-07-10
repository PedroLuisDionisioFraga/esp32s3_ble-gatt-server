/**
 * @file ble-gatts.c
 * @author Pedro Luis Dionísio Fraga (pedrodfraga@hotmail.com)
 * @brief GATT Server implementation with characteristic abstraction
 * @version 0.2
 * @date 2025-07-20
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "ble-gatts.h"

#include <esp_bt_main.h>
#include <esp_err.h>
#include <esp_gatt_common_api.h>
#include <esp_gatt_defs.h>
#include <esp_gatts_api.h>
#include <esp_log.h>
#include <string.h>

#include "ble-gap.h"

#define GATTS_TAG "BLE_GATTS"

// Constants
#define MAX_SERVICES                8
#define MAX_CHARACTERISTICS_PER_SVC 16
#define HANDLES_PER_CHAR            3  // 1 for char declaration + 1 for char value + 1 for descriptor
#define SERVICE_HANDLE_COUNT        1
#define MAX_MTU_SIZE                500
#define GATTS_APP_ID                0

// Calculate handles: service + (characteristics * handles_per_char)
#define CALC_NUM_HANDLES(char_count) (SERVICE_HANDLE_COUNT + ((char_count) * HANDLES_PER_CHAR))

// Internal structure to track characteristic handles
typedef struct
{
  uint16_t char_handle;       // Characteristic value handle
  uint16_t cccd_handle;       // CCCD handle (for notifications)
  uint16_t descr_handle;      // User description handle
  ble_characteristic_t *def;  // Pointer to user definition
} ble_char_handle_t;

// Internal structure to track service state
typedef struct
{
  uint16_t service_handle;                                      // Service handle assigned by the stack
  ble_service_t *def;                                           // Pointer to user service definition
  ble_char_handle_t char_handles[MAX_CHARACTERISTICS_PER_SVC];  // Characteristic handles for this service
  size_t registered_chars;                                      // Number of characteristics registered so far
  size_t pending_descr_char;                                    // Index of char waiting for descriptor registration
} ble_service_state_t;

// Module state
static ble_service_t *s_services = NULL;
static size_t s_service_count = 0;
static size_t s_current_service_idx = 0;  // Service being created in the sequential registration chain
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_conn_id = 0;
static bool s_is_connected = false;

// Per-service state tracking
static ble_service_state_t s_service_states[MAX_SERVICES];

// Forward declarations
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void handle_char_read(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void handle_char_write(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static ble_char_handle_t *find_char_by_handle(uint16_t handle);
static ble_char_handle_t *find_char_by_descr_handle(uint16_t handle);
static void create_next_service(void);
static void add_char_to_service(ble_service_state_t *state, size_t char_idx);
static void advance_to_next_char_or_service(ble_service_state_t *state);

/**
 * @brief Initialize GATTS with user-defined services and characteristics
 */
esp_err_t ble_gatts_init(ble_service_t *services, size_t count)
{
  if (services == NULL || count == 0)
  {
    ESP_LOGE(GATTS_TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  if (count > MAX_SERVICES)
  {
    ESP_LOGE(GATTS_TAG, "Too many services (max %d)", MAX_SERVICES);
    return ESP_ERR_NO_MEM;
  }

  for (size_t i = 0; i < count; i++)
  {
    if (services[i].characteristic_count > MAX_CHARACTERISTICS_PER_SVC)
    {
      ESP_LOGE(GATTS_TAG,
               "Service '%s' has too many characteristics (max %d)",
               services[i].name,
               MAX_CHARACTERISTICS_PER_SVC);
      return ESP_ERR_NO_MEM;
    }
  }

  s_services = services;
  s_service_count = count;
  s_current_service_idx = 0;

  memset(s_service_states, 0, sizeof(s_service_states));
  for (size_t i = 0; i < count; i++)
  {
    s_service_states[i].def = &services[i];
  }

  esp_err_t ret = esp_ble_gatts_register_callback(gatts_event_handler);
  if (ret != ESP_OK)
  {
    ESP_LOGE(GATTS_TAG, "GATTS callback registration failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = esp_ble_gatts_app_register(GATTS_APP_ID);
  if (ret != ESP_OK)
  {
    ESP_LOGE(GATTS_TAG, "GATTS app register failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = esp_ble_gatt_set_local_mtu(MAX_MTU_SIZE);
  if (ret != ESP_OK)
  {
    ESP_LOGW(GATTS_TAG, "Set MTU failed: %s", esp_err_to_name(ret));
  }

  ESP_LOGI(GATTS_TAG, "GATTS initialized with %d services", count);
  return ESP_OK;
}

/**
 * @brief Deinitialize GATTS
 */
esp_err_t ble_gatts_deinit()
{
  esp_err_t ret = esp_ble_gatts_app_unregister(s_gatts_if);
  if (ret != ESP_OK)
  {
    ESP_LOGE(GATTS_TAG, "App unregister failed: %s", esp_err_to_name(ret));
    return ret;
  }

  s_services = NULL;
  s_service_count = 0;
  s_current_service_idx = 0;
  s_gatts_if = ESP_GATT_IF_NONE;

  return ESP_OK;
}

// ============================================================================
// Service/Characteristic Registration Helpers
// ============================================================================

/**
 * @brief Create the next service in the sequential registration chain
 */
static void create_next_service(void)
{
  if (s_current_service_idx >= s_service_count)
  {
    ESP_LOGI(GATTS_TAG, "All %d services registered successfully", s_service_count);
    return;
  }

  ble_service_t *svc = &s_services[s_current_service_idx];

  esp_gatt_srvc_id_t service_id = {
    .is_primary = true,
    .id =
      {
        .inst_id = s_current_service_idx,
        .uuid =
          {
            .len = ESP_UUID_LEN_16,
            .uuid.uuid16 = svc->uuid,
          },
      },
  };

  uint16_t num_handles = CALC_NUM_HANDLES(svc->characteristic_count);
  ESP_LOGI(GATTS_TAG, "Creating service '%s' (UUID: 0x%04X, %d handles)", svc->name, svc->uuid, num_handles);
  esp_ble_gatts_create_service(s_gatts_if, &service_id, num_handles);
}

/**
 * @brief Add a specific characteristic to a service
 */
static void add_char_to_service(ble_service_state_t *state, size_t char_idx)
{
  ble_characteristic_t *ch = &state->def->characteristics[char_idx];

  esp_gatt_char_prop_t props = 0;
  if (ch->read != NULL)
    props |= ESP_GATT_CHAR_PROP_BIT_READ;
  if (ch->write != NULL)
    props |= ESP_GATT_CHAR_PROP_BIT_WRITE;

  esp_gatt_perm_t perms = 0;
  if (ch->read != NULL)
    perms |= ESP_GATT_PERM_READ;
  if (ch->write != NULL)
    perms |= ESP_GATT_PERM_WRITE;

  esp_bt_uuid_t char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid.uuid16 = ch->uuid,
  };

  esp_err_t ret = esp_ble_gatts_add_char(state->service_handle, &char_uuid, perms, props, NULL, NULL);
  if (ret != ESP_OK)
    ESP_LOGE(GATTS_TAG, "Add char failed: %s", esp_err_to_name(ret));
  else
    ESP_LOGI(GATTS_TAG,
             "Adding characteristic '%s' (UUID: 0x%04X) to service '%s'",
             ch->name,
             ch->uuid,
             state->def->name);
}

/**
 * @brief Advance to the next characteristic in the current service, or move to the next service
 */
static void advance_to_next_char_or_service(ble_service_state_t *state)
{
  if (state->registered_chars < state->def->characteristic_count)
  {
    add_char_to_service(state, state->registered_chars);
  }
  else
  {
    ESP_LOGI(GATTS_TAG,
             "All %d characteristics registered for service '%s'",
             state->registered_chars,
             state->def->name);
    s_current_service_idx++;
    create_next_service();
  }
}

/**
 * @brief Main GATTS event handler
 */
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
  switch (event)
  {
    case ESP_GATTS_REG_EVT:
    {
      if (param->reg.status != ESP_GATT_OK)
      {
        ESP_LOGE(GATTS_TAG, "App registration failed, status %d", param->reg.status);
        return;
      }

      s_gatts_if = gatts_if;
      ESP_LOGI(GATTS_TAG, "App registered, gatts_if %d", gatts_if);

      // Create the first service (starts the sequential chain)
      s_current_service_idx = 0;
      create_next_service();
      break;
    }

    case ESP_GATTS_CREATE_EVT:
    {
      if (param->create.status != ESP_GATT_OK)
      {
        ESP_LOGE(GATTS_TAG, "Service creation failed, status %d", param->create.status);
        return;
      }

      ble_service_state_t *state = &s_service_states[s_current_service_idx];
      state->service_handle = param->create.service_handle;
      ESP_LOGI(GATTS_TAG, "Service '%s' created, handle %d", state->def->name, state->service_handle);

      // Start the service
      esp_ble_gatts_start_service(state->service_handle);

      // Add first characteristic or move to next service if empty
      if (state->def->characteristic_count > 0)
      {
        add_char_to_service(state, 0);
      }
      else
      {
        ESP_LOGI(GATTS_TAG, "Service '%s' has no characteristics, moving to next", state->def->name);
        s_current_service_idx++;
        create_next_service();
      }
      break;
    }

    case ESP_GATTS_ADD_CHAR_EVT:
    {
      if (param->add_char.status != ESP_GATT_OK)
      {
        ESP_LOGE(GATTS_TAG, "Add char failed, status %d", param->add_char.status);
        return;
      }

      ble_service_state_t *state = &s_service_states[s_current_service_idx];
      size_t idx = state->registered_chars;

      if (idx < state->def->characteristic_count)
      {
        state->char_handles[idx].char_handle = param->add_char.attr_handle;
        state->char_handles[idx].def = &state->def->characteristics[idx];

        ESP_LOGI(GATTS_TAG,
                 "Characteristic added: '%s' handle=%d (service '%s')",
                 state->def->characteristics[idx].name,
                 param->add_char.attr_handle,
                 state->def->name);

        // Add User Description descriptor if description is provided
        ble_characteristic_t *current_ch = &state->def->characteristics[idx];
        if (current_ch->description != NULL && current_ch->description[0] != '\0')
        {
          state->pending_descr_char = idx;

          esp_bt_uuid_t descr_uuid = {
            .len = ESP_UUID_LEN_16,
            .uuid.uuid16 = ESP_GATT_UUID_CHAR_DESCRIPTION,  // 0x2901 - Characteristic User Description
          };

          esp_attr_value_t descr_value = {
            .attr_max_len = strlen(current_ch->description),
            .attr_len = strlen(current_ch->description),
            .attr_value = (uint8_t *)current_ch->description,
          };

          esp_err_t ret =
            esp_ble_gatts_add_char_descr(state->service_handle, &descr_uuid, ESP_GATT_PERM_READ, &descr_value, NULL);
          if (ret != ESP_OK)
          {
            ESP_LOGE(GATTS_TAG, "Add char descr failed: %s", esp_err_to_name(ret));
          }
          else
          {
            ESP_LOGI(GATTS_TAG, "Adding descriptor for '%s': \"%s\"", current_ch->name, current_ch->description);
          }
        }
        else
        {
          // No descriptor, advance to next characteristic or service
          state->registered_chars++;
          advance_to_next_char_or_service(state);
        }
      }
      break;
    }

    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
    {
      if (param->add_char_descr.status != ESP_GATT_OK)
      {
        ESP_LOGE(GATTS_TAG, "Add char descriptor failed, status %d", param->add_char_descr.status);
        return;
      }

      ble_service_state_t *state = &s_service_states[s_current_service_idx];

      if (state->pending_descr_char < state->def->characteristic_count)
      {
        state->char_handles[state->pending_descr_char].descr_handle = param->add_char_descr.attr_handle;

        ESP_LOGI(GATTS_TAG,
                 "Descriptor added for '%s' handle=%d (service '%s')",
                 state->def->characteristics[state->pending_descr_char].name,
                 param->add_char_descr.attr_handle,
                 state->def->name);

        state->registered_chars++;
        advance_to_next_char_or_service(state);
      }
      break;
    }

    case ESP_GATTS_CONNECT_EVT:
    {
      s_conn_id = param->connect.conn_id;
      s_is_connected = true;

      ESP_LOGI(GATTS_TAG,
               "Client connected, conn_id=%d, remote=" ESP_BD_ADDR_STR,
               param->connect.conn_id,
               ESP_BD_ADDR_HEX(param->connect.remote_bda));

      // Update connection parameters
      ble_gap_update_connection_params(param->connect.remote_bda, 0x20, 0x40, 0, 400);
      break;
    }

    case ESP_GATTS_DISCONNECT_EVT:
    {
      s_is_connected = false;

      ESP_LOGI(GATTS_TAG, "Client disconnected, reason=0x%x", param->disconnect.reason);

      // Restart advertising
      ble_gap_start_adv();
      break;
    }

    case ESP_GATTS_READ_EVT:
    {
      handle_char_read(gatts_if, param);
      break;
    }

    case ESP_GATTS_WRITE_EVT:
    {
      handle_char_write(gatts_if, param);
      break;
    }

    case ESP_GATTS_MTU_EVT:
    {
      ESP_LOGI(GATTS_TAG, "MTU updated to %d", param->mtu.mtu);
      break;
    }

    case ESP_GATTS_START_EVT:
    {
      ESP_LOGI(GATTS_TAG, "Service started, status %d", param->start.status);
      break;
    }

    default:
    {
      ESP_LOGD(GATTS_TAG, "Unhandled event: %d", event);
      break;
    }
  }
}

/**
 * @brief Handle characteristic read request
 */
static void handle_char_read(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
  // First check if this is a descriptor read
  ble_char_handle_t *ch_descr = find_char_by_descr_handle(param->read.handle);
  if (ch_descr != NULL)
  {
    // This is a read request for a descriptor (User Description)
    esp_gatt_rsp_t rsp = {0};
    rsp.attr_value.handle = param->read.handle;

    if (ch_descr->def->description != NULL)
    {
      size_t total_len = strlen(ch_descr->def->description);
      uint16_t offset = param->read.offset;

      // Handle long read with offset
      if (offset >= total_len)
      {
        // All data has been sent
        rsp.attr_value.len = 0;
      }
      else
      {
        size_t remaining = total_len - offset;
        size_t to_send = remaining;
        if (to_send > sizeof(rsp.attr_value.value))
          to_send = sizeof(rsp.attr_value.value);

        memcpy(rsp.attr_value.value, ch_descr->def->description + offset, to_send);
        rsp.attr_value.len = to_send;
        rsp.attr_value.offset = offset;

        ESP_LOGI(GATTS_TAG,
                 "Sending descriptor for '%s' (offset=%d, len=%d/%d)",
                 ch_descr->def->name,
                 offset,
                 to_send,
                 total_len);
      }
    }
    else
    {
      rsp.attr_value.len = 0;
    }

    esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
    return;
  }

  // Check if this is a characteristic value read
  ble_char_handle_t *ch = find_char_by_handle(param->read.handle);

  if (ch == NULL)
  {
    ESP_LOGW(GATTS_TAG, "Read request for unknown handle %d", param->read.handle);
    esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_INVALID_HANDLE, NULL);
    return;
  }

  ESP_LOGI(GATTS_TAG, "Read request for '%s'", ch->def->name);

  esp_gatt_rsp_t rsp = {0};
  rsp.attr_value.handle = param->read.handle;

  if (ch->def->read == NULL)
  {
    // Write-only characteristic
    ESP_LOGW(GATTS_TAG, "Characteristic '%s' is write-only", ch->def->name);
    esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_READ_NOT_PERMIT, NULL);
    return;
  }

  // Call user's read handler
  int bytes_read = ch->def->read(rsp.attr_value.value, sizeof(rsp.attr_value.value));

  if (bytes_read < 0)
  {
    ESP_LOGE(GATTS_TAG, "Read handler error for '%s'", ch->def->name);
    esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_ERROR, NULL);
    return;
  }

  rsp.attr_value.len = bytes_read;

  ESP_LOGI(GATTS_TAG, "Sending %d bytes for '%s'", bytes_read, ch->def->name);
  ESP_LOG_BUFFER_HEX_LEVEL(GATTS_TAG, rsp.attr_value.value, bytes_read, ESP_LOG_DEBUG);

  esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
}

/**
 * @brief Handle characteristic write request
 */
static void handle_char_write(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
  ble_char_handle_t *ch = find_char_by_handle(param->write.handle);

  if (ch == NULL)
  {
    ESP_LOGW(GATTS_TAG, "Write request for unknown handle %d", param->write.handle);
    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_INVALID_HANDLE, NULL);
    return;
  }

  ESP_LOGI(GATTS_TAG, "Write request for '%s', len=%d", ch->def->name, param->write.len);
  ESP_LOG_BUFFER_HEX_LEVEL(GATTS_TAG, param->write.value, param->write.len, ESP_LOG_DEBUG);

  if (ch->def->write == NULL)
  {
    // Read-only characteristic
    ESP_LOGW(GATTS_TAG, "Characteristic '%s' is read-only", ch->def->name);
    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_WRITE_NOT_PERMIT, NULL);
    return;
  }

  // Call user's write handler
  ble_char_error_t result = ch->def->write(param->write.value, param->write.len);

  // Map error code to GATT status
  esp_gatt_status_t status;
  switch (result)
  {
    case BLE_CHAR_OK:
      status = ESP_GATT_OK;
      ESP_LOGI(GATTS_TAG, "Write to '%s' successful", ch->def->name);
      break;
    case BLE_CHAR_ERR_SIZE:
      status = ESP_GATT_INVALID_ATTR_LEN;
      ESP_LOGW(GATTS_TAG, "Write to '%s' failed: invalid size", ch->def->name);
      break;
    case BLE_CHAR_ERR_VALUE:
      status = ESP_GATT_OUT_OF_RANGE;
      ESP_LOGW(GATTS_TAG, "Write to '%s' failed: value out of range", ch->def->name);
      break;
    case BLE_CHAR_ERR_READONLY:
      status = ESP_GATT_WRITE_NOT_PERMIT;
      ESP_LOGW(GATTS_TAG, "Write to '%s' failed: read-only", ch->def->name);
      break;
    case BLE_CHAR_ERR_BUSY:
      status = ESP_GATT_BUSY;
      ESP_LOGW(GATTS_TAG, "Write to '%s' failed: busy", ch->def->name);
      break;
    default:
      status = ESP_GATT_ERROR;
      ESP_LOGE(GATTS_TAG, "Write to '%s' failed: unknown error %d", ch->def->name, result);
      break;
  }

  // Send response if needed
  if (param->write.need_rsp)
  {
    esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, status, NULL);
  }
}

/**
 * @brief Find characteristic by attribute handle (searches across all services)
 */
static ble_char_handle_t *find_char_by_handle(uint16_t handle)
{
  for (size_t s = 0; s < s_service_count; s++)
  {
    ble_service_state_t *state = &s_service_states[s];
    for (size_t c = 0; c < state->def->characteristic_count; c++)
    {
      if (state->char_handles[c].char_handle == handle)
      {
        return &state->char_handles[c];
      }
    }
  }
  return NULL;
}

/**
 * @brief Find characteristic by descriptor handle (searches across all services)
 */
static ble_char_handle_t *find_char_by_descr_handle(uint16_t handle)
{
  for (size_t s = 0; s < s_service_count; s++)
  {
    ble_service_state_t *state = &s_service_states[s];
    for (size_t c = 0; c < state->def->characteristic_count; c++)
    {
      if (state->char_handles[c].descr_handle != 0 && state->char_handles[c].descr_handle == handle)
      {
        return &state->char_handles[c];
      }
    }
  }
  return NULL;
}

/**
 * @brief Check if a BLE client is currently connected
 */
bool ble_gatts_is_connected(void)
{
  return s_is_connected;
}
