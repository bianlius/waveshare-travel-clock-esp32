<p align="right"><strong>简体中文</strong> · <a href="README.md">English</a></p>

# 共用应用与设备适配

应用逻辑只维护一份。各设备目录负责硬件服务和适合该板的资源配置，不另外复制
游戏页面、商店规则或存档模型。

| 位置 | 职责 |
| --- | --- |
| `main/PAGE/` | 共用界面、伙伴行为、商店、花园和设置 |
| `main/app_runtime.c` | 共用应用状态、返回首页、自动保存和闹钟检查 |
| `main/rom.c`、`wifi_ssid.c`、`sound_catalog.c`、`pinyin.c` | 共用数据模型与服务 |
| `main/NET/` | 两种固件共用的网络逻辑 |
| `main/pic_data/`、`main/assets/` | 共用原始图片和音乐 |
| `components/GVIEW/` | 共用渲染器、字体和视口计算 |
| `ports/waveshare/` | ESP32-S3 启动、按键／触摸、IMU、LCD／SPI、背光及音频适配 |
| `ports/ai-passport/` | ESP32-C3 启动、横屏按键／显示、电量及定点音频适配 |
| `ports/desktop/` | macOS 窗口和主机服务，编译同一份应用源码 |

共用服务的接口头文件仍在 `main/`，由各设备提供对应实现。GPIO、I2S、解码器、
帧缓冲和屏幕方向等差异无需复制应用页面；布局或硬件能力差异可在共用页面中按
构建目标选择。

Wi-Fi 页面已将布局参数分到
[`ai-passport/include/wifi_layout_profile.h`](ai-passport/include/wifi_layout_profile.h)
和 [`waveshare/include/wifi_layout_profile.h`](waveshare/include/wifi_layout_profile.h)。
共用页面只包含 `main/PAGE/WIFI/wifi_layout.h`，由设备构建的头文件路径选择参数；
模拟器也按目标使用同一份参数。列表、连接及密码输入逻辑继续共用。

其他页面的布局锚点由各端的 `include/page_layout_profile.h` 提供，通过
`main/PAGE/page_layout.h` 使用。日历列宽、城市面板、设置表单、闹钟字段和商店
边距依照设备宽度排布。像素图片、数字字模和花园草地保留原生尺寸；横屏两端
的高度均为 240 像素，不应将高度或素材尺寸当作宽度问题替换。

## 新增或修改功能

已有玩法直接修改共用源文件。例如，伙伴动画修改 `main/PAGE/HOME/pkmMovie.c`，
定价修改共用商店源码，两种固件和模拟器都会使用同一份实现。

新增共用编译源文件，只需在
[`main/shared_sources.txt`](../main/shared_sources.txt) 登记一次：

- `app`：共用页面、模型和运行逻辑，固件与模拟器均编译。
- `network`：两种固件使用的真实网络服务，模拟器提供模拟主机服务。
- `renderer`：共用引擎源码。
- `viewport`：共用视口源码，由相应显示组件编译。

两种 ESP-IDF 构建通过 `cmake/shared_sources.cmake` 读取清单，模拟器通过
`tools/project_sources.py` 读取同一份清单。修改清单也会触发 CMake 重新配置。
`tests/test_shared_sources.py` 检查文件唯一性、页面覆盖及 CMake／Python 选择一致性。

新增某块板专用的驱动，只修改该设备自己的构建文件，不加入共用清单；实现须遵守
共用页面使用的服务接口。

## 构建入口

仓库根目录仍是微雪 ESP-IDF 工程，`main/CMakeLists.txt` 转交给
`ports/waveshare/component.cmake`。AI Passport 的独立工程位于 `ports/ai-passport/`。
模拟器使用 `python3 ports/desktop/build.py`，Passport 预览添加 `--passport`。

模拟器直接编译 `main/app_runtime.c`，不再从某块板的启动文件中截取闹钟代码。
构建产物、模拟存档和固件归档与共用原始素材分开保存。
