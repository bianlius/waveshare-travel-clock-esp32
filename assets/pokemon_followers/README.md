# 首页新增宝可梦素材

七套现成动画和傲骨燕、长翅鸥、可可多拉、金属怪、青绵鸟、脱壳忍者的原始素材表来自 [christt105/PokemonEmeraldIntroVideo](https://github.com/christt105/PokemonEmeraldIntroVideo) 的 `project/pokemon/`，固定到提交 `870520f6161ee521835d2a1bb6b4d037d61c5278`。喵喵、洛奇亚、凤王和比雕来自用户提供的 `firmware-3.4-resources/` 导出目录，按 `comparison.json` 的帧尺寸拆分；四个 PNG 原样复制，原目录不修改。原文件路径、帧尺寸、拆帧数、SHA-256 和派生参数记录在 `manifest.json`。

| 素材 | 帧尺寸与数量 | 原作与整理者 |
| --- | --- | --- |
| 火稚鸡 `torchic_walk.png` | 18×24，4 帧 | Nintendo / Game Freak；christt105 整理 |
| 水跃鱼 `mudkip_walk.png` | 22×22，3 帧 | Nintendo / Game Freak；christt105 整理 |
| 蛇纹熊 `zigzagoon_run.png` | 68×47，7 帧 | JoshR691 绘制；christt105 拆帧、对齐 |
| 雷电兽 `manectric_run.png` | 54×48，4 帧 | Nintendo / Game Freak；christt105 整理 |
| 傲骨燕 `taillow_fly.png` | 40×40，4 帧 | JoshR691 绘制；本地按选定的 steady-flight 版本拆帧、对齐 |
| 长翅鸥 `wingull_fly.png` | 64×56，10 帧 | JoshR691 绘制；本地按选定的 steady-side-flight 版本拆帧、对齐 |
| 电萤虫 `volbeat_fly.png` | 30×28，2 帧 | Nintendo / Game Freak；christt105 整理 |
| 铁面忍者 `ninjask_fly.png` | 34×29，2 帧 | JoshR691 绘制；christt105 拆帧、对齐 |
| 热带龙 `tropius_fly.png` | 190×70，12 帧 | JoshR691 绘制；christt105 拆帧、对齐 |
| 喵喵 `meowth_run.png` | 最大 41×44，4 帧 | 固件 3.4 导出 `02dc44.png` |
| 洛奇亚 `lugia_fly.png` | 最大 124×80，5 帧 | 固件 3.4 导出 `0386cc.png` |
| 凤王 `ho_oh_fly.png` | 最大 91×76，5 帧 | 固件 3.4 导出 `05bb74.png` |
| 比雕 `pidgeot_fly.png` | 最大 68×52，4 帧 | 固件 3.4 导出 `06a74c.png` |
| 可可多拉 `aron_run.png` | 36×34，5 帧 | JoshR691 绘制；本地从原始素材表第三行提取奔跑动作 |
| 金属怪 `metang_hover.png` | 84×76，12 帧 | JoshR691 绘制；本地从原始素材表第三排提取摆臂、收臂动作 |
| 青绵鸟 `swablu_fly.png` | 90×58，7 帧 | JoshR691 绘制；本地从原始素材表拆帧、对齐 |
| 脱壳忍者 `shedinja_hover.png` | 34×38，4 帧 | JoshR691 绘制；本地从原始素材表拆帧、对齐 |

JoshR691 的署名保留在上游同目录的原始素材表中，并注明使用时须署名。本项目的代码许可证不授予这些第三方素材的权利；此处记录来源与作者，不表示获得了额外授权。

安装 Pillow 后，可在工程根目录执行 `python tools/pack_pokemon_followers.py` 重新生成 `main/pic_data/pocketMon/followers.h`。参考项目的十三套动画按帧水平翻转，使伙伴与首页人物朝向一致；固件 3.4 的四套已经向右，无需翻转。火稚鸡和水跃鱼按 140% 最近邻放大，分别打包为 25×34 和 31×31 像素，其他伙伴保持原始尺寸。水跃鱼保留 16 位颜色，固件 3.4 四套保留 RGB454 的全部颜色及八级透明度，其余使用 4 位索引色，不做运行时缩放。预览图和 GIF 输出到 `build/pokemon_followers/`，`--check` 可核对生成文件是否与源码一致。

六张带作者署名的原始素材表原样保存在 `source_sheets/`，完整交接包不修改。运行 `python3 tools/extract_pokemon_sheets.py` 可重新生成六套透明动画条；先修改 manifest 的 `sheet_extract` 中的选区、身体支点及循环顺序，再执行提取和打包。提取只去除原始底色，按完整连通部分排除邻帧边缘，保留脱壳忍者分离的光环。可可多拉按整行动画的共同地面基线对齐，保留腾空与落地起伏；其他五种按身体支点对齐。不重新绘制或缩放。`python3 tools/extract_pokemon_sheets.py --check` 验证动画条和原始素材表、配方一致。

傲骨燕采用用户选定的 `steady-flight.gif`：闭嘴振翅，按上举 → 平展 → 下拍 → 平展的四帧顺序循环。三种姿势使用同一身体支点，消除原三帧循环的约 10 像素头部跳动；源像素和透明区域与获选预览一致。原三帧版本仍在完整交接包的 `pokemon/ready/taillow_fly.png` 中，原路径和 SHA256 保存在 manifest 的 `upstream_ready_*` 字段。存档编号 15、100 金币价格和拥有记录沿用现有设置。

长翅鸥采用用户选定的 `steady-side-flight.gif` 平稳侧飞版本：六种侧身姿势按 `0,1,2,3,4,5,4,3,2,1` 顺序循环，身体按喙部固定点对齐，补齐回程动作。姿势与获选预览一致，每帧 160 毫秒，十帧循环共 1.6 秒；首页往返飞行和商店预览均使用固定慢档，飞行方向不再改变振翅速度。64×56 画布保留完整翅膀，源像素不缩放。原六帧条带和完整原始素材表仍保留在交接包中，原路径、校验和及拆帧配方记录在 manifest；存档编号 16 和 100 金币价格沿用现有设置。

热带龙采用参考工程的完整十二帧飞行动画，按 `0…11 → 0` 持续循环，每帧固定 120 毫秒，一轮 1.44 秒。保留原生 190×70 尺寸和全部源像素，循环不会停在末帧；存档编号 19 和 1200 金币价格不变。原五帧振翅条带及配置保留在 `artifacts/firmware/tropius-fly-loop-review/`，完整交接包中的全部版本不修改。

喵喵使用地面跟随，洛奇亚、凤王、比雕和此前五种飞行伙伴使用飞行跟随。移动范围包含全部帧与锚点的包围范围，并保留四像素边距，展翅帧也完整显示在屏幕内。喵喵的脚底对齐，三套飞行动画按身体位置设置锚点。原生渲染器的对象偏移使用有符号 32 位字段，兼容列表使用的负偏移；动画帧偏移和累计计算使用 32 位，支持超过 64KB 的完整动画。RGB454 按字节读取，支持五帧头部产生的奇数数据偏移。

固件 3.4 素材的存档编号为 20 喵喵、21 洛奇亚、22 凤王、23 比雕；解锁、装备、保存沿用现有流程。固件导出目录未记录四套像素动画的具体绘制者，不能归属到前述 JoshR691 素材。

原始素材表新增伙伴使用存档编号 24 可可多拉、25 金属怪、26 青绵鸟、27 脱壳忍者。可可多拉地面跟随，其余三种悬浮或飞行跟随；旧伙伴及退役编号均保留。

可可多拉使用原始素材表第三行从左到右的连续五帧奔跑动作，每帧 120 毫秒，一轮 0.6 秒；第六个独立姿势不混入循环。全部帧共用原图的地面基线 `y=100`，保留第二帧腾空的高度。顶行旧动作和提取配方保留在 `artifacts/firmware/aron-run-review/`；原始素材表不修改，存档编号 24 和 300 金币价格不变。

金属怪使用原始素材表第三排从左到右的完整七个姿势，按 `0,1,2,3,4,5,6,5,4,3,2,1` 往返播放，表现摆臂、收臂飞行和展开恢复。头部支点对齐，保留完整手臂；固定每帧 160 毫秒，一轮 1.92 秒，往返飞行不切换动作速度。顶行旧悬浮条带与提取配方保留在 `artifacts/firmware/metang-third-row-review/`；原始素材表不修改，存档编号 25 和 2000 金币价格不变。

本作按以下收集稀有档位定价，同档同价。商店继续按价格升序展示；已有的拥有位和装备编号保持原样，改价后仍可直接使用已解锁的伙伴。

| 收集档位 | 伙伴 | 金币 |
| --- | --- | ---: |
| 常见 | 蛇纹熊、傲骨燕、长翅鸥、喵喵 | 100 |
| 较少见 | 鬼斯、皮卡丘、电萤虫、可可多拉、青绵鸟 | 300 |
| 进化伙伴 | 烈焰马、雷电兽、铁面忍者、比雕 | 700 |
| 稀有／御三家 | 风速狗、沙漠蜻蜓、热带龙、火稚鸡、水跃鱼、脱壳忍者 | 1200 |
| 珍稀进化伙伴 | 喷火龙、金属怪 | 2000 |
| 传说 | 炎帝、拉蒂奥斯、拉蒂亚斯 | 3500 |
| 封面传说 | 洛奇亚、凤王 | 5000 |

价格是本作的游戏平衡设定。传说分组参考官方介绍：[炎帝](https://www.pokemon.com/uk/strategy/dominate-battles-with-johto-legendary-pokemon)、[拉蒂兄妹](https://www.pokemon.com/us/pokemon-tcg/product-gallery/dragon-majesty-pin-collections-latias-and-latios)、[洛奇亚与凤王](https://www.pokemon.com/us/news/pokemon-center-celebrates-pokemon-gold-and-pokemon-silver-with-plush)。运行时价格在 `main/PAGE/SHOP/shop.c` 的 `ShopListMonsterCost`，按固定存档编号排列；manifest 同步记录有 `saved_id` 的动画价格。

斗笠菇已从当前游戏及此目录移除。存档编号 14 永久保留为空位，商店遍历时跳过；读取旧存档时清除这一位的拥有记录，正在跟随时改为“不跟随”。原有其他伙伴的编号和购买记录保持不变，新伙伴从编号 15 开始，不会继承斗笠菇的解锁记录。完整交接包仍保留原始历史素材。
