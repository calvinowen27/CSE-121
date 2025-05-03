/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* i2c - Simple Example

   Simple I2C example that shows how to initialize I2C
   as well as reading and writing from and to registers for a sensor connected over I2C.

   The sensor used in this example is a MPU9250 inertial measurement unit.
*/
#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "example";

#define I2C_MASTER_SCL_IO           8       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           10       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_0                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          100000 /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       2000

#define SHTC3_SENSOR_ADDR         0x70        /*!< Address of the SHTC3 sensor */

#define SHTC3_WAKEUP_CMD            0x3517
#define SHTC3_SLEEP_CMD             0xB098
#define SHTC3_MEASURE_CMD           0x7CA2  // read temp first, clock stretching enabled

esp_err_t shtc3_read(i2c_master_dev_handle_t dev_handle, uint8_t *buf, size_t bufsize)
{
    return i2c_master_receive(
        dev_handle,
        buf, bufsize,
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

esp_err_t shtc3_send_cmd(i2c_master_dev_handle_t dev_handle, uint16_t cmd)
{
    uint8_t data[2] = { (uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xff) };
    return i2c_master_transmit(
        dev_handle,
        data, sizeof(data),
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

static void i2c_master_init(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle)
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

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHTC3_SENSOR_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
}

bool verify_checksum(uint8_t *data, size_t byteCount, uint8_t checksum)
{
    uint8_t res = 0xff;
    uint8_t poly = 0x31;

    for (uint8_t i = 0; i < byteCount; i++)
    {
        res ^= data[i];

        for (uint8_t j = 0; j < 8; j++)
        {
            if (res & 0x80)
                res = (res << 1) ^ poly;
            else
                res <<= 1;
        }
    }

    return (uint8_t)res == checksum;
}

void app_main(void)
{
    // uint8_t data[2];
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    i2c_master_init(&bus_handle, &dev_handle);
    ESP_LOGI(TAG, "I2C initialized successfully");

    while (true) {
        uint8_t data[6] = { 0 };

        ESP_ERROR_CHECK(shtc3_send_cmd(dev_handle, SHTC3_WAKEUP_CMD));

        // delay for wake time
        vTaskDelay(10 / portTICK_PERIOD_MS);

        ESP_ERROR_CHECK(shtc3_send_cmd(dev_handle, SHTC3_MEASURE_CMD));

        // delay for measure time
        vTaskDelay(15 / portTICK_PERIOD_MS);

        ESP_ERROR_CHECK(shtc3_read(dev_handle, data, sizeof(data)));

        ESP_ERROR_CHECK(shtc3_send_cmd(dev_handle, SHTC3_SLEEP_CMD));

        if (!verify_checksum(data, 2, data[2])) {
            printf("error detected in data transmission.\n");
            continue;
        }

        if (!verify_checksum(data + 3, 2, data[5])) {
            printf("error detected in data transmission.\n");
            continue;
        }

        // read temperature from first 2 bytes
        uint16_t tempReading = (uint16_t)((((uint16_t)data[0]) << 8) | ((uint16_t)data[1]));

        // convert to celsius
        float tempC = ((float)tempReading / (float)65535)*175 - 45;
        
        // convert to farenheit
        float tempF = tempC * ((float)9 / (float)5) + 32;

        // read humidity from bytes 4 and 5
        uint16_t humReading = (uint16_t)((((uint16_t)data[3]) << 8) | ((uint16_t)data[4]));
        // convert humidity to percentage
        float hum = ((float)humReading / (float)65535) * 100;

        printf("Temperature is %.2fC (or %.2fF) with a %.2f%% humidity\n", tempC, tempF, hum);

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    ESP_ERROR_CHECK(i2c_master_bus_rm_device(dev_handle));
    ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
    ESP_LOGI(TAG, "I2C de-initialized successfully");
}
