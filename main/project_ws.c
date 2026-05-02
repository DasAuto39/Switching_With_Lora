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
#include "mbedtls/aes.h"
#include "location.c"

// Kunci Rahasia AES-128 (Wajib 16 Karakter / 16 Byte)
const unsigned char AES_SECRET_KEY[16] = "VMS_ITS_KEY_2026";

// Mengonversi data biner ke teks Hex (contoh: 0A 1B 2C...)
void bin_to_hex(const unsigned char* bin, int len, char* hex_out) {
    for(int i = 0; i < len; i++) {
        sprintf(hex_out + (i * 2), "%02X", bin[i]);
    }
}

// Mengonversi teks Hex kembali ke data biner
void hex_to_bin(const char* hex_in, int len, unsigned char* bin_out) {
    for(int i = 0; i < len; i += 2) {
        sscanf(hex_in + i, "%2hhx", &bin_out[i / 2]);
    }
}

// Fungsi Enkripsi AES-128 (Mengubah String Teks menjadi Biner Terenkripsi)
// Payload GPS kita butuh 2 blok AES (2 x 16 byte = 32 byte)
void encrypt_payload(const char* input_str, unsigned char* output_bin) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, AES_SECRET_KEY, 128);

    unsigned char input_padded[32] = {0}; 
    strcpy((char*)input_padded, input_str); // Otomatis mengisi sisa ruang dengan {0}

    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, input_padded, output_bin);
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, input_padded + 16, output_bin + 16);

    mbedtls_aes_free(&aes);
}

// Fungsi Dekripsi AES-128 (Mengembalikan Biner Terenkripsi menjadi Teks Asli)
void decrypt_payload(const unsigned char* input_bin, char* output_str) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, AES_SECRET_KEY, 128);

    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, input_bin, (unsigned char*)output_str);
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, input_bin + 16, (unsigned char*)(output_str + 16));

    mbedtls_aes_free(&aes);
}

// ==============================================================================
// MASTER SWITCH: TENTUKAN PERANGKAT YANG AKAN DI-FLASH DI SINI!
// Silakan Uncomment salah satu baris di bawah ini, dan Comment yang lainnya.
// ==============================================================================

#define COMPILE_NODE_B  // Aktifkan baris ini untuk mem-flash Kapal (GPS + Slave)
//#define COMPILE_NODE_B  // Aktifkan baris ini untuk mem-flash Pelabuhan (Master) circuit my 

// ==============================================================================

#ifdef COMPILE_NODE_A
// ==============================================================================
// KODE KHUSUS NODE A (KAPAL / SLAVE)
// ==============================================================================

static const char *TAG = "NODE_A_KAPAL";

// Pin GPS (Revisi Bebas Bentrok)


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
    //ESP_LOGI(TAG, "ini didalam slave gps");
    uint8_t data[BUF_SIZE] = {0};
    char line[128] = {0};

    int line_pos = 0;

    while (1) {
        memset(data, 0, BUF_SIZE);
        int len = uart_read_bytes(GPS_UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(100));
        
        for (int i = 0; i < len; i++) {
            char c = (char)data[i];
            //ESP_LOGI(TAG, "Data from gps uart : %c", c);
            if (c == '\n') {
                line[line_pos] = '\0'; 
                //ESP_LOGI(TAG,"LINE : %s", line);
                if (strncmp(line, "$GPRMC", 6) == 0) {
                    char *tokens[15];
                    int token_count = 0;
                    char *token = strtok(line, ",");
                    //ESP_LOGI(TAG,"token : %c", token);
                    while (token != NULL && token_count < 15) {
                        tokens[token_count++] = token;
                        token = strtok(NULL, ",");
                        
                    }
                    if (token_count > 6 && strcmp(tokens[2], "A") == 0) {
                        float raw_lat = atof(tokens[3]);
                        char lat_dir = tokens[4][0];
                        float raw_lon = atof(tokens[5]);
                        char lon_dir = tokens[6][0];
                        //ESP_LOGI(TAG,"raw lat : %f | lat dir : %c | raw lon : %f | lon dir : %c", raw_lat,lat_dir,raw_lon,lon_dir);
                        
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
                    //ESP_LOGI(TAG,"LINE : %s", line);

            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Task LoRa Slave
void lora_slave_task(void *pvParameters) {
    
    char payload[128] = {0};
    
    while (1) {
        if (xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (is_gps_valid) {
                //snprintf(payload, sizeof(payload), "LAT:%.6f,LON:%.6f", current_lat, current_lon);
                //snprintf(payload, sizeof(payload), "TES");
                snprintf(payload, sizeof(payload), "GPS_NO_FIX");
            } else {
                snprintf(payload, sizeof(payload), "GPS_NO_FIX");
            }
            xSemaphoreGive(gps_mutex);
        }
        uart_flush_input(UART_NUM_2);
        vTaskDelay(pdMS_TO_TICKS(100)); 
        // Broadcast data ke udara
        uart_write_bytes(UART_NUM_2, payload, strlen(payload));
        ESP_LOGI(TAG, "=> Memancarkan Pesan: %s", payload);
        
        
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}

void app_main(void) {
    ESP_LOGW(TAG, "MEMULAI FIRMWARE NODE A (KAPAL)");
    gps_mutex = xSemaphoreCreateMutex();
    init_all_hardware();
    //configure_lora_channel();
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
    

    xTaskCreate(gps_reading_task, "gps_task", 4096, NULL, 4, NULL);
    xTaskCreate(lora_slave_task, "lora_slave", 4096, NULL, 5, NULL);
    //ESP_LOGI(TAG, "ini disini");
}

#elif defined(COMPILE_NODE_B)
// ==============================================================================
// KODE KHUSUS NODE B (PELABUHAN / MASTER)
// ==============================================================================

static const char *TAG = "NODE_B_MASTER";
int current_noise_floor_dbm = -105;
int noise_retry_count = 0;
const int MAX_NOISE_RETRIES = 5;
int last_valid_rssi_dbm = 0; 


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
    uint8_t data[BUF_SIZE] = {0};
    
    while (1) {
        // Terus membaca buffer UART
        //uart_flush_input(UART_NUM_2); 
        //vTaskDelay(pdMS_TO_TICKS(50));

        int len = uart_read_bytes(UART_NUM_2, data, BUF_SIZE - 1, pdMS_TO_TICKS(100));

        
        if (len > 0) {
            // FILTER 1: APAKAH INI BALASAN AMBIENT NOISE?
            // Ebyte selalu membalas command dengan 4 byte, diawali 0xC1
            if (len == 4 && data[0] == 0xC1) {
                int temp_noise = - (256 - (int)data[3]);
                
                if(abs(temp_noise - last_valid_rssi_dbm) <= 15)
                {
                    noise_retry_count++;
                    ESP_LOGW(TAG, "Noise mencurigakan (%d dBm) mirip RSSI. Retry: %d/%d", 
                             temp_noise, noise_retry_count, MAX_NOISE_RETRIES);
                    
                    if (noise_retry_count >= MAX_NOISE_RETRIES) {
                        ESP_LOGW(TAG, "Limit retry tercapai. Menerima %d dBm sebagai Noise Valid.", temp_noise);
                        current_noise_floor_dbm = temp_noise;
                        noise_retry_count = 0; // Reset counter
                    } else {
                        // Jika belum 5 kali, tembak ulang request noise secara instan
                        uint8_t query_noise_cmd[] = {0xC0, 0xC1, 0xC2, 0xC3, 0x00, 0x01};
                        uart_write_bytes(UART_NUM_2, (const char*)query_noise_cmd, sizeof(query_noise_cmd));
                    }
                }
                else {
                    // Jika nilainya jauh dari RSSI, berarti aman
                    current_noise_floor_dbm = temp_noise;
                    noise_retry_count = 0; // Reset counter
                    ESP_LOGI(TAG, "Update Ambient Noise: %d dBm", current_noise_floor_dbm);
                }
            }
            // FILTER 2: JIKA BUKAN 0xC1, BERARTI INI PAYLOAD RF BEACON
            else if (len > 1) { 
                uint8_t rssi_byte = data[len - 1];
                int rssi_dbm = (int)rssi_byte - 256; 

                int snr_db = rssi_dbm - current_noise_floor_dbm;

                last_valid_rssi_dbm = rssi_dbm;
                
                data[len - 1] = '\0'; // Potong byte RSSI dari string
                
                char* payload = (char*)data;

                ESP_LOGI(TAG, "     [PESAN MASUK : %s", payload);
                ESP_LOGI(TAG, "     RSSI: %d dBm | SNR: %d dB | Ambient Noise: %d", rssi_dbm, snr_db, current_noise_floor_dbm);
                float ship_lat, ship_lon;
                /*
                if (strncmp(payload, "GPS_NO_FIX", 10) != 0) {
                    float ship_lat, ship_lon;
                    if (sscanf(payload, "LAT:%f,LON:%f", &ship_lat, &ship_lon) == 2) {
                        float distance = calculate_distance(BASE_LAT, BASE_LON, ship_lat, ship_lon);
                        ESP_LOGI(TAG, "    [SPASIAL] Jarak: %.3f KM", distance);
                        gpio_set_level(GREEN_LED_PIN, 1);
                        vTaskDelay(pdMS_TO_TICKS(50));
                        gpio_set_level(GREEN_LED_PIN, 0);
                    } 
                    
                    else {
                        // Data Tidak Sesuai Harapan / Format Salah -> Kedip Merah
                        ESP_LOGE(TAG, "Bukan Data yang Diharapkan.");
                        
                        gpio_set_level(RED_LED_PIN, 1);
                        vTaskDelay(pdMS_TO_TICKS(50));
                        gpio_set_level(RED_LED_PIN, 0);
                    }
                    
                } 
                */
               if (strncmp(payload, "GPS_NO_FIX", 10) == 0) {
                    ESP_LOGW(TAG, "    [SPASIAL] Kapal belum mendapat sinyal GPS.");
                    
                    // Data dikenali (Asli) -> Kedip Hijau
                    gpio_set_level(GREEN_LED_PIN, 1);
                    vTaskDelay(pdMS_TO_TICKS(50));
                    gpio_set_level(GREEN_LED_PIN, 0);
                }
                else if (sscanf(payload, "LAT:%f,LON:%f", &ship_lat, &ship_lon) == 2) {
                    float distance = calculate_distance(BASE_LAT, BASE_LON, ship_lat, ship_lon);
                    ESP_LOGI(TAG, "    [SPASIAL] Jarak: %.3f KM", distance);
                    ESP_LOGI(TAG, "Longitude : %.6f Latitude : %.6f", ship_lon, ship_lat);
                    
                    // Data dikenali (Asli) -> Kedip Hijau
                    gpio_set_level(GREEN_LED_PIN, 1);
                    vTaskDelay(pdMS_TO_TICKS(50));
                    gpio_set_level(GREEN_LED_PIN, 0);
                }
                else {
                    ESP_LOGE(TAG, "Bukan Data yang Diharapkan");
                    
                    // Data tidak valid -> Kedip Merah
                    gpio_set_level(RED_LED_PIN, 1);
                    vTaskDelay(pdMS_TO_TICKS(50));
                    gpio_set_level(RED_LED_PIN, 0);
                }
                
                
                
            }
            else {
                //jika ada sampah di buffer
                ESP_LOGW(TAG, "Data tidak dikenal masuk, len: %d, byte[0]: 0x%02X", len, data[0]);
            }
        }
    }
}

void noise_reading_task(void *pvParameters) {
    uint8_t query_noise_cmd[] = {0xC0, 0xC1, 0xC2, 0xC3, 0x00, 0x01};
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(15000));
        uart_flush_input(UART_NUM_2); 
        vTaskDelay(pdMS_TO_TICKS(50));

        ESP_LOGI(TAG, "--- Meminta Update Ambient Noise ---");
        uart_write_bytes(UART_NUM_2, (const char*)query_noise_cmd, sizeof(query_noise_cmd));
    }
}

void app_main(void) {
    
    ESP_LOGW(TAG, "MEMULAI FIRMWARE NODE B (PELABUHAN)");
    
    init_all_hardware();
    configure_lora_channel();
    xTaskCreate(lora_master_task, "lora_master", 4096, NULL, 5, NULL);
    xTaskCreate(noise_reading_task, "noise_task", 4096, NULL, 4, NULL);
}

#else
// ==============================================================================
// ERROR HANDLING JIKA LUPA MEMILIH NODE
// ==============================================================================
#error "ANDA BELUM MEMILIH NODE! Silakan uncomment COMPILE_NODE_A atau COMPILE_NODE_B di atas."
#endif