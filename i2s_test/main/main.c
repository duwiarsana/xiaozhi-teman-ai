#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

static const char *TAG = "i2s_test";

#define BCLK_GPIO GPIO_NUM_8
#define WS_GPIO   GPIO_NUM_7
#define DOUT_GPIO GPIO_NUM_10

#define SAMPLE_RATE 16000
#define TONE_FREQ    1000

void app_main(void) {
    i2s_chan_handle_t tx_chan = NULL;

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = BCLK_GPIO,
            .ws = WS_GPIO,
            .dout = DOUT_GPIO,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
    ESP_LOGI(TAG, "I2S TX enabled: BCLK=%d WS=%d DOUT=%d rate=%d tone=%dHz",
             BCLK_GPIO, WS_GPIO, DOUT_GPIO, SAMPLE_RATE, TONE_FREQ);

    int16_t buf[512];
    const float w = 2.0f * M_PI * TONE_FREQ / SAMPLE_RATE;
    uint32_t n = 0;

    while (1) {
        for (int i = 0; i < 512; i++) {
            buf[i] = (int16_t)(sinf(w * (float)n) * 18000.0f);
            n++;
        }
        size_t written = 0;
        esp_err_t err = i2s_channel_write(tx_chan, buf, sizeof(buf), &written, portMAX_DELAY);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "write failed: %s", esp_err_to_name(err));
        }
    }
}
