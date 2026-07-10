idf_component_register(SRCS "src/ble-gatts.c" "src/ble-gatt.c" "src/ble-gap.c" "src/ble.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES bt nvs_flash)
