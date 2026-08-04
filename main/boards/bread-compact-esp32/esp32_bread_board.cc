#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/led.h"

#include <esp_log.h>
#include <driver/gpio.h>

#define TAG "ESP32-MarsbearSupport"

class SimpleGpioLed : public Led {
public:
    SimpleGpioLed(gpio_num_t gpio) : gpio_(gpio) {
        gpio_config_t io_conf = {
            .pin_bit_mask = 1ULL << gpio_,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf);
        gpio_set_level(gpio_, 0);
    }

    void OnStateChanged() override {
        auto& app = Application::GetInstance();
        auto state = app.GetDeviceState();
        switch (state) {
            case kDeviceStateListening:
            case kDeviceStateSpeaking:
            case kDeviceStateConnecting:
                gpio_set_level(gpio_, 1);
                break;
            case kDeviceStateStarting:
            case kDeviceStateWifiConfiguring:
            case kDeviceStateUpgrading:
            case kDeviceStateActivating:
                gpio_set_level(gpio_, 1);
                break;
            case kDeviceStateIdle:
            default:
                gpio_set_level(gpio_, 0);
                break;
        }
    }

private:
    gpio_num_t gpio_;
};

class CompactWifiBoard : public WifiBoard {
private:
    Button boot_button_;
    Button touch_button_;
    Button asr_button_;

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        asr_button_.OnClick([this]() {
            std::string wake_word="你好小智";
            Application::GetInstance().WakeWordInvoke(wake_word);
        });

        touch_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        touch_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });
    }

    void InitializeTools() {
        static LampController lamp(LAMP_GPIO);
    }

public:
    CompactWifiBoard() : WifiBoard(), boot_button_(BOOT_BUTTON_GPIO), touch_button_(TOUCH_BUTTON_GPIO), asr_button_(ASR_BUTTON_GPIO)
    {
        InitializeButtons();
        InitializeTools();
    }

    virtual AudioCodec* GetAudioCodec() override 
    {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Led* GetLed() override {
        static SimpleGpioLed led(BUILTIN_LED_GPIO);
        return &led;
    }

};

DECLARE_BOARD(CompactWifiBoard);
