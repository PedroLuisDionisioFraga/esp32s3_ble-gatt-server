/**
 * @file ble-gatt.h
 * @author Pedro Luis Dionísio Fraga (pedrodfraga@hotmail.com)
 * @brief GATT internal definitions (deprecated - kept for compatibility)
 * @version 0.2
 * @date 2025-07-20
 *
 * @copyright Copyright (c) 2025
 *
 * This header is deprecated. Use ble.h for the public API.
 */

#ifndef BLE_GATT_H
#define BLE_GATT_H

#include <esp_err.h>

// Legacy function - kept for compatibility
esp_err_t ble_gatt_init(void);

#endif  // BLE_GATT_H
