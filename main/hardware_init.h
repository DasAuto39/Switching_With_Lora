#ifndef HARDWARE_INIT_H
#define HARDWARE_INIT_H

#include <stdint.h>
#define COMPILE_MINSIS
//#define COMPILE_BREADBOARD

#ifdef COMPILE_MINSIS



// --- KONFIGURASI PIN (MINSIS CUSTOM) ---
#define LORA_UART_NUM      2 
#define LORA_TXD_PIN       26   // Sesuai rute RXLD ke IO14
#define LORA_RXD_PIN       27 // Sesuai rute TXLD ke IO27

// Pin Mode LoRa tidak disambungkan ke ESP32 pada skematik
#define LORA_M0_PIN        -1   
#define LORA_M1_PIN        -1   
#define LORA_AUX_PIN       -1   

#define GPS_UART_NUM       1 
#define GPS_RX_PIN_ESP     16   // Sesuai rute TXGD ke IO16
#define GPS_TX_PIN_ESP     17   // Sesuai rute RXGD ke IO17

// Komponen ekstra ini tidak ada di skematik klien
#define GREEN_LED_PIN      -1   
#define RED_LED_PIN        -1   
#define BUTTON_PIN         -1
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

#elif defined(COMPILE_BREADBOARD)

// --- KONFIGURASI PIN ---
#define LORA_UART_NUM      2 // Menggunakan angka 2 langsung lebih aman untuk header
#define LORA_TXD_PIN       17 
#define LORA_RXD_PIN       16 
#define LORA_M0_PIN        23
#define LORA_M1_PIN        22
#define LORA_AUX_PIN       4   
#define GREEN_LED_PIN      32
#define RED_LED_PIN        33
#define BUTTON_PIN         -1//26
//#define GPS_RX_PIN_ESP -1//32  // Ke pin TX modul Neo-6M
//#define GPS_TX_PIN_ESP -1//33  // Ke pin RX modul Neo-6M
//#define GPS_UART_NUM UART_NUM_1

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
#endif


#endif // HARDWARE_INIT_H