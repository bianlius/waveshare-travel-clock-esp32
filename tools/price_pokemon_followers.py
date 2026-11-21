"""Generate stage-first prices, retaining the nine original companion prices.

Edit assets/pokemon_followers/pricing.json, then run this tool. --check verifies
the C table, animation manifest and documented prices without changing files.
No network access or sprite regeneration is needed.
"""

import argparse
from collections import defaultdict
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets/pokemon_followers"
POLICY = ASSETS / "pricing.json"


def load_pricing():
    policy = json.loads(POLICY.read_text())
    bonuses = {tier["id"]: tier["price_bonus"] for tier in policy["tiers"]}
    stages = {row["stage"]: row["base_price"] for row in policy["stage_prices"]}
    original = {int(saved_id): price for saved_id, price in policy["original_prices"].items()}
    assert set(original) == set(range(1, 10))
    assert set(stages) == {0, 1, 2}
    assert stages[0] == policy["minimum_price"] == 100
    assert list(bonuses.values()) == sorted(set(bonuses.values()))
    assert min(bonuses.values()) == 0
    for stage in range(2):
        assert stages[stage] + max(bonuses.values()) < stages[stage + 1]
    rows = policy["species"]
    ids = [row["saved_id"] for row in rows]
    assert len(ids) == len(set(ids))
    assert set(ids) == set(range(max(ids) + 1)) - set(policy["reserved_ids"])
    assert policy["reserved_ids"] == sorted(set(policy["reserved_ids"]))
    assert policy["reserved_ids"][0] == 0 and policy["reserved_ids"][-1] <= max(ids)
    families = defaultdict(list)
    prices = [0] * (max(ids) + 1)
    for row in rows:
        path = row["evolution_path"]
        assert path[-1] == row["pokedex_id"] and len(path) == len(set(path))
        assert 0 <= row["evolution_stage"] == len(path) - 1 <= 2
        price = original.get(row["saved_id"],
                             stages[row["evolution_stage"]] + bonuses[row["rarity"]])
        assert policy["minimum_price"] <= price <= 65535
        prices[row["saved_id"]] = price
        families[path[0]].append(row)
    for family in families.values():
        assert len({row["rarity"] for row in family}) == 1
    return policy, prices


def pricing_section(policy, prices):
    lines = ["<!-- pokemon-pricing:start -->",
             "原版 9 只伙伴保持参考工程原价和原顺序，排在所有新增伙伴之前；“不跟随”仍在首项。",
             "新增伙伴采用 **进化阶段基础价 + 同阶段稀有度加价**，先分进化阶段，再在阶段内定稀有度；各阶段的价格区间不交叉。",
             "基础形态为第 0 阶，进化一次为第 1 阶，进化两次为第 2 阶；后补幼年形态计入进化路径，无进化形态使用基础阶段。",
             "", "| 进化阶段 | 基础价 | " + " | ".join(t["name_zh"] for t in policy["tiers"]) + " |",
             "| --- | ---: | " + " | ".join("---:" for _ in policy["tiers"]) + " |"]
    for stage in policy["stage_prices"]:
        base = stage["base_price"]
        values = [str(base + tier["price_bonus"]) for tier in policy["tiers"]]
        lines.append(f"| {stage['name_zh']} | {base} | " + " | ".join(values) + " |")
    lines.extend(("", "原版价格来自参考工程 `examples/GameProject_travel_2.5/main/PAGE/SHOP/shop.c`：",
                  "", "| 原版伙伴（展示顺序） | 金币 |", "| --- | ---: |"))
    for row in policy["species"]:
        if str(row["saved_id"]) in policy["original_prices"]:
            lines.append(f"| {row['name_zh']} | {prices[row['saved_id']]} |")
    lines.extend(("", f"当前同链价格示例：木守宫 {prices[29]} → 森林蜥蜴 {prices[30]}；"
                  f"烛光灵 {prices[55]} → 灯火幽灵 {prices[56]}。",
                  "御三家族系统一为稀有档；铁哑铃、波克比、小卡比兽、单首龙、黏黏宝、洛托姆、索罗亚、谜拟丘族系为珍稀档。",
                  "同阶段稀有度从常见到限定珍稀／幻依次加 0、100、300、600、900、1200 金币；原版价格不套用新增规则。",
                  "", "稀有档和金额是本作的收集平衡设定；进化路径按 [PokéAPI 物种数据](https://github.com/PokeAPI/pokeapi/blob/master/data/v2/csv/pokemon_species.csv)记录，"
                  "并参考[官方木守宫图鉴](https://ph.portal-pokemon.com/pokedex/0252/)、[皮卡丘图鉴](https://www.pokemon.com/uk/pokedex/pikachu)、"
                  "[风铃铃图鉴](https://www.pokemon.com/us/pokedex/0358)及[幻之宝可梦介绍](https://www.pokemon.com/us/pokemon-tcg/product-gallery/volcanion-and-magearna-mythical-collections/)。",
                  "", f"全部 {len(policy['species'])} 只伙伴的稀有档、进化路径和固定编号集中在 [`pricing.json`](pricing.json)。"
                  "修改后运行 `python3 tools/price_pokemon_followers.py`，同步生成共用 C 价格表、素材 manifest 和本说明；"
                  "`--check` 检查是否同步。微雪、AI Passport 和模拟器共用同一张价格表。",
                  "", "商店顺序为“不跟随 → 原版 9 只（原顺序）→ 新增伙伴（价格升序）”；新增同价按原编号排列。编号、金币余额与保留伙伴的购买记录不变，不重复收取已购伙伴费用。",
                  "<!-- pokemon-pricing:end -->"))
    return "\n".join(lines)


def outputs(policy, prices):
    shop_path = ROOT / "main/PAGE/SHOP/shop.c"
    shop = shop_path.read_bytes().decode("gb18030")
    names = re.findall(r'"([^"\n]*)"', re.search(
        r"ShopListMonsterTxt\[\]\s*=\s*\{([^}]*)\}", shop).group(1))
    symbols = [x.strip() for x in re.search(
        r"momsterListArr\[\]\s*=\s*\{([^}]*)\}", shop).group(1).split(",")]
    for row in policy["species"]:
        saved_id = row["saved_id"]
        assert names[saved_id] == row["name_zh"] and symbols[saved_id - 1] == row["symbol"]
    shop = re.sub(r"// Rarity tiers:[^\n]*\n// rare evolution[^\n]*\n", "", shop)
    declaration = "const u16 ShopListMonsterCost[] = {" + ",".join(map(str, prices)) + "};"
    shop, count = re.subn(r"const u16 ShopListMonsterCost\[\] = \{[^}]*\};", declaration, shop)
    assert count == 1
    # Keep the definition in this file for existing consumers and source tests.
    marker = "// Generated by tools/price_pokemon_followers.py; edit assets/pokemon_followers/pricing.json.\n"
    shop = shop.replace(marker, "").replace(declaration, marker + declaration)
    result = {shop_path: shop.encode("gb18030")}

    manifest_path = ASSETS / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    by_symbol = {row["symbol"]: row for row in policy["species"]}
    for entry in manifest["assets"]:
        row = by_symbol[entry["symbol"]]
        if "saved_id" in entry:
            assert entry["saved_id"] == row["saved_id"]
        entry["saved_id"] = row["saved_id"]
        entry["price"] = prices[row["saved_id"]]
    result[manifest_path] = (json.dumps(manifest, ensure_ascii=False, indent=2) + "\n").encode()

    readme_path = ASSETS / "README.md"
    readme = readme_path.read_text()
    if "<!-- pokemon-pricing:start -->" in readme:
        readme = re.sub(r"<!-- pokemon-pricing:start -->.*?<!-- pokemon-pricing:end -->",
                        lambda _: pricing_section(policy, prices), readme, flags=re.S)
    else:
        start = readme.index("本作按以下收集稀有档位定价")
        end = readme.index("\n\n斗笠菇已", start)
        readme = readme[:start] + pricing_section(policy, prices) + readme[end:]
    lines = readme.splitlines()
    for i, line in enumerate(lines):
        match = re.match(r"\| (\d+) \|", line)
        if match:
            saved_id = int(match.group(1))
            assert prices[saved_id] > 0
            lines[i] = line.rsplit("|", 2)[0] + f"| {prices[saved_id]} |"
    result[readme_path] = ("\n".join(lines) + "\n").encode()

    readme_path = ROOT / "README.md"
    readme = readme_path.read_text()
    bases = "／".join(str(s["base_price"]) for s in policy["stage_prices"])
    summary = (f"伙伴按进化阶段定价并优先显示原版：原版 9 只恢复参考工程原价和原顺序，“不跟随”置于首项；"
               f"新增伙伴放在原版之后，基础形态／一阶进化／二阶进化的基础价为 {bases} 金币，"
               "同阶段按常见／较少见／稀有／珍稀／传说／限定珍稀与幻的稀有度加 0／100／300／600／900／1200 金币。"
               "新增伙伴按价格升序排列，同价保持原编号顺序，各进化阶段价格区间不交叉。"
               "价格由 [`pricing.json`](assets/pokemon_followers/pricing.json)统一生成，两种设备与模拟器共用；"
               "改价不修改存档编号、金币余额和已有购买记录。")
    readme, count = re.subn(r"伙伴按(?:收集稀有程度|族系收集稀有度|进化阶段).*?(?=陆地伙伴)",
                            lambda _: summary, readme)
    assert count == 1
    result[readme_path] = readme.encode()
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    policy, prices = load_pricing()
    for path, data in outputs(policy, prices).items():
        if args.check:
            if path.read_bytes() != data:
                raise SystemExit(f"Stale price output: {path.relative_to(ROOT)}")
        elif path.read_bytes() != data:
            path.write_bytes(data)
    print(f"PASS: {len(policy['species'])} shared prices; original prices + stage-first added prices; IDs preserved")


if __name__ == "__main__":
    main()
