#include "hardware_init.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG_HW = "HARDWARE_INIT";

void button_init(void) {
    gpio_config_t btn_config = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&btn_config);
}

void led_init(void) {
    gpio_reset_pin(GREEN_LED_PIN);
    gpio_set_direction(GREEN_LED_PIN, GPIO_MODE_OUTPUT);

    gpio_reset_pin(RED_LED_PIN);
    gpio_set_direction(RED_LED_PIN, GPIO_MODE_OUTPUT);
}

void m0_m1_lora_init(void) {
    gpio_config_t m0_m1_config = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LORA_M0_PIN) | (1ULL << LORA_M1_PIN),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&m0_m1_config);

    gpio_set_level(LORA_M0_PIN, 0);
    gpio_set_level(LORA_M1_PIN, 0);
}

void aux_lora_init(void) {
    gpio_config_t aux_config = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << LORA_AUX_PIN),
        .pull_up_en = 1,
    };
    gpio_config(&aux_config);

    ESP_LOGI(TAG_HW, "Menunggu LoRa AUX stabil...");
    while(gpio_get_level(LORA_AUX_PIN) == 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGI(TAG_HW, "Modul LoRa Siap pada Mode Mendengar (Default)!");
}

void uart_lora_init(void) {
    uart_config_t uart_config = {
        .baud_rate = LORA_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // Menggunakan macro UART_NUM_2 secara eksplisit
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, LORA_TXD_PIN, LORA_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}


// Fungsi bungkus agar di main.c cukup panggil satu baris ini saja
void init_all_hardware(void) {
    button_init();
    led_init();
    m0_m1_lora_init();
    aux_lora_init();
    uart_lora_init();
    ESP_LOGI(TAG_HW, "Semua hardware berhasil diinisialisasi.");
}