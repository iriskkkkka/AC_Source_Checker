#include <stdio.h>
#include <math.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "st7735.h"

#define RELAY_GPIO GPIO_NUM_32
#define BUTTON_GPIO GPIO_NUM_0
#define SENSOR_ADC_CHANNEL ADC1_CHANNEL_6  // GPIO34
#define DEFAULT_VREF 1100  // in mV for ADC calibration

#define SAMPLE_RATE_HZ 1000
#define SAMPLE_INTERVAL_MS (1000 / SAMPLE_RATE_HZ)
#define NUM_SAMPLES 200  // average over 200 samples per update

#define SENSOR_ZERO_VOLTAGE_MV 2500.0
#define SENSOR_SENSITIVITY_MV_PER_A 100.0

static const char *TAG = "MAIN";
static esp_adc_cal_characteristics_t adc_chars;
static bool relay_on = false;

void read_ac_current_task(void *arg) {
    float last_rms = -1.0f;
    bool last_relay = !relay_on;
    char buffer[32];

    while (1) {
        float sum_sq = 0;
        for (int i = 0; i < NUM_SAMPLES; i++) {
            uint32_t raw = adc1_get_raw(SENSOR_ADC_CHANNEL);
            uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);
            float current = (voltage_mv - SENSOR_ZERO_VOLTAGE_MV) / SENSOR_SENSITIVITY_MV_PER_A;
            sum_sq += current * current;
            vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
        }
        float rms_current = sqrtf(sum_sq / NUM_SAMPLES);

        if (fabs(rms_current - last_rms) > 0.01f) {
            last_rms = rms_current;
            snprintf(buffer, sizeof(buffer), "RMS: %.2f", rms_current);
            st7735_draw_text(2, 10, buffer, 0xFFFF, 0x0000, 2);
        }

        if (last_relay != relay_on) {
            last_relay = relay_on;
            st7735_fill_rect(0, 30, 128, 16, 0x0000); // Clear old relay text
            snprintf(buffer, sizeof(buffer), "Relay: %s", relay_on ? "ON" : "OFF");
            st7735_draw_text(2, 30, buffer, relay_on ? 0x07E0 : 0xF800, 0x0000, 2);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void button_check_task(void *arg) {
    bool last_state = gpio_get_level(BUTTON_GPIO);
    while (1) {
        bool current = gpio_get_level(BUTTON_GPIO);
        if (current == 0 && last_state == 1) {
            relay_on = !relay_on;
            gpio_set_level(RELAY_GPIO, relay_on);
            ESP_LOGI(TAG, "Relay %s", relay_on ? "ON" : "OFF");
        }
        last_state = current;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(SENSOR_ADC_CHANNEL, ADC_ATTEN_DB_11);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, DEFAULT_VREF, &adc_chars);

    gpio_set_direction(RELAY_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY_GPIO, relay_on);

    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);

    st7735_init();
    st7735_fill_screen(0x0000);

    xTaskCreate(read_ac_current_task, "read_ac_current_task", 4096, NULL, 5, NULL);
    xTaskCreate(button_check_task, "button_check_task", 2048, NULL, 5, NULL);
}
