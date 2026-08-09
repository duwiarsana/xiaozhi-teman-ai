#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// MAX98357A Speaker (I2S TX, I2S port 0)
#define AUDIO_I2S_SPK_GPIO_BCLK  GPIO_NUM_8
#define AUDIO_I2S_SPK_GPIO_LRCK  GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_DOUT  GPIO_NUM_10

// INMP441 Microphone (I2S RX, also on I2S port 0).
// The ESP32-C3 I2S (HW v2) has SEPARATE BCLK/WS signals for TX (I2SO) and
// RX (I2SI), so the mic uses its OWN pins (GPIO4/GPIO5) at 16kHz while the
// speaker keeps GPIO8/GPIO7 at 24kHz. Do NOT share the same GPIO between
// SPK BCLK/LRCK and MIC SCK/WS: the RX channel init overwrites the GPIO
// matrix routing, killing the speaker clock.
#define AUDIO_I2S_MIC_GPIO_SCK   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_WS    GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN   GPIO_NUM_6

#define BOOT_BUTTON_GPIO        GPIO_NUM_9

// GPIO8 is the onboard blue LED on the C3 SuperMini, but it is reused as
// the MAX98357A BCLK here, so an external status LED is used instead.
#define BUILTIN_LED_GPIO        GPIO_NUM_2

#endif // _BOARD_CONFIG_H_
