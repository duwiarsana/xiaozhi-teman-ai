# ESP32-C3 Super Mini

Firmware for the **ESP32-C3 Super Mini** development board with:

- **MAX98357A** I2S digital amplifier (speaker output)
- **INMP441** I2S digital MEMS microphone (audio input)

## Hardware wiring

### MAX98357A (speaker)

| MAX98357A | ESP32-C3 Super Mini |
|-----------|---------------------|
| VIN       | 3.3V                |
| GND       | GND                 |
| BCLK      | GPIO8               |
| LRC       | GPIO7               |
| DIN       | GPIO10              |
| GAIN      | 不接 (默认 9dB)     |
| SD        | 3.3V                |

> **SD must be tied high to 3.3V** — when left floating/low the amplifier is
> in shutdown and produces no sound. SD high also selects the left channel,
> matching `I2S_STD_SLOT_LEFT` used by the firmware.

### INMP441 (microphone)

| INMP441 | ESP32-C3 Super Mini |
|---------|---------------------|
| VDD     | 3.3V                |
| GND     | GND                 |
| SCK     | GPIO4               |
| WS      | GPIO5               |
| SD      | GPIO6               |
| L/R     | GND (left channel)  |

> The ESP32-C3 has a single I2S controller, but its TX and RX sides have
> SEPARATE BCLK/WS clock signals (I2SO vs I2SI). The speaker TX runs on
> GPIO8/GPIO7 at 24kHz, the mic RX on GPIO4/GPIO5 at 16kHz, independently.
> Do NOT share GPIO8/GPIO7 with the mic — the RX channel init overwrites the
> GPIO matrix routing for those pins and silences the speaker.

### Buttons

| Function    | Pin    |
|-------------|--------|
| BOOT button | GPIO9  |

- 短按: 手动唤醒/停止对话
- 启动时短按: 进入配网模式

### Status LED (external)

| LED   | ESP32-C3 Super Mini |
|-------|---------------------|
| Anode (+ with resistor) | GPIO2 |
| Cathode (-) | GND |

> The onboard blue LED (GPIO8) is reused as the MAX98357A BCLK, so a plain
> external LED is driven on GPIO2 (PWM via LEDC) as the status indicator.

## Build

```bash
idf.py set-target esp32c3
idf.py build
```

Or via the release script:

```bash
python scripts/release.py esp32c3-supermini
```

## Note

The ESP32-C3 has only one I2S controller. This board uses the
`NoAudioCodecSimplexSinglePort` codec, which runs the speaker TX channel and
the mic RX channel on the same I2S port (port 0). Because TX and RX have
separate BCLK/WS signals in the C3 hardware, they use independent GPIO pins
(speaker: GPIO8/GPIO7/GPIO10; mic: GPIO4/GPIO5/GPIO6) and independent sample
rates (24kHz out / 16kHz in).
