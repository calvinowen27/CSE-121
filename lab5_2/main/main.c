/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "sdkconfig.h"
#include <unistd.h>
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

const static char *TAG = "ADC";

/*---------------------------------------------------------------
        ADC General Macros
---------------------------------------------------------------*/
//ADC1 Channels
#if CONFIG_IDF_TARGET_ESP32
#define EXAMPLE_ADC1_CHAN0          ADC_CHANNEL_4
#define EXAMPLE_ADC1_CHAN1          ADC_CHANNEL_5
#else
#define EXAMPLE_ADC1_CHAN0          ADC_CHANNEL_2
#define EXAMPLE_ADC1_CHAN1          ADC_CHANNEL_3
#endif

#if (SOC_ADC_PERIPH_NUM >= 2) && !CONFIG_IDF_TARGET_ESP32C3
/**
 * On ESP32C3, ADC2 is no longer supported, due to its HW limitation.
 * Search for errata on espressif website for more details.
 */
#define EXAMPLE_USE_ADC2            1
#endif

#if EXAMPLE_USE_ADC2
//ADC2 Channels
#if CONFIG_IDF_TARGET_ESP32
#define EXAMPLE_ADC2_CHAN0          ADC_CHANNEL_0
#else
#define EXAMPLE_ADC2_CHAN0          ADC_CHANNEL_0
#endif
#endif  //#if EXAMPLE_USE_ADC2

#define EXAMPLE_ADC_ATTEN           ADC_ATTEN_DB_12

// #define PERIOD 500000
#define HIGH_THRESHOLD 30
#define SAMPLES 5
#define PERIOD 25000 / SAMPLES

static int adc_raw[2][10];
static int voltage[2][10];
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void example_adc_calibration_deinit(adc_cali_handle_t handle);


char morse[37][7] = {
                        "a.-/",
                        "b-.../",
                        "c-.-./",
                        "d-../",
                        "e./",
                        "f..-./",
                        "g--./",
                        "h..../",
                        "i../",
                        "j.---/",
                        "k-.-/",
                        "l.-../",
                        "m--/",
                        "n-./",
                        "o---/",
                        "p.--./",
                        "q--.-/",
                        "r.-./",
                        "s.../",
                        "t-/",
                        "u..-/",
                        "v...-/",
                        "w.--/",
                        "x-..-/",
                        "y-.--/",
                        "z--../",
                        "1.----/",
                        "2..---/",
                        "3...--/",
                        "4....-/",
                        "5...../",
                        "6-..../",
                        "7--.../",
                        "8---../",
                        "9----./",
                        "0-----/",
                        " -...-/"
};

char msg[64] = { 0 };
int idx = 0;

int v_buf[12] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
int s_buf[12] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
char c_buf[12] = { 0 };

esp_timer_handle_t periodic_timer;
adc_oneshot_unit_handle_t adc1_handle;
adc_cali_handle_t adc1_cali_chan0_handle = NULL;

static void periodic_timer_callback(void* arg)
{
    // int64_t time_since_boot = esp_timer_get_time();
    // ESP_LOGI("timer", "Periodic timer called, time since boot: %lld us", time_since_boot);
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN0, &adc_raw[0][0]));
    // ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, adc_raw[0][0]);
    // if (do_calibration1_chan0) {
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan0_handle, adc_raw[0][0], &voltage[0][0]));
    // ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, voltage[0][0]);
    // }

    // return;

    for (int i = 0; i < 12 - 1; i++)
        v_buf[i] = v_buf[i+1];

    v_buf[11] = voltage[0][0];
    if (v_buf[12 - SAMPLES] == -1) return;

    for (int j = 0; j < 12 - 1; j++)
        s_buf[j] = s_buf[j+1];

    bool high = false;
    for (int i = 0; i < 12 - (SAMPLES - 1); i++)
    {
        if (v_buf[i] == -1) continue;

        int high_count = 0;
        for (int j = 0; j < SAMPLES; j++)
        {
            if (v_buf[i+j] > HIGH_THRESHOLD) high_count++;
        }

        // printf("high count %d\n", high_count);

        if (high_count > 2)
        {
            s_buf[11] = 1;
            high = true;
            // ESP_LOGI("s", "high");
            break;
        }
    }

    if (!high)
    {
        s_buf[11] = 0;
        // ESP_LOGI("s", "low");
    }
    
    for (int i = 12 - SAMPLES; i < 12; i++)
        v_buf[i] = -1;

    for (int i = 0; i < 12 - 4; i++)
    {
        if (s_buf[i] == 1 && !s_buf[i+1])
        {
            // dot
            ESP_LOGI("morse", ".");
            for (int i = 0; i < 12 - 1; i++)
                c_buf[i] = c_buf[i+1];
            c_buf[11] = '.';
            s_buf[i] = -1;
            s_buf[i+1] = -1;
            break;
        }
        else if (s_buf[i] == 1 && s_buf[i+1] == 1 && s_buf[i+2] == 1 && s_buf[i+3] == 1 && !s_buf[i+4])
        {
            // dash
            ESP_LOGI("morse", "-");
            for (int i = 0; i < 12 - 1; i++)
                c_buf[i] = c_buf[i+1];
            c_buf[11] = '-';
            s_buf[i] = -1;
            s_buf[i+1] = -1;
            s_buf[i+2] = -1;
            s_buf[i+3] = -1;
            s_buf[i+4] = -1;
            break;
        }
        else if (!s_buf[i] && !s_buf[i+1] && !s_buf[i+2] && !s_buf[i+3])
        {
            // ESP_LOGI("morse", "/");
            for (int i = 0; i < 12 - 1; i++)
                c_buf[i] = c_buf[i+1];
            c_buf[11] = '/';
            s_buf[i] = -1;
            s_buf[i+1] = -1;
            s_buf[i+2] = -1;
            s_buf[i+3] = -1;
            // s_buf[i+4] = -1;
            break;
        }
        else
        {
            return;
        }
    }

    // char c_buf_cpy[13] = { 0 };
    // for (int i = 0; i < 12; i++)
    // {
    //     if (c_buf[i] == -1) c_buf_cpy[i] = ' ';
    //     else if (c_buf[i] == 0) c_buf_cpy[i] = 'z';
    //     else c_buf_cpy[i] = c_buf[i];
    // }

    // ESP_LOGI("c_buf:", "%s\n", c_buf_cpy);

    char c[7] = { 0 };
    c[6] = 0;

    for (int l = 0; l < 12 - 6; l++)
    {
        if (c_buf[l] != 0)
        {
            bool good = false;
            for (int i = 0; i < 37; i++)
            {
                // printf("looking at %s\n", morse[i]);

                good = true;
                int last_idx = 0;

                for (int j = 0; j < 6; j++)
                {
                    last_idx = j;
                    // ESP_LOGI("s", "compare %c == %c", morse[i][j+1], c_buf[l+j]);
                    if (morse[i][j+1] != c_buf[l+j])
                    {
                        good = false;
                        break;
                    }
                    
                    if (c_buf[l+j] == '/') break;
                }
                
                if (good)
                {
                    // for (int j = 0; j < 6; j++)
                    //     ESP_LOGI("good comparison", "compare %c == %c", morse[i][j+1], c_buf[l+j]);

                    msg[idx++] = morse[i][0];
                    
                    for (int k = 0; k < last_idx+1; k++)
                        c[k] = c_buf[l+k];

                    for (int j = 0; j < last_idx+1; j++)
                        c_buf[l+j] = 0;
                    
                    // ESP_LOGI("code", "valid code found: %s", c);
                    // ESP_LOGI("msg:", "%s", msg);

                    break;
                }
            }
            
            if (good) break;
        }
    }

    if (strlen(msg) == 0) return;

    bool end = true;
    for (int i = 0; i < 12; i++)
    {
        if (c_buf[i] != '/')
        {
            end = false;
        }
    }

    if (end)
    {
        ESP_LOGI("final msg:", "%s", msg);
        memset(msg, 0, 64);
        idx = 0;
        memset(c_buf, 0, 12);
    }
}

void create_timer()
{
    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &periodic_timer_callback,
            /* name is optional, but may help identify the timer when debugging */
            .name = "periodic"
    };

    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    /* The timer has been created but is not running yet */

    /* Start the timer */
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, PERIOD));
    ESP_LOGI(TAG, "Started timers, time since boot: %lld us", esp_timer_get_time());
}

void app_main(void)
{
    //-------------ADC1 Init---------------//
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .atten = EXAMPLE_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_ADC1_CHAN0, &config));

    //-------------ADC1 Calibration Init---------------//
    // adc_cali_handle_t adc1_cali_chan0_handle = NULL;
    // bool do_calibration1_chan0 = example_adc_calibration_init(ADC_UNIT_1, EXAMPLE_ADC1_CHAN0, EXAMPLE_ADC_ATTEN, &adc1_cali_chan0_handle);

    example_adc_calibration_init(ADC_UNIT_1, EXAMPLE_ADC1_CHAN0, EXAMPLE_ADC_ATTEN, &adc1_cali_chan0_handle);

    create_timer();

    // while (1) {
    //     ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_ADC1_CHAN0, &adc_raw[0][0]));
    //     ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, adc_raw[0][0]);
    //     if (do_calibration1_chan0) {
    //         ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan0_handle, adc_raw[0][0], &voltage[0][0]));
    //         ESP_LOGI(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, EXAMPLE_ADC1_CHAN0, voltage[0][0]);
    //     }
    //     vTaskDelay(pdMS_TO_TICKS(100));
    // }

    while (1) { vTaskDelay(1); }

    //Tear Down
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    // if (do_calibration1_chan0) {
    example_adc_calibration_deinit(adc1_cali_chan0_handle);
    // }

    /* Let the timer run for a little bit more */
    usleep(2000000);

    /* Clean up and finish the example */
    ESP_ERROR_CHECK(esp_timer_stop(periodic_timer));
    ESP_ERROR_CHECK(esp_timer_delete(periodic_timer));
    ESP_LOGI(TAG, "Stopped and deleted timer");
}

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}
