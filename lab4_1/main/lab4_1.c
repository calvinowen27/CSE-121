/*
* SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
*
* SPDX-License-Identifier: Apache-2.0
*/

#include <stdio.h>
#include <memory.h>
#include "unity.h"
#include "driver/i2c_master.h"
#include "icm42670.h"
#include "esp_system.h"
#include "esp_log.h"
#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Pinout for ESP32-S3-BOX
#define I2C_MASTER_SCL_IO 8       /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO 10       /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0  /*!< I2C port number for master dev */

static const char *TAG = "icm42670 test";
static icm42670_handle_t icm42670 = NULL;
static i2c_master_bus_handle_t i2c_handle = NULL;

static void i2c_bus_init(void)
{
	const i2c_master_bus_config_t bus_config = {
		.i2c_port = I2C_MASTER_NUM,
		.sda_io_num = I2C_MASTER_SDA_IO,
		.scl_io_num = I2C_MASTER_SCL_IO,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
	};

	esp_err_t ret = i2c_new_master_bus(&bus_config, &i2c_handle);
	TEST_ASSERT_EQUAL_MESSAGE(ESP_OK, ret, "I2C install returned error");

    ESP_LOGI(TAG, "I2C bus initialized successfully");
}

static void i2c_sensor_icm42670_init(void)
{
	esp_err_t ret;

	i2c_bus_init();

	ret = icm42670_create(i2c_handle, ICM42670_I2C_ADDRESS, &icm42670);

    ESP_LOGI(TAG, "ICM42670 sensor created");

	TEST_ASSERT_EQUAL(ESP_OK, ret);
	TEST_ASSERT_NOT_NULL_MESSAGE(icm42670, "icm42670 create returned NULL");

	/* Configuration of the accelerometer and gyroscope */
	const icm42670_cfg_t imu_cfg = {
		.acce_fs = ACCE_FS_2G,
		.acce_odr = ACCE_ODR_400HZ,
		.gyro_fs = GYRO_FS_2000DPS,
		.gyro_odr = GYRO_ODR_400HZ,
	};
	vTaskDelay(1);
	ret = icm42670_config(icm42670, &imu_cfg);
	TEST_ASSERT_EQUAL(ESP_OK, ret);

    ESP_LOGI(TAG, "ICM42670 sensor initialized successfully");
}

void app_main(void)
{
	icm42670_value_t acc, gyro;

	i2c_sensor_icm42670_init();

	// vTaskDelay(1);

    ESP_LOGI(TAG, "Powering ICM42670 accelerometer on");

	ESP_ERROR_CHECK(icm42670_acce_set_pwr(icm42670, ACCE_PWR_LOWNOISE));

    ESP_LOGI(TAG, "Accelerometer ready");

    ESP_LOGI(TAG, "Powering ICM42670 gyroscope on");

	ESP_ERROR_CHECK(icm42670_gyro_set_pwr(icm42670, GYRO_PWR_LOWNOISE));

    ESP_LOGI(TAG, "Gyroscope ready");

	// negative Y = right
	// positive Y = left
	// negative X = up
	// positive X = down

	float pitchThreshold = 20;
	float rollThreshold = 20;

	char dirStr[11] = { 0 };

	complimentary_angle_t angle;

	while (1)
	{
		memset(dirStr, 0, sizeof(dirStr));

		vTaskDelay(pdMS_TO_TICKS(100));

		ESP_ERROR_CHECK(icm42670_get_acce_value(icm42670, &acc));
		ESP_ERROR_CHECK(icm42670_get_gyro_value(icm42670, &gyro));

		icm42670_complimentory_filter(icm42670, &acc, &gyro, &angle);

		// angles[0] += gyro.x;
		
		// ESP_LOGI(TAG, "ACCE [g]: X=%0.3f Y=%0.3f Z=%0.3f", acc.x, acc.y, acc.z);
	 	// ESP_LOGI(TAG, "GYRO [˚/s]: X=%0.3f Y=%0.3f Z=%0.3f", gyro.x, gyro.y, gyro.z);

		if (angle.roll > rollThreshold)
		{
			sprintf(dirStr + strlen(dirStr), "UP ");
		}
		else if (angle.roll < -rollThreshold)
		{
			sprintf(dirStr + strlen(dirStr), "DOWN ");
		}

		if (angle.pitch > pitchThreshold)
		{
			sprintf(dirStr + strlen(dirStr), "RIGHT");
		}
		else if (angle.pitch < -pitchThreshold)
		{
			sprintf(dirStr + strlen(dirStr), "LEFT");
		}

		// ESP_LOGI(TAG, "ANGLE :-: ROLL= %0.3f PITCH= %0.3f", angle.roll, angle.pitch);
		ESP_LOGI(TAG, "%s", dirStr);
	}

	// icm42670_delete(icm42670);
	// ESP_ERROR_CHECK(i2c_del_master_bus(i2c_handle));
	// vTaskDelay(10); // Give FreeRTOS some time to free its resources
}
 