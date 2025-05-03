#ifndef I2C_HELPER_H
#define I2C_HELPER_H

#include "esp_log.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

// read data from peripheral into buffer
esp_err_t i2c_read(i2c_master_dev_handle_t dev_handle, uint8_t *buf, size_t bufsize);

// send command to peripheral
esp_err_t i2c_send_cmd(i2c_master_dev_handle_t dev_handle, uint16_t cmd);

esp_err_t i2c_send_data(i2c_master_dev_handle_t dev_handle, uint8_t *data, size_t data_len);

void i2c_master_bus_init(i2c_master_bus_handle_t *bus_handle);
void i2c_dev_init(i2c_master_dev_handle_t *dev_handle, uint16_t dev_addr, i2c_master_bus_handle_t *bus_handle);

void i2c_master_deinit(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle);

#ifdef __cplusplus
}
#endif

#endif
