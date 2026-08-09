#include "gpio_led.h"
#include "application.h"
#include "device_state.h"
#include <esp_log.h>

#define TAG "GpioLed"

#define DEFAULT_BRIGHTNESS 50
#define HIGH_BRIGHTNESS 100
#define LOW_BRIGHTNESS 10
#define SPEAKING_BRIGHTNESS 75
#define UPGRADING_BRIGHTNESS 25
#define ACTIVATING_BRIGHTNESS 35

#define BLINK_INFINITE -1

// GPIO_LED
#define LEDC_LS_TIMER          LEDC_TIMER_1
#define LEDC_LS_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_LS_CH0_CHANNEL    LEDC_CHANNEL_0

#define LEDC_DUTY              (8191)
#define LEDC_FADE_TIME    (1000)
// GPIO_LED

GpioLed::GpioLed(gpio_num_t gpio)
        : GpioLed(gpio, 0, LEDC_LS_TIMER, LEDC_LS_CH0_CHANNEL) {
}

GpioLed::GpioLed(gpio_num_t gpio, int output_invert)
        : GpioLed(gpio, output_invert, LEDC_LS_TIMER, LEDC_LS_CH0_CHANNEL) {
}

GpioLed::GpioLed(gpio_num_t gpio, int output_invert, ledc_timer_t timer_num, ledc_channel_t channel) {
    // If the gpio is not connected, you should use NoLed class
    assert(gpio != GPIO_NUM_NC);

    /*
     * Prepare and set configuration of timers
     * that will be used by LED Controller
     */
    ledc_timer_config_t ledc_timer = {};
    ledc_timer.duty_resolution = LEDC_TIMER_13_BIT;  // resolution of PWM duty
    ledc_timer.freq_hz = 4000;                      // frequency of PWM signal
    ledc_timer.speed_mode = LEDC_LS_MODE;           // timer mode
    ledc_timer.timer_num = timer_num;               // timer index
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;              // Auto select the source clock

    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_.channel    = channel,
    ledc_channel_.duty       = 0,
    ledc_channel_.gpio_num   = gpio,
    ledc_channel_.speed_mode = LEDC_LS_MODE,
    ledc_channel_.hpoint     = 0,
    ledc_channel_.timer_sel  = timer_num,
    ledc_channel_.flags.output_invert = output_invert & 0x01,

    // Set LED Controller with previously prepared configuration
    ledc_channel_config(&ledc_channel_);

    // Initialize fade service.
    ledc_fade_func_install(0);

    // When the callback registered by ledc_cb_degister is called, run led ->OnFadeEnd()
    ledc_cbs_t ledc_callbacks = {
        .fade_cb = FadeCallback
    };
    ledc_cb_register(ledc_channel_.speed_mode, ledc_channel_.channel, &ledc_callbacks, this);

    esp_timer_create_args_t blink_timer_args = {
        .callback = [](void *arg) {
            auto led = static_cast<GpioLed*>(arg);
            led->OnBlinkTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "Blink Timer",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&blink_timer_args, &blink_timer_));

    esp_timer_create_args_t pulse_timer_args = {
        .callback = [](void *arg) {
            auto led = static_cast<GpioLed*>(arg);
            led->OnPulseTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "Pulse Timer",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&pulse_timer_args, &pulse_timer_));

    xTaskCreate(EventTask, "LedEvent", 2048, this, 
            tskIDLE_PRIORITY + 2, &event_task_handle_);

    ledc_initialized_ = true;
}

GpioLed::~GpioLed() {
    esp_timer_stop(blink_timer_);
    esp_timer_stop(pulse_timer_);
    if (ledc_initialized_) {
        ledc_fade_stop(ledc_channel_.speed_mode, ledc_channel_.channel);
        ledc_fade_func_uninstall();
    }
}


void GpioLed::SetBrightness(uint8_t brightness) {
    if (brightness == 100) {
        duty_ = LEDC_DUTY;
    } else {
        duty_ = brightness * LEDC_DUTY / 100;
    }
}

void GpioLed::TurnOn() {
    if (!ledc_initialized_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    heartbeat_mode_ = false;
    esp_timer_stop(blink_timer_);
    esp_timer_stop(pulse_timer_);
    ledc_fade_stop(ledc_channel_.speed_mode, ledc_channel_.channel);
    ledc_set_duty(ledc_channel_.speed_mode, ledc_channel_.channel, duty_);
    ledc_update_duty(ledc_channel_.speed_mode, ledc_channel_.channel);
}

void GpioLed::TurnOff() {
    if (!ledc_initialized_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    heartbeat_mode_ = false;
    esp_timer_stop(blink_timer_);
    esp_timer_stop(pulse_timer_);
    ledc_fade_stop(ledc_channel_.speed_mode, ledc_channel_.channel);
    ledc_set_duty(ledc_channel_.speed_mode, ledc_channel_.channel, 0);
    ledc_update_duty(ledc_channel_.speed_mode, ledc_channel_.channel);
}

void GpioLed::BlinkOnce() {
    Blink(1, 100);
}

void GpioLed::Blink(int times, int interval_ms) {
    StartBlinkTask(times, interval_ms);
}

void GpioLed::StartContinuousBlink(int interval_ms) {
    StartBlinkTask(BLINK_INFINITE, interval_ms);
}

void GpioLed::StartBlinkTask(int times, int interval_ms) {
    if (!ledc_initialized_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    heartbeat_mode_ = false;
    esp_timer_stop(blink_timer_);
    esp_timer_stop(pulse_timer_);
    ledc_fade_stop(ledc_channel_.speed_mode, ledc_channel_.channel);

    blink_counter_ = times * 2;
    blink_interval_ms_ = interval_ms;
    esp_timer_start_periodic(blink_timer_, interval_ms * 1000);
}

void GpioLed::StartHeartbeat(uint32_t period_ms, uint32_t pulse_ms) {
    if (!ledc_initialized_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    heartbeat_mode_ = true;
    heartbeat_pulse_ms_ = pulse_ms;
    esp_timer_stop(pulse_timer_);
    ledc_fade_stop(ledc_channel_.speed_mode, ledc_channel_.channel);
    ledc_set_duty(ledc_channel_.speed_mode, ledc_channel_.channel, 0);
    ledc_update_duty(ledc_channel_.speed_mode, ledc_channel_.channel);
    esp_timer_stop(blink_timer_);
    esp_timer_start_periodic(blink_timer_, period_ms * 1000);
}

void GpioLed::OnBlinkTimer() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (heartbeat_mode_) {
        // Turn on for a short pulse, then schedule the turn-off.
        ledc_set_duty(ledc_channel_.speed_mode, ledc_channel_.channel, duty_);
        ledc_update_duty(ledc_channel_.speed_mode, ledc_channel_.channel);
        esp_timer_stop(pulse_timer_);
        esp_timer_start_once(pulse_timer_, heartbeat_pulse_ms_ * 1000);
        return;
    }
    blink_counter_--;
    if (blink_counter_ & 1) {
        ledc_set_duty(ledc_channel_.speed_mode, ledc_channel_.channel, duty_);
    } else {
        ledc_set_duty(ledc_channel_.speed_mode, ledc_channel_.channel, 0);

        if (blink_counter_ == 0) {
            esp_timer_stop(blink_timer_);
        }
    }
    ledc_update_duty(ledc_channel_.speed_mode, ledc_channel_.channel);
}

void GpioLed::OnPulseTimer() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (heartbeat_mode_) {
        ledc_set_duty(ledc_channel_.speed_mode, ledc_channel_.channel, 0);
        ledc_update_duty(ledc_channel_.speed_mode, ledc_channel_.channel);
    }
}

void GpioLed::StartFadeTask() {
    if (!ledc_initialized_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    heartbeat_mode_ = false;
    esp_timer_stop(blink_timer_);
    esp_timer_stop(pulse_timer_);
    ledc_fade_stop(ledc_channel_.speed_mode, ledc_channel_.channel);
    fade_up_ = true;
    ledc_set_fade_with_time(ledc_channel_.speed_mode,
                            ledc_channel_.channel, LEDC_DUTY, LEDC_FADE_TIME);
    ledc_fade_start(ledc_channel_.speed_mode,
                    ledc_channel_.channel, LEDC_FADE_NO_WAIT);
}

void GpioLed::OnFadeEnd() {
    std::lock_guard<std::mutex> lock(mutex_);
    fade_up_ = !fade_up_;
    ledc_set_fade_with_time(ledc_channel_.speed_mode,
                            ledc_channel_.channel, fade_up_ ? LEDC_DUTY : 0, LEDC_FADE_TIME);
    ledc_fade_start(ledc_channel_.speed_mode,
                    ledc_channel_.channel, LEDC_FADE_NO_WAIT);
}

bool IRAM_ATTR GpioLed::FadeCallback(const ledc_cb_param_t *param, void *user_arg) {
    if (param->event == LEDC_FADE_END_EVT) {
        auto led = static_cast<GpioLed*>(user_arg);
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR(led->event_task_handle_, 0x01, eSetValueWithOverwrite,
                           &xHigherPriorityTaskWoken);
    }
    return true;
}

void GpioLed::OnStateChanged() {
    auto& app = Application::GetInstance();
    auto device_state = app.GetDeviceState();
    switch (device_state) {
        case kDeviceStateStarting:
            SetBrightness(DEFAULT_BRIGHTNESS);
            StartContinuousBlink(100);
            break;
        case kDeviceStateWifiConfiguring:
            SetBrightness(DEFAULT_BRIGHTNESS);
            StartContinuousBlink(500);
            break;
        case kDeviceStateIdle:
            // Power-efficient heartbeat: a short pulse every 2s instead of a
            // continuously lit LED (~0.05% duty cycle).
            SetBrightness(DEFAULT_BRIGHTNESS);
            StartHeartbeat(2000, 1);
            break;
        case kDeviceStateConnecting:
            SetBrightness(DEFAULT_BRIGHTNESS);
            TurnOn();
            break;
        case kDeviceStateListening:
        case kDeviceStateAudioTesting:
            if (app.IsVoiceDetected()) {
                SetBrightness(HIGH_BRIGHTNESS);
            } else {
                SetBrightness(LOW_BRIGHTNESS);
            }
            // TurnOn();
            StartFadeTask();
            break;
        case kDeviceStateSpeaking:
            SetBrightness(SPEAKING_BRIGHTNESS);
            TurnOn();
            break;
        case kDeviceStateUpgrading:
            SetBrightness(UPGRADING_BRIGHTNESS);
            StartContinuousBlink(100);
            break;
        case kDeviceStateActivating:
            SetBrightness(ACTIVATING_BRIGHTNESS);
            StartContinuousBlink(500);
            break;
        default:
            ESP_LOGE(TAG, "Unknown gpio led event: %d", device_state);
            return;
    }
}

void GpioLed::EventTask(void* arg) {
    GpioLed* led = static_cast<GpioLed*>(arg);

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        led->OnFadeEnd();
    }
}