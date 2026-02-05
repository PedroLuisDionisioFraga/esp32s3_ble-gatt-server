# Non-volatile Memory (NVM) Driver
This is a driver for write, read, and erase operations on non-volatile memory (NVM) using the ESP-IDF NVS (Non-Volatile Storage) library.

## Understanding NVM Basics
NVM is a type of memory that retains its data even when the power is turned off. It is commonly used to store configuration settings, calibration data, and other persistent information that needs to be preserved across reboots.

## Keys and Values
In NVM, data is stored in key-value pairs. Each key is a unique identifier for a value, and each value can be of various types. The keys are case-sensitive and must be unique within the NVM namespace.\
The maximum **key** length is currently 15 characters.\
**Values** can have one of the following types:
- integer types: uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t
- zero-terminated string
- variable length binary data, like blob (binary large object).

## References
- [ESP-IDF documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html#_CPPv423nvs_open_from_partitionPKcPKc15nvs_open_mode_tP12nvs_handle_t)
- [ESP-IDF NVS Example](https://github.com/espressif/esp-idf/tree/master/examples/storage/nvs/nvs_rw_value)
