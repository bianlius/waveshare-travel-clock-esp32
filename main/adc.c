#include "PAGE/INPUTTXT/inputtxt.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/usb_serial_jtag.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "LCD/backLight.h"
#include "adc.h"
#include "rom.h"
#include "sound.h"
#include "PAGE/pageDraw.h"
#include "LCD/lcd_init.h"

#define TOUCH_ADDR 0x15
#define KEY_COUNT 5
#define USB_BOOT_GUARD_US 2000000
static i2c_master_bus_handle_t touch_bus;
static i2c_master_dev_handle_t touch_dev;
static adc_oneshot_unit_handle_t battery_adc;
static adc_cali_handle_t battery_cali;
static bool battery_calibrated;
static bool shutting_down;

// Temporary input trace for diagnosing a BOOT long press during USB attach.
// Keep events in RAM while USB is detached, then replay them after enumeration.
#define INPUT_TRACE_CAPACITY 24
typedef struct {
    int64_t time_ms;
    const char *event;
    int page;
    bool boot_low;
    bool usb_connected;
} input_trace_event_t;
static input_trace_event_t input_trace[INPUT_TRACE_CAPACITY];
static unsigned input_trace_next;
static unsigned input_trace_count;
static int64_t input_trace_last_dump_us;

static void input_trace_record(const char *event, int64_t now_us,
                               bool boot_low, bool usb_connected)
{
    input_trace[input_trace_next] = (input_trace_event_t){
        .time_ms = now_us / 1000,
        .event = event,
        .page = pageNow,
        .boot_low = boot_low,
        .usb_connected = usb_connected,
    };
    input_trace_next = (input_trace_next + 1) % INPUT_TRACE_CAPACITY;
    if (input_trace_count < INPUT_TRACE_CAPACITY) ++input_trace_count;
}

static void input_trace_dump(int64_t now_us, bool boot_low, bool usb_connected)
{
    if (!usb_connected || now_us - input_trace_last_dump_us < 5000000) return;
    input_trace_last_dump_us = now_us;
    printf("INPUT_TRACE snapshot t=%lld page=%d boot=%d usb=%d count=%u\n",
           (long long)(now_us / 1000), pageNow, boot_low, usb_connected,
           input_trace_count);
    const unsigned start = (input_trace_next + INPUT_TRACE_CAPACITY - input_trace_count)
                           % INPUT_TRACE_CAPACITY;
    for (unsigned i = 0; i < input_trace_count; ++i) {
        const input_trace_event_t *item = &input_trace[(start + i) % INPUT_TRACE_CAPACITY];
        printf("INPUT_TRACE event t=%lld name=%s page=%d boot=%d usb=%d\n",
               (long long)item->time_ms, item->event, item->page,
               item->boot_low, item->usb_connected);
    }
}

static void shutdown_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(350));
    writeData();
    for (int level = backLightNum; level >= 0; --level) {
        updateBackLight(level);
        vTaskDelay(pdMS_TO_TICKS(55));
    }
    while (!gpio_get_level(GPIO_NUM_5)) vTaskDelay(pdMS_TO_TICKS(20));
    ESP_ERROR_CHECK(esp_sleep_enable_ext0_wakeup(GPIO_NUM_5, 0));
    gpio_set_level(GPIO_NUM_2, 0);
    esp_deep_sleep_start();
}

void requestPowerOff(void)
{
    if (shutting_down) return;
    shutting_down = true;
    showTips("POWER OFF", 0);
    if (xTaskCreate(shutdown_task, "power_off", 4096, NULL, 5, NULL) != pdPASS)
        shutting_down = false;
}

uint16_t KEY0Temp, KEY1Temp, KEY2Temp, KEY3Temp, KEY4Temp;
uint16_t KEY5Temp, KEY6Temp, KEY7Temp;
uint8_t KEYCLDOWNTemp, KEYCRDOWNTemp;
uint8_t longRDown, longLDown, roundStateGet;
uint16_t KeyBtn_Val;
uint16_t VoltageVal;

static bool touch_point(int *x, int *y)
{
    uint8_t reg = 0x02;
    uint8_t data[5] = {0};
    if (!touch_dev || i2c_master_transmit_receive(touch_dev, &reg, 1, data, sizeof(data), 20) != ESP_OK)
        return false;
    if ((data[0] & 0x0f) == 0) return false;
    *x = ((data[1] & 0x0f) << 8) | data[2];
    *y = ((data[3] & 0x0f) << 8) | data[4];
    if(*x < 0 || *x >= 240 || *y < 0 || *y >= 240) return false;
    LCD_MapTouchToLogical(x, y);
    return true;
}

static int touch_key_at(int x,int y)
{
    if(x<0 || x>=WS_UI_WIDTH || y<0) return -1;
    const bool main_view=pageNow==PAGE_HOME || pageNow==PAGE_CALENDAR ||
                         pageNow==PAGE_CLOCK || pageNow==PAGE_WEATHER;
    if(main_view) return -1;
    const int headerY=stage.movieclipArr[moveClipMaxNum-1].y;
    if(softKeyHeaderVisible && y>=headerY && y<headerY+WS_UI_HEADER_HEIGHT){
        return pageUIKeyAction(x/WS_UI_KEY_WIDTH);
    }
    return -1;
}

enum { MAIN_SWIPE_NEXT=5, MAIN_SWIPE_PREVIOUS=6 };

static int content_gesture_key(int sx, int sy, int ex, int ey)
{
    const int dx = ex - sx;
    const int dy = ey - sy;
    const bool main_view = pageNow == PAGE_HOME || pageNow == PAGE_CALENDAR ||
                           pageNow == PAGE_CLOCK || pageNow == PAGE_WEATHER;
    const int headerY=stage.movieclipArr[moveClipMaxNum-1].y;
    if(!main_view && softKeyHeaderVisible && sy>=headerY && sy<headerY+WS_UI_HEADER_HEIGHT)
        return -1; // Empty header cells must not become a generic confirm tap.
    if (abs(dx) > 25 && abs(dx) > 2 * abs(dy)) {
        if (main_view) return dx < 0 ? MAIN_SWIPE_NEXT : MAIN_SWIPE_PREVIOUS;
        if (pageNow == PAGE_MENU || pageNow == PAGE_POWER_MENU)
            return dx < 0 ? 0 : 2;
        return -1;
    }
    // Main views change only on horizontal swipes; taps have no page action.
    if (main_view) return -1;
    if(pageReturnHintVisible() && sy>=WS_UI_FOOTER_TOP) return -1;
    if (dy < -20) return 0;
    if (dy > 20) return 2;
    if (pageNow == PAGE_MENU) {
        if (sx < 80) return 2;
        if (sx >= 160) return 0;
    }
    if (pageNow == PAGE_SHOP || pageNow == PAGE_GARDENSHOP) return -1;
    return 1;
}

void keyCheckC(void)
{
    static bool contact;
    static bool ignore_touch_until_release;
    static uint8_t touch_rotation;
    static int start_x, start_y, last_x, last_y, active_touch_key = -1;
    static int64_t pwr_press_us;
    static int64_t left_press_us, right_press_us;
    static bool left_main_press, right_main_press, pwr_main_press;
    static bool left_long_handled, right_long_handled, pwr_long_handled;
    uint16_t *held[KEY_COUNT] = {&KEY0Temp, &KEY1Temp, &KEY2Temp, &KEY3Temp, &KEY4Temp};
    bool down[KEY_COUNT] = {0};
    int x = 0, y = 0, gesture_key = -1;
    bool power_click = false;
    int left_action = -1, right_action = -1;
    if (shutting_down) { keyState = 0; return; }
    const bool touching = touch_point(&x, &y);
    const uint8_t rotation = LCD_GetRotation();
    if(rotation != touch_rotation){
        touch_rotation = rotation;
        contact = false;
        active_touch_key = -1;
        ignore_touch_until_release = touching;
        KEY0Temp = KEY1Temp = KEY2Temp = KEY3Temp = KEY4Temp = 0;
    }
    if(!touching) ignore_touch_until_release = false;
    if (touching && !ignore_touch_until_release) {
        if (!contact) {
            contact = true;
            start_x = x;
            start_y = y;
            active_touch_key = touch_key_at(x, y);
        }
        last_x = x;
        last_y = y;
        if (active_touch_key >= 0) down[active_touch_key] = true;
    } else if (contact) {
        if(active_touch_key<0){
            if(pageNow==PAGE_WIFI_PW)
                inputPageGesture(start_x,start_y,last_x,last_y);
            else
                gesture_key=content_gesture_key(start_x,start_y,last_x,last_y);
        }
        contact = false;
        active_touch_key = -1;
    }

    /* Short actions occur on release; long actions occur at the threshold. */
    const int64_t now = esp_timer_get_time();
    const bool main_view = pageNow == PAGE_HOME || pageNow == PAGE_CALENDAR ||
                           pageNow == PAGE_CLOCK || pageNow == PAGE_WEATHER;
    const bool left_down = !gpio_get_level(GPIO_NUM_0);
    const bool right_down = !gpio_get_level(GPIO_NUM_4);
    const bool usb_connected = usb_serial_jtag_is_connected();
    static bool boot_guard_initialized, prior_usb_connected, boot_wait_for_release;
    static int64_t boot_guard_until_us;
    if (!boot_guard_initialized) {
        boot_guard_initialized = true;
        if (usb_connected) boot_guard_until_us = now + USB_BOOT_GUARD_US;
        boot_wait_for_release = left_down || usb_connected;
    } else if (usb_connected && !prior_usb_connected) {
        boot_guard_until_us = now + USB_BOOT_GUARD_US;
        boot_wait_for_release = true;
    }
    prior_usb_connected = usb_connected;
    if (boot_wait_for_release && !left_down && now >= boot_guard_until_us)
        boot_wait_for_release = false;
    const bool boot_blocked = boot_wait_for_release || now < boot_guard_until_us;
    const bool left_action_down = left_down && !boot_blocked;
    if (boot_blocked) {
        left_press_us = 0;
        left_long_handled = false;
    }
    static bool input_trace_initialized;
    static bool previous_left_down, previous_usb_connected;
    if (!input_trace_initialized) {
        input_trace_initialized = true;
        input_trace_record("INITIAL", now, left_down, usb_connected);
    } else {
        if (usb_connected != previous_usb_connected)
            input_trace_record(usb_connected ? "USB_CONNECT" : "USB_DISCONNECT",
                               now, left_down, usb_connected);
        if (left_down != previous_left_down)
            input_trace_record(left_down ? "BOOT_DOWN" : "BOOT_UP",
                               now, left_down, usb_connected);
    }
    previous_left_down = left_down;
    previous_usb_connected = usb_connected;
    // Main views: BOOT opens shop, PLUS opens garden; holds change view.
    if (left_action_down) {
        if (!left_press_us) {
            left_press_us = now;
            left_main_press = main_view;
            left_long_handled = false;
        }
        if (!left_long_handled && now - left_press_us >= 650000) {
            left_long_handled = true;
            left_action = 3;
            input_trace_record("BOOT_LONG", now, left_down, usb_connected);
        }
    } else if (left_press_us) {
        if (!left_long_handled)
            left_action = left_main_press && main_view ? 2 :
                          softKeyHeaderVisible ? pageUIKeyAction(0) : 2;
        left_press_us = 0;
    }
    if (right_down) {
        if (!right_press_us) {
            right_press_us = now;
            right_main_press = main_view;
            right_long_handled = false;
        }
        if (!right_long_handled && now - right_press_us >= 650000) {
            right_long_handled = true;
            right_action = 4;
        }
    } else if (right_press_us) {
        if (!right_long_handled)
            right_action = right_main_press && main_view ? 0 :
                           softKeyHeaderVisible ? pageUIKeyAction(2) : 0;
        right_press_us = 0;
    }
    if (!gpio_get_level(GPIO_NUM_5)) {
        if (!pwr_press_us) {
            pwr_press_us = now;
            pwr_main_press = main_view;
        }
        if (!pwr_long_handled && now - pwr_press_us >= 1500000) {
            pwr_long_handled = true;
            keyState = 0;
            if(pageNow == PAGE_SOUND_SET) saveSoundSettingsIfDirty();
            if(pageNow == PAGE_CLOCKCOUNT_SET) saveTimerSoundSettingsIfDirty();
            if(pageNow == PAGE_SYSTEM_SET) saveSystemSettingsIfDirty();
            if(pageNow == PAGE_CLOCK_SET || pageNow == PAGE_BGLIGHT_SCHEDULE)
                leaveClockSettings();
            renderPowerMenuInit();
            return;
        }
    } else if (pwr_press_us) {
        pwr_press_us = 0;
        power_click = !pwr_long_handled && !pwr_main_press && !main_view;
        pwr_long_handled = false;
    }

    keyState = 0;
    for (int i = 0; i < KEY_COUNT; ++i) {
        if (down[i]) {
            if (*held[i] == 0) keyState |= 1 << (i * 2);
            if (*held[i] < UINT16_MAX) ++*held[i];
        } else {
            if (*held[i] > keySureTime) keyState |= 1 << (i * 2 + 1);
            *held[i] = 0;
        }
    }
    if(softKeyHeaderVisible && (gesture_key==0 || gesture_key==2))
        gesture_key=pageUIKeyAction(gesture_key==2 ? 0 : 2);
    // Main-view swipes use release callbacks so they remain separate from
    // the physical short presses that open the shop and garden.
    if (gesture_key == MAIN_SWIPE_NEXT) keyState |= 1 << 1; // KMU1
    else if (gesture_key == MAIN_SWIPE_PREVIOUS) keyState |= 1 << 5; // KMU3
    else if (gesture_key >= 0) keyState |= 1 << (gesture_key * 2);
    if (left_action >= 0) keyState |= 1 << (left_action * 2);
    if (right_action >= 0) keyState |= 1 << (right_action * 2);
    if (power_click) keyState |= 1 << (1 * 2);
    if (keyState & (1 << (3 * 2)))
        input_trace_record("KML_DISPATCH", now, left_down, usb_connected);
    input_trace_dump(now, left_down, usb_connected);
    if (keyState) {
        backHomeTime = 120;
        if (aClockTimes) {
            aClockTimes = 0;
            hideTips();
            keyState = 0;
            soundStopAll();
        }
    }
}

void adcInit(void)
{
    gpio_config_t buttons = {
        .pin_bit_mask = (1ULL << 0) | (1ULL << 4) | (1ULL << 5),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&buttons));
    gpio_config_t power = {.pin_bit_mask = 1ULL << 2, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&power));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_2, 1));

    adc_oneshot_unit_init_cfg_t unit = {.unit_id = ADC_UNIT_1};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit, &battery_adc));
    adc_oneshot_chan_cfg_t channel = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};
    ESP_ERROR_CHECK(adc_oneshot_config_channel(battery_adc, ADC_CHANNEL_0, &channel));
    adc_cali_curve_fitting_config_t cal = {
        .unit_id = ADC_UNIT_1, .chan = ADC_CHANNEL_0,
        .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    battery_calibrated = adc_cali_create_scheme_curve_fitting(&cal, &battery_cali) == ESP_OK;

    gpio_config_t reset = {.pin_bit_mask = 1ULL << 47, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&reset));
    gpio_set_level(GPIO_NUM_47, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(GPIO_NUM_47, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_42,
        .scl_io_num = GPIO_NUM_41,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &touch_bus));
    esp_err_t touch_probe = i2c_master_probe(touch_bus, TOUCH_ADDR, 100);
    printf("CST816 touch probe: %s\n", esp_err_to_name(touch_probe));
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TOUCH_ADDR,
        .scl_speed_hz = 400000,
    };
    if (touch_probe == ESP_OK && i2c_master_bus_add_device(touch_bus, &dev_cfg, &touch_dev) != ESP_OK)
        touch_dev = NULL;
}

i2c_master_bus_handle_t waveshare_i2c_bus(void) { return touch_bus; }

void adc_task(void *arg)
{
    snprintf(tempTxt, 10, "--");
    while (1) {
        int raw = 0, mv = 0;
        if (adc_oneshot_read(battery_adc, ADC_CHANNEL_0, &raw) == ESP_OK) {
            if (!battery_calibrated || adc_cali_raw_to_voltage(battery_cali, raw, &mv) != ESP_OK)
                mv = raw * 3300 / 4095;
            VoltageVal = (uint16_t)(mv * 3);
        }
        // The home battery indicator refreshes once a minute. Ten seconds
        // is sufficient for voltage tracking without waking the ADC each second.
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
