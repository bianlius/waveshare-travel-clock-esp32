# 已停用的伙伴素材

这些动画不参与当前商店、首页跟随、模拟器快捷切换或固件打包。每个目录保留动画 PNG 和 `recipe.json`，记录原始来源、作者、校验和、拆帧配方与历史定价。

| 伙伴 | 保留的存档编号 | 素材目录 |
| --- | ---: | --- |
| 蛇纹熊 | 12 | [zigzagoon](zigzagoon/) |
| 电萤虫 | 17 | [volbeat](volbeat/) |
| 喵喵 | 20 | [meowth](meowth/) |
| 洛奇亚 | 21 | [lugia](lugia/) |
| 凤王 | 22 | [ho_oh](ho_oh/) |
| 比雕 | 23 | [pidgeot](pidgeot/) |
| 雷吉艾斯 | 38 | [regice](regice/) |
| 卢卡利欧 | 45 | [lucario](lucario/) |

上述编号和此前停用的斗笠菇编号 14 永久保留为空位。读取旧存档时清除其拥有位；当前跟随已停用伙伴时回到“不跟随”，其他伙伴与存档字段继续沿用。

原始图纸继续保存在 `../source_sheets/`；`source_sheet_preserved` 和动画配方中的路径均相对于 `assets/pokemon_followers/`。完整原始导出目录、历史交接包和本机备份保留原样。当前 `manifest.json` 只登记仍在使用的动画，打包器不会读取此目录。

雷吉艾斯的历史八帧动画与 `recipe.json`、静态第一帧与 `static_recipe.json` 均保存在 [regice](regice/)，编号 38 保留为空位。
