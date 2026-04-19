#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "hardware_init.h"
#include "freertos/semphr.h" // Wajib di-include untuk Mutex

// Variabel Global
int current_noise_floor_dbm = -105;
SemaphoreHandle_t uart_mutex;

// Ganti nama node untuk flash lora kedua
#define NODE_NAME "Node B"

static const char *TAG = "LORA B";



// --- TASK 1: MENGAMBIL DATA NOISE FLOOR ---
void lora_noise_task(void *pvParameters) {
    uint8_t query_noise_cmd[] = {0xC0, 0xC1, 0xC2, 0xC3, 0x00, 0x01};

    while (1) {
        // Tunggu 10 detik
        vTaskDelay(pdMS_TO_TICKS(10000));

        // Ambil kunci Mutex sebelum memakai UART (Tunggu maksimal 1 detik jika sedang dipakai RX)
        if (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            
            // Tembakkan perintah hanya jika LoRa sedang santai (AUX=1)
            if (gpio_get_level(LORA_AUX_PIN) == 1) {
                uart_write_bytes(UART_NUM_2, (const char*)query_noise_cmd, sizeof(query_noise_cmd));
            }
            
            // Kembalikan kunci Mutex agar Task RX bisa jalan lagi
            xSemaphoreGive(uart_mutex);
        }
    }
}

// --- TASK 2: MENERIMA PESAN UTAMA ---
void lora_rx_task(void *pvParameters) {
    uint8_t data[BUF_SIZE];
    uart_flush_input(UART_NUM_2);

    while (1) {
        int len = uart_read_bytes(UART_NUM_2, data, BUF_SIZE - 1, pdMS_TO_TICKS(20));
        
        // Memastikan ada teks pesan + 1 byte RSSI
        if (len > 1) { 
            // Ambil byte paling belakang
            uint8_t rssi_byte = data[len - 1];
            
            // Ubah ke nilai minus
            int rssi_dbm = (int)rssi_byte - 256;
            
            // Potong byte RSSI dengan Null Terminator
            data[len - 1] = '\0'; 

            ESP_LOGI(TAG, ">>> PESAN MASUK : %s", (char*)data);
            ESP_LOGI(TAG, "    [ANALISIS RF] RSSI Aktual: %d dBm", rssi_dbm);
            
            gpio_set_level(GREEN_LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
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
    //bool is_pressed = false;

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
    init_all_hardware();
    configure_lora_channel();

    

    uart_mutex = xSemaphoreCreateMutex();
        if (uart_mutex == NULL) {
            ESP_LOGE(TAG, "Gagal membuat Mutex!");
            return;
        }
    
    //xTaskCreate(lora_noise_task, "lora_noise", 2048, NULL, 4, NULL);
    xTaskCreate(lora_rx_task, "lora_rx", 4096, NULL, 5, NULL);
    //xTaskCreate(lora_tx_task, "lora_tx", 4096, NULL, 3, NULL);
}