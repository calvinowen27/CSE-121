#include "i2c_helper.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"

#define I2C_MASTER_SCL_IO           8       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           10       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          100000 /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       2000

static const char *TAG = "i2c_helper";

// read data from peripheral into buffer
esp_err_t i2c_read(i2c_master_dev_handle_t dev_handle, uint8_t *buf, size_t bufsize)
{
	return i2c_master_receive(
        dev_handle,
        buf, bufsize,
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

// send command to peripheral
esp_err_t i2c_send_cmd(i2c_master_dev_handle_t dev_handle, uint16_t cmd)
{
	uint8_t data[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xff) };
    return i2c_master_transmit(
        dev_handle,
        data, sizeof(data),
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

esp_err_t i2c_send_data(i2c_master_dev_handle_t dev_handle, uint8_t *data, size_t data_len)
{
    return i2c_master_transmit(
        dev_handle,
        data, data_len,
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

void i2c_master_bus_init(i2c_master_bus_handle_t *bus_handle)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));

    ESP_LOGI(TAG, "I2C bus initialized successfully");
}

void i2c_dev_init(i2c_master_dev_handle_t *dev_handle, uint16_t dev_addr, i2c_master_bus_handle_t *bus_handle)
{
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = dev_addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));

    ESP_LOGI(TAG, "I2C device initialized successfully");
}

void i2c_master_deinit(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle) {
    ESP_ERROR_CHECK(i2c_del_master_bus(*bus_handle));
	ESP_ERROR_CHECK(i2c_master_bus_rm_device(*dev_handle));

    ESP_LOGI(TAG, "I2C de-initialized successfully");
}
