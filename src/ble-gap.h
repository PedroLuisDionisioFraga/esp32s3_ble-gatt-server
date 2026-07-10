/**
 * @file ble-gap.h
 * @author Pedro Luis Dionísio Fraga (pedrodfraga@hotmail.com)
 * @brief GAP (Generic Access Profile) internal API - manages advertising
 * @version 0.2
 * @date 2025-07-20
 *
 * @copyright Copyright (c) 2025
 *
 * This is an internal header. Users should use ble.h instead.
 */

#ifndef BLE_GAP_H
#define BLE_GAP_H

#include <esp_err.h>
#include <stdint.h>

/**
 * @brief Initialize GAP and configure advertising
 *
 * @param device_name BLE device name for advertising
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_gap_init(const char *device_name);

/**
 * @brief Start BLE advertising
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_gap_start_adv(void);

/**
 * @brief Stop BLE advertising
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_gap_stop_adv(void);

/**
 * @brief Update connection parameters for a connected device
 *
 * @param bda Bluetooth device address
 * @param min_interval Minimum connection interval
 * @param max_interval Maximum connection interval
 * @param latency Slave latency
 * @param timeout Supervision timeout
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ble_gap_update_connection_params(uint8_t *bda, uint16_t min_interval, uint16_t max_interval,
                                           uint16_t latency, uint16_t timeout);

#endif  // BLE_GAP_H
