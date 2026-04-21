#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "hardware_init.h"

// ==============================================================================
// MASTER SWITCH: TENTUKAN PERANGKAT YANG AKAN DI-FLASH DI SINI!
// Silakan Uncomment salah satu baris di bawah ini, dan Comment yang lainnya.
// ==============================================================================

#define COMPILE_NODE_B  // Aktifkan baris ini untuk mem-flash Kapal (GPS + Slave)
//#define COMPILE_NODE_B  // Aktifkan baris ini untuk mem-flash Pelabuhan (Master)

// ==============================================================================

#ifdef COMPILE_NODE_A
// ==============================================================================
// KODE KHUSUS NODE A (KAPAL / SLAVE)
// ==============================================================================

static const char *TAG = "NODE_A_KAPAL";

// Pin GPS (Revisi Bebas Bentrok)
#define GPS_RX_PIN_ESP 32  // Ke pin TX modul Neo-6M
#define GPS_TX_PIN_ESP 33  // Ke pin RX modul Neo-6M
#define GPS_UART_NUM UART_NUM_1

SemaphoreHandle_t gps_mutex;
float current_lat = 0.0;
float current_lon = 0.0;
bool is_gps_valid = false;

// Konverter NMEA ke Desimal
float convert_nmea_to_decimal(float nmea_coord, char direction) {
    int degrees = (int)(nmea_coord / 100);
    float minutes = nmea_coord - (degrees * 100);
    float decimal = degrees + (minutes / 60.0);
    if (direction == 'S' || direction == 'W') {
        decimal *= -1.0;
    }
    return decimal;
}

// Task GPS Background
void gps_reading_task(void *pvParameters) {
    uint8_t data[BUF_SIZE];
    char line[128];
    int line_pos = 0;

    while (1) {
        int len = uart_read_bytes(GPS_UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(100));
        for (int i = 0; i < len; i++) {
            char c = (char)data[i];
            if (c == '\n') {
                line[line_pos] = '\0'; 
                if (strncmp(line, "$GPRMC", 6) == 0) {
                    char *tokens[15];
                    int token_count = 0;
                    char *token = strtok(line, ",");
                    while (token != NULL && token_count < 15) {
                        tokens[token_count++] = token;
                        token = strtok(NULL, ",");
                    }
                    if (token_count > 6 && strcmp(tokens[2], "A") == 0) {
                        float raw_lat = atof(tokens[3]);
                        char lat_dir = tokens[4][0];
                        float raw_lon = atof(tokens[5]);
                        char lon_dir = tokens[6][0];

                        if (xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                            current_lat = convert_nmea_to_decimal(raw_lat, lat_dir);
                            current_lon = convert_nmea_to_decimal(raw_lon, lon_dir);
                            is_gps_valid = true;
                            xSemaphoreGive(gps_mutex);
                        }
                    } else {
                        is_gps_valid = false;
                    }
                }
                line_pos = 0; 
            } else if (c != '\r' && line_pos < sizeof(line) - 1) {
                line[line_pos++] = c;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Task LoRa Slave
void lora_slave_task(void *pvParameters) {
    uint8_t data[BUF_SIZE];
    uart_flush_input(UART_NUM_2);

    while (1) {
        int len = uart_read_bytes(UART_NUM_2, data, BUF_SIZE - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            data[len - 1] = '\0'; 
            if (strncmp((char*)data, "REQ_DATA", 8) == 0) {
                char payload[128];
                if (xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    if (is_gps_valid) {
                        snprintf(payload, sizeof(payload), "LAT:%.6f,LON:%.6f", current_lat, current_lon);
                    } else {
                        snprintf(payload, sizeof(payload), "GPS_NO_FIX");
                    }
                    xSemaphoreGive(gps_mutex);
                }
                vTaskDelay(pdMS_TO_TICKS(50)); 
                uart_write_bytes(UART_NUM_2, payload, strlen(payload));
                ESP_LOGI(TAG, "=> Membalas Master: %s", payload);
                
                gpio_set_level(GREEN_LED_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(20));
                gpio_set_level(GREEN_LED_PIN, 0);
            }
        }
    }
}

void app_main(void) {
    ESP_LOGW(TAG, "MEMULAI FIRMWARE NODE A (KAPAL)");
    gps_mutex = xSemaphoreCreateMutex();
    init_all_hardware();
    
    uart_config_t gps_uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(GPS_UART_NUM, &gps_uart_config);
    uart_set_pin(GPS_UART_NUM, GPS_TX_PIN_ESP, GPS_RX_PIN_ESP, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(GPS_UART_NUM, 1024 * 2, 0, 0, NULL, 0);

    configure_lora_channel();

    xTaskCreate(gps_reading_task, "gps_task", 4096, NULL, 4, NULL);
    xTaskCreate(lora_slave_task, "lora_slave", 4096, NULL, 5, NULL);
}

#elif defined(COMPILE_NODE_B)
// ==============================================================================
// KODE KHUSUS NODE B (PELABUHAN / MASTER)
// ==============================================================================

static const char *TAG = "NODE_B_MASTER";
int current_noise_floor_dbm = -105; 

// Koordinat Base Station 
const float BASE_LAT = -7.284916; 
const float BASE_LON = 112.795808;

float calculate_distance(float lat1, float lon1, float lat2, float lon2) {
    float dLat = (lat2 - lat1) * M_PI / 180.0;
    float dLon = (lon2 - lon1) * M_PI / 180.0;
    lat1 = (lat1) * M_PI / 180.0;
    lat2 = (lat2) * M_PI / 180.0;
    float a = pow(sin(dLat / 2), 2) + pow(sin(dLon / 2), 2) * cos(lat1) * cos(lat2);
    float rad = 6371.0; 
    float c = 2 * asin(sqrt(a));
    return rad * c; 
}

void lora_master_task(void *pvParameters) {
    uint8_t data[BUF_SIZE];
    uint8_t query_noise_cmd[] = {0xC0, 0xC1, 0xC2, 0xC3, 0x00, 0x01};
    const char* req_cmd = "REQ_DATA";
    
    while (1) {
        // Fase 1: Baca Noise
        uart_flush_input(UART_NUM_2); 
        uart_write_bytes(UART_NUM_2, (const char*)query_noise_cmd, sizeof(query_noise_cmd));
        int len = uart_read_bytes(UART_NUM_2, data, 4, pdMS_TO_TICKS(200));
        if (len == 4 && data[0] == 0xC1) {
            current_noise_floor_dbm = - (256 - (int)data[3]);
        } 

        // Fase 2: Panggil Kapal
        ESP_LOGW(TAG, "Meminta data dari Kapal...");
        uart_flush_input(UART_NUM_2); 
        uart_write_bytes(UART_NUM_2, req_cmd, strlen(req_cmd));

        // Fase 3 & 4: Terima & Kalkulasi
        len = uart_read_bytes(UART_NUM_2, data, BUF_SIZE - 1, pdMS_TO_TICKS(2000));
        if (len > 0) {
            uint8_t rssi_byte = data[len - 1];
            int rssi_dbm = (int)rssi_byte - 256; 
            int snr_db = rssi_dbm - current_noise_floor_dbm;
            data[len - 1] = '\0'; 
            char* payload = (char*)data;

            ESP_LOGI(TAG, ">>> PESAN MASUK : %s", payload);
            ESP_LOGI(TAG, "    [RF DATA] RSSI: %d dBm | SNR: %d dB", rssi_dbm, snr_db);

            if (strncmp(payload, "GPS_NO_FIX", 10) != 0) {
                float ship_lat, ship_lon;
                if (sscanf(payload, "LAT:%f,LON:%f", &ship_lat, &ship_lon) == 2) {
                    float distance = calculate_distance(BASE_LAT, BASE_LON, ship_lat, ship_lon);
                    ESP_LOGI(TAG, "    [SPASIAL] Jarak: %.3f KM", distance);
                    ESP_LOGI(TAG, "Longitude : %.6f Latitude : %.6f", ship_lon, ship_lat);
                } 
            } else {
                ESP_LOGE(TAG, "    [SPASIAL] Satelit GPS belum terkunci!");
            }

            
            gpio_set_level(GREEN_LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(GREEN_LED_PIN, 0);
        } else {
            ESP_LOGE(TAG, "Timeout! Kapal tidak merespons.");
        }
        ESP_LOGW(TAG, "--------------------------------------------------");
        vTaskDelay(pdMS_TO_TICKS(3000)); 
    }
}

void app_main(void) {
    ESP_LOGW(TAG, "MEMULAI FIRMWARE NODE B (PELABUHAN)");
    init_all_hardware();
    configure_lora_channel();
    xTaskCreate(lora_master_task, "lora_master", 4096, NULL, 5, NULL);
}

#else
// ==============================================================================
// ERROR HANDLING JIKA LUPA MEMILIH NODE
// ==============================================================================
#error "ANDA BELUM MEMILIH NODE! Silakan uncomment COMPILE_NODE_A atau COMPILE_NODE_B di atas."
#endif