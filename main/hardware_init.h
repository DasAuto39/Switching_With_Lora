#ifndef HARDWARE_INIT_H
#define HARDWARE_INIT_H

#include <stdint.h>

// --- KONFIGURASI PIN ---
#define LORA_UART_NUM      2 // Menggunakan angka 2 langsung lebih aman untuk header
#define LORA_TXD_PIN       17 
#define LORA_RXD_PIN       16 
#define LORA_M0_PIN        4
#define LORA_M1_PIN        5
#define LORA_AUX_PIN       27   
#define GREEN_LED_PIN      23
#define RED_LED_PIN        25
#define BUTTON_PIN         26

// --- KONFIGURASI UART ---
#define LORA_BAUD_RATE     9600
#define BUF_SIZE           (1024)

// --- DEKLARASI FUNGSI (Prototypes) ---
void button_init(void);
void led_init(void);
void m0_m1_lora_init(void);
void aux_lora_init(void);
void uart_lora_init(void);
void init_all_hardware(void); // Fungsi pembungkus (Opsional tapi disarankan)
void configure_lora_channel(void);

#endif // HARDWARE_INIT_H