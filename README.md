# 🚢 Switching With LoRa — Sistem Switching Komunikasi Berbasis LoRa

## 📌 Tentang Projek Ini

Projek ini merupakan sistem embedded yang dirancang untuk melakukan **switching (perpindahan) teknologi komunikasi secara cerdas** pada lingkungan maritim. Sistem ini memanfaatkan modul **LoRa (Long Range)** sebagai jalur komunikasi utama antara kapal dan pelabuhan, serta modul **GPS** untuk mengetahui posisi kapal secara real-time.

Data telemetri berupa kualitas sinyal (RSSI, SNR), jarak, dan tingkat keberhasilan pengiriman paket (PDR) dikumpulkan oleh perangkat di sisi pelabuhan, kemudian dikirimkan ke cloud melalui protokol **MQTT**. Data tersebut selanjutnya akan digunakan oleh model **Machine Learning** untuk mengambil keputusan — apakah komunikasi sebaiknya tetap menggunakan LoRa, atau perlu beralih (*switching*) ke teknologi komunikasi lain yang lebih optimal pada kondisi tersebut.

Secara gambaran besar, projek ini menjadi bagian dari riset tentang bagaimana keputusan perpindahan teknologi komunikasi dapat dilakukan secara **otomatis dan berbasis data**, bukan lagi bergantung pada keputusan manual.

> **Catatan:** Repository ini mencakup bagian **embedded/firmware** saja. Bagian Machine Learning tidak termasuk di dalamnya.

---

## ❓ Rumusan Masalah

Komunikasi di lingkungan laut memiliki beberapa tantangan yang cukup signifikan:

1. **Jarak komunikasi yang besar** — Kapal dapat berlayar hingga puluhan kilometer dari pelabuhan, sehingga dibutuhkan teknologi komunikasi dengan jangkauan yang memadai.
2. **Kualitas sinyal yang tidak stabil** — Kondisi di laut menyebabkan kualitas sinyal berfluktuasi akibat faktor cuaca, gelombang, dan interferensi. Diperlukan mekanisme pemantauan kualitas sinyal secara real-time.
3. **Tidak adanya mekanisme switching otomatis** — Ketika kualitas sinyal LoRa menurun, belum tersedia sistem yang mampu secara otomatis beralih ke jalur komunikasi lain. Proses perpindahan masih dilakukan secara manual dan kurang efisien.
4. **Minimnya data telemetri sebagai dasar keputusan** — Tanpa pengukuran parameter seperti RSSI, SNR, noise floor, dan jarak secara berkala, tidak terdapat landasan data yang memadai untuk mendukung pengambilan keputusan switching yang tepat.

---

## 🎯 Tujuan

1. Membangun sistem komunikasi antara kapal dan pelabuhan menggunakan **LoRa** sebagai medium utama transmisi data.
2. Mengumpulkan data telemetri komunikasi (**RSSI, SNR, jarak, dan PDR**) secara real-time untuk dijadikan dataset.
3. Mengirimkan data telemetri ke cloud melalui **MQTT broker** agar dapat diproses oleh model Machine Learning sebagai dasar keputusan switching.
4. Mengamankan data yang ditransmisikan melalui LoRa menggunakan enkripsi **AES-128** untuk menjaga kerahasiaan informasi.
5. Menyediakan infrastruktur embedded yang mendukung pengambilan keputusan switching berbasis **probabilistik** — yaitu perpindahan ke teknologi komunikasi yang lebih optimal berdasarkan kondisi sinyal pada saat itu.

---

## 💡 Solusi yang Diterapkan

Sistem ini terdiri dari **dua node** dengan peran yang berbeda:

### Node A — Kapal (Slave)
- Membaca data koordinat GPS dari modul **Neo-6M** dengan melakukan parsing terhadap sentence NMEA `$GPRMC`.
- Mengonversi koordinat dari format NMEA ke format **desimal** (latitude dan longitude).
- Mengirimkan data lokasi kapal ke pelabuhan melalui modul **LoRa E220-900T22D**. Apabila GPS belum mendapatkan sinyal, sistem akan mengirimkan status `GPS_NO_FIX`.

### Node B — Pelabuhan (Master)
- Menerima data lokasi dari kapal melalui LoRa.
- Menghitung nilai **RSSI** (kekuatan sinyal yang diterima) dan **SNR** (perbandingan sinyal terhadap noise) dari setiap paket yang masuk.
- Membaca **Ambient Noise Floor** dari modul LoRa secara periodik sebagai referensi tingkat noise lingkungan.
- Menghitung **jarak** antara kapal dan pelabuhan menggunakan rumus **Haversine**.
- Menghitung **PDR (Packet Delivery Ratio)** — persentase paket yang berhasil diterima dari total paket yang diharapkan.
- Mengirimkan seluruh data telemetri ke **HiveMQ MQTT Broker** dalam format **JSON**.
- Menerima keputusan switching dari model Machine Learning melalui MQTT (subscribe).

### Diagram Alur Sistem

```
[Kapal + GPS] --LoRa--> [Pelabuhan] --WiFi/MQTT--> [Cloud/ML] --MQTT--> [Keputusan Switching]
```

---

## 📊 Parameter dan Nilai yang Digunakan

### Konfigurasi Komunikasi LoRa
| Parameter | Nilai |
|---|---|
| Baud Rate | 9600 bps |
| Frekuensi Operasi | 900 MHz (E220-900T22D) |
| Ukuran Buffer | 1024 byte |
| Interval Pengiriman Data | 2 detik |
| Interval Pembacaan Noise | 15 detik |

### Parameter Telemetri yang Dikumpulkan
| Parameter | Satuan | Keterangan |
|---|---|---|
| **RSSI** | dBm | Kekuatan sinyal yang diterima. Semakin mendekati 0, semakin kuat sinyalnya. |
| **SNR** | dB | Selisih antara kekuatan sinyal dan noise. Semakin besar nilainya, semakin bersih sinyal yang diterima. |
| **Ambient Noise** | dBm | Tingkat noise lingkungan sekitar. Nilai default awal: -105 dBm. |
| **Jarak** | km | Jarak antara kapal dan base station, dihitung menggunakan rumus Haversine. |
| **PDR** | % | Persentase paket yang berhasil diterima dibandingkan total paket yang seharusnya diterima. |

### Koordinat Base Station (Pelabuhan)
| Parameter | Nilai |
|---|---|
| Latitude | -7.284916 |
| Longitude | 112.795808 |

### Keamanan Data
| Parameter | Nilai |
|---|---|
| Metode Enkripsi | AES-128 ECB |
| Panjang Kunci | 16 byte (128 bit) |
| Kunci Rahasia | `VMS_ITS_KEY_2026` |

### Konfigurasi MQTT
| Parameter | Nilai |
|---|---|
| Broker | `mqtt://broker.hivemq.com` |
| Topic Publish (Telemetri) | `vms_hybrid_2026/telemetry` |
| Topic Subscribe (Keputusan ML) | `vms_hybrid_2026/switching_decision` |

---

## 🔧 Sensor dan Perangkat Keras

### Mikrokontroler
- **ESP32** — Mikrokontroler utama yang menjalankan seluruh logika sistem. Dipilih karena memiliki dukungan WiFi bawaan, jumlah GPIO yang memadai, serta kemampuan menjalankan **FreeRTOS** untuk multitasking.

### Modul Komunikasi LoRa
- **E220-900T22D (EBYTE)** — Modul transceiver LoRa yang beroperasi pada frekuensi 900 MHz. Modul ini mampu berkomunikasi pada jarak beberapa kilometer dengan konsumsi daya yang rendah, serta memiliki fitur pembacaan ambient noise.

### Modul GPS
- **Neo-6M (u-blox)** — Modul GPS yang mengirimkan data posisi melalui UART dalam format NMEA. Sentence yang di-parsing pada projek ini adalah `$GPRMC` untuk mendapatkan koordinat latitude dan longitude.

### Indikator LED
- **LED Hijau (GPIO 32)** — Menyala sesaat ketika data yang diterima teridentifikasi sebagai paket valid.
- **LED Merah (GPIO 33)** — Menyala sesaat ketika data yang diterima tidak dikenali oleh sistem.

### Konektivitas Internet
- **WiFi (built-in ESP32)** — Digunakan pada Node B (Pelabuhan) untuk terhubung ke jaringan internet dan melakukan publish data telemetri ke MQTT broker.

---

## 🏗️ Struktur Projek

```
Switching_With_Lora/
├── project_ws/
│   └── main/
│       ├── project_ws.c      # Kode utama (Node A dan Node B)
│       ├── hardware_init.c   # Inisialisasi hardware (GPIO, UART, LoRa)
│       ├── hardware_init.h   # Konfigurasi pin dan deklarasi fungsi
│       └── location.c        # Koordinat base station
├── datasheet/                # Datasheet komponen yang digunakan
├── pertemuan/                # Catatan progress pertemuan
├── build/                    # Hasil build firmware
└── README.md                 # Dokumentasi projek
```

---

## ⚙️ Cara Build dan Flash

Projek ini menggunakan **ESP-IDF** (framework resmi dari Espressif untuk ESP32).

1. **Instalasi ESP-IDF** — Ikuti panduan resmi di [docs.espressif.com](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
2. **Pemilihan Node** — Buka file `project_ws.c`, kemudian uncomment salah satu:
   ```c
   #define COMPILE_NODE_A  // Untuk flash ke perangkat Kapal (Slave + GPS)
   #define COMPILE_NODE_B  // Untuk flash ke perangkat Pelabuhan (Master)
   ```
3. **Pemilihan Board** — Buka file `hardware_init.h`, kemudian uncomment salah satu:
   ```c
   #define COMPILE_MINSIS       // Untuk PCB custom
   #define COMPILE_BREADBOARD   // Untuk rangkaian breadboard
   ```
4. **Build dan Flash**:
   ```bash
   cd project_ws
   idf.py build
   idf.py -p /dev/ttyUSBx flash monitor
   ```

---

## 📡 Format Data

### Transmisi LoRa (Kapal → Pelabuhan)
```
LAT:-7.284916,LON:112.795808    # Koordinat GPS valid
GPS_NO_FIX                       # GPS belum mendapatkan sinyal
```

### Publish MQTT (Pelabuhan → Cloud)
```json
{
  "rssi": -78,
  "snr": 27,
  "distance_km": 1.234,
  "pdr_percent": 95.5
}
```
