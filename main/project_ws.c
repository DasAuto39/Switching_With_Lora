#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "hardware_init.h"

static const char *TAG = "NODE B (MASTER)";
int current_noise_floor_dbm = -105; 

void lora_master_task(void *pvParameters) {
    uint8_t data[BUF_SIZE];
    uint8_t query_noise_cmd[] = {0xC0, 0xC1, 0xC2, 0xC3, 0x00, 0x01};
    const char* req_cmd = "REQ_DATA";
    
    while (1) {
        // ==========================================
        // FASE 1: BACA NOISE FLOOR (Udara Dijamin Kosong)
        // ==========================================
        uart_flush_input(UART_NUM_2); // Pastikan pipa bersih
        uart_write_bytes(UART_NUM_2, (const char*)query_noise_cmd, sizeof(query_noise_cmd));
        
        // Tunggu balasan C1 00 01 XX
        int len = uart_read_bytes(UART_NUM_2, data, 4, pdMS_TO_TICKS(200));
        if (len == 4 && data[0] == 0xC1) {
            current_noise_floor_dbm = - (256 - (int)data[3]);
            ESP_LOGW(TAG, "[FASE 1] Noise Floor Aktual: %d dBm", current_noise_floor_dbm);
        } else {
            ESP_LOGE(TAG, "[FASE 1] Gagal mendapat Noise, pakai nilai lama.");
        }

        // ==========================================
        // FASE 2: PANGGIL NODE A
        // ==========================================
        ESP_LOGW(TAG, "[FASE 2] Mengirim REQ_DATA ke Node A...");
        uart_flush_input(UART_NUM_2); // Bilas lagi sebelum meminta data
        uart_write_bytes(UART_NUM_2, req_cmd, strlen(req_cmd));

        // ==========================================
        // FASE 3: TERIMA BALASAN (Timeout 2 Detik)
        // ==========================================
        // Kita beri waktu 2 detik. Jika Node A mati/hilang sinyal, sistem tidak akan hang.
        len = uart_read_bytes(UART_NUM_2, data, BUF_SIZE - 1, pdMS_TO_TICKS(2000));
        
        if (len > 0) {
            // Kita jamin 100% ini adalah pesan teks, bukan C1 00 01 XX
            uint8_t rssi_byte = data[len - 1];
            int rssi_dbm = (int)rssi_byte - 256; 
            int snr_db = rssi_dbm - current_noise_floor_dbm;

            data[len - 1] = '\0'; // Potong RSSI

            ESP_LOGI(TAG, ">>> [FASE 3] PESAN MASUK : %s", (char*)data);
            ESP_LOGI(TAG, "    [RF DATA] RSSI: %d dBm | SNR: %d dB | Floor Noise: %d dBm", 
                     rssi_dbm, snr_db, current_noise_floor_dbm);
            
            gpio_set_level(GREEN_LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(GREEN_LED_PIN, 0);
        } else {
            ESP_LOGE(TAG, "[FASE 3] Timeout! Node A tidak merespons (Kapal di luar jangkauan/Mati).");
        }

        // ==========================================
        // FASE 4: DELAY SIKLUS (Interval Polling)
        // ==========================================
        ESP_LOGW(TAG, "--------------------------------------------------");
        // Atur seberapa sering Master melakukan patroli data (misal: 3 detik sekali)
        vTaskDelay(pdMS_TO_TICKS(3000)); 
    }
}

void app_main(void) {
    init_all_hardware();
    configure_lora_channel();
    xTaskCreate(lora_master_task, "lora_master", 4096, NULL, 5, NULL);
}


/*Node A
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "hardware_init.h"

static const char *TAG = "NODE A (KAPAL)";
int pesan_counter = 1;

void lora_slave_task(void *pvParameters) {
    uint8_t data[BUF_SIZE];
    uart_flush_input(UART_NUM_2);

    while (1) {
        // Kapal standby mendengarkan perintah dari pelabuhan
        int len = uart_read_bytes(UART_NUM_2, data, BUF_SIZE - 1, pdMS_TO_TICKS(100));
        
        if (len > 0) {
            // Karena Node B menset RSSI byte=On, permintaan "REQ_DATA" 
            // juga akan memiliki 1 byte RSSI di ujungnya. Kita abaikan saja.
            data[len - 1] = '\0'; 

            // Jika perintah yang datang adalah "REQ_DATA"
            if (strncmp((char*)data, "REQ_DATA", 8) == 0) {
                ESP_LOGI(TAG, "Menerima panggilan dari Master. Mempersiapkan data...");
                
                // Di sini nanti Anda bisa memasukkan fungsi pembacaan GPS Neo-6M
                // Untuk sekarang, kita gunakan mock data
                char payload[100];
                snprintf(payload, sizeof(payload), "[Node A] Data Koordinat Kapal ke-%d", pesan_counter++);
                
                // Jeda 50ms (Turn-around time) untuk memastikan Node B siap menerima
                vTaskDelay(pdMS_TO_TICKS(50)); 
                
                // Tembakkan payload ke udara!
                uart_write_bytes(UART_NUM_2, payload, strlen(payload));
                ESP_LOGI(TAG, "Data berhasil dikirim: %s", payload);
                
                gpio_set_level(GREEN_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(20));
                gpio_set_level(GREEN_LED_PIN, 0);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    init_all_hardware();
    configure_lora_channel();
    xTaskCreate(lora_slave_task, "lora_slave", 4096, NULL, 5, NULL);
}


*/