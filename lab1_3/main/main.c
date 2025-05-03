#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
// #include "esp_flash.h"
#include "esp_system.h"
#include "driver/gpio.h"

#define BLINK_GPIO CONFIG_BLINK_GPIO
#define BLINK_PERIOD CONFIG_BLINK_PERIOD

static uint8_t s_led_state = 0;

void config_gpio(void) {
	gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

void blink_task(void) {
	while (true) {
		gpio_set_level(BLINK_GPIO, s_led_state);
		
		s_led_state = !s_led_state;

		printf("blinking %s\n", !s_led_state ? "on" : "off");

        vTaskDelay(BLINK_PERIOD / portTICK_PERIOD_MS);
	}
}

void app_main(void)
{
	config_gpio();

	xTaskCreate((TaskFunction_t)blink_task, "blink_task", 2048, NULL, 5, NULL);
}