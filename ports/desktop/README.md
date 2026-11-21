# 旅行时钟电脑模拟器（macOS）

双击 [Run Simulator.command](Run%20Simulator.command)，会编译当前工程并打开窗口；已有模拟器运行时，会先保存并退出，再加载新版本。已编译的应用在 `artifacts/desktop/GVIEW Travel Simulator.app`，也可直接双击运行。需要 macOS 和 Xcode Command Line Tools；使用系统 Cocoa／AVFoundation，不需要 SDL。

模拟器直接编译当前 `main/PAGE/` 页面、GVIEW 渲染器、全部图片和 LCD 原生字体。画面以设备实际的 240×240 像素显示，默认放大两倍。包含原版人物、62 种宝可梦、最低 100 金币的价格，以及商店、森林、花园、日历、天气、计时工具和设置页面。

窗口上方可直接进入各页面。下方三个按钮对应设备的左、中、右键；鼠标按住后释放执行长按动作：两侧按键约 0.65 秒，中键约 1.5 秒，与设备保持一致。

“上一只／下一只”可循环切换全部 62 种宝可梦，中间显示当前名称和序号；点击后直接回到主页查看动画。快捷切换会在本机解锁并装备所选宝可梦，不扣模拟金币，重启后继续使用所选伙伴。

| 操作 | 快捷键 |
| --- | --- |
| 左键、右键、中键 | `←`、`→`、`Enter` |
| 左键或右键长按 | 按住后释放，或 `Shift` 配合方向键 |
| 返回 | `Esc` |
| 暂停／继续整个模拟 | 空格 |
| 进入上方七个页面 | `1`～`7` |
| 切换预览时段、天气 | `D`、`W` |
| 上一只／下一只宝可梦 | `[`、`]` |
| 退出并保存 | 关闭窗口或 `⌘Q` |

三个主画面单击中键（`Enter`）按“首页 → 日历 → 天气 → 首页”循环，左右长按不切页；短按左键进入商店、右键进入花园，长按中键打开操作菜单；微雪包含关机选项，AI Passport 只提供设置、返回主界面和首页音乐。商店短按左右键浏览、中键解锁或装备，右键长按切换分类，左键长按返回。还可点击游戏画面顶部的按键区域，或在三个主画面中左右拖动切页。

首次启动提供 10 万模拟金币、原版人物及青绵鸟，方便检查动画；“+5万金币”可补充模拟金币。声音默认静音，点击“声音：开”可播放工程里的 MP3；声音页仍可调节音量、选择曲目和试听。

网络、天气、电量、温度和姿态是模拟值；时钟使用电脑时间，场景默认白天，可独立切换时段。蓝牙／USB、真实 Wi-Fi 连接及传感器不由此程序控制。存档使用正式结构和读写逻辑，保存在 `~/Library/Application Support/GVIEW Travel Simulator/`，与设备分开；恢复出厂只清空模拟器自己的设置和花园文件。启动器和模拟器均不执行刷机。

命令行构建与后台检查：

```bash
python3 ports/desktop/build.py
"artifacts/desktop/GVIEW Travel Simulator.app/Contents/MacOS/GVIEW-Simulator" --smoke artifacts/desktop/check
```

内存检查构建：

```bash
GVIEW_ASAN=1 python3 ports/desktop/build.py
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
  "artifacts/desktop/GVIEW Travel Simulator.app/Contents/MacOS/GVIEW-Simulator" --smoke artifacts/desktop/check-asan
python3 ports/desktop/build.py
```

检查覆盖九个页面、森林、全部 62 种伙伴、原生字体、最低售价、实际购买与服饰预览回调、本地存档重读，以及包内音频目录；截帧位于各自的检查目录。构建时将音频一并放入应用包，运行时从包内读取。

模拟器与两种固件通过同一份 `main/shared_sources.txt` 选择公共源文件，并直接编译
`main/app_runtime.c`；不另外维护页面清单或截取硬件入口中的闹钟实现。板级职责与
新增功能约定见[工程结构说明](../README.zh_CN.md)。
