#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "hardware_init.h"

static const char *TAG = "LORA B";

int current_noise_floor_dbm = -105; 
// Deklarasi Semaphore
SemaphoreHandle_t aux_idle_sem;

// ------------------------------------------------------------------
// FUNGSI ISR (INTERRUPT SERVICE ROUTINE)
// IRAM_ATTR memaksa fungsi ini disimpan di RAM internal (bukan Flash)
// agar bisa dieksekusi secepat kilat tanpa delay.
// ------------------------------------------------------------------
static void IRAM_ATTR aux_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // Lemparkan Semaphore saat Rising Edge terdeteksi
    xSemaphoreGiveFromISR(aux_idle_sem, &xHigherPriorityTaskWoken);
    
    // Jika pelemparan ini membangunkan task dengan prioritas lebih tinggi, 
    // langsung alihkan konteks CPU ke task tersebut
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// ------------------------------------------------------------------
// TASK 1: NOISE POLLING (Dikendalikan oleh Hardware Interrupt)
// ------------------------------------------------------------------
void lora_noise_task(void *pvParameters) {
    uint8_t query_noise_cmd[] = {0xC0, 0xC1, 0xC2, 0xC3, 0x00, 0x01};
    TickType_t last_query_time = 0;
    const TickType_t throttle_delay = pdMS_TO_TICKS(5000); // Batas aman 5 detik

    while (1) {
        // Task ini tertidur lelap tanpa mengonsumsi CPU, 
        // menunggu Semaphore dari ISR. Timeout maksimal 10 detik.
        if (xSemaphoreTake(aux_idle_sem, pdMS_TO_TICKS(10000)) == pdTRUE) {
            
            TickType_t current_time = xTaskGetTickCount();
            
            // THROTTLE (Pembatasan): 
            // Jika Node A mengirim 10 pesan beruntun, ISR akan memicu 10 kali.
            // Kita batasi agar penembakan perintah Noise maksimal hanya 1 kali setiap 5 detik
            if ((current_time - last_query_time) >= throttle_delay) {
                // Tembakkan secepat kilat tepat di momen "Rising Edge"
                uart_write_bytes(UART_NUM_2, (const char*)query_noise_cmd, sizeof(query_noise_cmd));
                last_query_time = current_time;
                // Hilangkan log ini nanti di versi final agar lebih cepat, 
                // ini hanya untuk visualisasi debugging Anda.
                ESP_LOGW(TAG, "[ISR TRIGGER] Celah terbuka! Meminta data Noise...");
            }
            
        } else {
            // Jika 10 detik berlalu tanpa ada pesan sama sekali (udara murni kosong),
            // paksa ambil noise floor jika pin AUX memang sedang HIGH.
            if (gpio_get_level(LORA_AUX_PIN) == 1) {
                uart_write_bytes(UART_NUM_2, (const char*)query_noise_cmd, sizeof(query_noise_cmd));
                last_query_time = xTaskGetTickCount();
            }
        }
    }
}

// ------------------------------------------------------------------
// TASK 2: SMART RECEIVER (Tunggal & Tidak Terputus)
// ------------------------------------------------------------------
void lora_rx_task(void *pvParameters) {
    uint8_t data[BUF_SIZE];
    uart_flush_input(UART_NUM_2);

    while (1) {
        int len = uart_read_bytes(UART_NUM_2, data, BUF_SIZE - 1, pdMS_TO_TICKS(100));
        
        if (len > 0) {
            
            // FASE A: SCANNER - Cari balasan Noise di seluruh area buffer
            int noise_idx = -1;
            // Looping untuk memindai setiap byte
            for (int i = 0; i <= len - 4; i++) {
                if (data[i] == 0xC1 && data[i+1] == 0x00 && data[i+2] == 0x01) {
                    noise_idx = i;
                    break; // Ditemukan! Catat index-nya dan hentikan pencarian
                }
            }

            // Jika "paket penyusup" (Noise Floor) ditemukan di dalam buffer
            if (noise_idx != -1) {
                // Ekstrak byte ke-4 sebagai nilai Noise
                uint8_t noise_hex = data[noise_idx + 3];
                current_noise_floor_dbm = - (256 - (int)noise_hex);
                ESP_LOGW(TAG, "[SYSTEM] Noise Floor Update: %d dBm", current_noise_floor_dbm);

                // OPERASI BEDAH: Cabut 4 byte noise, dan rapatkan kembali sisa array-nya
                int bytes_to_shift = len - (noise_idx + 4);
                if (bytes_to_shift > 0) {
                    memmove(&data[noise_idx], &data[noise_idx + 4], bytes_to_shift);
                }
                len -= 4; // Panjang total paket sekarang berkurang 4 byte
            }

            // FASE B: Ekstrak Teks dan RSSI (Sekarang buffer sudah 100% bersih dari "C1 00 01")
            if (len > 1) {
                // Ambil RSSI yang benar-benar berada di ujung (bukan tersandung Noise lagi)
                uint8_t rssi_byte = data[len - 1];
                int rssi_dbm = (int)rssi_byte - 256; 
                int snr_db = rssi_dbm - current_noise_floor_dbm;

                // Potong byte RSSI agar pesan jadi string murni
                data[len - 1] = '\0'; 

                ESP_LOGI(TAG, ">>> PESAN MASUK : %s", (char*)data);
                ESP_LOGI(TAG, "    [RF DATA] RSSI: %d dBm | SNR: %d dB | Floor Noise: %d dBm", 
                         rssi_dbm, snr_db, current_noise_floor_dbm);
                
                gpio_set_level(GREEN_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(20));
                gpio_set_level(GREEN_LED_PIN, 0);
            }
        }
    }
}

void app_main(void) {
    init_all_hardware();
    configure_lora_channel();

    // 1. Inisialisasi Binary Semaphore
    aux_idle_sem = xSemaphoreCreateBinary();

    // 2. Konfigurasi Interrupt pada Pin AUX
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,       // Trigger pada Rising Edge (0 -> 1)
        .pin_bit_mask = (1ULL << LORA_AUX_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1
    };
    gpio_config(&io_conf);
    
    // 3. Pasang layanan ISR dan hubungkan ke fungsi handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(LORA_AUX_PIN, aux_isr_handler, NULL);

    // 4. Eksekusi Task. RX diberi prioritas lebih tinggi (5) daripada Noise (4).
    xTaskCreate(lora_rx_task, "lora_rx", 4096, NULL, 5, NULL);
    xTaskCreate(lora_noise_task, "lora_noise", 2048, NULL, 4, NULL);
}