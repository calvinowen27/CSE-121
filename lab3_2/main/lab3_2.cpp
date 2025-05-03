#include "RGBLCD.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void) {
	RGBLCD lcd(RGB_ADDR);

	while (true) {
		lcd.init();

		lcd.setRGB(0xff, 0xff, 0xff);
		lcd.print("Hello CSE121!");
		lcd.setCursor(0, 1);
		lcd.print("Calvin");

		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}
