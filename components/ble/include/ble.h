/**
 * @file ble.h
 * @author Pedro Luis Dionísio Fraga (pedrodfraga@hotmail.com)
 * @brief BLE Server Abstraction Layer - Simple API for GATT Server
 * @version 0.2
 * @date 2025-07-20
 *
 * @copyright Copyright (c) 2025
 *
 * This module provides a simple abstraction over ESP-IDF BLE stack.
 * Users only need to define characteristics with read/write handlers.
 */

#ifndef BLE_H
#define BLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ble-return-code.h"

/**
 * @brief Read handler function type for characteristics
 *
 * Called when a BLE client reads a characteristic value.
 *
 * @param out_buffer Buffer to fill with data
 * @param max_len Maximum buffer size available
 * @return Number of bytes written to buffer, or negative error code
 *
 * @example
 * int read_temperature(uint8_t *out_buffer, size_t max_len) {
 *     if (max_len < sizeof(int16_t)) return -1;
 *     int16_t temp = get_sensor_temp();
 *     memcpy(out_buffer, &temp, sizeof(temp));
 *     return sizeof(temp);
 * }
 */
typedef int (*ble_char_read_t)(uint8_t *out_buffer, size_t max_len);

/**
 * @brief Write handler function type for characteristics
 *
 * Called when a BLE client writes to a characteristic.
 *
 * @param in_data Pointer to received data
 * @param len Length of received data in bytes
 * @return BLE_CHAR_OK on success, or error code from ble_char_error_t
 *
 * @example
 * ble_char_error_t write_led_state(const uint8_t *in_data, size_t len) {
 *     if (len != 1) return BLE_CHAR_ERR_SIZE;
 *     if (in_data[0] > 1) return BLE_CHAR_ERR_VALUE;
 *     set_led(in_data[0]);
 *     return BLE_CHAR_OK;
 * }
 */
typedef ble_char_error_t (*ble_char_write_t)(const uint8_t *in_data, size_t len);

/**
 * @brief Characteristic definition structure
 *
 * Each characteristic acts as a "data register" that can be read and/or written
 * by BLE clients. Define handlers for read/write operations.
 */
typedef struct
{
  uint16_t uuid;            ///< 16-bit UUID for the characteristic (e.g., 0xFF01)
  const char *name;         ///< Human-readable name for debugging (e.g., "Temperature")
  const char *description;  ///< User description for the characteristic (shown to BLE clients)
  uint8_t size;             ///< Maximum data size in bytes (1, 2, 4, etc.)
  ble_char_read_t read;     ///< Read handler (NULL = write-only characteristic)
  ble_char_write_t write;   ///< Write handler (NULL = read-only characteristic)
} ble_characteristic_t;

/**
 * @brief Service definition structure
 *
 * Each service groups related characteristics under a single UUID.
 * Multiple services can be registered to organize functionality.
 */
typedef struct
{
  uint16_t uuid;                          ///< 16-bit UUID for the service (e.g., 0x00FE)
  const char *name;                       ///< Human-readable name for debugging (e.g., "Sensor Service")
  ble_characteristic_t *characteristics;  ///< Array of characteristic definitions for this service
  size_t characteristic_count;            ///< Number of characteristics in the array
} ble_service_t;

/**
 * @brief BLE server configuration structure
 *
 * Contains all settings needed to initialize the BLE GATT server.
 */
typedef struct
{
  const char *device_name;  ///< BLE device name shown during discovery
  ble_service_t *services;  ///< Array of service definitions
  size_t service_count;     ///< Number of services in the array
} ble_server_config_t;

/**
 * @brief Initialize and start the BLE GATT server
 *
 * Initializes the BLE stack, creates the GATT service with all defined
 * characteristics, and starts advertising.
 *
 * @param config Pointer to server configuration (must remain valid)
 * @return 0 on success, negative value on error
 */
ble_return_code_t ble_server_init(const ble_server_config_t *config);

/**
 * @brief Stop the BLE server and release resources
 *
 * Stops advertising, disconnects any connected clients, and frees resources.
 *
 * @return 0 on success, negative value on error
 */
ble_return_code_t ble_server_stop();

/**
 * @brief Check if a BLE client is currently connected
 *
 * @return true if a client is connected, false otherwise
 */
bool ble_server_is_connected();

#endif  // BLE_H
