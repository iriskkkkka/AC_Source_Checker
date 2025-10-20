#include <stdio.h>
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"

static const char *TAG = "TANG_NANO";

// Pin definitions
#define RELAY_PIN           GPIO_NUM_15
#define BUTTON_PIN          GPIO_NUM_5
#define ACS712_PIN          ADC1_CHANNEL_6  // GPIO34
#define UART_TX_PIN         GPIO_NUM_17
#define UART_RX_PIN         GPIO_NUM_16

// UART configuration
#define UART_NUM            UART_NUM_2
#define UART_BAUD_RATE      115200
#define UART_BUF_SIZE       1024

// ACS712 configuration for AC measurement (20A version)
#define ACS712_SENSITIVITY      100.0f
#define ACS712_VREF             2500.0f
#define SAMPLE_RATE_HZ          1000
#define SAMPLE_INTERVAL_MS      (1000 / SAMPLE_RATE_HZ)
#define NUM_SAMPLES             200
#define DEFAULT_VREF            1100

// ADC calibration
static esp_adc_cal_characteristics_t adc_chars;

// Shared state
static bool relay_on = false;
static float current_rms = 0.0f;

void relay_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RELAY_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(RELAY_PIN, 0);
    relay_on = false;
    
    // Wait for power to stabilize after GPIO config
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "Relay initialized on GPIO15");
}

void button_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    ESP_LOGI(TAG, "Button initialized on GPIO5");
}

void adc_init(void) {
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ACS712_PIN, ADC_ATTEN_DB_11);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 
                            DEFAULT_VREF, &adc_chars);
    ESP_LOGI(TAG, "ADC initialized for ACS712-20A");
}

void uart_init(void) {
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM, UART_BUF_SIZE, UART_BUF_SIZE, 0, NULL, 0);
    
    ESP_LOGI(TAG, "UART initialized: TX=GPIO%d, Baud=%d", UART_TX_PIN, UART_BAUD_RATE);
}

void send_data_to_fpga(const char* data, size_t len) {
    uart_write_bytes(UART_NUM, data, len);
}

void read_ac_current_task(void *arg) {
    ESP_LOGI(TAG, "AC current monitoring started");
    
    while (1) {
        float sum_sq = 0.0f;
        
        for (int i = 0; i < NUM_SAMPLES; i++) {
            uint32_t raw = adc1_get_raw(ACS712_PIN);
            uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);
            float current = (voltage_mv - ACS712_VREF) / ACS712_SENSITIVITY;
            sum_sq += current * current;
            vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
        }
        
        current_rms = sqrtf(sum_sq / NUM_SAMPLES);
        
        if (current_rms < 0.01f) current_rms = 0.0f;
        if (current_rms > 20.0f) current_rms = 0.0f;
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void button_check_task(void *arg) {
    bool last_button_state = gpio_get_level(BUTTON_PIN);
    
    ESP_LOGI(TAG, "Button monitoring started");
    
    while (1) {
        bool current_button_state = gpio_get_level(BUTTON_PIN);
        
        if (current_button_state == 0 && last_button_state == 1) {
            relay_on = !relay_on;
            gpio_set_level(RELAY_PIN, relay_on ? 1 : 0);
            
            ESP_LOGI(TAG, "Button pressed! Relay: %s", relay_on ? "ON" : "OFF");
            
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        
        last_button_state = current_button_state;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void uart_send_task(void *arg) {
    char buffer[32];
    
    ESP_LOGI(TAG, "UART send task started");
    vTaskDelay(pdMS_TO_TICKS(500));
    
    while (1) {
        int len;
        if (current_rms < 10.0f) {
            len = snprintf(buffer, sizeof(buffer), "I:%.2fA\n", current_rms);
        } else {
            len = snprintf(buffer, sizeof(buffer), "I:%.1fA\n", current_rms);
        }
        send_data_to_fpga(buffer, len);
        
        len = snprintf(buffer, sizeof(buffer), "R:%d\n", relay_on ? 1 : 0);
        send_data_to_fpga(buffer, len);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void) {
    // CRITICAL: Disable brownout detector first
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    
    // Wait for power to stabilize at startup
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "=== Tang Nano 9K Power Monitor ===");
    ESP_LOGI(TAG, "Starting application...");
    
    // Initialize peripherals with delays between each
    relay_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    button_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    adc_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    uart_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Send initial message
    send_data_to_fpga("ESP32 Ready\n", 12);
    
    // Create tasks
    xTaskCreate(read_ac_current_task, "read_ac_current", 4096, NULL, 5, NULL);
    xTaskCreate(button_check_task, "button_check", 2048, NULL, 5, NULL);
    xTaskCreate(uart_send_task, "uart_send", 2048, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "System initialized successfully");
    ESP_LOGI(TAG, "Press button on GPIO5 to toggle relay");
}