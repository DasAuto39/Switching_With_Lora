# Switching With LoRa — LoRa-Based Communication Switching System

## About This Project

This project explores how a communication system can automatically switch from one technology to another, depending on the signal conditions at any given moment. The context used here is communication between a ship and a port.

In simple terms, a ship at sea continuously sends its position data (via GPS) to the port using **LoRa** communication. On the port side, the system measures how good the received signal quality is — from signal strength, noise levels, to how many data packets successfully arrive. All of this information is then sent to the cloud via **MQTT**, where a **Machine Learning** model analyzes it and determines whether LoRa is still good enough to use, or whether it's time to switch to a different communication technology.

So the core of this project isn't just about sending GPS data — it's about **collecting communication quality data** that will serve as the basis for making switching decisions automatically.

> **Note:** This repository only covers the embedded/firmware side. The Machine Learning component is not included here.

---

## Problem Statement

There are several challenges that motivated this project:

1. **Large communication distances** — Ships can travel tens of kilometers from the port. Not every communication technology can reliably cover that kind of range.

2. **Unstable signal quality** — In a maritime environment, signal quality can suddenly degrade due to weather, waves, or interference from other sources. Without monitoring, there's no way to know when conditions start to deteriorate.

3. **No automatic switching mechanism** — When LoRa signal quality drops, switching to another technology is still done manually. This is inefficient, especially when conditions change rapidly.

4. **Lack of data for decision-making** — To make good switching decisions, you need telemetry data (RSSI, SNR, noise, distance, etc.) collected continuously. Without this data, decisions are based on guesswork.

---

## Objectives

1. Build a communication system between ship and port using LoRa as the primary data transmission channel.
2. Collect communication telemetry data (RSSI, SNR, distance, and PDR) in real-time to form a dataset.
3. Send that data to the cloud via an MQTT broker, so it can be processed by a Machine Learning model to produce switching decisions.
4. Secure data transmitted over LoRa using AES-128 encryption to prevent eavesdropping.
5. Provide an embedded infrastructure that supports probabilistic switching decisions — meaning the system switches to a better communication technology when signal conditions are no longer adequate.

---

## Solution

The system consists of two devices (nodes), each with a different role:

### Node A — Ship (Slave)
- Reads GPS coordinates from a Neo-6M module by parsing the NMEA `$GPRMC` sentence.
- Converts coordinates from NMEA format to decimal format (latitude and longitude).
- Transmits location data to the port via the LoRa E220-900T22D module. If the GPS hasn't acquired a signal yet, the system sends a `GPS_NO_FIX` status instead.

### Node B — Port (Master)
- Receives location data from the ship via LoRa.
- Measures RSSI (received signal strength) and SNR (signal-to-noise ratio) from each incoming packet.
- Periodically reads the ambient noise floor from the LoRa module as a reference for environmental noise levels.
- Calculates the distance between the ship and the port using the Haversine formula.
- Calculates PDR (Packet Delivery Ratio) — the percentage of packets successfully received out of the total expected.
- Sends all telemetry data to the HiveMQ MQTT Broker in JSON format.
- Receives switching decisions from the Machine Learning model via MQTT.

### Overall System Flow

```
[Ship + GPS] --LoRa--> [Port] --WiFi/MQTT--> [Cloud/ML] --MQTT--> [Switching Decision]
```

---

## Parameters and Values

### LoRa Configuration
| Parameter | Value |
|---|---|
| Baud Rate | 9600 bps |
| Operating Frequency | 900 MHz (E220-900T22D) |
| Buffer Size | 1024 bytes |
| Data Transmission Interval | 2 seconds |
| Noise Reading Interval | 15 seconds |

### Collected Telemetry Data
| Parameter | Unit | Description |
|---|---|---|
| RSSI | dBm | Received signal strength. Closer to 0 means stronger. |
| SNR | dB | Difference between signal and noise. Higher is better. |
| Ambient Noise | dBm | Environmental noise level. Default initial value: -105 dBm. |
| Distance | km | Distance from ship to base station, calculated using the Haversine formula. |
| PDR | % | Percentage of packets successfully received out of the total expected. |

### Base Station Coordinates (Port)
| Parameter | Value |
|---|---|
| Latitude | -7.284916 |
| Longitude | 112.795808 |

### Data Security
| Parameter | Value |
|---|---|
| Encryption Method | AES-128 ECB |
| Key Length | 16 bytes (128 bits) |

### MQTT Configuration
| Parameter | Value |
|---|---|
| Broker | `mqtt://broker.hivemq.com` |
| Publish Topic (Telemetry) | `vms_hybrid_2026/telemetry` |
| Subscribe Topic (ML Decision) | `vms_hybrid_2026/switching_decision` |

---

## Sensors and Hardware

### Microcontroller
**ESP32** — The main microcontroller that runs all of the system logic. It was chosen because it has built-in WiFi, plenty of GPIO pins, and can run FreeRTOS for handling multiple tasks simultaneously.

### LoRa Module
**E220-900T22D (EBYTE)** — A LoRa transceiver module operating at 900 MHz. It can communicate over several kilometers with low power consumption. This module also has an ambient noise reading feature, which is used to calculate SNR.

### GPS Module
**Neo-6M (u-blox)** — A GPS module that sends position data over UART in NMEA format. In this project, the `$GPRMC` sentence is parsed to obtain latitude and longitude coordinates.

### LED Indicators
- **Green LED (GPIO 32)** — Blinks briefly when the received data is a valid packet.
- **Red LED (GPIO 33)** — Blinks briefly when the incoming data is not recognized by the system.

### Internet Connectivity
**WiFi (built-in ESP32)** — Used on Node B (Port) to connect to the internet and publish telemetry data to the MQTT broker.

---

## Project Structure

```
Switching_With_Lora/
├── project_ws/
│   └── main/
│       ├── project_ws.c      # Main code (Node A and Node B)
│       ├── hardware_init.c   # Hardware initialization (GPIO, UART, LoRa)
│       ├── hardware_init.h   # Pin configuration and function declarations
│       └── location.c        # Base station coordinates
├── datasheet/                # Component datasheets
├── pertemuan/                # Meeting progress notes
├── build/                    # Firmware build output
└── README.md                 # Project documentation
```

---

## How to Build and Flash

This project uses **ESP-IDF**, the official framework from Espressif for ESP32 development.

1. **Install ESP-IDF** — Follow the official guide at [docs.espressif.com](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)

2. **Select which node to flash** — Open `project_ws.c` and uncomment one of the following:
   ```c
   #define COMPILE_NODE_A  // For the Ship device (Slave + GPS)
   #define COMPILE_NODE_B  // For the Port device (Master)
   ```

3. **Select the board type** — Open `hardware_init.h` and uncomment one of the following:
   ```c
   #define COMPILE_MINSIS       // For custom PCB
   #define COMPILE_BREADBOARD   // For breadboard circuit
   ```

4. **Build and Flash**:
   ```bash
   cd project_ws
   idf.py build
   idf.py -p /dev/ttyUSBx flash monitor
   ```

---

## Data Format

### LoRa Transmission (Ship to Port)
```
LAT:-7.284916,LON:112.795808    # Valid GPS coordinates
GPS_NO_FIX                       # GPS has not acquired a signal yet
```

### MQTT Publish (Port to Cloud)
```json
{
  "rssi": -78,
  "snr": 27,
  "distance_km": 1.234,
  "pdr_percent": 95.5
}
```
