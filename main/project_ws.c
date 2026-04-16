#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

// Definisikan pin LED. Ubah jika dev board Anda menggunakan pin lain.
#define BLINK_GPIO 23

void app_main(void)
{
    // 1. Inisialisasi dan konfigurasi GPIO
    // Sangat disarankan di ESP-IDF v5+ untuk mereset pin sebelum digunakan
    gpio_reset_pin(BLINK_GPIO); 
    
    // Set arah pin sebagai output
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    uint8_t led_state = 0;

    printf("Blink test started. Mempersiapkan modul untuk integrasi E220...\n");

    // 2. Loop utama
    while (1) {
        // Toggle state
        led_state = !led_state;
        
        // Terapkan state ke GPIO
        gpio_set_level(BLINK_GPIO, led_state);
        
        // Print status ke serial monitor untuk verifikasi tambahan
        printf("LED State: %s\n", led_state == 1 ? "ON" : "OFF");
        
        // Delay 1000 milidetik (1 detik)
        // portTICK_PERIOD_MS memastikan durasi delay akurat terlepas dari konfigurasi tick rate FreeRTOS
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}