<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# AI Passport Travel Clock

Independent ESP32-C3 target for FoloToy AI Passport, sharing the current GVIEW
pages, original rider, 61 companions, shop, garden, alarms, settings and network
code with the Waveshare build at the repository root.

Board-only implementation stays in this directory. The shared application,
renderer and source manifest are described in [the port layout](../README.md).
Waveshare adapters live separately in `ports/waveshare/`; neither target copies
the game pages or maintains a separate common-source list.

The ST7789P3 panel uses **320 × 240 landscape** after hardware rotation. The UI
uses the complete width, with original pixel sprites and native fonts. Headers,
status anchors, weather cards and footer hints use the landscape bounds.
“Screen direction” selects either landscape orientation; Passport has no IMU
or touch screen.

Wi-Fi home, scanned/saved lists, network details, connection states and the
password keyboard use the same full landscape viewport. Content has consistent
side margins; wide network rows, all four keyboard categories, password text,
length and action buttons stay inside their own regions. A 63-character password
uses two lines above its length indicator. Wi-Fi geometry lives in this port's
`include/wifi_layout_profile.h`; Waveshare supplies its original geometry through
the same interface, while network state and keyboard behavior remain shared.

Calendar, city
selection, sound/system/brightness/alarm settings and shop layouts use this
port's `include/page_layout_profile.h`. Notifications wrap to the active width.
Sprites and clock digits keep their native size, and the 224-pixel garden floor
stays centred so stored plant positions remain compatible.
The weather page shows three cards for today, tomorrow and the day after, with
aligned temperature/humidity/weekday rows. Waveshare keeps tomorrow and the day
after in two cards; both targets use the same existing three-day forecast data.

## Controls

Controls follow the physical left/middle/right positions while viewing the
landscape display. The original portrait button labels do not select actions.

| Physical key | Waveshare action | Default landscape | Reversed landscape |
| --- | --- | --- | --- |
| Leftmost | Left key | UP | OK |
| Middle | Confirm / middle key | DOWN | DOWN |
| Rightmost | Right key | OK | UP |

Changing screen direction also swaps the side-key mapping. Any key already
held during rotation is cancelled until released, preventing a stale hold or
release from firing another action. The default physical order was checked
against the [official button diagram](https://ai-passport.folotoy.cn/guides/getting-started/)
and the device owner's observation that OK was on the right.

Short/hold behavior follows Waveshare: left/right browse, middle confirms;
on main views, short left opens the shop and short right opens the garden.
A middle click cycles home, calendar, weather, then home.
Side holds do not change the main view. Hold right in the shop to change category and hold
left to return. Hold middle opens the action menu: Settings, Return home and
Home music. It has no shutdown option. Holds trigger at
650 ms on the sides and 1.5 s on the middle key, suppressing the release click. An empty short action promotes the
available hold action, matching the button header.

## Build and preview

Use **ESP-IDF 5.5.3** for ESP32-C3, a C compiler and Python with Pillow.
Audio packing needs `ffmpeg` or `imageio-ffmpeg`; `FFMPEG` can select an executable.
If the IDF Python lacks Pillow, set `PYTHON_FOR_HOST` to a host Python that has it.
No global shell configuration is required. In an activated IDF shell, run from
this directory:

```bash
./tools/validate.sh             # complete gate
./tools/validate.sh --static    # host checks
./tools/validate.sh --firmware  # isolated build, verification and archive
```

This target has independent defaults and a dependency lock. It uses a 16-row,
10 KB RGB565 render buffer, a separate 10 KB DMA staging buffer, native text,
and a 256-object pool. The bounded scene list is prepared once per frame;
numbers and animation transforms are computed once, and later strips skip
images and rectangles outside their rows. Rendering overlaps the preceding DMA
transfer. Staging memory is reused only after completion, and even partial
frames drain their last transfer before returning. Four forest sprites use a
bounded 4 KB row-bounds/palette cache instead of decoded images. Only the packed
pixel loops are placed in IRAM; optional Wi-Fi/PHY IRAM acceleration stays
disabled. Full-screen text/RGB565 double buffers, PSRAM and LVGL are omitted.
CW2017 supplies battery percentage, or an unavailable indication.
All ten complete songs are repacked to 12 kHz mono MP3 at 16 kbit/s; original
audio inputs remain unchanged. The Passport uses the pinned Apache-2.0 microMP3/OpenCore
integer decoder, avoiding minimp3 software floating-point work on C3. Audio files,
sample rate and bitrate are unchanged by this decoder replacement. Waveshare
continues to use its existing decoder. The playback-only BSP avoids unused microphone RX
DMA, and weather responses use a bounded 8 KB buffer allocated only per request.
Button callbacks only queue events, while an
audio worker owns decoding and codec writes. Its 24 KB task stack is checked
against the C3 compiler's decoder/worker stack reports with a 4 KB margin.

Every five seconds, `travel_perf` reports measured FPS, average/worst frame
update duration, remaining DMA wait per frame, scene lookup/draw counts, free
heap and its largest block, plus the playing music slot, volume, decoder busy/peak
time and audio stack headroom. The target is 25 FPS (40 ms per frame). Validate
home animation with music, weather and Wi-Fi/TLS on hardware; host timings and
successful startup alone do not establish the physical frame rate. The host
gate also checks distinct numeric text, nested/masked objects and delayed DMA
ownership through 120 transfers, including partial-frame completion.

On macOS, double-click [Run Passport Simulator.command](Run%20Passport%20Simulator.command),
or run `python3 ports/desktop/build.py --passport` from the repository root.
This app and its saves are separate from the Waveshare simulator. It uses the
device's action dispatcher and strip renderer. Host checks compare strip/full
captures at 320 × 240, including nine categories, seven weather states, a full
30-plant garden, 18 Wi-Fi states and utility pages. It also checks list scrolling,
keyboard actions, long names/passwords and rounded bitmaps wider than 255 pixels.
Additional page states verify target viewports, forms, calendar/city geometry,
alarms and notification wrapping. The source check rejects square viewports
hardcoded in shared pages; Waveshare retains its own square profile.

## Firmware and stored data

The gate retains `build/FoloToy-AI-Passport-full.bin`, matching ELF/MAP, app,
bootloader, table, music and `flash_args`. The verified archive is
`build/firmware/<full-image-sha256>/`; its `manifest.json` binds the build files.

```bash
python tools/archive_firmware.py verify build/firmware/<full-image-sha256>
```

| Region | Offset | Size |
| --- | --- | --- |
| NVS / PHY | `0x9000` / `0xF000` | `0x6000` / `0x1000` |
| App | `0x10000` | `0x300000` |
| Game / Wi-Fi | `0x310000` / `0x311000` | `0x1000` each |
| Protected `cardid` | `0x356000` | `0x4000` |
| Music | `0x35A000` | `0x4A6000` |

The gate checks table MD5, bounds, the 3 MB app limit, protected identity,
all audio payloads, merged bytes and matching ELF. Since music follows `cardid`,
**do not raw-flash the merged image to a provisioned Passport**: padding would
erase identity. Use the verified segmented images at their `flash_args`
offsets; these omit identity, NVS and saves. A compatible later app-only update
at `0x10000` preserves data. Never put an app-only binary at `0x0` or perform a
full-chip erase on a provisioned device.

Building does not flash hardware. Actual panel direction/colors, ADC thresholds,
speaker playback, network RAM pressure, frame rate, sleep/wakeup and retained
data need device acceptance.

The MIT BSP derives from [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport)
and the calibrated ADC/release fixes in [Tama-Go](https://github.com/bianlius/Tama-go-esp32).
Pinned revisions, hashes and license are in `components/bsp/`. Verification and
archive tools derive from the same projects. The Passport test-menu UI is not included.
