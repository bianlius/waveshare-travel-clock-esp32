<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Shared application and device ports

The application is maintained once. Device ports implement hardware services
and select resources appropriate to their board; they do not carry their own
copies of the game pages, shop rules or saved-data model.

| Location | Responsibility |
| --- | --- |
| `main/PAGE/` | Shared screens, companion behavior, shop, garden and settings |
| `main/app_runtime.c` | Shared application state, home navigation, autosave and alarm checks |
| `main/app_input.c` | Shared left/middle/right actions, hold dispatch and alarm cancellation |
| `main/backlight_schedule.c` | Shared daily brightness selection, midnight wrap and editing preview rules |
| `main/audio_policy.c`, `audio_policy.h` | Shared playback priority, ringtone selection, preview validation and repeat counts |
| `main/rom.c`, `wifi_ssid.c`, `sound_catalog.c`, `pinyin.c` | Shared models and services |
| `main/NET/` | Networking used by both firmware targets |
| `main/pic_data/`, `main/assets/` | Shared source art and music |
| `components/GVIEW/` | Common renderer, fonts and viewport calculations |
| `ports/waveshare/` | ESP32-S3 startup, keys/touch, IMU, LCD/SPI, backlight and audio adapters |
| `ports/ai-passport/` | ESP32-C3 startup, landscape keys/display, battery and fixed-point audio adapters |
| `ports/desktop/` | macOS window and host services; compiles the same application sources |

Shared service headers remain under `main/`. Each device supplies the relevant
implementation, so differences in GPIO, I2S, decoder, frame buffers and screen
direction do not require duplicated application pages. A target's layout or
capability differences may be selected in the common page implementation.

Text layout, clipping and glyph scanning live in
`components/GVIEW/native_text_raster.h`. The Waveshare adapter owns its indexed
text buffer; the Passport adapter writes directly into the current RGB565 strip.
The shared loop is inlined at compile time, without a full-screen allocation or
per-pixel callback.

Devices retain key sampling, debounce, hold timing and orientation conversion,
then use the shared action mapping. Audio adapters retain synchronization,
decoding and physical output; common policy selects tracks and priorities.

Wi-Fi geometry is provided separately by
[`ai-passport/include/wifi_layout_profile.h`](ai-passport/include/wifi_layout_profile.h)
and [`waveshare/include/wifi_layout_profile.h`](waveshare/include/wifi_layout_profile.h).
Common pages include `main/PAGE/WIFI/wifi_layout.h`; target include paths select
the geometry for firmware and desktop alike. List, connection and password
input behavior remain shared.

Other page anchors use each port's `include/page_layout_profile.h`, consumed
through `main/PAGE/page_layout.h`. Calendar columns, city panels, forms, alarm
fields and shop margins therefore use the active target's width. Pixel artwork,
digit glyphs and the garden floor retain their native dimensions; a 240-pixel
height is valid on both landscape targets.

## Adding or changing a feature

Change existing application rules at their shared source. For example, companion
animation changes belong in `main/PAGE/HOME/pkmMovie.c`; shop pricing belongs in
the shared shop sources. Both firmware targets and desktop consume those files.

Register a new common translation unit once in
[`main/shared_sources.txt`](../main/shared_sources.txt):

- `app`: shared pages/models/runtime, compiled by firmware and desktop.
- `network`: production network services for both firmware targets; desktop
  supplies simulated host services.
- `renderer`: common engine sources.
- `viewport`: common viewport source, attached to the appropriate display component.

Both ESP-IDF builds use `cmake/shared_sources.cmake`; the desktop uses
`tools/project_sources.py`. They read the same manifest. Manifest changes also
trigger CMake reconfiguration. `tests/test_shared_sources.py` checks source
uniqueness, page coverage and CMake/Python selection parity.

Add a device-only driver to that port's build file instead of the common
manifest. Keep public service behavior compatible with the shared pages.

## Build entry points

The repository root remains the Waveshare ESP-IDF project. `main/CMakeLists.txt`
delegates to `ports/waveshare/component.cmake`. AI Passport is an independent
project at `ports/ai-passport/`. Desktop builds use
`python3 ports/desktop/build.py` or the same command with `--passport`.

The simulator compiles `main/app_runtime.c` directly; no alarm code is extracted
from a device's entrypoint. Builds, simulator saves and firmware archives remain
separate from shared source assets.
