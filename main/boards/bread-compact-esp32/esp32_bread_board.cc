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
#include <esp_timer.h>
#include <driver/gpio.h>

#define TAG "ESP32-MarsbearSupport"

class SimpleGpioLed : public Led {
public:
    static constexpr uint32_t BLINK_TICK_MS = 50;
    // Blink periods (full on/off cycle each)
    static constexpr uint32_t BLINK_FAST_MS = 100;   // starting / upgrading / connecting
    static constexpr uint32_t BLINK_MEDIUM_MS = 300; // speaking
    static constexpr uint32_t BLINK_SLOW_MS = 500;   // wifi configuring / activating

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

        esp_timer_create_args_t args = {
            .callback = &SimpleGpioLed::TimerCb,
            .arg = this,
            .name = "led_blink"
        };
        ESP_ERROR_CHECK(esp_timer_create(&args, &timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(timer_, BLINK_TICK_MS * 1000ULL));
    }

    ~SimpleGpioLed() {
        if (timer_) {
            esp_timer_stop(timer_);
            esp_timer_delete(timer_);
        }
    }

    void OnStateChanged() override {
        auto& app = Application::GetInstance();
        auto state = app.GetDeviceState();
        switch (state) {
            case kDeviceStateStarting:
            case kDeviceStateUpgrading:
            case kDeviceStateConnecting:
                SetPattern(true, BLINK_FAST_MS);   // fast blink
                break;
            case kDeviceStateSpeaking:
                SetPattern(true, BLINK_MEDIUM_MS); // medium blink
                break;
            case kDeviceStateWifiConfiguring:
            case kDeviceStateActivating:
                SetPattern(true, BLINK_SLOW_MS);   // slow blink
                break;
            case kDeviceStateListening:
                SetPattern(true, 0);               // steady on
                break;
            case kDeviceStateIdle:
            default:
                SetPattern(false, 0);              // off
                break;
        }
    }

private:
    void SetPattern(bool on, uint32_t blink_period_ms) {
        steady_level_ = on ? 1 : 0;
        blink_ = blink_period_ms > 0;
        blink_ticks_ = blink_period_ms / BLINK_TICK_MS;
        tick_ = 0;
        level_ = steady_level_;
        SetLevel(level_);
    }

    void TimerCbImpl() {
        if (!blink_) {
            return;
        }
        if (++tick_ >= blink_ticks_) {
            tick_ = 0;
            level_ = !level_;
            SetLevel(level_);
        }
    }

    static void TimerCb(void* arg) {
        static_cast<SimpleGpioLed*>(arg)->TimerCbImpl();
    }

    void SetLevel(bool level) {
        gpio_set_level(gpio_, level ? 1 : 0);
    }

    gpio_num_t gpio_;
    esp_timer_handle_t timer_ = nullptr;
    int level_ = 0;
    int steady_level_ = 0;
    bool blink_ = false;
    uint32_t blink_ticks_ = 0;
    uint32_t tick_ = 0;
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
