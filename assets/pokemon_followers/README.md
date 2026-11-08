# 首页新增宝可梦素材

这些 PNG 来自 [christt105/PokemonEmeraldIntroVideo](https://github.com/christt105/PokemonEmeraldIntroVideo) 的 `project/pokemon/`，固定到提交 `870520f6161ee521835d2a1bb6b4d037d61c5278`。原文件、帧尺寸、拆帧数和 SHA-256 记录在 `manifest.json`，PNG 保持来源文件内容不变。

| 素材 | 帧尺寸与数量 | 原作与整理者 |
| --- | --- | --- |
| 火稚鸡 `torchic_walk.png` | 18×24，4 帧 | Nintendo / Game Freak；christt105 整理 |
| 水跃鱼 `mudkip_walk.png` | 22×22，3 帧 | Nintendo / Game Freak；christt105 整理 |
| 蛇纹熊 `zigzagoon_run.png` | 68×47，7 帧 | JoshR691 绘制；christt105 拆帧、对齐 |
| 雷电兽 `manectric_run.png` | 54×48，4 帧 | Nintendo / Game Freak；christt105 整理 |
| 斗笠菇 `breloom_run.png` | 60×74，10 帧 | JoshR691 绘制；christt105 拆帧、对齐 |

JoshR691 的署名保留在上游同目录的原始蛇纹熊、斗笠菇素材表中，并注明使用时须署名。本项目的代码许可证不授予这些第三方素材的权利；此处记录来源与作者，不表示获得了额外授权。

安装 Pillow 后，可在工程根目录执行 `python tools/pack_pokemon_followers.py` 重新生成 `main/pic_data/pocketMon/followers.h`。生成时按帧水平翻转，使伙伴与首页人物朝向一致；保持原始像素尺寸、透明区域和 RGB565 颜色，不做运行时缩放。水跃鱼保留 16 位颜色，其余使用 4 位索引色。预览图和 GIF 输出到 `build/pokemon_followers/`，`--check` 可核对生成文件是否与源码一致。
