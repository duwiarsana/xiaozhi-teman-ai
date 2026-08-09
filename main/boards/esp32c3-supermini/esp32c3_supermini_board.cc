#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "application.h"
#include "button.h"
#include "led/gpio_led.h"
#include "config.h"

#include <esp_log.h>

#define TAG "Esp32C3SuperMiniBoard"

class Esp32C3SuperMiniBoard : public WifiBoard {
private:
    Button boot_button_;

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        boot_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        boot_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });
    }

public:
    Esp32C3SuperMiniBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeButtons();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static NoAudioCodecSimplexSinglePort audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
        return &audio_codec;
    }

    virtual Led* GetLed() override {
        static GpioLed led(BUILTIN_LED_GPIO);
        return &led;
    }
};

DECLARE_BOARD(Esp32C3SuperMiniBoard);
