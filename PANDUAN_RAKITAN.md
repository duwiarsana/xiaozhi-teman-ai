# Panduan Lengkap Merakit Asisten Suara Xiaozhi ESP32

> Dokumen ini membahas secara detail cara merakit asisten suara berbasis ESP32 menggunakan proyek **xiaozhi-esp32**, mulai dari daftar komponen, wiring, konfigurasi software, penjelasan kode, hingga proses upload firmware.

---

## Daftar Isi

1. [Sekilas Tentang Proyek](#1-sekilas-tentang-proyek)
2. [Daftar Komponen](#2-daftar-komponen)
3. [Rangkaian & Wiring Detail](#3-rangkaian--wiring-detail)
4. [Persiapan Environment Development](#4-persiapan-environment-development)
5. [Struktur Kode & Konfigurasi Board](#5-struktur-kode--konfigurasi-board)
6. [Penjelasan Kode Board](#6-penjelasan-kode-board)
7. [Sistem State Machine & LED Indikator](#7-sistem-state-machine--led-indikator)
8. [Konfigurasi Build & menuconfig](#8-konfigurasi-build--menuconfig)
9. [Compile & Upload Firmware](#9-compile--upload-firmware)
10. [Pendaftaran Perangkat ke Server](#10-pendaftaran-perangkat-ke-server)
11. [Pengujian & Troubleshooting](#11-pengujian--troubleshooting)

---

## 1. Sekilas Tentang Proyek

**Xiaozhi Assistant** adalah asisten suara open-source berbasis ESP32 yang menghubungkan mikrokontroler ke server AI (cloud). Perangkat ini dapat:

- **Mendengarkan** perintah suara melalui microphone I2S
- **Berbicara** membalas melalui speaker I2S (TTS - Text To Speech)
- **Wake word detection** - memanggil dengan kata pemicu "你好小智" (Ni hao xiao zhi)
- **Kontrol IoT** - mengendalikan lampu/perangkat via MCP (Model Context Protocol)
- **OTA Update** - pembaruan firmware over-the-air

Versi ini menggunakan **board `bread-compact-esp32`** yang dirancang untuk ESP32 DevKit standar (chip ESP32 klasik, bukan ESP32-S3). Tidak menggunakan LCD/OLED, melainkan **LED sederhana sebagai indikator status**.

---

## 2. Daftar Komponen

### Komponen Utama

| No | Komponen | Jumlah | Keterangan |
|----|----------|--------|------------|
| 1 | ESP32 DevKit V1 (30 pin atau 38 pin) | 1 | Chip ESP32-WROOM-32, 4MB Flash |
| 2 | Modul Speaker I2S (MAX98357A) | 1 | Amplifier I2S untuk output audio |
| 3 | Modul Microphone I2S (INMP441) | 1 | MEMS mic I2S untuk input suara |
| 4 | Speaker 4 ohm / 8 ohm (3W-5W) | 1 | Dipasang ke output MAX98357A |
| 5 | LED 5mm (warna bebas) | 1 | Indikator status |
| 6 | Resistor 330 ohm | 1 | Pembatas arus LED |
| 7 | Tactile Push Button (momentary) | 2-3 | Tombol boot, touch, dan ASR |
| 8 | Breadboard 830 titik | 1 | Untuk merangkai tanpa solder |
| 9 | Jumper wires (Dupont M-M, M-F) | ~20 | Koneksi antar komponen |
| 10 | Kabel USB Micro-USB | 1 | Untuk upload & power |

### Komponen Opsional

| No | Komponen | Jumlah | Keterangan |
|----|----------|--------|------------|
| 11 | Relay Module 5V atau LED + Resistor | 1 | Demo IoT (kontrol lampu via suara) |
| 12 | Capacitor 10uF | 1 | Filter noise pada power mic |

---

## 3. Rangkaian & Wiring Detail

### 3.1 Pemetaan Pin ESP32

Konfigurasi pin didefinisikan di file `main/boards/bread-compact-esp32/config.h`:

```
+-------------------------------------------------------------+
|                     PEMETAAN PIN ESP32                       |
+-------------------+--------------+--------------------------+
|  Komponen          |  GPIO Pin    |  Fungsi                 |
+-------------------+--------------+--------------------------+
|  SPK - DOUT       |  GPIO 22     |  I2S Data Out (Speaker)  |
|  SPK - BCLK       |  GPIO 26     |  I2S Bit Clock (Speaker) |
|  SPK - LRCK/WS    |  GPIO 25     |  I2S WS/LRCK (Speaker)   |
|  MIC - WS         |  GPIO 27     |  I2S WS (Microphone)     |
|  MIC - SCK        |  GPIO 14     |  I2S Bit Clock (Mic)     |
|  MIC - DIN (SD)   |  GPIO 32     |  I2S Data In (Mic)       |
|  BOOT Button      |  GPIO 0      |  Tombol Boot (Flash)    |
|  Touch Button     |  GPIO 5      |  Push-to-Talk            |
|  ASR Button       |  GPIO 19     |  Trigger Wake Word       |
|  LED Indikator    |  GPIO 2      |  Status LED              |
|  Lampu (MCP IoT)  |  GPIO 18     |  Relay/Lamp Controller   |
+-------------------+--------------+--------------------------+
```

### 3.2 Wiring MAX98357A (Speaker Amplifier)

```
ESP32                    MAX98357A
-------                  ----------
GPIO 22  ------------>  DIN
GPIO 25  ------------>  LRC (LRCK / WS)
GPIO 26  ------------>  BCLK
3V3      ------------>  VIN
GND      ------------>  GND
                        +-----------------+
                        | Speaker +  ---> | --> Speaker
                        |  Speaker -  --> |
                        +-----------------+
```

> **Catatan:** Pin GAIN pada MAX98357A biarkan menggantung (floating) untuk gain default 9dB, atau hubungkan ke GND untuk 12dB.

### 3.3 Wiring INMP441 (Microphone I2S)

```
ESP32                    INMP441
-------                  -------
GPIO 27  ------------>  WS
GPIO 14  ------------>  SCK
GPIO 32  ------------>  SD (Data Out)
3V3      ------------>  VDD
GND      ------------>  GND
GND      ------------>  L/R (untuk channel kiri)
```

> **Penting:** Pin L/R pada INMP441 harus dihubungkan ke **GND** (channel kiri) atau **3V3** (channel kanan). Pilih salah satu, jangan dibiarkan floating.

### 3.4 Wiring LED Indikator

```
ESP32                    LED
-------                  ----
GPIO 2   ---[330 ohm]-->  Anode (+)
GND      -------------->  Katode (-)
```

### 3.5 Wiring Tombol

```
Tombol BOOT (Push-to-Talk utama):
  GPIO 0  -------->  Terminal A
  GND     -------->  Terminal B
  (GPIO 0 sudah punya internal pull-up di ESP32)

Tombol TOUCH (Push-to-Talk alternatif):
  GPIO 5  -------->  Terminal A
  GND     -------->  Terminal B
  (Pull-up diaktifkan oleh software Button class)

Tombol ASR (Trigger Wake Word):
  GPIO 19 -------->  Terminal A
  GND     -------->  Terminal B
  (Pull-up diaktifkan oleh software Button class)
```

### 3.6 Wiring Lampu IoT (Opsional - untuk demo kontrol suara)

```
ESP32                    Relay Module
-------                  ------------
GPIO 18  ------------>  IN / Signal
5V       ------------>  VCC
GND      ------------>  GND
                         COM  ---> Lampu
                         NO   ---> Sumber 220V/12V
```

### 3.7 Skema Ringkas Semua Koneksi

```
                     ESP32 DevKit
               +----------------------+
  (Mic)        |  3V3          GPIO22|----> MAX98357A DIN
  INMP441      |  GND          GPIO25|----> MAX98357A LRC
   VDD <-------+--3V3          GPIO26|----> MAX98357A BCLK
   GND <-------+--GND          GPIO27|----> INMP441 WS
   SD  ------->|--GPIO32       GPIO14|----> INMP441 SCK
   SCK ------->|--GPIO14       GPIO19|----> [ASR Button] --> GND
   WS  ------->|--GPIO27       GPIO18|----> [Relay/Lamp]
   L/R -- GND  |                GPIO5|----> [Touch Button] --> GND
               |                GPIO0|----> [Boot Button] --> GND
  (Speaker)    |                GPIO2|----> [330 ohm] --> LED --> GND
  MAX98357A    |               5V/GND|
   BCLK <------|--GPIO26             |
   LRC  <------|--GPIO25             |
   VIN  <------|--3V3                |
   GND  <------|--GND                |
               +----------------------+
```

---

## 4. Persiapan Environment Development

### 4.1 Instalasi ESP-IDF

Proyek ini menggunakan **ESP-IDF v5.3+** (versi 6.0 juga didukung).

#### macOS (via Homebrew)

```bash
# Install dependencies
brew install cmake ninja dfu-util python3

# Clone ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout v5.3.1
git submodule update --init --recursive

# Install toolchain
./install.sh esp32
```

#### Windows

1. Download installer resmi dari https://dl.espressif.com/dl/esp-idf/
2. Jalankan installer, pilih target **ESP32**
3. Setelah selesai, buka "ESP-IDF PowerShell" atau "ESP-IDF CMD"

#### Linux (Ubuntu/Debian)

```bash
sudo apt install git wget flex bison gperf python3 python3-pip \
  python3-venv cmake ninja-build ccache libffi-dev libssl-dev \
  dfu-util libusb-1.0-0

mkdir -p ~/esp && cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32
```

### 4.2 Aktivasi Environment

Setiap kali membuka terminal baru, jalankan:

```bash
# macOS / Linux
source ~/esp/esp-idf/export.sh

# Windows (PowerShell)
~\esp\esp-idf\export.ps1
```

Verifikasi:

```bash
idf.py --version
# Output: ESP-IDF v5.3.1
```

### 4.3 Clone Proyek Xiaozhi

```bash
git clone https://github.com/78/xiaozhi-esp32.git
cd xiaozhi-esp32
```

### 4.4 Install Python Dependencies

```bash
pip install -r requirements.txt
# atau biarkan idf.py yang menangani
idf.py reconfigure
```

---

## 5. Struktur Kode & Konfigurasi Board

### 5.1 Struktur Folder yang Relevan

```
xiaozhi-esp32/
+-- main/
|   +-- boards/
|   |   +-- bread-compact-esp32/          <-- Board yang kita gunakan
|   |   |   +-- config.h                  <-- Definisi pin & konstanta hardware
|   |   |   +-- config.json               <-- Konfigurasi build untuk board ini
|   |   |   +-- esp32_bread_board.cc      <-- Implementasi kelas board
|   |   |   +-- README.md
|   |   +-- common/
|   |       +-- board.h / board.cc        <-- Base class Board
|   |       +-- wifi_board.h / .cc        <-- Base class untuk WiFi board
|   |       +-- button.h / button.cc      <-- Class Button (debounce)
|   |       +-- lamp_controller.h         <-- Kontrol lampu via MCP
|   +-- led/
|   |   +-- led.h                         <-- Abstract class Led
|   |   +-- single_led.h / .cc            <-- WS2812 LED (tidak dipakai di ESP32)
|   |   +-- gpio_led.h / .cc              <-- PWM LED (tidak dipakai di ESP32)
|   +-- display/
|   |   +-- display.h / .cc               <-- Abstract class Display
|   |   +-- oled_display.h / .cc          <-- OLED (tidak dipakai)
|   +-- application.cc / .h               <-- Logic utama aplikasi
|   +-- device_state.h                    <-- Enum DeviceState
|   +-- CMakeLists.txt                    <-- Build configuration
|   +-- Kconfig.projbuild                 <-- menuconfig options
+-- partitions/                           <-- Partition table
+-- sdkconfig.defaults                     <-- Default config global
+-- sdkconfig.defaults.esp32               <-- Default config khusus ESP32
+-- sdkconfig                              <-- Generated config (jangan edit manual)
```

### 5.2 File `config.h` - Definisi Pin Hardware

File `main/boards/bread-compact-esp32/config.h` berisi semua definisi pin yang digunakan oleh board. Inilah file yang perlu diubah jika ingin mengganti pin:

```cpp
#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// Sample rate audio
#define AUDIO_INPUT_SAMPLE_RATE  16000   // Mic: 16kHz (cukup untuk speech)
#define AUDIO_OUTPUT_SAMPLE_RATE 24000   // Speaker: 24kHz (kualitas TTS)

// Mode I2S: SIMPLEX (pin terpisah untuk mic & speaker)
#define AUDIO_I2S_METHOD_SIMPLEX

#ifdef AUDIO_I2S_METHOD_SIMPLEX
    // Pin Speaker (MAX98357A)
    #define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_22
    #define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_26
    #define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_25

    // Pin Microphone (INMP441)
    #define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_27
    #define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_14
    #define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_32
#else
    // Mode Duplex (mic & speaker share pin yang sama)
    #define AUDIO_I2S_GPIO_WS   GPIO_NUM_4
    #define AUDIO_I2S_GPIO_BCLK GPIO_NUM_5
    #define AUDIO_I2S_GPIO_DIN  GPIO_NUM_6
    #define AUDIO_I2S_GPIO_DOUT GPIO_NUM_7
#endif

#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define TOUCH_BUTTON_GPIO       GPIO_NUM_5
#define ASR_BUTTON_GPIO         GPIO_NUM_19
#define BUILTIN_LED_GPIO        GPIO_NUM_2

// Pin untuk kontrol lampu via MCP (IoT)
#define LAMP_GPIO GPIO_NUM_18

#endif
```

**Penjelasan:**

- `AUDIO_I2S_METHOD_SIMPLEX` - Mode I2S Simplex berarti mic dan speaker menggunakan pin terpisah. Ini lebih fleksibel karena bisa menggunakan modul yang berbeda tanpa konflik pin.
- `AUDIO_INPUT_SAMPLE_RATE 16000` - Mic menangkap audio pada 16kHz, sudah cukup untuk pengenalan suara.
- `AUDIO_OUTPUT_SAMPLE_RATE 24000` - Speaker memutar audio TTS pada 24kHz untuk kualitas yang baik.
- `BUILTIN_LED_GPIO GPIO_NUM_2` - LED built-in pada ESP32 DevKit (GPIO 2 adalah pin LED bawaan).

### 5.3 File `config.json` - Konfigurasi Build

File `main/boards/bread-compact-esp32/config.json`:

```json
{
    "target": "esp32",
    "builds": [
        {
            "name": "bread-compact-esp32"
        }
    ]
}
```

File ini memberitahu build system bahwa board ini menarget chip ESP32. Tidak ada konfigurasi OLED tambahan lagi karena kita sudah tidak menggunakan display.

---

## 6. Penjelasan Kode Board

### 6.1 File `esp32_bread_board.cc` - Kode Utama Board

Ini adalah file terpenting yang mendefinisikan bagaimana board berperilaku. File lengkapnya berada di `main/boards/bread-compact-esp32/esp32_bread_board.cc`.

#### 6.1.1 Class `SimpleGpioLed` - LED Indikator Sederhana

Karena ESP32 klasik tidak mendukung WS2812 (SingleLed) dan LEDC (GpioLed) di-exclude dari build ESP32, kita membuat class LED sendiri yang sangat sederhana:

```cpp
#include "led/led.h"
#include <driver/gpio.h>

class SimpleGpioLed : public Led {
public:
    SimpleGpioLed(gpio_num_t gpio) : gpio_(gpio) {
        // Konfigurasi GPIO sebagai output
        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << gpio_,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf);
        gpio_set_level(gpio_, 0);  // LED mati saat start
    }

    void OnStateChanged() override {
        auto& app = Application::GetInstance();
        auto state = app.GetDeviceState();
        switch (state) {
            // LED nyala saat: mendengarkan, berbicara, atau connecting
            case kDeviceStateListening:
            case kDeviceStateSpeaking:
            case kDeviceStateConnecting:
                gpio_set_level(gpio_, 1);
                break;
            // LED nyala saat: startup, wifi config, upgrade, activating
            case kDeviceStateStarting:
            case kDeviceStateWifiConfiguring:
            case kDeviceStateUpgrading:
            case kDeviceStateActivating:
                gpio_set_level(gpio_, 1);
                break;
            // LED mati saat: standby (idle)
            case kDeviceStateIdle:
            default:
                gpio_set_level(gpio_, 0);
                break;
        }
    }

private:
    gpio_num_t gpio_;
};
```

**Cara kerja:**

1. Constructor menerima nomor GPIO, lalu mengkonfigurasi pin tersebut sebagai output digital biasa (HIGH/LOW).
2. Method `OnStateChanged()` dipanggil otomatis oleh `Application` setiap kali state perangkat berubah.
3. Saat state = `kDeviceStateIdle` (standby), LED **mati** (level = 0).
4. Saat state = `kDeviceStateListening` (sedang mendengarkan), LED **nyala** (level = 1).
5. Saat state = `kDeviceStateSpeaking` (sedang berbicara), LED **nyala**.

#### 6.1.2 Class `CompactWifiBoard` - Board Utama

```cpp
#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "lamp_controller.h"
#include "led/led.h"

class CompactWifiBoard : public WifiBoard {
private:
    Button boot_button_;
    Button touch_button_;
    Button asr_button_;

    void InitializeButtons() {
        // Tombol BOOT: klik untuk toggle chat (mulai/berhenti bicara)
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        // Tombol ASR: trigger wake word "你好小智"
        asr_button_.OnClick([this]() {
            std::string wake_word = "你好小智";
            Application::GetInstance().WakeWordInvoke(wake_word);
        });

        // Tombol TOUCH: tahan untuk bicara, lepas untuk berhenti
        touch_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        touch_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });
    }

    void InitializeTools() {
        // Daftarkan lampu sebagai perangkat IoT yang bisa dikontrol AI
        static LampController lamp(LAMP_GPIO);
    }

public:
    CompactWifiBoard() : WifiBoard(),
        boot_button_(BOOT_BUTTON_GPIO),
        touch_button_(TOUCH_BUTTON_GPIO),
        asr_button_(ASR_BUTTON_GPIO)
    {
        InitializeButtons();
        InitializeTools();
    }

    // Override: kembalikan codec audio (mic + speaker I2S)
    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK,
            AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS,
            AUDIO_I2S_MIC_GPIO_DIN
        );
        return &audio_codec;
    }

    // Override: kembalikan LED indikator
    virtual Led* GetLed() override {
        static SimpleGpioLed led(BUILTIN_LED_GPIO);
        return &led;
    }
};

DECLARE_BOARD(CompactWifiBoard);
```

**Penjelasan baris per baris:**

**Inheritance:** `CompactWifiBoard` mewarisi `WifiBoard` yang sudah menangani koneksi WiFi, OTA, dan network management.

**Tombol:**
- `boot_button_` (GPIO 0): Klik untuk memulai/menghentikan percakapan. Jika ditekan saat startup, masuk mode konfigurasi WiFi.
- `asr_button_` (GPIO 19): Klik untuk mensimulasikan wake word "你好小智" tanpa benar-benar mengucapkan.
- `touch_button_` (GPIO 5): Tahan (push-to-talk) untuk merekam suara, lepaskan untuk mengirim.

**`NoAudioCodecSimplex`:** Class ini menangani I2S tanpa codec chip eksternal. ESP32 langsung mengirim/menerima PCM data ke MAX98357A dan dari INMP441.

**`DECLARE_BOARD(CompactWifiBoard)`:** Macro yang mendaftarkan board ini agar bisa dipanggil oleh `Board::GetInstance()`.

---

## 7. Sistem State Machine & LED Indikator

### 7.1 Enum DeviceState

State machine perangkat didefinisikan di `main/device_state.h`:

```cpp
enum DeviceState {
    kDeviceStateUnknown,        // 0: State tidak diketahui
    kDeviceStateStarting,      // 1: Sedang booting/startup
    kDeviceStateWifiConfiguring,// 2: Mode konfigurasi WiFi (AP mode)
    kDeviceStateIdle,          // 3: Standby (siap menerima perintah)
    kDeviceStateConnecting,    // 4: Sedang connect ke server
    kDeviceStateListening,     // 5: Sedang mendengarkan suara user
    kDeviceStateSpeaking,      // 6: Sedang berbicara (TTS)
    kDeviceStateUpgrading,      // 7: Sedang OTA upgrade
    kDeviceStateActivating,     // 8: Sedang aktivasi perangkat
    kDeviceStateAudioTesting,   // 9: Mode test audio
    kDeviceStateFatalError     // 10: Error fatal
};
```

### 7.2 Diagram Alur State

```
                    +------------------+
                    |   kDeviceState   |
                    |    Starting      |   LED: ON
                    +--------+---------+
                             |
                             v
                    +--------+---------+
                    |   kDeviceState   |
                    |  WifiConfiguring |   LED: ON (jika mode config)
                    +--------+---------+
                             |
                             v
              +--------------+--------------+
              |              |              |
              v              v              v
    +---------+--+  +-------+------+  +---+--------+
    |  Idle    |  | Activating  |  | Connecting |
    | (Standby)|  |              |  |            |
    | LED: OFF |  | LED: ON     |  | LED: ON    |
    +-----+----+  +------+-----+  +-----+------+
          |                |              |
          |                v              v
          |       +--------+---------+    |
          |       |     Idle         |    |
          |       +------------------+    |
          |                                 |
          v                                 v
    +-----+----------+           +----------+----+
    |   Listening    |<--------->|  Connecting   |
    | (mendengarkan) |           +---------------+
    | LED: ON        |
    +-----+----------+
          |
          v
    +-----+----------+
    |   Speaking     |
    | (berbicara)    |
    | LED: ON        |
    +----------------+
          |
          v
    +-----+----------+
    |     Idle       |
    | LED: OFF       |
    +----------------+
```

### 7.3 Tabel Perilaku LED

| State | Deskripsi | LED Status |
|-------|-----------|------------|
| `kDeviceStateStarting` | Booting awal | **NYALA** |
| `kDeviceStateWifiConfiguring` | Mode config WiFi (AP) | **NYALA** |
| `kDeviceStateIdle` | Standby, siap dipanggil | **MATI** |
| `kDeviceStateConnecting` | Connecting ke server AI | **NYALA** |
| `kDeviceStateListening` | Mendengarkan suara user | **NYALA** |
| `kDeviceStateSpeaking` | Membalas dengan suara (TTS) | **NYALA** |
| `kDeviceStateUpgrading` | OTA firmware update | **NYALA** |
| `kDeviceStateActivating` | Aktivasi perangkat ke server | **NYALA** |
| `kDeviceStateFatalError` | Error fatal | **MATI** |

### 7.4 Cara State Change Memicu LED

Di `main/application.cc` baris 881-891, setiap kali state berubah, method `OnStateChanged()` dipanggil:

```cpp
void Application::HandleStateChangedEvent() {
    DeviceState new_state = state_machine_.GetState();
    // ...
    auto led = board.GetLed();
    led->OnStateChanged();  // <-- Ini yang memicu update LED
    // ...
}
```

Selain itu, saat VAD (Voice Activity Detection) mendeteksi perubahan suara saat listening, LED juga diupdate (baris 251-256):

```cpp
if (bits & MAIN_EVENT_VAD_CHANGE) {
    if (GetDeviceState() == kDeviceStateListening) {
        auto led = Board::GetInstance().GetLed();
        led->OnStateChanged();
    }
}
```

### 7.5 Class `Led` (Abstract)

Di `main/led/led.h`, class `Led` adalah interface sederhana:

```cpp
class Led {
public:
    virtual ~Led() = default;
    virtual void OnStateChanged() = 0;  // Dipanggil saat state berubah
};

class NoLed : public Led {
public:
    virtual void OnStateChanged() override {}  // No-op
};
```

Jika board tidak punya LED, bisa return `NoLed` dari `GetLed()`.

---

## 8. Konfigurasi Build & menuconfig

### 8.1 Set Target Chip

Pertama kali setelah clone, set target ke ESP32:

```bash
idf.py set-target esp32
```

Ini akan menghasilkan file `sdkconfig` default dan mengkonfigurasi build system.

### 8.2 Buka menuconfig

```bash
idf.py menuconfig
```

#### Pilih Board Type

Navigasi ke:

```
Xiaozhi Assistant  --->
    Board Type  --->
        (X) Bread Compact ESP32 DevKit
```

#### Pilih Bahasa

```
Xiaozhi Assistant  --->
    Default Language  --->
        (X) Chinese           <-- Default (wake word "你好小智")
        ( ) English
        ( ) Japanese
        ( ) Korean
        ( ) Vietnamese
        ...
```

> **Catatan:** Bahasa default adalah Chinese karena wake word bawaan ESP32 adalah "你好小智". Pilih bahasa sesuai server AI yang digunakan.

#### OTA URL (Opsional)

```
Xiaozhi Assistant  --->
    (https://api.tenclass.net/xiaozhi/ota/) OTA URL
```

URL ini digunakan untuk:
1. Cek firmware update (OTA)
2. Mendapatkan alamat server WebSocket/MQTT

#### Flash Assets

```
Xiaozhi Assistant  --->
    Flash Assets  --->
        (X) Flash Default Assets   <-- Rekomendasi (include font & emoji)
        ( ) Do not flash assets
        ( ) Flash Custom Assets
```

### 8.3 Konfigurasi WiFi (Opsional - bisa via Web Config)

```
Xiaozhi Assistant  --->
    ( ) Disable WiFi config on first boot
    (myssid) WiFi SSID       <-- Isi jika ingin hardcode WiFi
    (mypassword) WiFi Password
```

Jika tidak diisi, perangkat akan masuk mode AP (Access Point) pada boot pertama untuk konfigurasi WiFi via web.

### 8.4 Partition Table

ESP32 dengan 4MB Flash menggunakan partition table custom:

File `sdkconfig.defaults.esp32`:
```
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/v2/4m.csv"
CONFIG_SR_WN_WN9_NIHAOXIAOZHI_TTS=y
CONFIG_ESP_TASK_WDT_TIMEOUT_S=20
```

Layout partisi 4MB:
```
Offset    Size     Partition     Description
0x0000    0x7000   bootloader    Bootloader
0x7000    0x1000   partition     Partition table
0x8000    0x1000   OTA data      OTA slot info
0xD000    0x1000   PHY           RF calibration data
0x10000   0x2F0000 app           Application firmware (~2.9MB)
0x300000  0x100000 assets        Font, emoji, sound assets (1MB)
```

### 8.5 Kustomisasi Pin (Jika Perlu)

Jika pin ESP32 yang dipakai konflik dengan modul lain, ubah di `config.h`:

```cpp
// Contoh: ganti LED ke GPIO 4
#define BUILTIN_LED_GPIO        GPIO_NUM_4

// Contoh: ganti speaker BCLK ke GPIO 15
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
```

> **Peringatan:** Pastikan pin yang dipilih tidak digunakan oleh flash (GPIO 6-11) atau tidak konflik dengan strapping pin (GPIO 0, 2, 12, 15).

---

## 9. Compile & Upload Firmware

### 9.1 Compile

```bash
source ~/esp/esp-idf/export.sh   # Aktivasi environment
idf.py build
```

Jika berhasil, output akhir akan menampilkan:

```
Project build complete. To flash, run:
 idf.py flash
or
 idf.py -p PORT flash
```

### 9.2 Cek Port Serial

#### macOS:
```bash
ls /dev/cu.*
# Biasanya: /dev/cu.usbserial-XXXX atau /dev/cu.SLAB_USBtoUART
```

#### Linux:
```bash
ls /dev/ttyUSB*
# Biasanya: /dev/ttyUSB0
```

#### Windows:
```
Device Manager -> Ports (COM & LPT) -> USB-SERIAL CH340 (COMX)
```

### 9.3 Upload Firmware

```bash
# Format: idf.py -p <PORT> flash
idf.py -p /dev/cu.usbserial-2110 flash
```

Proses flash akan menulis beberapa file ke ESP32:

```
Flash address  File                   Description
0x1000         bootloader.bin         Bootloader
0x8000         partition-table.bin    Partition table
0xD000         ota_data_initial.bin   OTA data (initial)
0x10000        xiaozhi.bin            Application firmware (utama)
0x300000       generated_assets.bin   Font, emoji, sound (assets)
```

### 9.4 Monitor Serial Output

Setelah flash, monitor output untuk debugging:

```bash
idf.py -p /dev/cu.usbserial-2110 monitor
```

Atau flash + monitor sekaligus:

```bash
idf.py -p /dev/cu.usbserial-2110 flash monitor
```

Tekan `Ctrl + ]` untuk keluar dari monitor.

### 9.5 Build Ulang Setelah Perubahan Kode

Jika mengubah kode di `config.h` atau `esp32_bread_board.cc`:

```bash
idf.py build
idf.py -p /dev/cu.usbserial-2110 flash
```

Build system cerdas dan hanya akan meng-compile ulang file yang berubah, jadi prosesnya cepat.

---

## 10. Pendaftaran Perangkat ke Server

### 10.1 Boot Pertama - Mode Konfigurasi WiFi

Saat pertama kali dinyalakan (belum ada WiFi tersimpan), ESP32 akan:

1. LED **nyala** (state = `kDeviceStateStarting`)
2. Berpindah ke mode `kDeviceStateWifiConfiguring`
3. Membuat Access Point (AP) bernama `Xiaozhi-XXXX`
4. LED tetap **nyala**

### 10.2 Konfigurasi WiFi via Web

1. Hubungkan smartphone/PC ke WiFi `Xiaozhi-XXXX`
2. Buka browser, akses `http://192.168.4.1`
3. Pilih WiFi rumah dan masukkan password
4. Klik "Save"
5. ESP32 akan restart dan connect ke WiFi
6. LED **mati** setelah berhasil connect dan masuk standby (idle)

### 10.3 Aktivasi Perangkat

Setelah WiFi terhubung:

1. ESP32 akan menghubungi OTA URL untuk mendapatkan alamat server
2. LED **nyala** (state = `kDeviceStateActivating`)
3. Server akan memberikan OTP code (6 digit)
4. OTP diucapkan melalui speaker (TTS)
5. Catat OTP tersebut
6. Buka https://xiaozhi.me dan login
7. Masukkan OTP untuk mengikat perangkat ke akun
8. Setelah aktivasi, LED **mati** (state = `kDeviceStateIdle`)

### 10.4 Verifikasi di Serial Monitor

Output serial yang sehat:

```
I (xxx) Board: UUID=xxxx-xxxx-xxxx SKU=bread-compact-esp32
I (xxx) WiFi: Connected to "MyWiFi"
I (xxx) WebSocket: Connected to wss://api.tenclass.net/...
I (xxx) Application: Device activated
I (xxx) Application: State changed to Idle
```

---

## 11. Pengujian & Troubleshooting

### 11.1 Pengujian Fungsional

#### Test 1: LED Indikator

| Aksi | LED yang Diharapkan |
|------|---------------------|
| Boot pertama | NYALA (startup) |
| Standby (idle) | MATI |
| Tekan tombol BOOT (klik) | NYALA (listening) |
| Bicara dan lepaskan | NYALA (speaking) lalu MATI (idle) |
| Tekan tombol ASR | NYALA (trigger wake word) |

#### Test 2: Push-to-Talk

1. Pastikan ESP32 sudah standby (LED mati)
2. Tekan dan tahan tombol TOUCH (GPIO 5)
3. LED harus **nyala** (listening)
4. Ucapkan: "Halo, apa kabar?"
5. Lepaskan tombol TOUCH
6. LED tetap **nyala** (speaking) - AI akan membalas
7. Setelah selesai bicara, LED **mati** (idle)

#### Test 3: Wake Word ( jika mic support ESP-SR)

1. Pastikan standby (LED mati)
2. Ucapkan: "你好小智" (Ni hao xiao zhi)
3. LED **nyala** (listening)
4. Ucapkan perintah
5. AI membalas, LED **nyala** (speaking)
6. Selesai, LED **mati** (idle)

> **Catatan:** Wake word detection pada ESP32 (bukan S3) menggunakan ESP-SR lite engine yang mungkin memerlukan model khusus.

#### Test 4: Tombol ASR (Alternatif Wake Word)

Jika wake word via mic tidak bekerja, gunakan tombol ASR (GPIO 19):
1. Tekan tombol ASR
2. LED **nyala** (listening)
3. Ucapkan perintah
4. AI membalas

### 11.2 Troubleshooting

#### Masalah: LED Tidak Menyala

**Kemungkinan penyebab:**
1. LED terbalik polaritasnya (anode/katode)
2. Resistor terlalu besar (gunakan 330 ohm)
3. Pin GPIO salah

**Solusi:**
- Coba balik LED (putar 180 derajat)
- Test dengan multimeter: GPIO 2 harus HIGH (3.3V) saat listening
- Pastikan menggunakan `BUILTIN_LED_GPIO` yang benar di `config.h`

#### Masalah: Tidak Ada Suara dari Speaker

**Kemungkinan penyebab:**
1. Wiring MAX98357A salah
2. Speaker tidak terhubung dengan benar
3. Volume terlalu rendah

**Solusi:**
- Cek serial monitor untuk error I2S
- Pastikan pin BCLK, LRC, DIN terhubung dengan benar
- Test speaker dengan audio source lain

#### Masalah: Mic Tidak Mendengarkan

**Kemungkinan penyebab:**
1. Pin L/R pada INMP441 floating
2. Wiring salah
3. Sample rate tidak cocok

**Solusi:**
- Hubungkan pin L/R ke GND (channel kiri)
- Pastikan WS, SCK, DIN terhubung benar
- Tambahkan capacitor 10uF antara VDD dan GND mic untuk filter noise

#### Masalah: Tidak Bisa Connect WiFi

**Solusi:**
1. Tekan tombol BOOT saat startup untuk masuk mode config
2. Connect ke AP `Xiaozhi-XXXX`
3. Buka `http://192.168.4.1`
4. Masukkan WiFi dan password baru

#### Masalah: Build Error - `gpio_led.cc` not found

Ini terjadi karena `gpio_led.cc` di-exclude untuk ESP32 (lihat `CMakeLists.txt` baris 1011). Board kita sudah menggunakan `SimpleGpioLed` custom, jadi tidak masalah.

#### Masalah: `CONFIG_OLED_SSD1306_128X64 is not set`

Jika muncul error tentang OLED config, hapus requirement OLED dari `config.json`. Pastikan `config.json` hanya berisi:

```json
{
    "target": "esp32",
    "builds": [
        {
            "name": "bread-compact-esp32"
        }
    ]
}
```

### 11.3 Debug Serial

Untuk melihat log detail, gunakan monitor:

```bash
idf.py -p /dev/cu.usbserial-2110 monitor
```

Log level bisa diubah di menuconfig:

```
Component config  --->
    Log output  --->
        Default log verbosity  --->
            ( ) None
            ( ) Error
            ( ) Warning
            (X) Info           <-- Default
            ( ) Debug
            ( ) Verbose
```

### 11.4 Mengubah Pinak LED/GPIO

Jika ingin mengubah pin LED dari GPIO 2 ke pin lain, edit `config.h`:

```cpp
#define BUILTIN_LED_GPIO        GPIO_NUM_4   // Ganti ke GPIO 4
```

Lalu rebuild dan flash:

```bash
idf.py build && idf.py -p /dev/cu.usbserial-2110 flash
```

### 11.5 Factory Reset

Jika perangkat bermasalah dan ingin reset ke pengaturan awal:

1. Tekan dan tahan tombol BOOT (GPIO 0) selama 10 detik
2. Atau via serial monitor dengan perintah:
   ```
   rm -rf /spiffs/*
   ```
   Lalu restart ESP32.

Perangkat akan kembali ke mode konfigurasi WiFi (AP mode).

---

## 12. Ringkasan Arsitektur Software

```
+-------------------------------------------------------------------+
|                        Application Layer                          |
|  (main/application.cc)                                            |
|  - State Machine (Idle -> Connecting -> Listening -> Speaking)    |
|  - Audio Service (record mic, play TTS)                          |
|  - Protocol (WebSocket/MQTT ke server AI)                        |
+----------+------------------+------------------+------------------+
           |                  |                  |
           v                  v                  v
+----------+-----+  +---------+------+  +-------+--------+
|    Board       |  |     LED        |  |   Display     |
| (CompactWifi   |  | (SimpleGpioLed)|  | (NoDisplay)   |
|  Board)        |  |                |  |               |
| - WiFi connect |  | - OnStateChanged| | - No-op       |
| - Buttons      |  |   GPIO HIGH/LOW|  |               |
| - AudioCodec   |  +----------------+  +---------------+
| - IoT (Lamp)   |
+-------+-------+
        |
        v
+-------+-------------------------------------------+
|              ESP-IDF Framework                     |
|  - I2S Driver (audio in/out)                      |
|  - WiFi Driver                                     |
|  - GPIO Driver (LED, buttons)                      |
|  - FreeRTOS (task scheduling)                      |
|  - ESP-SR (wake word detection - lite engine)     |
+----------------------------------------------------+
        |
        v
+-------+-------------------------------------------+
|              Hardware (ESP32)                      |
|  - CPU: Xtensa dual-core 240MHz                   |
|  - Flash: 4MB                                      |
|  - WiFi: 2.4GHz b/g/n                              |
|  - I2S: 2 bus (simplex mode)                      |
|  - GPIO: 22, 25, 26, 27, 14, 32, 0, 5, 19, 2, 18|
+----------------------------------------------------+
```

---

## 13. Referensi

- **Repository utama:** https://github.com/78/xiaozhi-esp32
- **ESP-IDF Documentation:** https://docs.espressif.com/projects/esp-idf/
- **Server platform:** https://xiaozhi.me
- **MAX98357A Datasheet:** Cari "MAX98357A datasheet" di Google
- **INMP441 Datasheet:** Cari "INMP441 datasheet" di Google

---

## 14. FAQ

### Q: Bisakah menggunakan ESP32-S3?

Bisa, tapi pilih board type yang berbeda di menuconfig (misal `bread-compact-wifi`). ESP32-S3 mendukung lebih banyak fitur seperti WS2812 LED dan AFE wake word engine.

### Q: Bisakah menggunakan codec chip seperti ES8311?

Pada ESP32 klasik, codec I2C seperti ES8311/ES8388 tidak didukung karena di-exclude dari build. Gunakan `NoAudioCodecSimplex` yang langsung menghubungkan I2S ke modul MAX98357A dan INMP441.

### Q: Bisakah mengganti wake word?

Wake word bawaan ESP32 adalah "你好小智" (Ni hao xiao zhi). Konfigurasi ada di `sdkconfig.defaults.esp32`:

```
CONFIG_SR_WN_WN9_NIHAOXIAOZHI_TTS=y
```

Untuk mengubah, perlu ESP-SR model yang berbeda yang tidak selalu tersedia untuk ESP32 klasik.

### Q: Berapa konsumsi daya?

- Standby (idle): ~80-100mA (WiFi connected)
- Listening: ~120-150mA
- Speaking: ~150-200mA

Power supply USB (500mA) sudah cukup. Untuk battery, gunakan 18650 atau LiPo 3.7V dengan boost converter.

### Q: Bisakah berjalan tanpa internet?

Tidak. Xiaozhi memerlukan koneksi internet ke server AI untuk memproses suara dan generate TTS. Semua pengolahan bahasa dilakukan di cloud, bukan di ESP32.

---

*Dokumen ini dibuat untuk board `bread-compact-esp32` dengan modifikasi LED indikator (tanpa OLED display).*

*Tanggal: Juli 2026*
