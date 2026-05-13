# Switching With LoRa — Sistem Switching Komunikasi Berbasis LoRa

## Tentang Projek Ini

Projek ini membahas tentang bagaimana sebuah sistem komunikasi bisa secara otomatis berpindah (switching) dari satu teknologi ke teknologi lain, tergantung kondisi sinyal yang terjadi saat itu. Konteks yang diangkat di sini adalah komunikasi antara kapal dan pelabuhan.

Secara sederhana, kapal yang sedang berlayar akan terus mengirimkan data posisinya (lewat GPS) ke pelabuhan menggunakan komunikasi **LoRa**. Di sisi pelabuhan, sistem akan mengukur seberapa bagus kualitas sinyal yang diterima — mulai dari kekuatan sinyal, tingkat noise, sampai berapa banyak paket data yang berhasil sampai. Semua informasi ini kemudian dikirim ke cloud melalui **MQTT**, supaya bisa dianalisis oleh model **Machine Learning** yang akan menentukan: apakah LoRa masih cukup baik untuk digunakan, atau sudah saatnya berpindah ke teknologi komunikasi lain.

Jadi inti dari projek ini bukan sekadar mengirim data GPS, melainkan **mengumpulkan data kualitas komunikasi** yang nantinya menjadi dasar pengambilan keputusan switching secara otomatis.

> **Catatan:** Repository ini hanya mencakup bagian embedded/firmware. Bagian Machine Learning tidak termasuk di dalamnya.

---

## Rumusan Masalah

Ada beberapa permasalahan yang melatarbelakangi projek ini:

1. **Jarak komunikasi yang besar** — Kapal bisa berlayar hingga puluhan kilometer dari pelabuhan. Tidak semua teknologi komunikasi mampu menjangkau jarak tersebut dengan baik.

2. **Kualitas sinyal yang tidak menentu** — Di lingkungan laut, sinyal bisa tiba-tiba melemah karena cuaca, gelombang, atau gangguan dari sumber lain. Tanpa pemantauan, kita tidak akan tahu kapan kondisi sinyal mulai memburuk.

3. **Belum ada mekanisme switching yang otomatis** — Saat kualitas sinyal LoRa sudah menurun, perpindahan ke teknologi lain masih dilakukan secara manual. Ini tentu tidak efisien, apalagi kalau kondisinya berubah cepat.

4. **Kurangnya data sebagai dasar keputusan** — Untuk bisa membuat keputusan switching yang tepat, dibutuhkan data telemetri (RSSI, SNR, noise, jarak, dsb.) yang dikumpulkan secara terus-menerus. Tanpa data ini, keputusan hanya berdasarkan perkiraan.

---

## Tujuan

1. Membangun sistem komunikasi antara kapal dan pelabuhan menggunakan LoRa sebagai jalur utama pengiriman data.
2. Mengumpulkan data telemetri komunikasi (RSSI, SNR, jarak, dan PDR) secara real-time untuk dijadikan dataset.
3. Mengirimkan data tersebut ke cloud melalui MQTT broker, sehingga bisa diproses oleh model Machine Learning untuk menghasilkan keputusan switching.
4. Mengamankan data yang dikirim lewat LoRa menggunakan enkripsi AES-128 agar tidak mudah disadap.
5. Menyediakan infrastruktur embedded yang mendukung keputusan switching secara probabilistik — yaitu berpindah ke teknologi komunikasi lain ketika kondisi sinyal sudah tidak memadai.

---

## Solusi yang Diterapkan

Sistem ini dibagi menjadi dua perangkat (node) yang masing-masing punya tugas berbeda:

### Node A — Kapal (Slave)
- Membaca data koordinat GPS dari modul Neo-6M dengan melakukan parsing terhadap sentence NMEA `$GPRMC`.
- Mengonversi koordinat dari format NMEA ke format desimal (latitude dan longitude).
- Mengirimkan data lokasi ke pelabuhan melalui modul LoRa E220-900T22D. Jika GPS belum mendapatkan sinyal, sistem akan mengirimkan status `GPS_NO_FIX`.

### Node B — Pelabuhan (Master)
- Menerima data lokasi dari kapal melalui LoRa.
- Mengukur RSSI (kekuatan sinyal) dan SNR (perbandingan sinyal terhadap noise) dari setiap paket yang diterima.
- Membaca ambient noise floor dari modul LoRa secara berkala sebagai referensi tingkat kebisingan lingkungan.
- Menghitung jarak antara kapal dan pelabuhan menggunakan rumus Haversine.
- Menghitung PDR (Packet Delivery Ratio), yaitu berapa persen paket yang berhasil diterima dari total yang seharusnya sampai.
- Mengirimkan seluruh data telemetri ke HiveMQ MQTT Broker dalam format JSON.
- Menerima keputusan switching dari model Machine Learning melalui MQTT.

### Alur Sistem Secara Keseluruhan

```
[Kapal + GPS] --LoRa--> [Pelabuhan] --WiFi/MQTT--> [Cloud/ML] --MQTT--> [Keputusan Switching]
```

---

## Parameter dan Nilai yang Digunakan

### Konfigurasi LoRa
| Parameter | Nilai |
|---|---|
| Baud Rate | 9600 bps |
| Frekuensi Operasi | 900 MHz (E220-900T22D) |
| Ukuran Buffer | 1024 byte |
| Interval Pengiriman Data | 2 detik |
| Interval Pembacaan Noise | 15 detik |

### Data Telemetri yang Dikumpulkan
| Parameter | Satuan | Keterangan |
|---|---|---|
| RSSI | dBm | Kekuatan sinyal yang diterima. Semakin mendekati 0, semakin kuat. |
| SNR | dB | Selisih antara sinyal dan noise. Semakin besar, semakin bersih sinyalnya. |
| Ambient Noise | dBm | Tingkat noise lingkungan. Nilai default awal: -105 dBm. |
| Jarak | km | Jarak kapal ke base station, dihitung dengan rumus Haversine. |
| PDR | % | Persentase paket yang berhasil diterima dari total yang diharapkan. |

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

### Konfigurasi MQTT
| Parameter | Nilai |
|---|---|
| Broker | `mqtt://broker.hivemq.com` |
| Topic Publish (Telemetri) | `vms_hybrid_2026/telemetry` |
| Topic Subscribe (Keputusan ML) | `vms_hybrid_2026/switching_decision` |

---

## Sensor dan Perangkat Keras

### Mikrokontroler
**ESP32** — Mikrokontroler utama yang menjalankan seluruh logika sistem. Dipilih karena sudah memiliki WiFi bawaan, GPIO yang cukup banyak, dan bisa menjalankan FreeRTOS untuk menangani beberapa tugas secara bersamaan.

### Modul LoRa
**E220-900T22D (EBYTE)** — Modul transceiver LoRa yang bekerja di frekuensi 900 MHz. Mampu berkomunikasi hingga beberapa kilometer dengan konsumsi daya yang rendah. Modul ini juga memiliki fitur pembacaan ambient noise yang digunakan untuk menghitung SNR.

### Modul GPS
**Neo-6M (u-blox)** — Modul GPS yang mengirimkan data posisi melalui UART dalam format NMEA. Pada projek ini, sentence yang digunakan adalah `$GPRMC` untuk mendapatkan koordinat latitude dan longitude.

### Indikator LED
- **LED Hijau (GPIO 32)** — Menyala sebentar ketika data yang diterima merupakan paket yang valid.
- **LED Merah (GPIO 33)** — Menyala sebentar ketika data yang masuk tidak dikenali oleh sistem.

### Koneksi Internet
**WiFi (bawaan ESP32)** — Digunakan di Node B (Pelabuhan) untuk terhubung ke internet dan mengirimkan data telemetri ke MQTT broker.

---

## Struktur Projek

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

## Cara Build dan Flash

Projek ini menggunakan **ESP-IDF**, framework resmi dari Espressif untuk pengembangan ESP32.

1. **Instalasi ESP-IDF** — Ikuti panduan resmi di [docs.espressif.com](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)

2. **Pilih Node yang akan di-flash** — Buka file `project_ws.c`, lalu uncomment salah satu baris berikut:
   ```c
   #define COMPILE_NODE_A  // Untuk perangkat Kapal (Slave + GPS)
   #define COMPILE_NODE_B  // Untuk perangkat Pelabuhan (Master)
   ```

3. **Pilih jenis board** — Buka file `hardware_init.h`, lalu uncomment salah satu:
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

## Format Data

### Transmisi LoRa (Kapal ke Pelabuhan)
```
LAT:-7.284916,LON:112.795808    # Koordinat GPS valid
GPS_NO_FIX                       # GPS belum mendapatkan sinyal
```

### Publish MQTT (Pelabuhan ke Cloud)
```json
{
  "rssi": -78,
  "snr": 27,
  "distance_km": 1.234,
  "pdr_percent": 95.5
}
```
