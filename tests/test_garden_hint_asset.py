"""Decode the packed garden hint using GVIEW's 9-byte sprite header."""

from pathlib import Path
import re


source = (Path(__file__).resolve().parents[1] / "main/pic_data/home/garden_hint.h").read_text()
body = source.split("gardenHint[] = {", 1)[1].split("};", 1)[0]
sprite = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]+)", body))

assert sprite[:6] == bytes((4, 1, 0, 16, 0, 20))
assert len(sprite) == 9 + 30 + 16 * 20 // 2


def color_at(x, y):
    position = y * 16 + x
    index = (sprite[39 + position // 2] >> (4 * (position & 1))) & 15
    if index == 15:
        return None
    rgb = int.from_bytes(sprite[9 + index * 2:11 + index * 2], "big")
    return (((rgb >> 11) & 31) * 255 // 31,
            ((rgb >> 5) & 63) * 255 // 63,
            (rgb & 31) * 255 // 31)


assert color_at(0, 0) is None
assert color_at(4, 1) == (255, 255, 255)
leaf = color_at(4, 5)
assert leaf is not None and leaf[1] > leaf[0] and leaf[1] > leaf[2]
assert color_at(7, 15) is not None

original_text = (Path(__file__).resolve().parents[1] / "main/pic_data/home/garden_miao.h").read_text()
original_body = original_text.split("garden_miao[] = {", 1)[1].split("};", 1)[0]
original = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]+)", original_body))
assert original[:2] == bytes((4, 2))
for y in range(16):
    source_y = ((2 * y + 1) * 25) // 32
    for x in range(12):
        source_x = ((2 * x + 1) * 19) // 24
        position = source_y * 19 + source_x
        index = (original[46 + position // 2] >> (4 * (position & 1))) & 15
        if index == 15:
            continue
        rgb = int.from_bytes(original[16 + index * 2:18 + index * 2], "big")
        expected = (((rgb >> 11) & 31) * 255 // 31,
                    ((rgb >> 5) & 63) * 255 // 63,
                    (rgb & 31) * 255 // 31)
        assert color_at(x + 2, y + 2) == expected

print("PASS: original garden sprout colors and shape, transparent outlined hint")
