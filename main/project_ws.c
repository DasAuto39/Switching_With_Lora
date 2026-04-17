#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "hardware_init.h"

// Ganti nama node untuk flash lora kedua
#define NODE_NAME "Node A"

static const char *TAG = "LORA A";



void lora_rx_task(void *pvParameters) {
    uint8_t data[BUF_SIZE];

    while (1) {
        // Baca buffer UART (Non-blocking)
        int len = uart_read_bytes(LORA_UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(20));
        
        if (len > 0) {
            data[len] = '\0'; // Tambahkan null terminator
            ESP_LOGI(TAG, ">>> DITERIMA: %s", (char*)data);
            
            // Kedipkan LED sebagai indikator visual pesan masuk
            gpio_set_level(GREEN_LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(100));
            gpio_set_level(GREEN_LED_PIN, 0);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// --- TASK FREERTOS: MENGIRIM DATA PTT (TX) ---
// Modul beralih state untuk mengirim hanya ketika tombol ditekan
void lora_tx_task(void *pvParameters) {
    char payload[128];
    int counter = 1;
    bool is_pressed = false;

    uart_flush_input(LORA_UART_NUM);
    
    while (1) {
        // Logika PTT: Cek tombol (0 berarti ditekan karena pull-up internal)
        if (gpio_get_level(BUTTON_PIN) == 0) {
            
            // Hindari pengiriman spam jika tombol ditahan terus
           
                // Format string pesan
            snprintf(payload, sizeof(payload), "[%s] Teks ke-%d", NODE_NAME, counter);

            // Verifikasi modul tidak sibuk sebelum mengirim data ke UART
            if (gpio_get_level(LORA_AUX_PIN) == 1) {
                uart_write_bytes(LORA_UART_NUM, payload, strlen(payload));
                ESP_LOGI(TAG, "<<< DIKIRIM (PTT): %s", payload);
                counter++;
                
                // Nyalakan LED selama tombol ditekan sebagai tanda TX aktif
                gpio_set_level(RED_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(1000)); 
                
                gpio_set_level(RED_LED_PIN, 0);
            } else {
                ESP_LOGW(TAG, "Gagal mengirim, modul LoRa sedang memproses data di udara (AUX=0)");
                vTaskDelay(pdMS_TO_TICKS(100)); // Cek lagi setelah 100ms
            }
            
            
        } else {
            // Jika tombol dilepas, reset status
            
            gpio_set_level(RED_LED_PIN, 0); // Matikan LED TX
            vTaskDelay(pdMS_TO_TICKS(50));
           
            
        }

        // Interval polling tombol (debouncing sederhana)
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}



void app_main(void) {
    // Panggil fungsi inisialisasi
    
    button_init();
    led_init();
    m0_m1_lora_init();
    aux_lora_init();
    uart_lora_init();
    

    xTaskCreate(lora_rx_task, "lora_rx", 4096, NULL, 5, NULL);
    xTaskCreate(lora_tx_task, "lora_tx", 4096, NULL, 4, NULL);
}