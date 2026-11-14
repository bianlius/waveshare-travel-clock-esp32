# 第三方代码与素材说明

本项目的许可范围见 [LICENSE](LICENSE)。原工程的 [MIT 许可证](LICENSES/GVIEW-Engine-MIT.txt)仍适用于原工程代码；本项目权利人拥有的新增内容按 [PolyForm Noncommercial 1.0.0](LICENSES/PolyForm-Noncommercial-1.0.0.md) 提供。第三方组件保留各自的许可。以上许可证均**不授予**下列音乐、角色或其他第三方美术素材的使用权。

| 内容 | 位置 | 来源与许可 |
| --- | --- | --- |
| GVIEW 原工程代码与资源 | `components/`、`main/` 中移植的部分 | [GVIEW-Engine](https://github.com/zhengjunlang-bit/GVIEW-Engine)，保留原工程的 MIT 版权声明 |
| minimp3 解码器 | `main/third_party/minimp3/` | [minimp3](https://github.com/lieff/minimp3)，CC0；许可文本见该目录的 `LICENSE` |
| ESP Codec Dev | `managed_components/espressif__esp_codec_dev/` | Espressif，Apache-2.0；许可文本见该目录的 `LICENSE` |
| ESP Codec Dev 内的 Microsemi 代码 | `managed_components/espressif__esp_codec_dev/device/zl38063/` | MIT；许可文本见该目录的 `LICENSE.txt` |
| 天气数据接口 | 运行时获取，不随仓库打包 | [Open-Meteo](https://open-meteo.com/)；数据来源与接口使用条件见其网站 |

## 音乐与图像

`main/assets/track_00.mp3` 至 `track_09.mp3` 是由 [Pokémon Red](https://www.zophar.net/music/gameboy-gbs/pokemon-red)、[Pokémon Gold](https://www.zophar.net/music/gameboy-gbs/pokemon-gold) 与 [Pokémon Sapphire](https://www.zophar.net/music/gameboy-advance-gsf/pokemon-sapphire) 的游戏音乐存档转码所得。各文件对应的源曲目和下载路径记录在 `tools/prepare_mp3.py` 中。

`main/pic_data/` 和相关图片资源包含从原工程移植、改编的游戏角色与像素图像。音乐和这些第三方图像的再分发授权未在本仓库得到确认；列出来源只用于说明素材出处，不表示素材已经获得开放许可。使用、修改或再分发这些素材前，请自行取得所需授权。

新增的火稚鸡、水跃鱼、蛇纹熊、雷电兽、傲骨燕、长翅鸥、电萤虫、铁面忍者、热带龙、可可多拉、金属怪、青绵鸟和脱壳忍者素材取自 [christt105/PokemonEmeraldIntroVideo](https://github.com/christt105/PokemonEmeraldIntroVideo)。蛇纹熊、傲骨燕、长翅鸥、铁面忍者、热带龙和本次拆帧的可可多拉、金属怪、青绵鸟、脱壳忍者由 **JoshR691** 绘制，其余四套的原始游戏美术归 Nintendo / Game Freak；christt105 提供参考项目及现成动画条带。傲骨燕采用本地对齐后的四帧振翅版本，长翅鸥采用本地对齐并补齐回程的十帧侧飞版本；六张原始素材表的作者署名完整保留在 `assets/pokemon_followers/source_sheets/`。固定来源版本、文件校验和及处理方式见 [素材说明](assets/pokemon_followers/README.md)。这些动画不属于本项目 PolyForm 非商业许可授予的原创内容。

小遥已从当前游戏和商店移除。历史七层资源、角色造型、绘图输入与生成器保留在 `emerald-restoration-handoff-2026-11-13-current/`；最近的美术编辑与参考图另存于本地 `artifacts/retired-characters/may-2026-11-14-before-removal.zip`，不参与当前固件编译。角色设计及原始精灵的权利属于 Nintendo / Game Freak；这些历史素材不属于本项目代码许可证授予的原创内容。

停用的脸部表情参考图来自 [pret/pokeemerald](https://github.com/pret/pokeemerald) 的原版开场、片尾骑行精灵（固定提交 `731ad5bfd6e6f265508d0efcca0ba42f9dcf5881`），原始文件和来源校验和随上述本地备份保留。

喵喵、洛奇亚、凤王、比雕的动画从用户提供的 `firmware-3.4-resources/` 原样导入，原始导出文件名与校验和记录在 `assets/pokemon_followers/manifest.json`。角色设计属于 Nintendo / Game Freak；导出资料未记录具体像素动画绘制者。这四套素材不属于本项目代码许可证授予的原创内容。
