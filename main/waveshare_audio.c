#include "waveshare_audio.h"
#include "mp3_player.h"
#include <assert.h>
#include <stdatomic.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define AUDIO_RATE 12000
#define RING_SIZE 4096
static int16_t samples[RING_SIZE];
static volatile uint32_t write_index;
static volatile uint32_t read_index;
static esp_codec_dev_handle_t codec;
static TaskHandle_t output_task_handle;
static SemaphoreHandle_t codec_lock;
static bool codec_open;
static bool mp3_active;
static bool mp3_is_home_bgm;
static atomic_bool home_bgm_output_allowed;
static atomic_uchar requested_volume = 100;
static uint8_t applied_codec_volume = UINT8_MAX;
static esp_codec_dev_sample_info_t codec_format = {
    .sample_rate = AUDIO_RATE,
    .channel = 1,
    .bits_per_sample = 16,
};

static uint8_t codecVolumeForUi(uint8_t volume)
{
    return volume > 100 ? 100 : volume;
}

// Called with codec_lock. UI percentage uses the codec's full 0–100 range.
static void applyVolumeLocked(void)
{
    if (!codec_open) return;
    const uint8_t volume = codecVolumeForUi(atomic_load(&requested_volume));
    if (volume == applied_codec_volume) return;
    int result = esp_codec_dev_set_out_vol(codec, volume);
    assert(result == ESP_CODEC_DEV_OK);
    result = esp_codec_dev_set_out_mute(codec, volume == 0);
    assert(result == ESP_CODEC_DEV_OK);
    applied_codec_volume = volume;
}

static bool mp3OutputAllowed(void)
{
    return !mp3_is_home_bgm || atomic_load(&home_bgm_output_allowed);
}

bool waveshare_audio_sample_from_isr(int16_t sample)
{
    const uint32_t before = (write_index - read_index) & (RING_SIZE - 1);
    uint32_t next = (write_index + 1) & (RING_SIZE - 1);
    if (next != read_index) {
        samples[write_index] = sample;
        write_index = next;
        if (before < 256 && before + 1 >= 256 && output_task_handle) {
            BaseType_t woken = pdFALSE;
            vTaskNotifyGiveFromISR(output_task_handle, &woken);
            return woken == pdTRUE;
        }
    }
    return false;
}

static void output_task(void *arg)
{
    int16_t block[256];
    while (1) {
        xSemaphoreTake(codec_lock, portMAX_DELAY);
        applyVolumeLocked();
        if (codec_open && mp3_active) {
            // Decode in the output task; never hold up the timer ISR or UI task.
            const bool audible = mp3OutputAllowed();
            for (int i = 0; i < 256; ++i) {
                block[i] = audible ? mp3_player_render(100) : 0;
            }
            int result = esp_codec_dev_write(codec, block, sizeof(block));
            xSemaphoreGive(codec_lock);
            if (result != ESP_CODEC_DEV_OK) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            taskYIELD();
            continue;
        }
        xSemaphoreGive(codec_lock);
        if (((write_index - read_index) & (RING_SIZE - 1)) < 256) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }
        for (int i = 0; i < 256; ++i) {
            block[i] = samples[read_index];
            read_index = (read_index + 1) & (RING_SIZE - 1);
        }
        xSemaphoreTake(codec_lock, portMAX_DELAY);
        if (codec_open && !mp3_active)
            (void)esp_codec_dev_write(codec, block, sizeof(block));
        xSemaphoreGive(codec_lock);
    }
}

void waveshare_audio_set_mp3_active(bool active, bool home_bgm)
{
    xSemaphoreTake(codec_lock, portMAX_DELAY);
    mp3_active = active;
    mp3_is_home_bgm = active && home_bgm;
    read_index = write_index;
    xSemaphoreGive(codec_lock);
    xTaskNotifyGive(output_task_handle);
}

void waveshare_audio_set_home_bgm_allowed(bool allowed)
{
    atomic_store(&home_bgm_output_allowed, allowed);
}

void waveshare_audio_set_volume(uint8_t volume)
{
    if (volume > 100) volume = 100;
    atomic_store(&requested_volume, volume);
    if (output_task_handle) xTaskNotifyGive(output_task_handle);
}

void waveshare_audio_set_active(bool active)
{
    xSemaphoreTake(codec_lock, portMAX_DELAY);
    if (active && !codec_open) {
        int result = esp_codec_dev_open(codec, &codec_format);
        assert(result == ESP_CODEC_DEV_OK);
        codec_open = true;
        applied_codec_volume = UINT8_MAX;
        applyVolumeLocked();
    } else if (!active && codec_open) {
        mp3_active = false;
        mp3_is_home_bgm = false;
        int result = esp_codec_dev_close(codec);
        assert(result == ESP_CODEC_DEV_OK);
        codec_open = false;
        applied_codec_volume = UINT8_MAX;
        read_index = write_index;
    }
    xSemaphoreGive(codec_lock);
}

void waveshare_audio_init(i2c_master_bus_handle_t bus)
{
    i2s_chan_handle_t tx;
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan.dma_desc_num = 6;
    chan.dma_frame_num = 240;
    ESP_ERROR_CHECK(i2s_new_channel(&chan, &tx, NULL));
    i2s_std_config_t std = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = GPIO_NUM_8,
            .bclk = GPIO_NUM_9,
            .ws = GPIO_NUM_10,
            .dout = GPIO_NUM_12,
            .din = I2S_GPIO_UNUSED,
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx, &std));

    audio_codec_i2s_cfg_t i2s_cfg = {.port = I2S_NUM_0, .tx_handle = tx};
    const audio_codec_data_if_t *data = audio_codec_new_i2s_data(&i2s_cfg);
    audio_codec_i2c_cfg_t i2c_cfg = {.addr = ES8311_CODEC_DEFAULT_ADDR, .bus_handle = bus};
    const audio_codec_ctrl_if_t *ctrl = audio_codec_new_i2c_ctrl(&i2c_cfg);
    const audio_codec_gpio_if_t *gpio = audio_codec_new_gpio();
    assert(data && ctrl && gpio);
    es8311_codec_cfg_t codec_cfg = {
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .ctrl_if = ctrl,
        .gpio_if = gpio,
        .pa_pin = GPIO_NUM_7,
        .use_mclk = true,
        .hw_gain = {.pa_voltage = 5.0, .codec_dac_voltage = 3.3},
    };
    const audio_codec_if_t *codec_if = es8311_codec_new(&codec_cfg);
    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec_if,
        .data_if = data,
    };
    codec = esp_codec_dev_new(&dev_cfg);
    assert(codec);
    codec_lock = xSemaphoreCreateMutex();
    assert(codec_lock);
    BaseType_t ok = xTaskCreate(output_task, "audio_out", 32768, NULL, 5,
                               &output_task_handle);
    assert(ok == pdPASS);
}
