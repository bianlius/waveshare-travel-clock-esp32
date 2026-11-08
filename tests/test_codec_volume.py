"""Check the full codec volume range, live changes, mute, and reopen behavior."""

from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / "main/waveshare_audio.c").read_text()


def function(signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


assert "block[i] = audible ? mp3_player_render(100) : 0;" in source
assert "applyVolumeLocked();" in function("static void output_task(void *arg)")

production = "\n".join(function(signature) for signature in (
    "static uint8_t codecVolumeForUi(uint8_t volume)",
    "static void applyVolumeLocked(void)",
    "static bool mp3OutputAllowed(void)",
    "void waveshare_audio_set_home_bgm_allowed(bool allowed)",
    "void waveshare_audio_set_volume(uint8_t volume)",
    "void waveshare_audio_set_active(bool active)",
))

harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#define ESP_CODEC_DEV_OK 0
#define portMAX_DELAY 0
static atomic_uchar requested_volume = 100;
static atomic_bool home_bgm_output_allowed;
static uint8_t applied_codec_volume = UINT8_MAX;
static bool codec_open, mp3_active, mp3_is_home_bgm;
static uint32_t read_index,write_index;
static int codec=1,codec_lock=1,codec_format,output_task_handle=1;
static int opens,closes,volume_writes,mute_writes,notifications,locks;
static int last_volume=-1;
static bool muted;
static void xSemaphoreTake(int lock,int wait){ (void)lock;(void)wait;++locks; }
static void xSemaphoreGive(int lock){ (void)lock; }
static void xTaskNotifyGive(int task){ (void)task;++notifications; }
static int esp_codec_dev_open(int handle,int *format){
    assert(handle==codec && format==&codec_format); ++opens; return ESP_CODEC_DEV_OK;
}
static int esp_codec_dev_close(int handle){
    assert(handle==codec); ++closes; return ESP_CODEC_DEV_OK;
}
static int esp_codec_dev_set_out_vol(int handle,int volume){
    assert(handle==codec && volume>=0 && volume<=100);
    last_volume=volume; ++volume_writes; return ESP_CODEC_DEV_OK;
}
static int esp_codec_dev_set_out_mute(int handle,bool state){
    assert(handle==codec); muted=state; ++mute_writes; return ESP_CODEC_DEV_OK;
}
__PRODUCTION__
int main(void){
    mp3_is_home_bgm=true;
    assert(!mp3OutputAllowed());
    waveshare_audio_set_home_bgm_allowed(true);
    assert(mp3OutputAllowed());
    waveshare_audio_set_home_bgm_allowed(false);
    assert(!mp3OutputAllowed());
    mp3_is_home_bgm=false;
    assert(mp3OutputAllowed()); // Alarm playback must work off the home page.
    waveshare_audio_set_volume(50);
    assert(!locks && !volume_writes && notifications==1);
    waveshare_audio_set_active(true);
    assert(opens==1 && last_volume==50 && !muted && volume_writes==1);
    waveshare_audio_set_volume(0);
    assert(last_volume==50 && volume_writes==1 && locks==1); // UI is nonblocking.
    applyVolumeLocked();
    assert(last_volume==0 && muted && volume_writes==2);
    waveshare_audio_set_volume(100);
    applyVolumeLocked();
    assert(last_volume==100 && !muted && volume_writes==3);
    applyVolumeLocked();
    assert(volume_writes==3 && mute_writes==3);
    waveshare_audio_set_volume(255);
    applyVolumeLocked();
    assert(last_volume==100 && volume_writes==3);
    waveshare_audio_set_volume(5);
    applyVolumeLocked();
    assert(last_volume==5 && volume_writes==4);
    waveshare_audio_set_active(false);
    assert(closes==1 && !codec_open && applied_codec_volume==UINT8_MAX);
    waveshare_audio_set_active(true);
    assert(opens==2 && last_volume==5 && volume_writes==5);
    puts("PASS: full codec volume range, mute, live adjustment, and reopen");
}
'''

with tempfile.TemporaryDirectory(prefix="gview-codec-volume-") as folder:
    test = Path(folder) / "test.c"
    binary = Path(folder) / "test"
    test.write_text(harness.replace("__PRODUCTION__", production), encoding="utf-8")
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", str(test), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
