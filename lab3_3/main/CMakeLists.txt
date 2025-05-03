idf_component_register(SRCS "lab3_3.cpp" "RGBLCD.cpp" "i2c_helper.c"
                    REQUIRES esp_driver_i2c freertos
                    INCLUDE_DIRS ".")
