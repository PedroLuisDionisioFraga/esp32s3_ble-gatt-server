/**
 * @file ble-return-code.h
 * @author Pedro Luis Dionísio Fraga (pedrodfraga@hotmail.com)
 * @brief BLE characteristic error codes for read/write handlers
 * @version 0.2
 * @date 2025-07-20
 *
 * @copyright Copyright (c) 2025
 *
 */

#ifndef BLE_RETURN_CODE_H
#define BLE_RETURN_CODE_H

/**
 * @brief General BLE return codes
 */
typedef enum
{
  BLE_SUCCESS = 0,          ///< Operation successful
  BLE_GENERIC_ERROR,        ///< Generic error
  BLE_ALREADY_INITIALIZED,  ///< BLE subsystem already initialized
  BLE_NOT_INITIALIZED,      ///< BLE subsystem not initialized
  BLE_INVALID_CONFIG,       ///< Invalid configuration provided
  BLE_INVALID_CHARS,        ///< Invalid characteristics definition
} ble_return_code_t;

/**
 * @brief Characteristic handler return codes
 *
 * Used by write handlers to indicate validation results.
 * These map to GATT error codes internally.
 */
typedef enum
{
  BLE_CHAR_OK = 0,        ///< Operation successful
  BLE_CHAR_ERR_SIZE,      ///< Invalid data size
  BLE_CHAR_ERR_VALUE,     ///< Value out of valid range
  BLE_CHAR_ERR_READONLY,  ///< Attempted write to read-only characteristic
  BLE_CHAR_ERR_BUSY,      ///< Device is busy, try again later
} ble_char_error_t;

#endif  // BLE_RETURN_CODE_H
