/* HTTP GET Example using plain POSIX sockets

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "driver/i2c_master.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "sdkconfig.h"

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

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER "10.0.0.2"
#define WEB_PORT "1234"
#define WEB_PATH "/"

char POST_PAYLOAD[512];
static const char *REQUEST = 
    "POST " WEB_PATH " HTTP/1.0\r\n"
    "Host: "WEB_SERVER":"WEB_PORT"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: %d\r\n"
    "\r\n"
    "%s";

static const char *TAG = "example";

i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t dev_handle;

void get_data(void);

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

static void http_post_task(void *pvParameters)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    char recv_buf[64];

    while(1) {
        int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);

        if(err != 0 || res == NULL) {
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
 
        /* Code to print the resolved IP.

           Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
        addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        s = socket(res->ai_family, res->ai_socktype, 0);
        if(s < 0) {
            ESP_LOGE(TAG, "... Failed to allocate socket.");
            freeaddrinfo(res);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... allocated socket");

        if(connect(s, res->ai_addr, res->ai_addrlen) != 0) {
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            close(s);
            freeaddrinfo(res);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "... connected");
        freeaddrinfo(res);

        get_data();

        char req[512];
        int request_len = snprintf(req, sizeof(req), REQUEST, strlen(POST_PAYLOAD), POST_PAYLOAD);

        if (write(s, req, request_len) < 0) {
            ESP_LOGE(TAG, "... socket send failed");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... socket send success");

        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                sizeof(receiving_timeout)) < 0) {
            ESP_LOGE(TAG, "... failed to set socket receiving timeout");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... set socket receiving timeout success");

        /* Read HTTP response */
        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = read(s, recv_buf, sizeof(recv_buf)-1);
            for(int i = 0; i < r; i++) {
                putchar(recv_buf[i]);
            }
        } while(r > 0);

        ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d.", r, errno);
        close(s);
        for(int countdown = 10; countdown >= 0; countdown--) {
            ESP_LOGI(TAG, "%d... ", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAG, "Starting again!");
    }
}

void get_data(void)
{
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
    float tempF = tempC * ((float)9 / (float)5) + 32;

    // read humidity from bytes 4 and 5
    uint16_t humReading = (uint16_t)((((uint16_t)data[3]) << 8) | ((uint16_t)data[4]));
    // convert humidity to percentage
    float hum = ((float)humReading / (float)65535) * 100;

    // printf("Temperature is %.2fC (or %.2fF) with a %.2f%% humidity\n", tempC, tempF, hum);

    memset(POST_PAYLOAD, 0, sizeof(POST_PAYLOAD));
    snprintf(POST_PAYLOAD, sizeof(POST_PAYLOAD), "%.2fC (or %.2fF) with a %.2f%% humidity", tempC, tempF, hum);
}

void app_main(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    i2c_master_init(&bus_handle, &dev_handle);
    ESP_LOGI(TAG, "I2C initialized successfully");

    xTaskCreate(&http_post_task, "http_post_task", 4096, NULL, 5, NULL);
}
