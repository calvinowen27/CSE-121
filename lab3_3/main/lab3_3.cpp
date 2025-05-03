#include "RGBLCD.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "i2c_helper.h"

#include "string.h"
#include "stdlib.h"

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

void get_measurements(i2c_master_dev_handle_t dev_handle, char *temp_str, char *hum_str)
{
    uint8_t data[6] = { 0 };

    ESP_ERROR_CHECK(i2c_send_cmd(dev_handle, SHTC3_WAKEUP_CMD));

    // delay for wake time
    vTaskDelay(10 / portTICK_PERIOD_MS);

    ESP_ERROR_CHECK(i2c_send_cmd(dev_handle, SHTC3_MEASURE_CMD));

    // delay for measure time
    vTaskDelay(15 / portTICK_PERIOD_MS);

    ESP_ERROR_CHECK(i2c_read(dev_handle, data, sizeof(data)));

    ESP_ERROR_CHECK(i2c_send_cmd(dev_handle, SHTC3_SLEEP_CMD));

    if (!verify_checksum(data, 2, data[2])) {
        printf("error detected in data transmission.\n");
        return;
    }

    if (!verify_checksum(data + 3, 2, data[5])) {
        printf("error detected in data transmission.\n");
        return;
    }

    // read temperature from first 2 bytes
    uint16_t tempReading = (uint16_t)((((uint16_t)data[0]) << 8) | ((uint16_t)data[1]));

    // convert to celsius
    float tempC = ((float)tempReading / (float)65535)*175 - 45;
    
    // convert to farenheit
    // float tempF = tempC * ((float)9 / (float)5) + 32;

    // read humidity from bytes 4 and 5
    uint16_t humReading = (uint16_t)((((uint16_t)data[3]) << 8) | ((uint16_t)data[4]));
    // convert humidity to percentage
    float hum = ((float)humReading / (float)65535) * 100;

    sprintf(temp_str, "Temp: %.2fC", tempC);
    sprintf(hum_str, "Hum : %.2f%%", hum);

    // printf("Temperature is %.2fC (or %.2fF) with a %.2f%% humidity\n", tempC, tempF, hum);
    printf("%s\n", temp_str);
    printf("%s\n\n", hum_str);
}

extern "C" void app_main(void) {
	RGBLCD lcd(RGB_ADDR);

    i2c_master_dev_handle_t dev_handle;
    i2c_dev_init(&dev_handle, SHTC3_SENSOR_ADDR, lcd.get_bus());

    char temp[14] = { 0 };
    char hum[14] = { 0 };

	while (true) {
        get_measurements(dev_handle, temp, hum);
        
		lcd.init();

		lcd.setRGB(0xff, 0xff, 0xff);
		lcd.print(temp);
		lcd.setCursor(0, 1);
		lcd.print(hum);

        memset(temp, 0, sizeof(temp));
        memset(hum, 0, sizeof(hum));

		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
}
