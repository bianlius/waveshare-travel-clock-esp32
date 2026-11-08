# Travel Clock ESP32 组件

- `GVIEW`：本工程使用的 UI 引擎、字体、图片与动画实现。
- `LCD`：微雪 240×240 ST7789 屏幕的初始化、等比例铺满与 SPI 刷屏。
- `SPI`：ESP32-S3 的 LCD 引脚与 SPI 驱动。

这些组件由本目录的 `CMakeLists.txt` 引用，是微雪工程的独立副本；根目录的 `components/` 继续供 ESP32-C3 示例使用。
