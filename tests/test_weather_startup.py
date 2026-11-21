"""Run the production weather worker with an unsynchronized clock and fake I/O."""
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "main/NET/weather_forecast.c").read_text()


def function(signature):
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <time.h>

#define pdTRUE 1
#define pdMS_TO_TICKS(ms) (ms)
#define portENTER_CRITICAL(mux) ((void)(mux))
#define portEXIT_CRITICAL(mux) ((void)(mux))
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static void logSink(const char *tag, const char *format, ...) {
    (void)tag; (void)format;
}
#define ESP_LOGI(...) logSink(__VA_ARGS__)
#define ESP_LOGW(...) logSink(__VA_ARGS__)
#define ESP_LOGE(...) logSink(__VA_ARGS__)
static const char *TAG = "test";
static uint32_t forecastMux, requestSerial, forecastVersion, IPAdressGet;
static uint8_t forecastReady, weatherCode, getweather;
static char weatherTxt[20], tempNetTxt[10];
static int weatherTaskHandle = 1;
typedef struct { char body[16]; } WeatherResponse;
static jmp_buf done;
static unsigned notifications, fetches, waits, stopAfterWait;
static unsigned retryWaits[16], retryCount;
static int succeed, duringFetch, duringWait;
static void *persistentResponse;
static void *heap_caps_malloc(size_t size, int caps) {
    (void)caps;
    persistentResponse = malloc(size);
    return persistentResponse;
}
static void vTaskDelete(void *task) { (void)task; abort(); }
static time_t __attribute__((unused)) fakeTime(time_t *result) {
    if(result) *result = 100;
    return 100; // Cold boot: time synchronization has not succeeded.
}
#define time fakeTime
static void xTaskNotifyGive(int task) { assert(task == 1); ++notifications; }
void weatherForecastRequest(uint8_t clear_previous_city);
static unsigned ulTaskNotifyTake(int clear, unsigned timeout) {
    assert(clear == pdTRUE);
    if(notifications) {
        unsigned count = notifications;
        notifications = 0;
        return count;
    }
    if(!timeout) return 0;
    if(timeout > 1000) retryWaits[retryCount++] = timeout;
    if(duringWait && timeout > 1000) {
        duringWait = 0;
        weatherForecastRequest(0); // Reconnect/city change interrupts backoff.
        notifications = 0;
        return 1;
    }
    if(++waits >= stopAfterWait) longjmp(done, 1);
    return 0;
}
static int weatherFetch(WeatherResponse *response, uint32_t serial) {
    assert(response && IPAdressGet && getweather);
    assert(serial == requestSerial);
    ++fetches;
    if(duringFetch) {
        duringFetch = 0;
        weatherForecastRequest(1); // A stale result must not consume this request.
        return 0;
    }
    return succeed;
}

__REQUEST__
__WORKER__

static void run(unsigned blockingWaits) {
    waits = 0; stopAfterWait = blockingWaits;
    if(setjmp(done) == 0) weatherTask(NULL);
    free(persistentResponse); persistentResponse = NULL;
}
static void reset(void) {
    requestSerial = notifications = fetches = waits = retryCount = 0;
    getweather = succeed = duringFetch = duringWait = 0;
    IPAdressGet = 1234;
}
int main(void) {
    reset(); succeed = 1;
    weatherForecastRequest(0); run(1);
    assert(fetches == 1 && !getweather); // No dependency on clock synchronization.

    reset(); IPAdressGet = 0; weatherForecastRequest(0); run(1);
    assert(fetches == 0 && getweather); // Keep the startup request while offline.
    IPAdressGet = 1234; succeed = 1; weatherForecastRequest(0); run(1);
    assert(fetches == 1 && !getweather);
    run(1); assert(fetches == 1); // Idle: no repeated HTTP requests.

    reset(); weatherForecastRequest(0); run(6);
    const unsigned expected[] = {5000, 10000, 20000, 40000, 60000, 60000};
    assert(fetches == 6 && getweather && retryCount == 6);
    assert(!memcmp(expected, retryWaits, sizeof(expected)));

    reset(); weatherForecastRequest(0); duringWait = 1; run(1);
    assert(fetches == 2 && retryCount == 2);
    assert(retryWaits[0] == 5000 && retryWaits[1] == 5000);

    reset(); weatherForecastRequest(0); duringFetch = 1; succeed = 1; run(1);
    assert(fetches == 2 && !getweather && retryCount == 0);
    assert(weatherCode == 39 && !strcmp(weatherTxt, "--"));
    puts("PASS: immediate weather before clock sync, offline pending request, bounded retries, interrupted backoff and in-flight request replacement");
}
'''
harness = harness.replace("__REQUEST__", function("void weatherForecastRequest("))
harness = harness.replace("__WORKER__", function("static void weatherTask("))
with tempfile.TemporaryDirectory(prefix="gview-weather-startup-") as temporary:
    folder = Path(temporary)
    test, binary = folder / "test.c", folder / "test"
    test.write_text(harness)
    for defines in ([], ["-DCONFIG_GVIEW_AI_PASSPORT=1"]):
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        "-Wno-unused-parameter", "-Wno-unused-function",
                        "-fsanitize=address,undefined", *defines,
                        str(test), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
