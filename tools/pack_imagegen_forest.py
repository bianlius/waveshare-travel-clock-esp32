"""Pack the hand-reviewed ImageGen forest PNGs into GVIEW sprite headers.

The PNGs extend only the upper canopy. Each sprite is 240 pixels high and
retains its original width, color palette, and lower trunk/root pixels.
"""

from pathlib import Path
import re

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
HOME = ROOT / "main/pic_data/home"
ART = ROOT / "assets/forest_top"
NAMES = ("senlinB", "treeF0", "treeF1", "treeF2", "treeF3")
HEIGHT = 240
PRESERVED_LOWER_ROWS = 30


def asset_bytes(path: Path) -> bytes:
    return bytes(int(value, 16) for value in re.findall(r"0x[0-9a-fA-F]+", path.read_text()))


def palette_rgb(data: bytes) -> list[tuple[int, int, int]]:
    colors = []
    for index in range(15):
        rgb565 = data[9 + index * 2] << 8 | data[10 + index * 2]
        colors.append(
            (((rgb565 >> 11) & 31) * 255 // 31,
             ((rgb565 >> 5) & 63) * 255 // 63,
             (rgb565 & 31) * 255 // 31)
        )
    return colors


def pixel(data: bytes, kind: int, width: int, x: int, y: int) -> int:
    offset = y * width + x
    packed = data[39:] if kind == 4 else data[9:]
    if kind == 4:
        return (packed[offset >> 1] >> ((offset & 1) * 4)) & 15
    return (packed[offset >> 3] >> (offset & 7)) & 1


def pack(name: str) -> None:
    original = asset_bytes(HOME / f"{name}.h")
    kind = original[0]
    assert kind in (1, 4) and original[1] == 1, name
    width = int.from_bytes(original[2:4], "big")
    old_height = int.from_bytes(original[4:6], "big")
    added_rows = HEIGHT - old_height
    assert added_rows > PRESERVED_LOWER_ROWS, name

    art = Image.open(ART / f"{name}Top.png").convert("RGBA")
    assert art.size == (width, HEIGHT), (name, art.size)
    if kind == 4:
        alpha = art.getchannel("A")
        assert not any(alpha.getpixel((x, 0)) >= 128 for x in range(width)), (name, "top edge")
        for edge_x in (0, width - 1):
            edge_pixels = sum(alpha.getpixel((edge_x, y)) >= 128 for y in range(HEIGHT))
            assert edge_pixels <= 10, (name, edge_x, "cropped side", edge_pixels)
    colors = palette_rgb(original) if kind == 4 else []
    color_to_index = {color: index for index, color in enumerate(colors)}
    packed = bytearray((width * HEIGHT * kind + 7) // 8)

    for y in range(HEIGHT):
        for x in range(width):
            rgba = art.getpixel((x, y))
            opaque = rgba[3] >= 128
            if y >= added_rows + PRESERVED_LOWER_ROWS:
                old_index = pixel(original, kind, width, x, y - added_rows)
                old_opaque = old_index != 15 if kind == 4 else old_index == 1
                assert opaque == old_opaque, (name, x, y, "alpha")
                if kind == 4 and opaque:
                    assert rgba[:3] == colors[old_index], (name, x, y, "color")
                index = old_index
            elif kind == 1:
                index = int(opaque)
            elif opaque:
                assert rgba[:3] in color_to_index, (name, x, y, rgba)
                index = color_to_index[rgba[:3]]
            else:
                index = 15

            offset = y * width + x
            if kind == 4:
                packed[offset >> 1] |= index << ((offset & 1) * 4)
            else:
                packed[offset >> 3] |= index << (offset & 7)

    output = (original[:4] + HEIGHT.to_bytes(2, "big") + original[6:9] +
              (original[9:39] if kind == 4 else b"") + bytes(packed))
    target = f"{name}Top"
    guard = f"__{target.upper()}_H__"
    lines = [f"#ifndef {guard}", f"#define {guard}",
             f"/* ImageGen canopy; width:{width} height:{HEIGHT}. Roots retained from {name}. */",
             f"const unsigned char {target}[] = {{"]
    for start in range(0, len(output), 24):
        chunk = output[start:start + 24]
        suffix = "," if start + 24 < len(output) else ""
        lines.append("    " + ",".join(f"0x{byte:02x}" for byte in chunk) + suffix)
    lines.extend(("};", "#endif", ""))
    (HOME / f"{target}.h").write_text("\n".join(lines))
    print(f"{target}: {width}x{HEIGHT}, {len(output)} bytes")


if __name__ == "__main__":
    for asset in NAMES:
        pack(asset)
