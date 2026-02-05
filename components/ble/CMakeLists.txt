idf_component_register(SRCS "ble-gatts.c" "ble-gatt.c" "ble-gap.c" "ble.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES nvm-driver bt)
