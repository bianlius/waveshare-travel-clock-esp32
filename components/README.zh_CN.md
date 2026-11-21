<p align="right"><strong>简体中文</strong> · <a href="README.md">English</a></p>

# 共用引擎组件

`GVIEW/` 包含共用渲染器、字体、图片／动画操作及视口计算，两种设备和模拟器通过
`main/shared_sources.txt` 编译同一份源码。

设备的 LCD、SPI 驱动分别放在 `ports/waveshare/components/` 和
`ports/ai-passport/components/`，不复制应用逻辑。目录职责和构建入口见
[工程结构说明](../ports/README.zh_CN.md)。
